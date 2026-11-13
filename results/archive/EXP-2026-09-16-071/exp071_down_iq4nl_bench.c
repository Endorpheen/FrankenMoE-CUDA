// EXP-2026-09-16-071: does a fused/reuse IQ4_NL down-projection beat the
// stock library vec_dot at the real decode verify shape (Ny=3)?
//
// Model-free microbenchmark adapted from benchmarks/exp021_fused_bench.c.
// Real shape from EXP-046/048: MUL_MAT_ID ffn_down IQ4_NL, ne00=640 (K),
// ne01=2560 (rows per expert), top-10, 3 activation rows.
//
// Pre-registered bar (stated before the first run):
//   bit-exact outputs (maxdiff == 0) AND >= 1.15x vs stock at Ny=3.
//
// Variants compared on identical quantized bytes:
//   stock  - library ggml_vec_dot_iq4_nl_q8_0 from the accepted build,
//            called once per (row, token) exactly like ggml_compute_forward_mul_mat_id.
//   fusedB - EXP-021 fused variant: unpack the row once into a per-row buffer,
//            then apply it to every token (store/load through the buffer).
//   fusedR - register-resident reuse: unpack each 32-block once and apply it to
//            every token in the same iteration, accumulators kept in registers.
//            This is the literal "unpack once, apply to all Ny rows" variant.
//
// The dequant/apply arithmetic is copied from the AVX2 ggml_vec_dot_iq4_nl_q8_0
// (arch/x86/quants.c), split at the weight-only / per-token boundary.
//
// Build (see REPORT.md for the exact commands):
//   gcc -O3 -march=native -std=gnu11 -o exp071_down_iq4nl_bench exp071_down_iq4nl_bench.c
//       -I <tree>/ggml/include -L <build>/bin -lggml-cpu -lggml-base -lm -Wl,-rpath,<build>/bin
// Run: taskset -c 8 ./exp071_down_iq4nl_bench

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <immintrin.h>

#include "ggml.h"
#include "ggml-cpu.h"

#define QK4_NL 32

typedef struct { uint16_t d; uint8_t qs[QK4_NL/2]; } b_iq4_nl;
typedef struct { uint16_t d; uint8_t qs[QK4_NL]; }   b_q8_0;

_Static_assert(sizeof(b_iq4_nl) == 18, "iq4_nl layout mismatch");
_Static_assert(sizeof(b_q8_0)   == 34, "q8_0 layout mismatch");

static const int8_t kvalues_iq4nl[16] = {
    -127, -104, -83, -65, -49, -35, -22, -10, 1, 13, 25, 38, 53, 69, 89, 113,
};

// ---- real decode verify shape ---------------------------------------------
static const int R    = 2560;   // ne01: output rows of one expert's down weight
static const int K    = 640;    // ne00: contraction dim
static const int NEXP = 512;    // experts in the layer (documented real shape)
static const int TOPK = 10;     // experts selected per token
static const int NEXP_STREAM = 128; // 128*0.88 MiB = 113 MiB, above the 64 MiB L3
static const int NYS[] = {1, 2, 3, 4, 8};
#define NNY  (int)(sizeof(NYS)/sizeof(NYS[0]))
#define NREP 5

// ---- SIMD helpers (verbatim from arch/x86/quants.c) ------------------------

static inline float hsum_float_8(__m256 x) {
    __m128 res = _mm256_extractf128_ps(x, 1);
    res = _mm_add_ps(res, _mm256_castps256_ps128(x));
    res = _mm_add_ps(res, _mm_movehl_ps(res, res));
    res = _mm_add_ss(res, _mm_movehdup_ps(res));
    return _mm_cvtss_f32(res);
}

static inline __m256i mul_add_epi8(const __m256i x, const __m256i y) {
    const __m256i ax = _mm256_sign_epi8(x, x);
    const __m256i sy = _mm256_sign_epi8(y, x);
    return _mm256_maddubs_epi16(ax, sy);
}

// Same table lookup as the library's GGML_CPU_FP16_TO_FP32; calling the
// ggml_fp16_to_fp32 function instead costs a PLT call per block in the hot loop.
extern float ggml_table_f32_f16[1 << 16];

static inline float h16_to_f32(uint16_t u) {
    return ggml_table_f32_f16[u];
}

// LUT-expand 16 packed nibbles into 32 int8 lanes (low nibble -> low 128,
// high nibble -> high 128), same construction as the stock AVX2 kernel.
#define EXPAND_Q4(q4bits, values128, m4b) \
    _mm256_insertf128_si256(_mm256_castsi128_si256( \
        _mm_shuffle_epi8((values128), _mm_and_si128((q4bits), (m4b)))), \
        _mm_shuffle_epi8((values128), _mm_and_si128(_mm_srli_epi16((q4bits), 4), (m4b))), 1)

// ---- fusedB: EXP-021 buffer variant ---------------------------------------

typedef struct {
    float   d;     // fp16 weight scale of the 32-block
    __m256i q4;    // LUT-expanded 32-lane quant values
} iq4nl_up;

static void unpack_row_iq4nl(const b_iq4_nl * w, int nb, iq4nl_up * up) {
    const __m128i values128 = _mm_loadu_si128((const __m128i*)kvalues_iq4nl);
    const __m128i m4b = _mm_set1_epi8(0x0f);
    for (int i = 0; i < nb; ++i) {
        const __m128i q4bits = _mm_loadu_si128((const __m128i*)w[i].qs);
        up[i].d = h16_to_f32(w[i].d);
        up[i].q4 = EXPAND_Q4(q4bits, values128, m4b);
    }
}

static void apply_row_iq4nl(const iq4nl_up * up, int nb, const b_q8_0 * y, const float * yd, float * out) {
    const __m256i mone = _mm256_set1_epi16(1);
    __m256 accum1 = _mm256_setzero_ps();
    __m256 accum2 = _mm256_setzero_ps();
    for (int ib = 0; ib + 1 < nb; ib += 2) {
        const __m256i q8b_1 = _mm256_loadu_si256((const __m256i *)y[ib + 0].qs);
        const __m256i q8b_2 = _mm256_loadu_si256((const __m256i *)y[ib + 1].qs);
        const __m256i p16_1 = mul_add_epi8(up[ib + 0].q4, q8b_1);
        const __m256i p16_2 = mul_add_epi8(up[ib + 1].q4, q8b_2);
        const __m256i p_1 = _mm256_madd_epi16(p16_1, mone);
        const __m256i p_2 = _mm256_madd_epi16(p16_2, mone);
        accum1 = _mm256_fmadd_ps(_mm256_set1_ps(yd[ib + 0] * up[ib + 0].d),
                _mm256_cvtepi32_ps(p_1), accum1);
        accum2 = _mm256_fmadd_ps(_mm256_set1_ps(yd[ib + 1] * up[ib + 1].d),
                _mm256_cvtepi32_ps(p_2), accum2);
    }
    *out = hsum_float_8(_mm256_add_ps(accum1, accum2));
}

// ---- inline stock control ---------------------------------------------------
//
// Arithmetic identical to the library AVX2 kernel, but inlined and without
// reuse. Separates the opaque call overhead from the reuse gain.

static inline float dot_iq4nl_inline(const b_iq4_nl * x, int nb, const b_q8_0 * y) {
    const __m128i values128 = _mm_loadu_si128((const __m128i *)kvalues_iq4nl);
    const __m128i m4b = _mm_set1_epi8(0x0f);
    const __m256i mone = _mm256_set1_epi16(1);
    __m256 accum1 = _mm256_setzero_ps(), accum2 = _mm256_setzero_ps();
    for (int ib = 0; ib + 1 < nb; ib += 2) {
        const __m128i q4bits_1 = _mm_loadu_si128((const __m128i *)x[ib + 0].qs);
        const __m128i q4bits_2 = _mm_loadu_si128((const __m128i *)x[ib + 1].qs);
        const __m256i q8b_1 = _mm256_loadu_si256((const __m256i *)y[ib + 0].qs);
        const __m256i q8b_2 = _mm256_loadu_si256((const __m256i *)y[ib + 1].qs);
        const __m256i q4b_1 = EXPAND_Q4(q4bits_1, values128, m4b);
        const __m256i q4b_2 = EXPAND_Q4(q4bits_2, values128, m4b);
        const __m256i p16_1 = mul_add_epi8(q4b_1, q8b_1);
        const __m256i p16_2 = mul_add_epi8(q4b_2, q8b_2);
        accum1 = _mm256_fmadd_ps(_mm256_set1_ps(h16_to_f32(y[ib + 0].d) * h16_to_f32(x[ib + 0].d)),
                _mm256_cvtepi32_ps(_mm256_madd_epi16(p16_1, mone)), accum1);
        accum2 = _mm256_fmadd_ps(_mm256_set1_ps(h16_to_f32(y[ib + 1].d) * h16_to_f32(x[ib + 1].d)),
                _mm256_cvtepi32_ps(_mm256_madd_epi16(p16_2, mone)), accum2);
    }
    return hsum_float_8(_mm256_add_ps(accum1, accum2));
}

// ---- fusedR: register-resident reuse, one function per Ny ------------------
//
// For each block pair the weight is expanded once; the inner token loop reuses
// it. Each token keeps its own accum1/accum2 pair, so the per-token summation
// order is identical to the stock kernel and the result is bit-exact.

#define DEFINE_FUSED_REG(NY) \
static void fused_reg_##NY(const b_iq4_nl * w, int nb, const uint8_t * Y, size_t yrow, \
                           const float * yd, int ydstride, float * out) { \
    const b_q8_0 * yt[NY]; \
    for (int t = 0; t < NY; ++t) yt[t] = (const b_q8_0 *)(Y + (size_t)t * yrow); \
    const __m128i values128 = _mm_loadu_si128((const __m128i *)kvalues_iq4nl); \
    const __m128i m4b = _mm_set1_epi8(0x0f); \
    const __m256i mone = _mm256_set1_epi16(1); \
    __m256 a1[NY], a2[NY]; \
    for (int t = 0; t < NY; ++t) { a1[t] = _mm256_setzero_ps(); a2[t] = _mm256_setzero_ps(); } \
    for (int ib = 0; ib + 1 < nb; ib += 2) { \
        const __m128i q4bits_1 = _mm_loadu_si128((const __m128i *)w[ib + 0].qs); \
        const __m128i q4bits_2 = _mm_loadu_si128((const __m128i *)w[ib + 1].qs); \
        const __m256i q4_1 = EXPAND_Q4(q4bits_1, values128, m4b); \
        const __m256i q4_2 = EXPAND_Q4(q4bits_2, values128, m4b); \
        const float xd1 = h16_to_f32(w[ib + 0].d); \
        const float xd2 = h16_to_f32(w[ib + 1].d); \
        for (int t = 0; t < NY; ++t) { \
            const __m256i q8b_1 = _mm256_loadu_si256((const __m256i *)yt[t][ib + 0].qs); \
            const __m256i q8b_2 = _mm256_loadu_si256((const __m256i *)yt[t][ib + 1].qs); \
            const __m256i p1 = mul_add_epi8(q4_1, q8b_1); \
            const __m256i p2 = mul_add_epi8(q4_2, q8b_2); \
            a1[t] = _mm256_fmadd_ps(_mm256_set1_ps(yd[(size_t)t * ydstride + ib + 0] * xd1), \
                    _mm256_cvtepi32_ps(_mm256_madd_epi16(p1, mone)), a1[t]); \
            a2[t] = _mm256_fmadd_ps(_mm256_set1_ps(yd[(size_t)t * ydstride + ib + 1] * xd2), \
                    _mm256_cvtepi32_ps(_mm256_madd_epi16(p2, mone)), a2[t]); \
        } \
    } \
    for (int t = 0; t < NY; ++t) out[t] = hsum_float_8(_mm256_add_ps(a1[t], a2[t])); \
}

DEFINE_FUSED_REG(1)
DEFINE_FUSED_REG(2)
DEFINE_FUSED_REG(3)
DEFINE_FUSED_REG(4)
DEFINE_FUSED_REG(8)

static void fused_reg_dispatch(int Ny, const b_iq4_nl * w, int nb, const uint8_t * Y,
                               size_t yrow, const float * yd, int ydstride, float * out) {
    switch (Ny) {
        case 1: fused_reg_1(w, nb, Y, yrow, yd, ydstride, out); break;
        case 2: fused_reg_2(w, nb, Y, yrow, yd, ydstride, out); break;
        case 3: fused_reg_3(w, nb, Y, yrow, yd, ydstride, out); break;
        case 4: fused_reg_4(w, nb, Y, yrow, yd, ydstride, out); break;
        case 8: fused_reg_8(w, nb, Y, yrow, yd, ydstride, out); break;
        default: fprintf(stderr, "no fused_reg for Ny=%d\n", Ny); exit(1);
    }
}

// ---- timing / data helpers -------------------------------------------------

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

static int cmp_double(const void * a, const void * b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

// plain AVX2 read-sum over two buffers, to calibrate the core's stream ceiling
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

static volatile float g_sink;

// ---- resident benchmark (EXP-021 style, working set fits L3) ---------------
//
// "call" = one expert's down projection: R rows x Ny tokens. Weight bytes per
// call = R * row_size. Both paths read the same bytes and share Y.

static void bench_resident(void) {
    const size_t row_w = ggml_row_size(GGML_TYPE_IQ4_NL, K);
    const size_t row_y = ggml_row_size(GGML_TYPE_Q8_0, K);
    const int nb = K / QK4_NL;
    const int NYMAX = 8;

    float * src_w = malloc((size_t)R * K * sizeof(float));
    float * src_y = malloc((size_t)NYMAX * K * sizeof(float));
    uint8_t * W = malloc((size_t)R * row_w);
    uint8_t * Y = malloc((size_t)NYMAX * row_y);
    float * stock_out = malloc((size_t)R * NYMAX * sizeof(float));
    float * inl_out = malloc((size_t)R * NYMAX * sizeof(float));
    float * fuseB_out = malloc((size_t)R * NYMAX * sizeof(float));
    float * fuseR_out = malloc((size_t)R * NYMAX * sizeof(float));
    if (!src_w || !src_y || !W || !Y || !stock_out || !inl_out || !fuseB_out || !fuseR_out) {
        fprintf(stderr, "alloc failed\n"); exit(1);
    }

    fill_random(src_w, (int64_t)R * K, 7100);
    fill_random(src_y, (int64_t)NYMAX * K, 7200);
    ggml_quantize_chunk(GGML_TYPE_IQ4_NL, src_w, W, 0, R, K, NULL);
    const struct ggml_type_traits_cpu * ytr = ggml_get_type_traits_cpu(GGML_TYPE_Q8_0);
    for (int t = 0; t < NYMAX; ++t) ytr->from_float(src_y + (int64_t)t * K, Y + (size_t)t * row_y, K);

    float * yd = malloc((size_t)NYMAX * nb * sizeof(float));
    for (int t = 0; t < NYMAX; ++t) {
        const b_q8_0 * y0 = (const b_q8_0 *)(Y + (size_t)t * row_y);
        for (int ib = 0; ib < nb; ++ib) yd[(size_t)t * nb + ib] = h16_to_f32(y0[ib].d);
    }
    iq4nl_up * up4 = aligned_alloc(64, ((size_t)nb * sizeof(iq4nl_up) + 63) / 64 * 64);

    const struct ggml_type_traits_cpu * tr = ggml_get_type_traits_cpu(GGML_TYPE_IQ4_NL);
    ggml_vec_dot_t dot = tr->vec_dot;

    const double call_bytes = (double)R * (double)row_w; // unique weight bytes per call

    fprintf(stderr, "[resident] data ready, Ny sweep starting\n");
    printf("\n=== resident (working set %.2f MiB, fits L3) ===\n", (double)R * row_w / 1048576.0);
    printf("shape K=%d rows=%d nb=%d | one call = rows*Ny dots, weight bytes/call=%.0f\n", K, R, nb, call_bytes);
    printf("%4s | %10s %10s %10s %10s | %6s %6s %6s | %9s %9s | %s\n",
           "Ny", "stock ns", "inline ns", "fusB ns", "fusR ns", "inl x", "fusB x", "fusR x",
           "stock B/ns", "fusR B/ns", "verdict");

    for (int in = 0; in < NNY; ++in) {
        const int Ny = NYS[in];
        const int ntok = R * Ny;
        const int iters = (int)(900000.0 / (double)ntok) + 1;
        fprintf(stderr, "[resident] Ny=%d iters=%d\n", Ny, iters);

        // correctness: full pass, identical bytes through all three paths
        for (int r = 0; r < R; ++r) {
            const uint8_t * wr = W + (size_t)r * row_w;
            for (int t = 0; t < Ny; ++t) {
                float s = 0.0f;
                dot(K, &s, 0, wr, 0, Y + (size_t)t * row_y, 0, 1);
                stock_out[(size_t)r * Ny + t] = s;
                inl_out[(size_t)r * Ny + t] = dot_iq4nl_inline((const b_iq4_nl *)wr, nb,
                                                              (const b_q8_0 *)(Y + (size_t)t * row_y));
            }
            unpack_row_iq4nl((const b_iq4_nl *)wr, nb, up4);
            for (int t = 0; t < Ny; ++t)
                apply_row_iq4nl(up4, nb, (const b_q8_0 *)(Y + (size_t)t * row_y),
                                yd + (size_t)t * nb, &fuseB_out[(size_t)r * Ny + t]);
            fused_reg_dispatch(Ny, (const b_iq4_nl *)wr, nb, Y, row_y, yd, nb,
                               &fuseR_out[(size_t)r * Ny]);
        }
        float maxdiffR = 0.0f, maxdiffI = 0.0f;
        for (int64_t i = 0; i < (int64_t)R * Ny; ++i) {
            float b = fabsf(stock_out[i] - fuseR_out[i]);
            float c = fabsf(stock_out[i] - inl_out[i]);
            if (b > maxdiffR) maxdiffR = b;
            if (c > maxdiffI) maxdiffI = c;
        }

        double t_stock[NREP], t_inl[NREP], t_fusB[NREP], t_fusR[NREP];
        for (int rep = 0; rep <= NREP; ++rep) { // rep 0 is warmup
            double t0, acc;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                for (int r = 0; r < R; ++r) {
                    const uint8_t * wr = W + (size_t)r * row_w;
                    for (int t = 0; t < Ny; ++t) {
                        float s = 0.0f;
                        dot(K, &s, 0, wr, 0, Y + (size_t)t * row_y, 0, 1);
                        acc += s;
                    }
                }
                g_sink = (float)acc;
            }
            const double dt_stock = (now_s() - t0) / (double)iters * 1e9;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                for (int r = 0; r < R; ++r) {
                    const b_iq4_nl * wr = (const b_iq4_nl *)(W + (size_t)r * row_w);
                    for (int t = 0; t < Ny; ++t)
                        acc += dot_iq4nl_inline(wr, nb, (const b_q8_0 *)(Y + (size_t)t * row_y));
                }
                g_sink = (float)acc;
            }
            const double dt_inl = (now_s() - t0) / (double)iters * 1e9;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                for (int r = 0; r < R; ++r) {
                    const uint8_t * wr = W + (size_t)r * row_w;
                    unpack_row_iq4nl((const b_iq4_nl *)wr, nb, up4);
                    for (int t = 0; t < Ny; ++t) {
                        float o;
                        apply_row_iq4nl(up4, nb, (const b_q8_0 *)(Y + (size_t)t * row_y),
                                        yd + (size_t)t * nb, &o);
                        acc += o;
                    }
                }
                g_sink = (float)acc;
            }
            const double dt_fusB = (now_s() - t0) / (double)iters * 1e9;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                for (int r = 0; r < R; ++r) {
                    float o[8];
                    fused_reg_dispatch(Ny, (const b_iq4_nl *)(W + (size_t)r * row_w), nb,
                                       Y, row_y, yd, nb, o);
                    for (int t = 0; t < Ny; ++t) acc += o[t];
                }
                g_sink = (float)acc;
            }
            const double dt_fusR = (now_s() - t0) / (double)iters * 1e9;

            if (rep > 0) { t_stock[rep-1] = dt_stock; t_inl[rep-1] = dt_inl;
                           t_fusB[rep-1] = dt_fusB; t_fusR[rep-1] = dt_fusR; }
        }

        qsort(t_stock, NREP, sizeof(double), cmp_double);
        qsort(t_inl,   NREP, sizeof(double), cmp_double);
        qsort(t_fusB,  NREP, sizeof(double), cmp_double);
        qsort(t_fusR,  NREP, sizeof(double), cmp_double);
        const double med_stock = t_stock[NREP/2], med_inl = t_inl[NREP/2];
        const double med_fusB = t_fusB[NREP/2], med_fusR = t_fusR[NREP/2];
        const double xI = med_stock / med_inl, xB = med_stock / med_fusB, xR = med_stock / med_fusR;
        const double xR_reuse = med_inl / med_fusR;
        const int okR = (maxdiffR == 0.0f);
        printf("%4d | %10.1f %10.1f %10.1f %10.1f | %5.3fx %5.3fx %5.3fx | %9.2f %9.2f | %s (inl %.1e %s, reuse-vs-inl %.3fx)\n",
               Ny, med_stock, med_inl, med_fusB, med_fusR, xI, xB, xR,
               call_bytes / med_stock, call_bytes / med_fusR,
               (xR >= 1.15 && okR) ? "PASS" : "FAIL",
               (double)maxdiffI, (maxdiffI == 0.0f) ? "EXACT" : "DIFF", xR_reuse);
    }

    free(up4); free(yd); free(src_w); free(src_y); free(W); free(Y);
    free(stock_out); free(inl_out); free(fuseB_out); free(fuseR_out);
}

// ---- streaming benchmark (working set > L3, real decode regime) ------------
//
// One call = top-10 experts x R rows x Ny tokens (~8.8 MiB of IQ4_NL weights).
// Consecutive calls use a rotating, disjoint expert window so the bytes come
// from DRAM, matching decode where routed experts are mostly cold.

static void bench_stream(void) {
    const size_t row_w = ggml_row_size(GGML_TYPE_IQ4_NL, K);
    const size_t row_y = ggml_row_size(GGML_TYPE_Q8_0, K);
    const int nb = K / QK4_NL;
    const int NYMAX = 8;

    const size_t expert_bytes = (size_t)R * row_w;
    const size_t W_bytes = (size_t)NEXP_STREAM * expert_bytes;

    uint8_t * W = malloc(W_bytes);
    float * src_w = malloc((size_t)R * K * sizeof(float));
    float * src_y = malloc((size_t)NYMAX * K * sizeof(float));
    uint8_t * Y = malloc((size_t)NYMAX * row_y);
    float * out = malloc((size_t)R * NYMAX * sizeof(float));
    if (!W || !src_w || !src_y || !Y || !out) { fprintf(stderr, "alloc failed\n"); exit(1); }

    printf("\n=== streaming (working set %.0f MiB, above L3) ===\n", (double)W_bytes / 1048576.0);
    fprintf(stderr, "[stream] filling %d experts (%zu MiB)...\n", NEXP_STREAM, W_bytes / 1048576);
    for (int e = 0; e < NEXP_STREAM; ++e) {
        fill_random(src_w, (int64_t)R * K, 9100u + (unsigned)e);
        ggml_quantize_chunk(GGML_TYPE_IQ4_NL, src_w, W + (size_t)e * expert_bytes, 0, R, K, NULL);
        if ((e + 1) % 32 == 0) fprintf(stderr, "[stream] experts %d/%d\n", e + 1, NEXP_STREAM);
    }
    fill_random(src_y, (int64_t)NYMAX * K, 7300);
    const struct ggml_type_traits_cpu * ytr = ggml_get_type_traits_cpu(GGML_TYPE_Q8_0);
    for (int t = 0; t < NYMAX; ++t) ytr->from_float(src_y + (int64_t)t * K, Y + (size_t)t * row_y, K);

    float * yd = malloc((size_t)NYMAX * nb * sizeof(float));
    for (int t = 0; t < NYMAX; ++t) {
        const b_q8_0 * y0 = (const b_q8_0 *)(Y + (size_t)t * row_y);
        for (int ib = 0; ib < nb; ++ib) yd[(size_t)t * nb + ib] = h16_to_f32(y0[ib].d);
    }
    iq4nl_up * up4 = aligned_alloc(64, ((size_t)nb * sizeof(iq4nl_up) + 63) / 64 * 64);

    const struct ggml_type_traits_cpu * tr = ggml_get_type_traits_cpu(GGML_TYPE_IQ4_NL);
    ggml_vec_dot_t dot = tr->vec_dot;

    const double call_bytes = (double)TOPK * expert_bytes;

    // stream ceiling of this core on the same sort of buffer
    const double ceiling = stream_read_gib_s(W, W_bytes, Y, (size_t)NYMAX * row_y, 3);
    printf("single-core read-sum ceiling on the same buffer: %.2f GiB/s (%.3f bytes/ns)\n",
           ceiling, ceiling * 1073741824.0 / 1e9);

    printf("%4s | %10s %10s %10s | %6s %6s | %9s %9s | %s\n",
           "Ny", "stock ns", "inline ns", "fusR ns", "inl x", "fusR x",
           "stock B/ns", "fusR B/ns", "verdict");

    for (int in = 0; in < NNY; ++in) {
        const int Ny = NYS[in];
        const int iters = 60;
        fprintf(stderr, "[stream] Ny=%d iters=%d\n", Ny, iters);

        // bit-exactness on a few experts (same arithmetic as the resident table)
        float maxdiffR = 0.0f, maxdiffI = 0.0f;
        {
            const int e0 = 0;
            for (int r = 0; r < R; ++r) {
                const uint8_t * wr = W + (size_t)e0 * expert_bytes + (size_t)r * row_w;
                for (int t = 0; t < Ny; ++t) {
                    float s = 0.0f;
                    dot(K, &s, 0, wr, 0, Y + (size_t)t * row_y, 0, 1);
                    out[r * Ny + t] = s;
                    float di = fabsf(s - dot_iq4nl_inline((const b_iq4_nl *)wr, nb,
                                                          (const b_q8_0 *)(Y + (size_t)t * row_y)));
                    if (di > maxdiffI) maxdiffI = di;
                }
                float o[8];
                fused_reg_dispatch(Ny, (const b_iq4_nl *)wr, nb, Y, row_y, yd, nb, o);
                for (int t = 0; t < Ny; ++t) {
                    float d = fabsf(out[r * Ny + t] - o[t]);
                    if (d > maxdiffR) maxdiffR = d;
                }
            }
        }

        double t_stock[NREP], t_inl[NREP], t_fusR[NREP];
        for (int rep = 0; rep <= NREP; ++rep) {
            double t0, acc;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                const int e0 = (it * TOPK) % NEXP_STREAM;
                for (int j = 0; j < TOPK; ++j) {
                    const int e = (e0 + j) % NEXP_STREAM;
                    for (int r = 0; r < R; ++r) {
                        const uint8_t * wr = W + (size_t)e * expert_bytes + (size_t)r * row_w;
                        for (int t = 0; t < Ny; ++t) {
                            float s = 0.0f;
                            dot(K, &s, 0, wr, 0, Y + (size_t)t * row_y, 0, 1);
                            acc += s;
                        }
                    }
                }
                g_sink = (float)acc;
            }
            const double dt_stock = (now_s() - t0) / (double)iters * 1e9;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                const int e0 = (it * TOPK) % NEXP_STREAM;
                for (int j = 0; j < TOPK; ++j) {
                    const int e = (e0 + j) % NEXP_STREAM;
                    for (int r = 0; r < R; ++r) {
                        const b_iq4_nl * wr = (const b_iq4_nl *)(W + (size_t)e * expert_bytes + (size_t)r * row_w);
                        for (int t = 0; t < Ny; ++t)
                            acc += dot_iq4nl_inline(wr, nb, (const b_q8_0 *)(Y + (size_t)t * row_y));
                    }
                }
                g_sink = (float)acc;
            }
            const double dt_inl = (now_s() - t0) / (double)iters * 1e9;

            t0 = now_s(); acc = 0.0;
            for (int it = 0; it < iters; ++it) {
                const int e0 = (it * TOPK) % NEXP_STREAM;
                for (int j = 0; j < TOPK; ++j) {
                    const int e = (e0 + j) % NEXP_STREAM;
                    for (int r = 0; r < R; ++r) {
                        float o[8];
                        fused_reg_dispatch(Ny, (const b_iq4_nl *)(W + (size_t)e * expert_bytes + (size_t)r * row_w),
                                           nb, Y, row_y, yd, nb, o);
                        for (int t = 0; t < Ny; ++t) acc += o[t];
                    }
                }
                g_sink = (float)acc;
            }
            const double dt_fusR = (now_s() - t0) / (double)iters * 1e9;

            if (rep > 0) { t_stock[rep-1] = dt_stock; t_inl[rep-1] = dt_inl; t_fusR[rep-1] = dt_fusR; }
        }

        qsort(t_stock, NREP, sizeof(double), cmp_double);
        qsort(t_inl,   NREP, sizeof(double), cmp_double);
        qsort(t_fusR,  NREP, sizeof(double), cmp_double);
        const double med_stock = t_stock[NREP/2], med_inl = t_inl[NREP/2], med_fusR = t_fusR[NREP/2];
        const double xI = med_stock / med_inl, xR = med_stock / med_fusR;
        printf("%4d | %10.1f %10.1f %10.1f | %5.3fx %5.3fx | %9.2f %9.2f | %s (maxdiffR %.1e, maxdiffI %.1e, reuse-vs-inl %.3fx)\n",
               Ny, med_stock, med_inl, med_fusR, xI, xR,
               call_bytes / med_stock, call_bytes / med_fusR,
               (xR >= 1.15 && maxdiffR == 0.0f) ? "PASS" : "FAIL",
               (double)maxdiffR, (double)maxdiffI, med_inl / med_fusR);
    }

    free(up4); free(yd); free(W); free(src_w); free(src_y); free(Y); free(out);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fprintf(stderr, "[start]\n");
    ggml_cpu_init(); // fills ggml_table_f32_f16 used by the library fp16 conversions

    printf("EXP-2026-09-16-071 IQ4_NL down-projection, model-free microbenchmark\n");
    printf("single pinned core (taskset -c 8), -O3 -march=native (AVX2+FMA)\n");
    printf("real decode verify shape: ne00/K=%d, ne01/rows=%d, experts=%d, top-%d, Ny tokens\n",
           K, R, NEXP, TOPK);
    printf("PRE-REGISTERED BAR: bit-exact (maxdiff==0) AND >= 1.15x vs stock at Ny=3\n");
    printf("stock = library ggml_vec_dot_iq4_nl_q8_0; fusB = EXP-021 buffer; "
           "fusR = register-resident unpack-once/apply-to-all-Ny\n");

    bench_resident();
    bench_stream();
    return 0;
}
