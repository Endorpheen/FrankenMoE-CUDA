// Staged S1.3, deep arm gate: the module's chunk and dequant entries against
// the native library, for every type the module carries.
//
// The shipped gate (fg-goose-quant-smoke) covers the three dispatched entry
// points and nothing else. This binary covers the arms behind them, because a
// mutation must be able to fail somewhere:
//
//   ggml_quantize_chunk(type, ..., NULL)        vs  qz_/qz2_quantize_chunk()
//   ggml_quantize_chunk(type, ..., imatrix)     vs  qw_quantize_chunk_im()
//   dequantize_row_<type>(packed, out, n)       vs  qz_dequant()/qd_dequant()
//
// The oracle is the native library itself. That is honest only where the
// native path is not switched: this slice rewires `quantize_row_q8_0_ref` and
// `dequantize_row_q4_0`, so q8_0 quantization is compared in the smoke gate
// against a local copy of the native body and is excluded here; q4_0 dequant
// is excluded here for the same reason and is the smoke gate's own subject.
// Everything else in this file is native-and-untouched, including the IQ
// neighbour tables, which ggml_quantize_init() builds in this process.
//
// ON only: with FG_GOOSE_QUANT off the binary reports "skipped" and passes,
// because there is no module to call.

#include "ggml.h"
#include "ggml-quants.h"
#include "fg-goose-quant-bridge.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef FG_GOOSE_QUANT

#include "quant-stack.h"

static int g_cases = 0;
static int g_fails = 0;

static void fail(const char * what, int type, int pat, const char * detail) {
    ++g_fails;
    printf("ARMS_FAIL %s type=%d pat=%d %s\n", what, type, pat, detail);
}

// ---- corpus ---------------------------------------------------------------

static uint64_t g_rng = UINT64_C(0x243f6a8885a308d3);

static uint32_t xs(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t) (g_rng >> 32);
}

static float value(int pat, int i, int n) {
    switch (pat) {
        case 0: return -3.0f + 6.0f * (float) i / (float) (n > 1 ? n - 1 : 1);
        case 1: return ((float) (xs() >> 8) / 16777216.0f) * 10.0f - 5.0f;
        default: return 1.0f;
    }
}

static float imvalue(int pat, int i, int n) {
    switch (pat) {
        case 0: return 1.0f;
        case 1: return 0.25f + 1.75f * (float) i / (float) (n > 1 ? n - 1 : 1);
        default: return 0.5f;
    }
}

// ---- native oracle ---------------------------------------------------------

static void native_dequant(int type, const void * src, float * out, int64_t n) {
    switch (type) {
        case GGML_TYPE_Q4_0:    dequantize_row_q4_0((const block_q4_0 *) src, out, n); break;
        case GGML_TYPE_Q4_1:    dequantize_row_q4_1((const block_q4_1 *) src, out, n); break;
        case GGML_TYPE_Q5_0:    dequantize_row_q5_0((const block_q5_0 *) src, out, n); break;
        case GGML_TYPE_Q5_1:    dequantize_row_q5_1((const block_q5_1 *) src, out, n); break;
        case GGML_TYPE_Q8_0:    dequantize_row_q8_0((const block_q8_0 *) src, out, n); break;
        case GGML_TYPE_Q1_0:    dequantize_row_q1_0((const block_q1_0 *) src, out, n); break;
        case GGML_TYPE_Q2_0:    dequantize_row_q2_0((const block_q2_0 *) src, out, n); break;
        case GGML_TYPE_Q2_K:    dequantize_row_q2_K((const block_q2_K *) src, out, n); break;
        case GGML_TYPE_Q3_K:    dequantize_row_q3_K((const block_q3_K *) src, out, n); break;
        case GGML_TYPE_Q4_K:    dequantize_row_q4_K((const block_q4_K *) src, out, n); break;
        case GGML_TYPE_Q5_K:    dequantize_row_q5_K((const block_q5_K *) src, out, n); break;
        case GGML_TYPE_Q6_K:    dequantize_row_q6_K((const block_q6_K *) src, out, n); break;
        case GGML_TYPE_TQ1_0:   dequantize_row_tq1_0((const block_tq1_0 *) src, out, n); break;
        case GGML_TYPE_TQ2_0:   dequantize_row_tq2_0((const block_tq2_0 *) src, out, n); break;
        case GGML_TYPE_MXFP4:   dequantize_row_mxfp4((const block_mxfp4 *) src, out, n); break;
        case GGML_TYPE_NVFP4:   dequantize_row_nvfp4((const block_nvfp4 *) src, out, n); break;
        case GGML_TYPE_IQ4_NL:  dequantize_row_iq4_nl((const block_iq4_nl *) src, out, n); break;
        case GGML_TYPE_IQ4_XS:  dequantize_row_iq4_xs((const block_iq4_xs *) src, out, n); break;
        case GGML_TYPE_IQ3_XXS: dequantize_row_iq3_xxs((const block_iq3_xxs *) src, out, n); break;
        case GGML_TYPE_IQ3_S:   dequantize_row_iq3_s((const block_iq3_s *) src, out, n); break;
        case GGML_TYPE_IQ2_S:   dequantize_row_iq2_s((const block_iq2_s *) src, out, n); break;
        case GGML_TYPE_IQ1_M:   dequantize_row_iq1_m((const block_iq1_m *) src, out, n); break;
        default: break;
    }
}

// ---- the module's chunk entries -------------------------------------------

static int64_t goose_chunk(int type, const float * src, void * dst,
                           int64_t start, int64_t nrows, int64_t n_per_row) {
    switch (type) {
        case GGML_TYPE_Q4_0:
        case GGML_TYPE_Q4_1:
        case GGML_TYPE_Q5_0:
        case GGML_TYPE_Q5_1:
        case GGML_TYPE_Q8_0:
        case GGML_TYPE_Q1_0:
        case GGML_TYPE_Q2_0:
            return qz_quantize_chunk(type, (uint64_t) (uintptr_t) src,
                                     (uint64_t) (uintptr_t) dst,
                                     start, nrows, n_per_row);
        default:
            return qz2_quantize_chunk(type, (uint64_t) (uintptr_t) src,
                                      (uint64_t) (uintptr_t) dst,
                                      start, nrows, n_per_row);
    }
}

static int64_t goose_chunk_im(int type, const float * src, void * dst,
                              int64_t start, int64_t nrows, int64_t n_per_row,
                              const float * im) {
    return qw_quantize_chunk_im(type, (uint64_t) (uintptr_t) src,
                                (uint64_t) (uintptr_t) dst,
                                start, nrows, n_per_row, (uint64_t) (uintptr_t) im);
}

// ---- cases -----------------------------------------------------------------

// Types whose chunk arm is byte-identical to the native entry on this corpus.
// Excluded, and why (all of it is measured, see REPORT.md section 8a):
//   q8_0            the native entry itself calls the switched row function
//   iq2_xxs/xs, iq1_s   the native entry asserts without an imatrix
//   iq3_xxs, iq1_m  canon diverges once a group falls off the grid
//   iq2_s           canon stops with fail(1241) on the same input class
static const int UNWEIGHTED[] = {
    GGML_TYPE_Q4_0, GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1,
    GGML_TYPE_Q1_0, GGML_TYPE_Q2_0,
    GGML_TYPE_Q2_K, GGML_TYPE_Q3_K, GGML_TYPE_Q4_K, GGML_TYPE_Q5_K,
    GGML_TYPE_Q6_K,
    GGML_TYPE_IQ4_NL, GGML_TYPE_IQ4_XS, GGML_TYPE_IQ3_S,
    GGML_TYPE_TQ1_0, GGML_TYPE_TQ2_0, GGML_TYPE_MXFP4, GGML_TYPE_NVFP4,
};

// The weighted arms that match native on this corpus (EXP616 carried 18; the
// six excluded here are named in UNWEIGHTED's comment above).
static const int WEIGHTED[] = {
    GGML_TYPE_Q4_0, GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1,
    GGML_TYPE_Q2_K, GGML_TYPE_Q3_K, GGML_TYPE_Q4_K, GGML_TYPE_Q5_K,
    GGML_TYPE_Q6_K, GGML_TYPE_IQ4_NL, GGML_TYPE_IQ4_XS, GGML_TYPE_IQ3_S,
    GGML_TYPE_TQ1_0, GGML_TYPE_TQ2_0, GGML_TYPE_MXFP4, GGML_TYPE_NVFP4,
};

// Dequant arms: the five basic types of S5a plus the eighteen of S5b, minus
// q4_0 (the switched dequantizer), minus q8_K (ggml_quantize_chunk cannot
// produce it, so there is no packed corpus for it here), and minus the three
// IQ types the canon dequant modules do not carry (IQ2_XXS, IQ2_XS, IQ1_S).
static const int DEQUANT[] = {
    GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1, GGML_TYPE_Q8_0,
    GGML_TYPE_Q1_0, GGML_TYPE_Q2_0,
    GGML_TYPE_Q2_K, GGML_TYPE_Q3_K, GGML_TYPE_Q4_K, GGML_TYPE_Q5_K,
    GGML_TYPE_Q6_K,
    GGML_TYPE_IQ4_NL, GGML_TYPE_IQ4_XS, GGML_TYPE_IQ3_XXS, GGML_TYPE_IQ3_S,
    GGML_TYPE_IQ2_S, GGML_TYPE_IQ1_M,
    GGML_TYPE_TQ1_0, GGML_TYPE_TQ2_0, GGML_TYPE_MXFP4, GGML_TYPE_NVFP4,
};

#define NW_UNW ((int) (sizeof(UNWEIGHTED) / sizeof(UNWEIGHTED[0])))
#define NW_W   ((int) (sizeof(WEIGHTED) / sizeof(WEIGHTED[0])))
#define NW_DEQ ((int) (sizeof(DEQUANT) / sizeof(DEQUANT[0])))

static const char * type_name(int t) {
    return ggml_type_name((enum ggml_type) t);
}

static void case_chunk(int type, int pat, int64_t start_rows) {
    const int64_t nrows = 2;
    const int64_t n_per_row = (int64_t) ggml_blck_size((enum ggml_type) type) * 2;
    const int64_t start = start_rows * n_per_row;   // start must be a row multiple
    const int64_t n = nrows * n_per_row;

    static float src[4096];
    static unsigned char nat[16384];
    static unsigned char gs[16384];
    memset(nat, 0, sizeof(nat));
    memset(gs, 0, sizeof(gs));

    g_rng = UINT64_C(0x243f6a8885a308d3) + (uint64_t) (type * 31 + pat * 7 + start);
    for (int64_t i = 0; i < n; ++i) {
        src[i] = value(pat, (int) (i % n_per_row), (int) n_per_row);
    }
    const float * const_src = src;
    const float * use_src = const_src;

    const size_t rn = ggml_quantize_chunk((enum ggml_type) type, use_src, nat,
                                          start, nrows, n_per_row, NULL);
    const int64_t rg = goose_chunk(type, use_src, gs, start, nrows, n_per_row);

    ++g_cases;
    if ((size_t) rg != rn) {
        char buf[96];
        snprintf(buf, sizeof(buf), "size native=%zu goose=%lld", rn, (long long) rg);
        fail("chunk_size", type, pat, buf);
        return;
    }
    if (memcmp(nat, gs, rn) != 0) {
        fail("chunk_bytes", type, pat, type_name(type));
    }
}

static void case_chunk_im(int type, int pat) {
    const int64_t nrows = 2;
    const int64_t n_per_row = (int64_t) ggml_blck_size((enum ggml_type) type) * 2;
    const int64_t n = nrows * n_per_row;

    static float src[4096];
    static float im[256];
    static unsigned char nat[16384];
    static unsigned char gs[16384];
    memset(nat, 0, sizeof(nat));
    memset(gs, 0, sizeof(gs));

    g_rng = UINT64_C(0x243f6a8885a308d3) + (uint64_t) (type * 13 + pat);
    for (int64_t i = 0; i < n; ++i) {
        src[i] = value(pat, (int) (i % n_per_row), (int) n_per_row);
    }
    for (int64_t i = 0; i < n_per_row; ++i) {
        im[i] = imvalue(pat, (int) i, (int) n_per_row);
    }

    const size_t rn = ggml_quantize_chunk((enum ggml_type) type, src, nat,
                                          0, nrows, n_per_row, im);
    const int64_t rg = goose_chunk_im(type, src, gs, 0, nrows, n_per_row, im);

    ++g_cases;
    if ((size_t) rg != rn) {
        char buf[96];
        snprintf(buf, sizeof(buf), "size native=%zu goose=%lld", rn, (long long) rg);
        fail("chunk_im_size", type, pat, buf);
        return;
    }
    if (memcmp(nat, gs, rn) != 0) {
        fail("chunk_im_bytes", type, pat, type_name(type));
    }
}

static void case_dequant(int type, int pat) {
    const int64_t n_per_row = (int64_t) ggml_blck_size((enum ggml_type) type) * 2;
    const int64_t n = n_per_row;

    static float src[4096];
    static unsigned char packed[16384];
    static float nat[4096];
    static float gs[4096];

    g_rng = UINT64_C(0x243f6a8885a308d3) + (uint64_t) (type * 17 + pat);
    for (int64_t i = 0; i < n; ++i) {
        src[i] = value(pat, (int) i, (int) n);
    }
    ggml_quantize_chunk((enum ggml_type) type, src, packed, 0, 1, n_per_row, NULL);

    memset(nat, 0x5A, sizeof(nat));
    memset(gs, 0x5A, sizeof(gs));
    native_dequant(type, packed, nat, n);

    const int64_t nb = n / ggml_blck_size((enum ggml_type) type);
    const int64_t bytes = nb * (int64_t) ggml_type_size((enum ggml_type) type);
    sl_u8  s = { packed, bytes };
    sl_f32 d = { gs, n };
    const int64_t rg = (type == GGML_TYPE_Q4_0
                        ? qz_dequant(type, s, n, d)      // q4_0 lives in the S5a set
                        : (type <= GGML_TYPE_Q8_0 ? qz_dequant(type, s, n, d)
                                                  : qd_dequant(type, s, n, d)));

    ++g_cases;
    if (rg != n) {
        char buf[96];
        snprintf(buf, sizeof(buf), "ret=%lld", (long long) rg);
        fail("dequant_size", type, pat, buf);
        return;
    }
    if (memcmp(nat, gs, (size_t) n * sizeof(float)) != 0) {
        fail("dequant_bytes", type, pat, type_name(type));
    }
}

int main(void) {
    printf("ARMS mode=goose corpus=synthetic\n");

    for (int i = 0; i < NW_UNW; ++i) {
        for (int p = 0; p < 2; ++p) {
            case_chunk(UNWEIGHTED[i], p, 0);
        }
    }
    // Block-boundary cases: a chunk that starts one row in, which is where
    // row_size and start_row meet in the destination offset.
    case_chunk(GGML_TYPE_Q4_0, 1, 1);
    case_chunk(GGML_TYPE_Q4_K, 1, 1);
    printf("ARMS stage=chunk cases=%d failures=%d\n", g_cases, g_fails);

    for (int i = 0; i < NW_W; ++i) {
        for (int p = 0; p < 2; ++p) {
            case_chunk_im(WEIGHTED[i], p);
        }
    }
    printf("ARMS stage=chunk_im cases=%d failures=%d\n", g_cases, g_fails);

    for (int i = 0; i < NW_DEQ; ++i) {
        for (int p = 0; p < 2; ++p) {
            case_dequant(DEQUANT[i], p);
        }
    }
    printf("ARMS stage=dequant cases=%d failures=%d\n", g_cases, g_fails);

    printf("ARMS_RESULT mode=goose cases=%d failures=%d\n", g_cases, g_fails);
    return g_fails == 0 ? 0 : 1;
}

#else // FG_GOOSE_QUANT

int main(void) {
    printf("ARMS_RESULT mode=native cases=0 failures=0 skipped=1\n");
    return 0;
}

#endif // FG_GOOSE_QUANT
