// Standalone micro-benchmark for the hot expert dot kernels (EXP-019).
// Links the working build's libggml-cpu, changes no library code.
// Measures ggml_vec_dot_iq2_s_q8_K and ggml_vec_dot_iq4_nl_q8_0 in three
// regimes: L1/L2-resident, L3-resident, and a DRAM-streaming working set.
// A plain AVX2 read-sum baseline calibrates the machine's stream ceiling.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <immintrin.h>

#include "ggml.h"
#include "ggml-cpu.h"

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void fill_random(float * dst, int64_t n, unsigned seed) {
    for (int64_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        dst[i] = (float)(int32_t)(seed >> 9) / (float)(1 << 21);
    }
}

static double stream_read_gib_s(const void * a, size_t as, const void * b, size_t bs, int iters) {
    const __m256i * pa = (const __m256i *)a;
    const __m256i * pb = (const __m256i *)b;
    size_t na = as / 32, nb = bs / 32;
    __m256i acc = _mm256_setzero_si256();
    double t0 = now_s();
    for (int it = 0; it < iters; ++it) {
        for (size_t i = 0; i < na; ++i) acc = _mm256_add_epi64(acc, pa[i]);
        for (size_t i = 0; i < nb; ++i) acc = _mm256_add_epi64(acc, pb[i]);
    }
    double dt = now_s() - t0;
    volatile int sink = _mm256_extract_epi32(acc, 0) + _mm256_extract_epi32(acc, 7);
    (void)sink;
    return (double)(as + bs) * iters / dt / 1073741824.0;
}

static void bench(const char * name, enum ggml_type xt, int n_blocks, int iters, const float * src) {
    const struct ggml_type_traits_cpu * tr = ggml_get_type_traits_cpu(xt);
    enum ggml_type yt = tr->vec_dot_type;
    int64_t n = (int64_t)ggml_blck_size(xt) * n_blocks;

    // ggml_cpu_init() fills ggml_table_f32_f16 used for every fp16 scale in the
    // hot loops; without it all d convert to 0 and every dot returns exactly 0.
    // Tables (iq2s_grid etc.) must stay allocated for the whole run: vec_dot
    // reads them too, so ggml_quantize_free() only happens in main().
    ggml_quantize_init(xt);
    size_t xs = ggml_row_size(xt, n);
    size_t ys = ggml_row_size(yt, n);
    void * xq = malloc(xs);
    void * yq = malloc(ys);
    ggml_quantize_chunk(xt, src, xq, 0, 1, n, NULL);
    // y uses an activation type (Q8_K/Q8_0) without ggml_quantize_chunk support
    const struct ggml_type_traits_cpu * ytr = ggml_get_type_traits_cpu(yt);
    if (!ytr->from_float) { fprintf(stderr, "no from_float for y type %s\n", ggml_type_name(yt)); exit(1); }
    ytr->from_float(src, yq, n);

    ggml_vec_dot_t dot = tr->vec_dot;
    float s = 0.0f;
    dot((int)n, &s, 0, xq, 0, yq, 0, 1); // warmup

    double t0 = now_s();
    for (int it = 0; it < iters; ++it) dot((int)n, &s, 0, xq, 0, yq, 0, 1);
    double dt = now_s() - t0;

    double bytes = (double)xs + (double)ys;
    printf("%-8s n=%10lld blocks=%8d iters=%7d | %9.2f ns/block | %8.2f GiB/s | %10.2f Melem/s | s=%.4f\n",
           name, (long long)n, n_blocks, iters,
           dt / ((double)n_blocks * iters) * 1e9,
           bytes * iters / dt / 1073741824.0,
           (double)n * iters / dt / 1e6, s);

    if (xs + ys > 128u << 20) {
        printf("%-8s stream baseline on the same buffers: %8.2f GiB/s\n",
               name, stream_read_gib_s(xq, xs, yq, ys, 8));
    }

    free(xq);
    free(yq);
}

int main(void) {
    ggml_cpu_init();
    const int max_blocks = 524288; // 128 MiB+ of combined x+y traffic, above the 64 MiB L3
    int64_t max_n = (int64_t)ggml_blck_size(GGML_TYPE_IQ2_S) * max_blocks;
    float * src = malloc(max_n * sizeof(float));
    if (!src) { fprintf(stderr, "alloc failed\n"); return 1; }
    fill_random(src, max_n, 42);

    printf("Zen 3 single pinned core, -O3 -march=native, working build libs\n\n");

    bench("iq2_s", GGML_TYPE_IQ2_S, 16,     200000, src);  // L1/L2
    bench("iq2_s", GGML_TYPE_IQ2_S, 16384,  400,   src);   // L3
    bench("iq2_s", GGML_TYPE_IQ2_S, 524288, 12,    src);   // DRAM
    printf("\n");
    bench("iq4nl", GGML_TYPE_IQ4_NL, 16,     200000, src);
    bench("iq4nl", GGML_TYPE_IQ4_NL, 16384,  400,   src);
    bench("iq4nl", GGML_TYPE_IQ4_NL, 524288, 12,    src);   // ~26 MiB: still L3-resident
    bench("iq4nl", GGML_TYPE_IQ4_NL, 4194304, 30,   src);   // ~208 MiB: true DRAM regime

    ggml_quantize_free();
    free(src);
    return 0;
}
