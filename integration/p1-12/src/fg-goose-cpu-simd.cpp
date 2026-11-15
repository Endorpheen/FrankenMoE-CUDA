// Runtime switch for the four CPU MoE block dot products (P1-12 integration).
// See fg-goose-cpu-simd.h for the contract and for why nb must be 1.
//
// Two ways to reach the Goose kernels:
//   FG_GOOSE_CPU_SIMD_OBJECT  the delivered C ABI object (cpu/production/blocks.o)
//   no object                 dlopen("libfg-cpu-simd.so") if the loader has it
// Both are optional. Without either, the switch stays inactive and every call
// takes the native arm, which is the shipped behaviour of a plain build.

#include "fg-goose-cpu-simd.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

struct KernelSet {
    fg_goose_dot_fn q4_k_q8_k;
    fg_goose_dot_fn q6_k_q8_k;
    fg_goose_dot_fn q8_0_q8_0;
    fg_goose_dot_fn q4_0_q8_0;
    int available;
};

// The delivered object exposes these as plain C symbols. Declaring them weak
// lets a build that does not link the object still compile and run natively.
extern "C" {
__attribute__((weak)) int32_t fg_cpu_simd_q4_k_q8_k(fg_cpu_simd_bytes, fg_cpu_simd_bytes, int32_t *, float *);
__attribute__((weak)) int32_t fg_cpu_simd_q6_k_q8_k(fg_cpu_simd_bytes, fg_cpu_simd_bytes, int32_t *, float *);
__attribute__((weak)) int32_t fg_cpu_simd_q8_0_q8_0(fg_cpu_simd_bytes, fg_cpu_simd_bytes, int32_t *, float *);
__attribute__((weak)) int32_t fg_cpu_simd_q4_0_q8_0(fg_cpu_simd_bytes, fg_cpu_simd_bytes, int32_t *, float *);
__attribute__((weak)) uint64_t fg_cpu_simd_features(void);
}

int env_truthy(const char * name) {
    const char * v = getenv(name);
    if (v == nullptr || v[0] == '\0') {
        return 0;
    }
    return !(strcmp(v, "0") == 0 || strcmp(v, "false") == 0 || strcmp(v, "no") == 0 ||
             strcmp(v, "off") == 0 || strcmp(v, "FALSE") == 0 || strcmp(v, "NO") == 0 ||
             strcmp(v, "OFF") == 0);
}

const KernelSet & kernels() {
    static KernelSet ks = [] {
        KernelSet k{};
        if (fg_cpu_simd_q4_k_q8_k && fg_cpu_simd_q6_k_q8_k &&
            fg_cpu_simd_q8_0_q8_0 && fg_cpu_simd_q4_0_q8_0) {
            k.q4_k_q8_k = fg_cpu_simd_q4_k_q8_k;
            k.q6_k_q8_k = fg_cpu_simd_q6_k_q8_k;
            k.q8_0_q8_0 = fg_cpu_simd_q8_0_q8_0;
            k.q4_0_q8_0 = fg_cpu_simd_q4_0_q8_0;
            k.available = 1;
            return k;
        }

        // No object linked: try a shared library of the same ABI.
        void * h = dlopen("libfg-cpu-simd.so", RTLD_NOW | RTLD_LOCAL);
        if (h != nullptr) {
            k.q4_k_q8_k = (fg_goose_dot_fn) dlsym(h, "fg_cpu_simd_q4_k_q8_k");
            k.q6_k_q8_k = (fg_goose_dot_fn) dlsym(h, "fg_cpu_simd_q6_k_q8_k");
            k.q8_0_q8_0 = (fg_goose_dot_fn) dlsym(h, "fg_cpu_simd_q8_0_q8_0");
            k.q4_0_q8_0 = (fg_goose_dot_fn) dlsym(h, "fg_cpu_simd_q4_0_q8_0");
            k.available = k.q4_k_q8_k && k.q6_k_q8_k && k.q8_0_q8_0 && k.q4_0_q8_0;
        }
        return k;
    }();
    return ks;
}

long g_calls = 0;

// Shared body of the four wrappers: the layouts differ only in block size.
int dispatch(int nb, const void * vx, const void * vy, float * value,
             fg_goose_dot_fn fn, int64_t bytes_x, int64_t bytes_y) {
    if (!fg_goose_cpu_simd_active() || fn == nullptr || nb != 1) {
        return 0;
    }
    int32_t minimum = 0;
    float out = 0.0f;
    fn((fg_cpu_simd_bytes) { (const uint8_t *) vx, bytes_x },
       (fg_cpu_simd_bytes) { (const uint8_t *) vy, bytes_y },
       &minimum, &out);
    *value = out;
    __atomic_add_fetch(&g_calls, 1, __ATOMIC_RELAXED);
    return 1;
}

} // namespace

extern "C" int fg_goose_cpu_simd_active(void) {
    // Rule 10: unset means the Goose kernel answers; the variable only disables.
    static const int native = env_truthy("FG_GOOSE_CPU_SIMD_NATIVE") ? 1 : 0;
    return native ? 0 : 1;
}

extern "C" int fg_goose_cpu_simd_available(void) {
    return kernels().available;
}

extern "C" long fg_goose_cpu_simd_calls(void) {
    return __atomic_load_n(&g_calls, __ATOMIC_RELAXED);
}

// Block sizes of the ggml layouts (ggml-common.h); q8_K is 292 bytes, which is
// not 8 * q8_0 and must not be derived from it.
#define FG_BLOCK_Q4_K 144
#define FG_BLOCK_Q6_K 210
#define FG_BLOCK_Q8_K 292
#define FG_BLOCK_Q8_0 34
#define FG_BLOCK_Q4_0 18

extern "C" int fg_goose_dot_q4_k_q8_k(int nb, const void * vx, const void * vy, float * value) {
    return dispatch(nb, vx, vy, value, kernels().q4_k_q8_k, FG_BLOCK_Q4_K, FG_BLOCK_Q8_K);
}

extern "C" int fg_goose_dot_q6_k_q8_k(int nb, const void * vx, const void * vy, float * value) {
    return dispatch(nb, vx, vy, value, kernels().q6_k_q8_k, FG_BLOCK_Q6_K, FG_BLOCK_Q8_K);
}

extern "C" int fg_goose_dot_q8_0_q8_0(int nb, const void * vx, const void * vy, float * value) {
    return dispatch(nb, vx, vy, value, kernels().q8_0_q8_0, FG_BLOCK_Q8_0, FG_BLOCK_Q8_0);
}

extern "C" int fg_goose_dot_q4_0_q8_0(int nb, const void * vx, const void * vy, float * value) {
    return dispatch(nb, vx, vy, value, kernels().q4_0_q8_0, FG_BLOCK_Q4_0, FG_BLOCK_Q8_0);
}
