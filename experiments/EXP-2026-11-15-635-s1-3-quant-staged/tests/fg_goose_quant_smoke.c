// Staged S1.3 gate: the three dispatched entry points, byte for byte.
//
// The oracle is a local copy of the native bodies (dequantize_row_q4_0/q4_1/
// q5_0/q5_1/q8_0 and quantize_row_q8_0_ref from ggml-quants.c), written here
// so that one binary can check itself in both configurations: the OFF build
// compares native against native (and must pass with zero Goose calls), the ON
// build compares the Goose path against the same text. A second, cross-build
// check comes from the per-case digests: the OFF and ON dumps must be
// identical files.
//
// No model, no server, no GPU: the corpus is synthetic blocks built from a
// fixed 64-bit generator, named byte patterns and finite fp16 scales/mins.
//
// Build and run: scripts/stage-build-quant.sh.

#include "ggml.h"
#include "ggml-quants.h"

// On OFF the header still declares the counters, as inlines that return 0.
#include "fg-goose-quant-bridge.h"

#ifdef FG_GOOSE_QUANT
// The module's own entry points, for the arms the native wrappers of this
// slice do not dispatch (only q4_0 is rewired on the dequant side).
#include "quant-stack.h"
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- counters -------------------------------------------------------------

static int g_cases  = 0;
static int g_fails  = 0;
static int g_deq    = 0;
static int g_qua    = 0;

static void fail(const char * what, int c, const char * name) {
    ++g_fails;
    printf("FAIL %s case=%d name=%s\n", what, c, name);
}

// ---- deterministic generators --------------------------------------------

static uint64_t g_rng = UINT64_C(0x0123456789abcdef);

static uint32_t xs(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t) (g_rng >> 32);
}

static float frand(float lo, float hi) {
    const float u = (float) (xs() >> 8) * (1.0f / 16777216.0f);
    return lo + u * (hi - lo);
}

// Payload byte for pattern `pat`, block `b`, byte `i` of the quant field.
static uint8_t deq_payload(int pat, int b, int i) {
    switch (pat) {
        case 0: return 0x00;                                  // every nibble 0
        case 1: return 0x88;                                  // every nibble 8
        case 2: return 0xFF;                                  // every nibble 15
        case 3: return (uint8_t) (b * 16 + i);                // ramp over both halves
        case 4: return (i & 1) ? 0xF0 : 0x0F;                 // alternating halves
        case 5: return (uint8_t) xs();                        // uniform random
        case 6: return (uint8_t) (((i % 14) + 1) | ((14 - i % 14) << 4));
        default: return 0x5A;
    }
}

// q8_0 input float for pattern `pat`, element `i` of `n`.
static float qua_value(int pat, int i, int n) {
    switch (pat) {
        case 0: return 0.0f;
        case 1: return 0.25f * (float) i;
        case 2: return (i & 1) ? -1.0f : 1.0f;
        case 3: return frand(-10.0f, 10.0f);
        case 4: return ((i & 1) ? 0.5f : -0.5f) * 127.0f;     // roundf ties
        case 5: return (i == 0) ? 100.0f : 1.0f / 1024.0f;    // single peak
        case 6: return 3.0f;
        case 7: return frand(-1.0e-30f, 1.0e-30f);            // near subnormal
        case 8: return (i & 1) ? 1.0e6f : -1.0e6f;
        case 9: return frand(-1.0e-3f, 1.0e-3f);
        case 10: return -0.5f * (float) (n - 1 - i);          // descending ramp
        default: return 0.5f;
    }
}

// ---- the oracle: the native bodies, copied ---------------------------------

static void ref_dequantize_row_q4_0(const block_q4_0 * x, float * y, int64_t k) {
    const int qk = QK4_0;
    const int nb = (int) (k / qk);
    for (int i = 0; i < nb; i++) {
        const float d = ggml_fp16_to_fp32(x[i].d);
        for (int j = 0; j < qk / 2; ++j) {
            const int x0 = (x[i].qs[j] & 0x0F) - 8;
            const int x1 = (x[i].qs[j] >> 4) - 8;
            y[i * qk + j + 0]      = x0 * d;
            y[i * qk + j + qk / 2] = x1 * d;
        }
    }
}

static void ref_dequantize_row_q4_1(const block_q4_1 * x, float * y, int64_t k) {
    const int qk = QK4_1;
    const int nb = (int) (k / qk);
    for (int i = 0; i < nb; i++) {
        const float d = ggml_fp16_to_fp32(x[i].d);
        const float m = ggml_fp16_to_fp32(x[i].m);
        for (int j = 0; j < qk / 2; ++j) {
            const int x0 = (x[i].qs[j] & 0x0F);
            const int x1 = (x[i].qs[j] >> 4);
            y[i * qk + j + 0]      = x0 * d + m;
            y[i * qk + j + qk / 2] = x1 * d + m;
        }
    }
}

static void ref_dequantize_row_q5_0(const block_q5_0 * x, float * y, int64_t k) {
    const int qk = QK5_0;
    const int nb = (int) (k / qk);
    for (int i = 0; i < nb; i++) {
        const float d = ggml_fp16_to_fp32(x[i].d);
        uint32_t qh;
        memcpy(&qh, x[i].qh, sizeof(qh));
        for (int j = 0; j < qk / 2; ++j) {
            const uint8_t xh_0 = ((qh >> (j + 0)) << 4) & 0x10;
            const uint8_t xh_1 = ((qh >> (j + 12))) & 0x10;
            const int32_t x0 = ((x[i].qs[j] & 0x0F) | xh_0) - 16;
            const int32_t x1 = ((x[i].qs[j] >> 4) | xh_1) - 16;
            y[i * qk + j + 0]      = x0 * d;
            y[i * qk + j + qk / 2] = x1 * d;
        }
    }
}

static void ref_dequantize_row_q5_1(const block_q5_1 * x, float * y, int64_t k) {
    const int qk = QK5_1;
    const int nb = (int) (k / qk);
    for (int i = 0; i < nb; i++) {
        const float d = ggml_fp16_to_fp32(x[i].d);
        const float m = ggml_fp16_to_fp32(x[i].m);
        uint32_t qh;
        memcpy(&qh, x[i].qh, sizeof(qh));
        for (int j = 0; j < qk / 2; ++j) {
            const uint8_t xh_0 = ((qh >> (j + 0)) << 4) & 0x10;
            const uint8_t xh_1 = ((qh >> (j + 12))) & 0x10;
            const int x0 = (x[i].qs[j] & 0x0F) | xh_0;
            const int x1 = (x[i].qs[j] >> 4) | xh_1;
            y[i * qk + j + 0]      = x0 * d + m;
            y[i * qk + j + qk / 2] = x1 * d + m;
        }
    }
}

static void ref_dequantize_row_q8_0(const block_q8_0 * x, float * y, int64_t k) {
    const int qk = QK8_0;
    const int nb = (int) (k / qk);
    for (int i = 0; i < nb; i++) {
        const float d = ggml_fp16_to_fp32(x[i].d);
        for (int j = 0; j < qk; ++j) {
            y[i * qk + j] = x[i].qs[j] * d;
        }
    }
}

static void ref_quantize_row_q8_0(const float * x, block_q8_0 * y, int64_t k) {
    const int nb = (int) (k / QK8_0);
    for (int i = 0; i < nb; i++) {
        float amax = 0.0f;
        for (int j = 0; j < QK8_0; j++) {
            const float v = x[i * QK8_0 + j];
            amax = amax > fabsf(v) ? amax : fabsf(v);
        }
        const float d = amax / 127.0f;
        const float id = d ? 1.0f / d : 0.0f;
        y[i].d = ggml_fp32_to_fp16(d);
        for (int j = 0; j < QK8_0; ++j) {
            const float x0 = x[i * QK8_0 + j] * id;
            y[i].qs[j] = roundf(x0);
        }
    }
}

// ---- digest ---------------------------------------------------------------

static uint64_t fnv1a(const void * data, size_t n) {
    const uint8_t * p = (const uint8_t *) data;
    uint64_t h = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= UINT64_C(0x100000001b3);
    }
    return h;
}

// ---- dequant cases: five basic types --------------------------------------

typedef struct {
    const char * name;
    int type;
    int pat;
    int nb;
    uint16_t d_bits;
    uint16_t m_bits;
} deq_case;

// ggml type ids and packed block sizes (bytes).
#define T_Q4_0 2
#define T_Q4_1 3
#define T_Q5_0 6
#define T_Q5_1 7
#define T_Q8_0 8

static int block_bytes(int type) {
    switch (type) {
        case T_Q4_0: return 18;
        case T_Q4_1: return 20;
        case T_Q5_0: return 22;
        case T_Q5_1: return 24;
        case T_Q8_0: return 34;
        default: return 0;
    }
}

static int block_elems(int type) { return type == T_Q8_0 ? 32 : 32; }

// Build one packed block: scale at 0, min at 2 for the _1 types, the high bits
// at the type's offset, the quant payload last.
static void build_block(int type, int pat, int b, uint16_t d_bits, uint16_t m_bits,
                        uint8_t * blk) {
    const int bs = block_bytes(type);
    memset(blk, 0, (size_t) bs);
    memcpy(blk + 0, &d_bits, 2);
    switch (type) {
        case T_Q4_0:
            for (int i = 0; i < 16; ++i) blk[2 + i] = deq_payload(pat, b, i);
            break;
        case T_Q4_1:
            memcpy(blk + 2, &m_bits, 2);
            for (int i = 0; i < 16; ++i) blk[4 + i] = deq_payload(pat, b, i);
            break;
        case T_Q5_0:
            for (int i = 0; i < 4; ++i) blk[2 + i] = (uint8_t) xs();
            for (int i = 0; i < 16; ++i) blk[6 + i] = deq_payload(pat, b, i);
            break;
        case T_Q5_1:
            memcpy(blk + 2, &m_bits, 2);
            for (int i = 0; i < 4; ++i) blk[4 + i] = (uint8_t) xs();
            for (int i = 0; i < 16; ++i) blk[8 + i] = deq_payload(pat, b, i);
            break;
        case T_Q8_0:
            for (int i = 0; i < 32; ++i) blk[2 + i] = deq_payload(pat, b, i);
            break;
        default:
            break;
    }
}

static void run_dequant(int type, const uint8_t * raw, float * y, int64_t k) {
    switch (type) {
        case T_Q4_0: dequantize_row_q4_0((const block_q4_0 *) raw, y, k); break;
        case T_Q4_1: dequantize_row_q4_1((const block_q4_1 *) raw, y, k); break;
        case T_Q5_0: dequantize_row_q5_0((const block_q5_0 *) raw, y, k); break;
        case T_Q5_1: dequantize_row_q5_1((const block_q5_1 *) raw, y, k); break;
        case T_Q8_0: dequantize_row_q8_0((const block_q8_0 *) raw, y, k); break;
        default: break;
    }
}

static void run_dequant_ref(int type, const uint8_t * raw, float * y, int64_t k) {
    switch (type) {
        case T_Q4_0: ref_dequantize_row_q4_0((const block_q4_0 *) raw, y, k); break;
        case T_Q4_1: ref_dequantize_row_q4_1((const block_q4_1 *) raw, y, k); break;
        case T_Q5_0: ref_dequantize_row_q5_0((const block_q5_0 *) raw, y, k); break;
        case T_Q5_1: ref_dequantize_row_q5_1((const block_q5_1 *) raw, y, k); break;
        case T_Q8_0: ref_dequantize_row_q8_0((const block_q8_0 *) raw, y, k); break;
        default: break;
    }
}

// 12 q4_0 cases with every scale edge that matters (zero, signed zero, half
// max, subnormal), then 3 cases per remaining basic type.
static const deq_case DEQ[] = {
    { "q4_0/zeros",            T_Q4_0, 0, 1, 0x0000, 0x0000 },
    { "q4_0/nibble8_d1",       T_Q4_0, 1, 1, 0x3C00, 0x0000 },
    { "q4_0/ff_d1",            T_Q4_0, 2, 1, 0x3C00, 0x0000 },
    { "q4_0/ramp_dhalf",       T_Q4_0, 3, 2, 0x3800, 0x0000 },
    { "q4_0/alt_dneg1",        T_Q4_0, 4, 3, 0xBC00, 0x0000 },
    { "q4_0/rand_dsubnormal",  T_Q4_0, 5, 4, 0x0001, 0x0000 },
    { "q4_0/rand_dhalfmax",    T_Q4_0, 5, 1, 0x7BFF, 0x0000 },
    { "q4_0/ramp_dnegzero",    T_Q4_0, 3, 1, 0x8000, 0x0000 },
    { "q4_0/alt_dthird",       T_Q4_0, 4, 2, 0x3555, 0x0000 },
    { "q4_0/rand_dneg2",       T_Q4_0, 5, 2, 0xC000, 0x0000 },
    { "q4_0/cycle_dsmall",     T_Q4_0, 6, 3, 0x2C00, 0x0000 },
    { "q4_0/zeros_d1",         T_Q4_0, 0, 2, 0x3C00, 0x0000 },

    { "q4_1/rand_d1_mhalf",    T_Q4_1, 5, 2, 0x3C00, 0xB800 },
    { "q4_1/ramp_dneg_m0",     T_Q4_1, 3, 1, 0xBC00, 0x0000 },
    { "q4_1/alt_dsmall_mneg",  T_Q4_1, 4, 3, 0x2C00, 0xC000 },

    { "q5_0/rand_d1",          T_Q5_0, 5, 2, 0x3C00, 0x0000 },
    { "q5_0/ramp_dnegzero",    T_Q5_0, 3, 1, 0x8000, 0x0000 },
    { "q5_0/ff_dhalf",         T_Q5_0, 2, 3, 0x3800, 0x0000 },

    { "q5_1/rand_d1_mhalf",    T_Q5_1, 5, 2, 0x3C00, 0x3800 },
    { "q5_1/nibble8_dneg",     T_Q5_1, 1, 1, 0xBC00, 0x0000 },
    { "q5_1/cycle_dsmall",     T_Q5_1, 6, 2, 0x2C00, 0xB800 },

    { "q8_0/rand_d1",          T_Q8_0, 5, 2, 0x3C00, 0x0000 },
    { "q8_0/ramp_dneg",        T_Q8_0, 3, 1, 0xBC00, 0x0000 },
    { "q8_0/ff_dsubnormal",    T_Q8_0, 2, 3, 0x0001, 0x0000 },
};

typedef struct { const char * name; int pat; int nb; } qua_case;

static const qua_case QUA[] = {
    { "all_zero",        0, 1 },
    { "ramp_up",         1, 1 },
    { "alternating_pm1", 2, 2 },
    { "rand_pm10",       3, 3 },
    { "ties_half",       4, 1 },
    { "single_peak",     5, 2 },
    { "constant",        6, 4 },
    { "near_subnormal",  7, 1 },
    { "large_pm1e6",     8, 2 },
    { "rand_pm1e3",      9, 1 },
    { "ramp_down",      10, 3 },
    { "flat_half",      11, 1 },
};

#define DEQ_COUNT ((int) (sizeof(DEQ) / sizeof(DEQ[0])))
#define QUA_COUNT ((int) (sizeof(QUA) / sizeof(QUA[0])))

int main(void) {
    printf("FGQ_SMOKE mode=%s corpus=synthetic\n",
#ifdef FG_GOOSE_QUANT
           "goose"
#else
           "native"
#endif
    );

    // 1. the five basic dequantizers ----------------------------------------
    g_rng = UINT64_C(0x0123456789abcdef);
    for (int c = 0; c < DEQ_COUNT; ++c) {
        const deq_case * tc = &DEQ[c];
        const int be  = block_elems(tc->type);
        const int k   = tc->nb * be;
        const int bs  = block_bytes(tc->type);

        uint8_t raw[4 * 34];
        float got[4 * 32];
        float want[4 * 32];

        for (int b = 0; b < tc->nb; ++b) {
            build_block(tc->type, tc->pat, b, tc->d_bits, tc->m_bits, raw + b * bs);
        }

        memset(got,  0x5A, sizeof(got));
        memset(want, 0x5A, sizeof(want));
        run_dequant(tc->type, raw, got, k);
        run_dequant_ref(tc->type, raw, want, k);

        ++g_cases;
        ++g_deq;
        if (memcmp(got, want, (size_t) k * sizeof(float)) != 0) {
            fail("dequant", c, tc->name);
        }
        printf("case=%02d dequant name=%-22s nb=%d bytes=%d digest=%016llx\n",
               c, tc->name, tc->nb, k * (int) sizeof(float),
               (unsigned long long) fnv1a(got, (size_t) k * sizeof(float)));
    }

    printf("stage=dequant row_calls=%lld\n",
           (long long) fg_goose_quant_row_calls());

#ifdef FG_GOOSE_QUANT
    // 1b. the same corpus through the module entry itself, for the types the
    //     native wrappers do not dispatch in this slice. This gates the module
    //     arm, not the seam: it calls qz_dequant() directly.
    g_rng = UINT64_C(0x0123456789abcdef);
    for (int c = 0; c < DEQ_COUNT; ++c) {
        const deq_case * tc = &DEQ[c];
        if (tc->type == T_Q4_0) {
            continue;
        }
        const int be = block_elems(tc->type);
        const int k  = tc->nb * be;
        const int bs = block_bytes(tc->type);

        uint8_t raw[4 * 34];
        float got[4 * 32];
        float want[4 * 32];
        for (int b = 0; b < tc->nb; ++b) {
            build_block(tc->type, tc->pat, b, tc->d_bits, tc->m_bits, raw + b * bs);
        }
        memset(got,  0x5A, sizeof(got));
        memset(want, 0x5A, sizeof(want));
        run_dequant_ref(tc->type, raw, want, k);

        sl_u8  s = { raw, (int64_t) tc->nb * bs };
        sl_f32 d = { got, k };
        const int64_t r = qz_dequant(tc->type, s, k, d);
        ++g_cases;
        if (r != k || memcmp(got, want, (size_t) k * sizeof(float)) != 0) {
            fail("module qz_dequant", c, tc->name);
        }
        printf("module_case=%02d qz_dequant name=%-22s nb=%d ret=%lld digest=%016llx\n",
               c, tc->name, tc->nb, (long long) r,
               (unsigned long long) fnv1a(got, (size_t) k * sizeof(float)));
    }
#endif

    // 2. quantize_row_q8_0_ref ------------------------------------------------
    g_rng = UINT64_C(0x0123456789abcdef);
    for (int c = 0; c < QUA_COUNT; ++c) {
        const qua_case * tc = &QUA[c];
        const int64_t k = (int64_t) tc->nb * QK8_0;
        float src[4 * QK8_0];
        block_q8_0 got[4];
        block_q8_0 want[4];

        for (int j = 0; j < (int) k; ++j) {
            src[j] = qua_value(tc->pat, j, (int) k);
        }

        memset(got,  0x5A, sizeof(got));
        memset(want, 0x5A, sizeof(want));
        quantize_row_q8_0_ref(src, got, k);
        ref_quantize_row_q8_0(src, want, k);

        ++g_cases;
        ++g_qua;
        if (memcmp(got, want, (size_t) tc->nb * sizeof(block_q8_0)) != 0) {
            fail("quant", c, tc->name);
        }
        printf("case=%02d quant   name=%-22s nb=%d bytes=%d digest=%016llx\n",
               DEQ_COUNT + c, tc->name, tc->nb,
               (int) (tc->nb * sizeof(block_q8_0)),
               (unsigned long long) fnv1a(got, (size_t) tc->nb * sizeof(block_q8_0)));
    }

    printf("stage=quant row_calls=%lld\n",
           (long long) fg_goose_quant_row_calls());

    // 3. ggml_blck_size -------------------------------------------------------
    // The seven types the module covers answer from Goose on ON; F32, F16 and
    // Q2_K are native-only and must keep the type-traits answer.
    static const struct { int type; int64_t want; const char * name; } BLCK[] = {
        { GGML_TYPE_Q4_0, 32,  "q4_0"  },
        { GGML_TYPE_Q4_1, 32,  "q4_1"  },
        { GGML_TYPE_Q5_0, 32,  "q5_0"  },
        { GGML_TYPE_Q5_1, 32,  "q5_1"  },
        { GGML_TYPE_Q8_0, 32,  "q8_0"  },
        { GGML_TYPE_Q1_0, 128, "q1_0"  },
        { GGML_TYPE_Q2_0, 64,  "q2_0"  },
        { GGML_TYPE_F32,  1,   "f32"   },
        { GGML_TYPE_F16,  1,   "f16"   },
        { GGML_TYPE_Q2_K, 256, "q2_K"  },
    };

    int blck_fail = 0;
    for (size_t i = 0; i < sizeof(BLCK) / sizeof(BLCK[0]); ++i) {
        const int64_t got = ggml_blck_size((enum ggml_type) BLCK[i].type);
        ++g_cases;
        if (got != BLCK[i].want) {
            ++blck_fail;
            fail("ggml_blck_size", (int) i, BLCK[i].name);
        }
    }
    printf("blck_size checked=%d failures=%d\n",
           (int) (sizeof(BLCK) / sizeof(BLCK[0])), blck_fail);

    // 4. counters ------------------------------------------------------------
    int64_t row_calls  = 0;
    int64_t blck_calls = 0;
#ifdef FG_GOOSE_QUANT
    row_calls  = fg_goose_quant_row_calls();
    blck_calls = fg_goose_quant_blck_calls();
#endif
    printf("FGQ_SMOKE_RESULT mode=%s cases=%d dequant=%d quant=%d failures=%d "
           "row_calls=%lld blck_calls=%lld\n",
#ifdef FG_GOOSE_QUANT
           "goose",
#else
           "native",
#endif
           g_cases, g_deq, g_qua, g_fails,
           (long long) row_calls, (long long) blck_calls);

    return g_fails == 0 ? 0 : 1;
}
