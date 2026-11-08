#!/usr/bin/env python3
# goose_cuda_adapter_moe.py - mechanical Goose-C -> CUDA qualifier adapter for
# the MoE routing block.  Same method as the frozen GDN adapter: emitted helper
# bodies are kept unchanged, host emitter wrappers are removed, strict device
# primitives are defined before the body, and no shared global scratch remains.
#
# kind=sort emits a phase kernel that walks the native bitonic stage order
# (init, (k,j) stages, copy) and calls the unchanged Goose entry after each
# __syncthreads().  kind=reduce emits a one-shot kernel.
import re, sys

RUNTIME = r'''
#include <cuda_runtime.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
typedef struct { void *data; int64_t len; } gg_slice;
typedef struct { unsigned char *base; int64_t len; } gg_header;
typedef struct { unsigned char *base; size_t top, committed; } gg_stack;
typedef struct { gg_header *header; gg_stack *stack; } gg_reference;
static __device__ __forceinline__ void gg_fail(const char *message) {
    (void) message; asm volatile("trap;");
}
static __device__ __forceinline__ gg_slice gg_view(void *p, int64_t n) {
    gg_slice v; v.data = p; v.len = n; return v;
}
static __device__ __forceinline__ int64_t gg_index(int64_t i, int64_t len) {
    return (i >= 0 && i < len) ? i : 0;
}
static __device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b) {
    return b ? (a / b) : 0;
}
static __device__ __forceinline__ int64_t gg_mod(int64_t a, int64_t b) {
    if (!b) return 0;
    int64_t r = a % b;
    return r < 0 ? r + (b < 0 ? -b : b) : r;
}
'''

SORT_PRIMS = r'''
#include "moe-routing/fg153-moe-abi.h"
extern __shared__ int fg153_moe_sh[];
static __device__ __forceinline__ int64_t fg153_moe_cfg_i64(int64_t c, int64_t k) {
    return ((const int64_t *)(uintptr_t) c)[k];
}
static __device__ __forceinline__ float fg153_moe_load(int64_t p, int64_t i) {
    return ((const float *)(uintptr_t) p)[i];
}
static __device__ __forceinline__ void fg153_moe_store_i64(int64_t p, int64_t i, int64_t v) {
    ((int *)(uintptr_t) p)[i] = (int) v;
}
static __device__ __forceinline__ int64_t fg153_moe_shload(int64_t i) {
    return (int64_t) fg153_moe_sh[i];
}
static __device__ __forceinline__ void fg153_moe_shstore(int64_t i, int64_t v) {
    fg153_moe_sh[i] = (int) v;
}
static __device__ __forceinline__ int64_t fg153_moe_tid(void) {
    return (int64_t) threadIdx.x;
}
static __device__ __forceinline__ int64_t fg153_moe_bdim(void) {
    return (int64_t) blockDim.x;
}
static __device__ __forceinline__ int64_t fg153_moe_bidx(int64_t a) {
    if (a == 0) return (int64_t) blockIdx.x;
    if (a == 1) return (int64_t) blockIdx.y;
    return (int64_t) blockIdx.z;
}
static __device__ __forceinline__ int64_t fg153_moe_reject(int64_t c, int64_t code) {
    ((fg153_moe_sort_cfg *)(uintptr_t) c)->v[FG153_MOE_ERROR] = code;
    return code;
}
'''

REDUCE_PRIMS = r'''
#include "moe-routing/fg153-moe-abi.h"
static __device__ __forceinline__ int64_t fg153_mr_cfg_i64(int64_t c, int64_t k) {
    return ((const int64_t *)(uintptr_t) c)[k];
}
static __device__ __forceinline__ float fg153_mr_load(int64_t p, int64_t i) {
    return ((const float *)(uintptr_t) p)[i];
}
static __device__ __forceinline__ void fg153_mr_store(int64_t p, int64_t i, float v) {
    ((float *)(uintptr_t) p)[i] = v;
}
static __device__ __forceinline__ int64_t fg153_mr_tid(void) {
    return (int64_t) threadIdx.x;
}
static __device__ __forceinline__ int64_t fg153_mr_bdim(void) {
    return (int64_t) blockDim.x;
}
static __device__ __forceinline__ int64_t fg153_mr_bidx(int64_t a) {
    if (a == 0) return (int64_t) blockIdx.x;
    if (a == 1) return (int64_t) blockIdx.y;
    return (int64_t) blockIdx.z;
}
static __device__ __forceinline__ int64_t fg153_mr_reject(int64_t c, int64_t code) {
    ((fg153_moe_reduce_cfg *)(uintptr_t) c)->v[FG153_MR_ERROR] = code;
    return code;
}
'''

SORT_WRAPPER = r'''
extern "C" __global__ void __KERNEL__(__CFG__ cfg) {
    cfg.v[FG153_MOE_PHASE] = 0;
    (void) gg_function_1((int64_t)(uintptr_t)&cfg, 0);
    __syncthreads();
    const int64_t npad = cfg.v[FG153_MOE_NPAD];
    for (int64_t kk = 2; kk <= npad; kk *= 2) {
        for (int64_t jj = kk / 2; jj > 0; jj /= 2) {
            cfg.v[FG153_MOE_PHASE] = 1;
            cfg.v[FG153_MOE_KK] = kk;
            cfg.v[FG153_MOE_JJ] = jj;
            (void) gg_function_1((int64_t)(uintptr_t)&cfg, 0);
            __syncthreads();
        }
    }
    cfg.v[FG153_MOE_PHASE] = 2;
    (void) gg_function_1((int64_t)(uintptr_t)&cfg, 0);
}
'''

REDUCE_WRAPPER = r'''
extern "C" __global__ void __KERNEL__(__CFG__ cfg) {
    (void) gg_function_1((int64_t)(uintptr_t)&cfg, 0);
}
'''

def remove_block(text, start_re):
    m = re.search(start_re, text, re.M)
    if not m:
        return text
    i = m.start()
    j = text.find("\n}\n", m.end())
    if j < 0:
        raise SystemExit("unterminated block for " + start_re)
    return text[:i] + text[j + 3:]

def main():
    if len(sys.argv) != 6:
        raise SystemExit("usage: goose_cuda_adapter_moe.py IN.c OUT.cu sort|reduce KERNEL CFGTYPE")
    src, out, kind, kernel, cfgtype = sys.argv[1:]
    text = open(src, "r", encoding="utf-8").read()
    types = re.findall(r"^typedef struct \{ [^\n]*\} gg_type_\d+;\s*$", text, re.M)
    marker = "} gg_globals;"
    k = text.find(marker)
    if k < 0:
        raise SystemExit("emitted C has no gg_globals marker")
    body = text[k + len(marker):]
    for name in ("gg_function_0", "gg_program"):
        body = remove_block(body, r"^static [^\n;]*\b" + name + r"\([^\n;]*\)\s*\{")
        body = re.sub(r"^static [^\n;]*\b" + name + r"\([^\n;]*\);\s*\n", "", body, flags=re.M)
    body = remove_block(body, r"^int main\([^\n;]*\)\s*\{")
    if "gg_stacks" in body or "gg_get_stack" in body:
        raise SystemExit("emitted body still uses shared global gg_stacks")
    sig = re.search(r"^static int64_t gg_function_1\(([^)]*)\);", body, re.M)
    if not sig or sig.group(1).count(",") != 1 or "int64_t" not in sig.group(1):
        raise SystemExit("gg_function_1 is not the expected 2-arg i64 entry")
    body = re.sub(r"^static ", "static __device__ ", body, flags=re.M)
    prims = SORT_PRIMS if kind == "sort" else REDUCE_PRIMS
    wrapper = SORT_WRAPPER if kind == "sort" else REDUCE_WRAPPER
    wrapper = wrapper.replace("__KERNEL__", kernel).replace("__CFG__", cfgtype)
    with open(out, "w", encoding="utf-8") as f:
        f.write("// Generated by goose_cuda_adapter_moe.py from %s; arithmetic unchanged.\n" % src)
        f.write(RUNTIME)
        f.write(prims)
        for t in types:
            f.write(t + "\n")
        f.write(body)
        f.write(wrapper)

if __name__ == "__main__":
    main()
