// EXP-021: fused dequant+GEMM prototype for the two hot expert kernels.
//
// The current ggml path calls the library vec_dot once per (weight row, token),
// so the weight-side dequant work (iq2_s grid lookups, sign masks, scale
// expansion; iq4_nl LUT expansion) is repeated for every token. The fused
// kernel below unpacks each weight block once and applies it to Ny Q8
// activation vectors - the reuse trick behind ik_llama.cpp's fused iqk GEMM.
//
// The dequant/apply arithmetic is copied verbatim from the working build's
// ggml_vec_dot_iq2_s_q8_K and ggml_vec_dot_iq4_nl_q8_0 (AVX2 paths), split at
// the weight-only / per-token boundary. No library code is modified.
//
// Run pinned to one core: taskset -c 8 build/exp021-fused-bench

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <immintrin.h>

#include "ggml.h"
#include "ggml-cpu.h"
#include "exp021_iq2s_grid.h"

// ---- block layouts, byte-identical to ggml-common.h -----------------------

#define QK_K   256
#define QK4_NL 32

typedef struct { uint16_t d; uint8_t qs[QK_K/4]; uint8_t qh[QK_K/32]; uint8_t scales[QK_K/32]; } b_iq2_s;
typedef struct { float   d; int8_t qs[QK_K]; int16_t bsums[QK_K/16]; }                              b_q8_K;
typedef struct { uint16_t d; uint8_t qs[QK4_NL/2]; }                                               b_iq4_nl;
typedef struct { uint16_t d; uint8_t qs[QK4_NL]; }                                                 b_q8_0;

_Static_assert(sizeof(b_iq2_s)  == 82, "iq2_s layout mismatch");
_Static_assert(sizeof(b_q8_K)   == 292, "q8_K layout mismatch");
_Static_assert(sizeof(b_iq4_nl) == 18, "iq4_nl layout mismatch");
_Static_assert(sizeof(b_q8_0)   == 34, "q8_0 layout mismatch");

static const int8_t kvalues_iq4nl[16] = {
    -127, -104, -83, -65, -49, -35, -22, -10, 1, 13, 25, 38, 53, 69, 89, 113,
};

// ---- shared SIMD helpers (verbatim from arch/x86/quants.c) -----------------

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

static inline __m256i get_scale_shuffle_k4(int i) {
    static const uint8_t k_shuffle[256] = {
         0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
         2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3,
         4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5,
         6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7,
         8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9,
        10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,
        12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,
        14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,15
    };
    return _mm256_loadu_si256((const __m256i*)k_shuffle + i);
}

static inline float h16_to_f32(uint16_t u) {
    return ggml_fp16_to_fp32((ggml_fp16_t)u);
}

// ---- fused iq2_s ------------------------------------------------------------
//
// Unpacked 256-element weight block: everything the AVX2 vec_dot computes from
// the weight side alone. The per-token part (q8 load, sign application, two
// madds) is all that remains inside the Ny loop.

typedef struct {
    float   d;        // fp16 weight scale of the 256-block
    __m256i q2[8];    // grid-expanded 32-lane quant values, one per subblock
    __m256i s2[8];    // sign masks (0x00/0xff lanes), one per subblock
    __m256i sc[8];    // pre-shuffled int16 scales, one per subblock
} iq2s_up;

static void unpack_row_iq2s(const b_iq2_s * w, int nb, iq2s_up * up) {
    static const uint8_t k_mask1[32] = {0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3};
    static const uint8_t k_mask2[32] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,
                                        0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
    const __m128i m4 = _mm_set1_epi8(0xf);
    const __m128i m1 = _mm_set1_epi8(1);
    const __m256i mask1 = _mm256_loadu_si256((const __m256i*)k_mask1);
    const __m256i mask2 = _mm256_loadu_si256((const __m256i*)k_mask2);

    for (int i = 0; i < nb; ++i) {
        const uint8_t * qs = w[i].qs;
        const uint8_t * qh = w[i].qh;
        const uint16_t * signs = (const uint16_t *)(w[i].qs + QK_K/8);

        up[i].d = h16_to_f32(w[i].d);

        uint64_t aux64;
        memcpy(&aux64, w[i].scales, 8);
        const __m128i scales8 = _mm_add_epi8(_mm_slli_epi16(_mm_and_si128(_mm_set_epi64x(aux64 >> 4, aux64), m4), 1), m1);
        const __m256i scales16 = _mm256_cvtepi8_epi16(scales8);

        for (int k = 0; k < 8; k += 2) {
            up[i].q2[k+0] = _mm256_set_epi64x(iq2s_grid[qs[3] | ((uint32_t)(qh[k+0] << 2) & 0x300)],
                                              iq2s_grid[qs[2] | ((uint32_t)(qh[k+0] << 4) & 0x300)],
                                              iq2s_grid[qs[1] | ((uint32_t)(qh[k+0] << 6) & 0x300)],
                                              iq2s_grid[qs[0] | ((uint32_t)(qh[k+0] << 8) & 0x300)]);
            up[i].q2[k+1] = _mm256_set_epi64x(iq2s_grid[qs[7] | ((uint32_t)(qh[k+1] << 2) & 0x300)],
                                              iq2s_grid[qs[6] | ((uint32_t)(qh[k+1] << 4) & 0x300)],
                                              iq2s_grid[qs[5] | ((uint32_t)(qh[k+1] << 6) & 0x300)],
                                              iq2s_grid[qs[4] | ((uint32_t)(qh[k+1] << 8) & 0x300)]);
            qs += 8;

            __m256i aux256 = _mm256_set1_epi32(signs[0] | ((uint32_t) signs[1] << 16));
            aux256 = _mm256_and_si256(_mm256_shuffle_epi8(aux256, mask1), mask2);
            up[i].s2[k+0] = _mm256_cmpeq_epi8(aux256, mask2);

            aux256 = _mm256_set1_epi32(signs[2] | ((uint32_t) signs[3] << 16));
            aux256 = _mm256_and_si256(_mm256_shuffle_epi8(aux256, mask1), mask2);
            up[i].s2[k+1] = _mm256_cmpeq_epi8(aux256, mask2);

            signs += 4;

            up[i].sc[k+0] = _mm256_shuffle_epi8(scales16, get_scale_shuffle_k4(k+0));
            up[i].sc[k+1] = _mm256_shuffle_epi8(scales16, get_scale_shuffle_k4(k+1));
        }
    }
}

static void apply_row_iq2s(const iq2s_up * up, int nb, const b_q8_K * y, float * out) {
    __m256 accumf = _mm256_setzero_ps();
    for (int i = 0; i < nb; ++i) {
        const int8_t * q8 = y[i].qs;
        const float d = up[i].d * y[i].d;

        __m256i sumi1 = _mm256_setzero_si256();
        __m256i sumi2 = _mm256_setzero_si256();
        for (int k = 0; k < 8; k += 2) {
            const __m256i q8_1 = _mm256_loadu_si256((const __m256i *)q8); q8 += 32;
            const __m256i q8_2 = _mm256_loadu_si256((const __m256i *)q8); q8 += 32;

            const __m256i q8s_1 = _mm256_sub_epi8(_mm256_xor_si256(up[i].s2[k+0], q8_1), up[i].s2[k+0]);
            const __m256i q8s_2 = _mm256_sub_epi8(_mm256_xor_si256(up[i].s2[k+1], q8_2), up[i].s2[k+1]);

            const __m256i dot1 = _mm256_maddubs_epi16(up[i].q2[k+0], q8s_1);
            const __m256i dot2 = _mm256_maddubs_epi16(up[i].q2[k+1], q8s_2);

            sumi1 = _mm256_add_epi32(sumi1, _mm256_madd_epi16(dot1, up[i].sc[k+0]));
            sumi2 = _mm256_add_epi32(sumi2, _mm256_madd_epi16(dot2, up[i].sc[k+1]));
        }

        accumf = _mm256_fmadd_ps(_mm256_set1_ps(d), _mm256_cvtepi32_ps(_mm256_add_epi32(sumi1, sumi2)), accumf);
    }
    *out = 0.125f * hsum_float_8(accumf);
}

// ---- fused iq4_nl ------------------------------------------------------------

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
        up[i].q4 = _mm256_insertf128_si256(_mm256_castsi128_si256(
                        _mm_shuffle_epi8(values128, _mm_and_si128(q4bits, m4b))),
                    _mm_shuffle_epi8(values128, _mm_and_si128(_mm_srli_epi16(q4bits, 4), m4b)), 1);
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

// ---- benchmark driver --------------------------------------------------------

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

static const int R = 768;          // weight rows (one expert's output rows)
static const int NYMAX = 64;       // activation rows swept down to 1
static const int NYS[] = {1, 2, 4, 8, 16, 32, 64};
static const int KS[]  = {2048, 4096};

static volatile float g_sink;

static void bench_type(const char * name, enum ggml_type xt) {
    const struct ggml_type_traits_cpu * tr  = ggml_get_type_traits_cpu(xt);
    const struct ggml_type_traits_cpu * ytr = ggml_get_type_traits_cpu(tr->vec_dot_type);
    ggml_vec_dot_t dot = tr->vec_dot;

    printf("\n=== %s (activation type %s) ===\n", name, ggml_type_name(tr->vec_dot_type));
    printf("%6s %5s %4s | %11s | %12s | %7s | %s\n", "K", "rows", "Ny", "cur ns/dot", "fused ns/dot", "speedup", "maxdiff");

    for (size_t ik = 0; ik < sizeof(KS)/sizeof(KS[0]); ++ik) {
        const int K = KS[ik];
        const size_t wrow = ggml_row_size(xt, K);
        const size_t yrow = ggml_row_size(tr->vec_dot_type, K);

        float * src_w = malloc((size_t)R * K * sizeof(float));
        float * src_y = malloc((size_t)NYMAX * K * sizeof(float));
        uint8_t * W = malloc((size_t)R * wrow);
        uint8_t * Y = malloc((size_t)NYMAX * yrow);
        float * cur_out = malloc((size_t)R * NYMAX * sizeof(float));
        float * fus_out = malloc((size_t)R * NYMAX * sizeof(float));
        if (!src_w || !src_y || !W || !Y || !cur_out || !fus_out) { fprintf(stderr, "alloc failed\n"); exit(1); }

        fill_random(src_w, (int64_t)R * K, 1000 + (unsigned)ik);
        fill_random(src_y, (int64_t)NYMAX * K, 2000 + (unsigned)ik);
        for (int r = 0; r < R; ++r) ggml_quantize_chunk(xt, src_w + (int64_t)r * K, W + (size_t)r * wrow, 0, 1, K, NULL);
        for (int t = 0; t < NYMAX; ++t) ytr->from_float(src_y + (int64_t)t * K, Y + (size_t)t * yrow, K);

        const int nb2 = K / QK_K;     // 256-element blocks (iq2_s)
        const int nb4 = K / QK4_NL;   // 32-element blocks (iq4_nl)
        // Pre-converted fp16->fp32 activation scales: the same y row multiplies
        // every weight row, so convert once per token instead of once per block.
        float * yd4 = malloc((size_t)NYMAX * nb4 * sizeof(float));
        if (!yd4) { fprintf(stderr, "alloc failed\n"); exit(1); }
        // __m256i members make the compiler emit 32-byte aligned stores at -O3,
        // so these buffers must be 32-byte aligned (plain malloc is only 16).
        iq2s_up  * up2 = aligned_alloc(64, ((size_t)nb2 * sizeof(iq2s_up) + 63) / 64 * 64);
        iq4nl_up * up4 = aligned_alloc(64, ((size_t)nb4 * sizeof(iq4nl_up) + 63) / 64 * 64);
        for (int t = 0; t < NYMAX; ++t) {
            const b_q8_0 * y0 = (const b_q8_0 *)(Y + (size_t)t * yrow);
            for (int ib = 0; ib < nb4; ++ib) yd4[(size_t)t * nb4 + ib] = h16_to_f32(y0[ib].d);
        }
        const int is_iq2s = (xt == GGML_TYPE_IQ2_S);

        for (size_t in = 0; in < sizeof(NYS)/sizeof(NYS[0]); ++in) {
            const int Ny = NYS[in];

            // correctness: full pass through both paths
            for (int r = 0; r < R; ++r) {
                for (int t = 0; t < Ny; ++t) {
                    float s = 0.0f;
                    dot(K, &s, 0, W + (size_t)r * wrow, 0, Y + (size_t)t * yrow, 0, 1);
                    cur_out[r * Ny + t] = s;
                }
                if (is_iq2s) {
                    unpack_row_iq2s((const b_iq2_s *)(W + (size_t)r * wrow), nb2, up2);
                    for (int t = 0; t < Ny; ++t) apply_row_iq2s(up2, nb2, (const b_q8_K *)(Y + (size_t)t * yrow), &fus_out[r * Ny + t]);
                } else {
                    unpack_row_iq4nl((const b_iq4_nl *)(W + (size_t)r * wrow), nb4, up4);
                    for (int t = 0; t < Ny; ++t) apply_row_iq4nl(up4, nb4, (const b_q8_0 *)(Y + (size_t)t * yrow), yd4 + (size_t)t * nb4, &fus_out[r * Ny + t]);
                }
            }
            float maxdiff = 0.0f;
            for (int64_t i = 0; i < (int64_t)R * Ny; ++i) {
                float d = fabsf(cur_out[i] - fus_out[i]);
                if (d > maxdiff) maxdiff = d;
            }

            // timing: current path (vec_dot per row x token)
            const int iters = (int)(900000.0 / ((double)R * Ny)) + 1;
            for (int rep = 0; rep < 2; ++rep) {  // rep 0 warms caches, rep 1 is reported
                double t0 = now_s();
                double acc = 0.0;
                for (int it = 0; it < iters; ++it) {
                    for (int r = 0; r < R; ++r) {
                        const uint8_t * wr = W + (size_t)r * wrow;
                        for (int t = 0; t < Ny; ++t) {
                            float s = 0.0f;
                            dot(K, &s, 0, wr, 0, Y + (size_t)t * yrow, 0, 1);
                            acc += s;
                        }
                    }
                    g_sink = (float)acc;
                }
                double t_cur = (now_s() - t0) / ((double)iters * R * Ny) * 1e9;

                t0 = now_s();
                acc = 0.0;
                for (int it = 0; it < iters; ++it) {
                    for (int r = 0; r < R; ++r) {
                        const uint8_t * wr = W + (size_t)r * wrow;
                        float o;
                        if (is_iq2s) {
                            unpack_row_iq2s((const b_iq2_s *)wr, nb2, up2);
                            for (int t = 0; t < Ny; ++t) { apply_row_iq2s(up2, nb2, (const b_q8_K *)(Y + (size_t)t * yrow), &o); acc += o; }
                        } else {
                            unpack_row_iq4nl((const b_iq4_nl *)wr, nb4, up4);
                            for (int t = 0; t < Ny; ++t) { apply_row_iq4nl(up4, nb4, (const b_q8_0 *)(Y + (size_t)t * yrow), yd4 + (size_t)t * nb4, &o); acc += o; }
                        }
                    }
                    g_sink = (float)acc;
                }
                double t_fus = (now_s() - t0) / ((double)iters * R * Ny) * 1e9;

                if (rep == 1)
                    printf("%6d %5d %4d | %11.2f | %12.2f | %6.2fx | %.2e\n", K, R, Ny, t_cur, t_fus, t_cur / t_fus, (double)maxdiff);
            }
        }

        free(up2); free(up4); free(yd4); free(src_w); free(src_y); free(W); free(Y); free(cur_out); free(fus_out);
    }
}

int main(void) {
    ggml_cpu_init();  // fills ggml_table_f32_f16 used by the library fp16 conversions
    printf("EXP-021 fused dequant+GEMM prototype, single pinned core, -O3 -march=native (AVX2+FMA)\n");
    printf("weight rows R=%d, Ny tokens per row, per-(row,token) times in ns\n", R);

    bench_type("iq2_s",  GGML_TYPE_IQ2_S);
    bench_type("iq4nl",  GGML_TYPE_IQ4_NL);
    return 0;
}
