#!/usr/bin/env python3
# goose_cuda_adapter.py - mechanical Goose-C -> CUDA qualifier adapter.
#
# The frozen Goose compiler emits ordinary scalar C for one module entry
# (gg_function_1).  This adapter keeps that emitted function body unchanged
# and only adds the mechanical CUDA environment:
#   * a small device runtime (bounds-checked index, fail trap, int division),
#   * the module's device/thread/load/store primitives,
#   * __device__ qualifiers on the emitted helper functions,
#   * a __global__ wrapper that passes the ABI config by value.
# It never rewrites arithmetic, loops, or the recurrence.  Host-only emitter
# wrappers (gg_function_0, gg_program, main) are removed; the emitted body is
# rejected if it would keep a shared global scratch reset (gg_stacks).
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
static __device__ __forceinline__ uint64_t gg_udiv(uint64_t a, uint64_t b) {
    return b ? (a / b) : 0;
}
static __device__ __forceinline__ uint64_t gg_umod(uint64_t a, uint64_t b) {
    return b ? (a % b) : 0;
}
'''

GDN_PRIMS = r'''
#include "fg153-gdn-abi.h"
static __device__ __forceinline__ int64_t fg153_gdn_cfg_i64(int64_t c, int64_t k) {
    return ((const int64_t *)(uintptr_t) c)[k];
}
static __device__ __forceinline__ float fg153_gdn_cfg_f32(int64_t c, int64_t k) {
    return ((const fg153_gdn_cfg *)(uintptr_t) c)->f[k];
}
static __device__ __forceinline__ float fg153_gdn_load(int64_t p, int64_t i) {
    return ((const float *)(uintptr_t) p)[i];
}
static __device__ __forceinline__ void fg153_gdn_store(int64_t p, int64_t i, float v) {
    ((float *)(uintptr_t) p)[i] = v;
}
static __device__ __forceinline__ int64_t fg153_gdn_tid(void) {
    return (int64_t) threadIdx.x;
}
static __device__ __forceinline__ int64_t fg153_gdn_bidx(int64_t a) {
    if (a == 0) return (int64_t) blockIdx.x;
    if (a == 1) return (int64_t) blockIdx.y;
    return (int64_t) blockIdx.z;
}
static __device__ __forceinline__ int64_t fg153_gdn_reject(int64_t c, int64_t code) {
    ((fg153_gdn_cfg *)(uintptr_t) c)->v[FG153_GDN_ERROR] = code;
    return code;
}
'''

CONV_PRIMS = r'''
#include "fg153-gdn-abi.h"
extern __shared__ float fg153_conv_smem[];
static __device__ __forceinline__ int64_t fg153_conv_cfg_i64(int64_t c, int64_t k) {
    return ((const int64_t *)(uintptr_t) c)[k];
}
static __device__ __forceinline__ float fg153_conv_load(int64_t p, int64_t i) {
    return ((const float *)(uintptr_t) p)[i];
}
static __device__ __forceinline__ void fg153_conv_store(int64_t p, int64_t i, float v) {
    ((float *)(uintptr_t) p)[i] = v;
}
static __device__ __forceinline__ float fg153_conv_shload(int64_t i) {
    return fg153_conv_smem[i];
}
static __device__ __forceinline__ void fg153_conv_shstore(int64_t i, float v) {
    fg153_conv_smem[i] = v;
}
static __device__ __forceinline__ void fg153_conv_sync(void) { __syncthreads(); }
static __device__ __forceinline__ int64_t fg153_conv_tid(void) {
    return (int64_t) threadIdx.x;
}
static __device__ __forceinline__ int64_t fg153_conv_bidx(int64_t a) {
    if (a == 0) return (int64_t) blockIdx.x;
    if (a == 1) return (int64_t) blockIdx.y;
    return (int64_t) blockIdx.z;
}
static __device__ __forceinline__ int64_t fg153_conv_reject(int64_t c, int64_t code) {
    ((fg153_conv_cfg *)(uintptr_t) c)->v[FG153_CONV_ERROR] = code;
    return code;
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
        raise SystemExit("usage: goose_cuda_adapter.py IN.c OUT.cu MODULE KERNEL CFGTYPE")
    src, out, module, kernel, cfgtype = sys.argv[1:]
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
        raise SystemExit("emitted body still resets/uses shared global gg_stacks")
    sig = re.search(r"^static int64_t gg_function_1\(([^)]*)\);", body, re.M)
    if not sig or sig.group(1).count(",") != 1 or "int64_t" not in sig.group(1):
        raise SystemExit("gg_function_1 is not the expected 2-arg i64 entry")
    body = re.sub(r"^static ", "static __device__ ", body, flags=re.M)
    prims = GDN_PRIMS if module == "gdn" else CONV_PRIMS
    with open(out, "w", encoding="utf-8") as f:
        f.write("// Generated by goose_cuda_adapter.py from %s; arithmetic unchanged.\n" % src)
        f.write(RUNTIME)
        f.write(prims)
        for t in types:
            f.write(t + "\n")
        f.write(body)
        f.write('\nextern "C" __global__ void %s(%s cfg) {\n' % (kernel, cfgtype))
        f.write('    (void) gg_function_1((int64_t)(uintptr_t)&cfg, 0);\n')
        f.write("}\n")

if __name__ == "__main__":
    main()
