// Adapter for the Goose Q4_Kx8 AVX2 repack provider.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

extern float fmaf(float, float, float);
static float fg143_half(uint16_t v) { return _cvtsh_ss(v); }
static float fg143_from_bits(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
static uint32_t fg_q8k_float_bits(float v) { uint32_t u; memcpy(&u, &v, 4); return u; }
static atomic_uint_fast64_t rp_calls[4], rp_blocks[4];
static void fg006_repack_witness(int64_t mode, int64_t blocks) {
    if (mode < 0 || mode >= 4 || blocks < 0) __builtin_trap();
    atomic_fetch_add_explicit(rp_calls + mode, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(rp_blocks + mode, (uint64_t) blocks, memory_order_relaxed);
}

#define main fg006_repack_unused_main
#include "repack-generated.c"
#undef main

static int fg006_repack_selected=1;
__attribute__((constructor)) static void fg006_repack_config(void) {
 const char *v=getenv("GGML_GOOSE_CPU_REPACK"),*legacy=getenv("FG006_REPACK_ROLLBACK");
 fg006_repack_selected=(!v || strcmp(v,"0")!=0) && (!legacy || strcmp(legacy,"1")!=0);
}
int fg006_repack_enabled(void) {return fg006_repack_selected;}
int fg006_repack_q4_Kx8(void * dst, const void * src, int64_t nrows, int64_t nblocks) {
    if (nrows <= 0 || nrows % 8 != 0 || nblocks <= 0) return -1;
    gg_function_1(gg_view(dst, (nrows / 8) * nblocks * 1152),
                  gg_view((void *) src, nrows * nblocks * 144), nrows, nblocks, 8, 0);
    return 0;
}
int fg006_quantize_mat_q8_K_4x8(const float * x, void * vy, int64_t k) {
    if (k <= 0 || k % 256 != 0) return -1;
    gg_function_5(gg_view((void *) x, 4 * k), gg_view(vy, (k / 256) * 1168), k, 0);
    return 0;
}
void fg006_gemv_q4_K_8x8_q8_K(int n, float * s, size_t bs, const void * vx, const void * vy, int nr, int nc) {
    if (n <= 0 || n % 256 != 0 || nr <= 0 || nc <= 0 || nc % 8 != 0) return;
    int64_t s_len = (int64_t)(nr-1) * nr + nc;
    gg_function_9(n, gg_view(s, s_len), (int64_t) bs, gg_view((void *) vx, (nc / 8) * (n / 256) * 1152),
                  gg_view((void *) vy, (int64_t) nr * (n / 256) * 292), nr, nc, 0);
}
void fg006_gemm_q4_K_8x8_q8_K(int n, float * s, size_t bs, const void * vx, const void * vy, int nr, int nc) {
    if (n <= 0 || n % 256 != 0 || nr <= 0 || nr % 4 != 0 || nc <= 0 || nc % 8 != 0) return;
    gg_function_16(n, gg_view(s, (int64_t) nr * (int64_t) bs), (int64_t) bs,
                   gg_view((void *) vx, (nc / 8) * (n / 256) * 1152),
                   gg_view((void *) vy, (nr / 4) * (n / 256) * 1168), nr, nc, 0);
}
void fg006_repack_report(void) {
    for (int i = 0; i < 4; ++i) {
        uint64_t n = atomic_load(rp_calls + i);
        if (n) fprintf(stderr, "FG006_REPACK mode=%d calls=%llu blocks=%llu\n", i,
                       (unsigned long long) n, (unsigned long long) atomic_load(rp_blocks + i));
    }
}

__attribute__((destructor)) static void fg006_repack_finish(void) {fg006_repack_report();}
