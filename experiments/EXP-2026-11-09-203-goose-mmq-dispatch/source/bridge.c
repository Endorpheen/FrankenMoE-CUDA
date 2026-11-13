// Bridge for the Goose dispatch-policy module. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_calls;

__attribute__((destructor)) static void fg203d_report(void) {
    if (!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG203D_MMQ_DISPATCH calls=%llu\n", atomic_load(&g_calls));
}

static void g_fail(int64_t c) {
    fprintf(stderr, "FG203D validation code %lld\n", (long long)c);
    abort();
}

#define fg203d_fail g_fail
#define main fg203d_generated_main
#include "dispatch-generated.c"
#undef main

int64_t fg203d_type_supported(int64_t type) { return gg_function_1(type, 0); }

// gg_function_2 is the only 11-argument body in this module, so it is unambiguous.
int64_t fg203d_should_use_mmq(int64_t type, int64_t cc, int64_t ne11, int64_t n_experts,
                              int64_t smpbo, int64_t highest_arch, int64_t turing_mma,
                              int64_t fp16_mma_hw, int64_t mfma, int64_t wmma,
                              int64_t force_cublas, int64_t force_mmq) {
    atomic_fetch_add(&g_calls, 1);
    return gg_function_2(type, cc, ne11, n_experts, smpbo, highest_arch, turing_mma,
                         fp16_mma_hw, mfma, wmma, force_cublas, force_mmq, 0);
}

int fg203d_enabled(void) {
    const char * f = getenv("GGML_GOOSE_CUDA_MMQ_DISPATCH");
    return !(f && strcmp(f, "0") == 0);
}
