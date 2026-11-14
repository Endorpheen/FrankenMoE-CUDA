// Differential gate for P7-04 S3b: the remaining unweighted row quantizers.
//
// The native oracle is ggml_quantize_chunk(type, src, dst, start, nrows,
// n_per_row, nullptr) from libggml-base (libggml-cpu supplies
// ggml_quantize_init). q8_K is the one exception: this tree's chunk switch has
// no GGML_TYPE_Q8_K case and aborts, so that type is compared against
// quantize_row_q8_K_ref through the row entry instead.
//
// Compared byte for byte: the whole destination buffer (including the region
// before start_row) and the returned byte count.
//
// The port's extern primitives are implemented here: fp16 load/store, f32
// loads/stores, byte reads/writes, fabsf/sqrtf/log2f/floorf/lroundf/ldexpf,
// float <-> bit reinterpretation, the native nearest_int, a 4 KiB scratch
// block, and the IQ grid/map/neighbour tables. The tables come from
// build/native-tables.c, which is the native constructor code extracted by
// tests/make-tables.py; the native oracle uses its own copy of the same tables.
//
// Build and run: tests/build.sh.

#include "runtime-gen.h"
#include "ggml.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
void quantize_row_q8_K_ref(const float * x, void * y, int64_t k);
}

// ---- native IQ tables (build/native-tables.c) -----------------------------

extern "C" {
void tbl_init_iq2(int type);
void tbl_init_iq3(int grid_size);
uint64_t tbl_iq2_grid(int g, int i);
int tbl_iq2_map(int g, int u);
int tbl_iq2_nb(int g, int off);
uint64_t tbl_iq3_grid(int g, int i);
int tbl_iq3_map(int g, int u);
int tbl_iq3_nb(int g, int off);
void tbl_free(void);
}

// ---- the port's extern primitives -----------------------------------------

static uint8_t g_scratch[4096] __attribute__((aligned(16)));

extern "C" {

float qz2_ld_f32(uint64_t a) {
    float v;
    memcpy(&v, (const void *) (uintptr_t) a, sizeof(v));
    return v;
}

void qz2_st_f32(uint64_t a, float v) {
    memcpy((void *) (uintptr_t) a, &v, sizeof(v));
}

float qz2_ld_f16(uint64_t a) {
    ggml_fp16_t h;
    memcpy(&h, (const void *) (uintptr_t) a, sizeof(h));
    return ggml_fp16_to_fp32(h);
}

void qz2_st_f16(uint64_t a, float v) {
    ggml_fp16_t h = ggml_fp32_to_fp16(v);
    memcpy((void *) (uintptr_t) a, &h, sizeof(h));
}

void qz2_st_u8(uint64_t a, int64_t v) {
    uint8_t b = (uint8_t) v;
    memcpy((void *) (uintptr_t) a, &b, sizeof(b));
}

int64_t qz2_ld_u8(uint64_t a) {
    uint8_t b;
    memcpy(&b, (const void *) (uintptr_t) a, sizeof(b));
    return b;
}

void qz2_st_u16(uint64_t a, int64_t v) {
    uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

void qz2_st_u32(uint64_t a, int64_t v) {
    uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

float qz2_abs(float v) { return fabsf(v); }
float qz2_sqrtf(float v) { return sqrtf(v); }
float qz2_log2f(float v) { return log2f(v); }
float qz2_floorf(float v) { return floorf(v); }
int64_t qz2_lroundf(float v) { return (int64_t) lroundf(v); }
float qz2_ldexpf(float v, int64_t e) { return ldexpf(v, (int) e); }

int64_t qz2_bits(float v) {
    uint32_t b;
    memcpy(&b, &v, sizeof(b));
    return (int64_t) b;
}

float qz2_of_bits(int64_t b) {
    uint32_t u = (uint32_t) b;
    float v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

// The native nearest_int, without its debug-only range assert: the reference
// library is built with NDEBUG, so the port must see the same conversion.
int64_t qz2_nearest(float fval) {
    float val = fval + 12582912.0f;
    int32_t i;
    memcpy(&i, &val, sizeof(i));
    return (int64_t) ((i & 0x007fffff) - 0x00400000);
}

uint64_t qz2_tmp(int64_t off) {
    return (uint64_t) (uintptr_t) (g_scratch + off);
}

void qz2_fail(int64_t code) {
    fprintf(stderr, "port failure %lld\n", (long long) code);
    abort();
}

int64_t qz2_iq2_grid(int64_t g, int64_t i) { return (int64_t) tbl_iq2_grid((int) g, (int) i); }
int64_t qz2_iq2_map(int64_t g, int64_t u) { return tbl_iq2_map((int) g, (int) u); }
int64_t qz2_iq2_nb(int64_t g, int64_t off) { return tbl_iq2_nb((int) g, (int) off); }
int64_t qz2_iq3_grid(int64_t g, int64_t i) { return (int64_t) tbl_iq3_grid((int) g, (int) i); }
int64_t qz2_iq3_map(int64_t g, int64_t u) { return tbl_iq3_map((int) g, (int) u); }
int64_t qz2_iq3_nb(int64_t g, int64_t off) { return tbl_iq3_nb((int) g, (int) off); }

// The IQ1_M split search sorts n records of {value, index} by value only, as
// iq1_sort_helper does. Equal keys are ordered by the C library; the fixture
// runs the same qsort call as the oracle, so both see the same permutation.
static int iq1_cmp(const void * l, const void * r) {
    const float * a = (const float *) l;
    const float * b = (const float *) r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

void qz2_sort16(uint64_t pairs, int64_t n) {
    qsort((void *) (uintptr_t) pairs, (size_t) n, 2 * sizeof(float), iq1_cmp);
}

} // extern "C"

// ---- result counters ------------------------------------------------------

static int g_cases  = 0;
static int g_values = 0;
static int g_fails  = 0;

static void fail(const std::string & what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 40) {
        printf("FAIL %s: %s\n", what.c_str(), detail.c_str());
    }
}

// ---- deterministic corpus -------------------------------------------------

static uint64_t sm64(uint64_t & s) {
    s += UINT64_C(0x9E3779B97F4A7C15);
    uint64_t z = s;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

static float frand01(uint64_t & s) {
    return (float) ((sm64(s) >> 40)) * (1.0f / 16777216.0f);
}

static float f32_from_bits(uint32_t bits) {
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

static const int PATTERN_COUNT = 17;

static const char * pattern_name(int p) {
    static const char * names[PATTERN_COUNT] = {
        "zeros", "neg_zero", "pm_fltmax", "min_subnormal", "rand_pm1", "rand_pm10",
        "grid_1_8", "ties_half", "first_max_tie", "single_peak", "constant", "ramp",
        "decade_mix", "q8_tie_grid", "subnormal_rand", "negative_only", "rand_exponent",
    };
    return names[p];
}

// The IQ grid quantizers build a weighted squared distance and abort in native
// code (iq3_find_best_neighbour, iq2_find_best_neighbour) when every candidate
// distance is inf, which happens when x*x overflows: pm_fltmax and decade_mix.
// Those two patterns are excluded for those types and called out in README.md.
static bool pattern_supported(ggml_type tt, int pp) {
    if (getenv("FG612_ALL_PATTERNS")) {
        return true;
    }
    // IQ3 aborts on both huge-value patterns; IQ2_S and IQ1_M only on the
    // 60-decade mix. All other types survive both and are gated on them.
    if (tt == GGML_TYPE_IQ3_XXS || tt == GGML_TYPE_IQ3_S) {
        return !(pp == 2 || pp == 12);
    }
    if (tt == GGML_TYPE_IQ2_S || tt == GGML_TYPE_IQ1_M) {
        return pp != 12;
    }
    return true;
}

static float pattern_value(int p, uint64_t & s, int64_t j, int64_t n) {
    switch (p) {
        case 0:  return 0.0f;
        case 1:  return -0.0f;
        case 2:  return (j & 1) ? -FLT_MAX : FLT_MAX;
        case 3:  return f32_from_bits((j & 1) ? UINT32_C(0x80000001) : UINT32_C(0x00000001));
        case 4:  return frand01(s) * 2.0f - 1.0f;
        case 5:  return frand01(s) * 20.0f - 10.0f;
        case 6:  return (float) (j % 17 - 8) * 0.125f;
        case 7:  return (float) ((int) (j % 13) - 6) * 0.5f;
        case 8:  if (j == 0) return 1.0f;
                 if (j == 1) return -1.0f;
                 return (float) (j % 7) * 0.001f;
        case 9:  return j == 3 ? 1.0f : 0.0f;
        case 10: return 0.75f;
        case 11: return (float) (j - n / 2) / (float) n;
        case 12: return (j & 1) ? 1.0e30f : 1.0e-30f;
        case 13: if (j == 0) return 254.0f;
                 if (j == 1) return -254.0f;
                 {
                     int v = (int) (j % 253) + 1;
                     return (float) ((j & 2) ? -v : v);
                 }
        case 14: {
                     uint32_t bits = UINT32_C(0x00000001) + (uint32_t) (sm64(s) % UINT32_C(0x7FFFFF));
                     float f = f32_from_bits(bits);
                     return (j & 1) ? -f : f;
                 }
        case 15: return -(0.5f + frand01(s) * 1.5f);
        case 16: {
                     const int e = (int) (sm64(s) % 61) - 30;
                     const int k = e < 0 ? -e : e;
                     float scale = 1.0f;
                     for (int i = 0; i < k; i++) {
                         scale = scale * 2.0f;
                     }
                     if (e < 0) {
                         scale = 1.0f / scale;
                     }
                     const float v = scale * (0.5f + frand01(s));
                     return (j & 1) ? -v : v;
                 }
        default: return 0.0f;
    }
}

// ---- types and shapes -----------------------------------------------------

struct TypeRow {
    ggml_type   t;
    const char * name;
};

static const TypeRow TYPES[] = {
    { GGML_TYPE_Q2_K,    "q2_K"    },
    { GGML_TYPE_Q3_K,    "q3_K"    },
    { GGML_TYPE_Q4_K,    "q4_K"    },
    { GGML_TYPE_Q5_K,    "q5_K"    },
    { GGML_TYPE_Q6_K,    "q6_K"    },
    { GGML_TYPE_Q8_K,    "q8_K"    },
    { GGML_TYPE_TQ1_0,   "tq1_0"   },
    { GGML_TYPE_TQ2_0,   "tq2_0"   },
    { GGML_TYPE_MXFP4,   "mxfp4"   },
    { GGML_TYPE_NVFP4,   "nvfp4"   },
    { GGML_TYPE_IQ4_NL,  "iq4_nl"  },
    { GGML_TYPE_IQ4_XS,  "iq4_xs"  },
    { GGML_TYPE_IQ3_XXS, "iq3_xxs" },
    { GGML_TYPE_IQ3_S,   "iq3_s"   },
    { GGML_TYPE_IQ2_S,   "iq2_s"   },
    { GGML_TYPE_IQ1_M,   "iq1_m"   },
};

static const int64_t LENGTHS[] = { 32, 64, 96, 128, 160, 256, 384, 512, 1024 };

// ---- oracle ---------------------------------------------------------------

// ggml_quantize_chunk for the chunk types; the q8_K row entry for q8_K.
// Returns the byte count, or -3 for q8_K to signal the row path is used.
static int64_t oracle(ggml_type t, const float * src, uint8_t * dst,
                      int64_t start, int64_t nrows, int64_t nper) {
    if (t == GGML_TYPE_Q8_K) {
        quantize_row_q8_K_ref(src + start, dst, nrows * nper);
        return nrows * nper / 256 * ggml_type_size(t);
    }
    return (int64_t) ggml_quantize_chunk(t, src, dst, start, nrows, nper, nullptr);
}

// ---- one case -------------------------------------------------------------

static void run_case(ggml_type t, const char * tname, int p, int64_t nper,
                     int64_t nrows, int64_t start_rows) {
    const int64_t blck = ggml_blck_size(t);
    if (nper % blck != 0) {
        return;
    }
    const int64_t row_size  = ggml_row_size(t, nper);
    const int64_t start     = start_rows * nper;
    const int64_t n_src     = start + nrows * nper;
    const int64_t dst_rows  = start_rows + nrows;
    const int64_t dst_bytes = dst_rows * row_size;

    std::vector<float>   src((size_t) n_src);
    uint64_t seed = UINT64_C(0x1234567890ABCDEF) ^ (uint64_t) t * 1315423911u
                    ^ (uint64_t) p * 2654435761u ^ (uint64_t) nper * 97u
                    ^ (uint64_t) nrows * 13u ^ (uint64_t) start_rows * 7u;
    for (int64_t j = 0; j < n_src; j++) {
        src[(size_t) j] = pattern_value(p, seed, j, nper);
    }

    std::vector<uint8_t> dst_ref((size_t) dst_bytes, 0xA5);
    std::vector<uint8_t> dst_port((size_t) dst_bytes, 0xA5);

    if (getenv("FG612_TRACE")) {
        fprintf(stderr, "TRACE %s/%s/nper=%lld/rows=%lld/start=%lld\n",
                tname, pattern_name(p), (long long) nper, (long long) nrows, (long long) start_rows);
    }
    int64_t r_ref  = 0;
    int64_t r_port = 0;
    if (t == GGML_TYPE_Q8_K) {
        // The chunk switch has no q8_K; compare the row entries over the whole
        // block-aligned region. start_rows is only meaningful for the refusals.
        if (start_rows != 0) {
            return;
        }
        r_ref  = oracle(t, src.data(), dst_ref.data(), 0, nrows, nper);
        r_port = qz2_quantize_row((int64_t) t, (uint64_t) (uintptr_t) src.data(),
                                 (uint64_t) (uintptr_t) dst_port.data(), nrows * nper);
    } else {
        r_ref  = oracle(t, src.data(), dst_ref.data(), start, nrows, nper);
        r_port = qz2_quantize_chunk((int64_t) t, (uint64_t) (uintptr_t) src.data(),
                                   (uint64_t) (uintptr_t) dst_port.data(), start, nrows, nper);
    }

    ++g_cases;
    char tag[256];
    snprintf(tag, sizeof(tag), "%s/%s/nper=%lld/rows=%lld/start=%lld",
             tname, pattern_name(p), (long long) nper, (long long) nrows, (long long) start_rows);

    if (r_ref != r_port) {
        char d[128];
        snprintf(d, sizeof(d), "byte count ref=%lld port=%lld", (long long) r_ref, (long long) r_port);
        fail(tag, d);
        return;
    }
    if (memcmp(dst_ref.data(), dst_port.data(), (size_t) dst_bytes) != 0) {
        std::string diff;
        for (int64_t j = 0; j < dst_bytes; j++) {
            ++g_values;
            if (dst_ref[(size_t) j] != dst_port[(size_t) j]) {
                char d[128];
                snprintf(d, sizeof(d), "first diff at byte %lld: ref=%02x port=%02x",
                         (long long) j, dst_ref[(size_t) j], dst_port[(size_t) j]);
                fail(tag, d);
                return;
            }
        }
        fail(tag, "buffer differs");
        return;
    }
    g_values += dst_bytes;
}

// ---- geometry -------------------------------------------------------------

static void run_geometry(void) {
    for (const TypeRow & tr : TYPES) {
        const int64_t b_ref  = ggml_blck_size(tr.t);
        const int64_t ts_ref = (int64_t) ggml_type_size(tr.t);
        if (qz2_blck((int64_t) tr.t) != b_ref || qz2_type_size((int64_t) tr.t) != ts_ref) {
            fail("geometry", std::string(tr.name) + " blck/type_size mismatch");
            continue;
        }
        for (int64_t ne : LENGTHS) {
            int64_t want = -2;
            if (ne % b_ref == 0) {
                want = (int64_t) ggml_row_size(tr.t, ne);
            }
            ++g_cases;
            if (qz2_row_size((int64_t) tr.t, ne) != want) {
                fail("row_size", std::string(tr.name) + " ne=" + std::to_string(ne));
            }
        }
    }
    // types outside the slice
    if (qz2_blck(0) != -1 || qz2_type_size(0) != -1 || qz2_row_size(0, 32) != -1) {
        fail("outside", "type 0 is not refused");
    }
    if (qz2_blck(2) != -1) { fail("outside", "q4_0 is S3a, not S3b"); }
}

// ---- refusals -------------------------------------------------------------

static void run_refusals(void) {
    std::vector<float>   src(4096, 0.25f);
    std::vector<uint8_t> dst(1 << 16, 0xA5);
    const uint64_t s = (uint64_t) (uintptr_t) src.data();
    const uint64_t d = (uint64_t) (uintptr_t) dst.data();

    struct Case { int64_t want; int64_t t, start, nrows, nper; const char * what; };
    const Case cases[] = {
        { -1, 0,   0, 1, 256, "unknown type" },
        { -1, 2,   0, 1, 256, "q4_0 belongs to S3a" },
        { -2, 10,  0, 1, 128, "q2_K n_per_row not a block multiple" },
        { -2, 10,  0, 0, 256, "zero rows" },
        { -2, 10,  0, 1,   0, "zero columns" },
        { -2, 10, -1, 1, 256, "negative start" },
        { -2, 10,  1, 1, 256, "start not a block multiple" },
        { -2, 10, 32, 1, 256, "start not a row multiple" },
        { -2, 40,  0, 1, 160, "nvfp4 n_per_row not a block multiple" },
        { -2, 20,  0, 1, 48,  "iq4_nl n_per_row not a block multiple" },
        { -3, 15,  0, 1, 256, "q8_K is not a chunk type" },
    };
    for (const Case & c : cases) {
        ++g_cases;
        const int64_t got = qz2_quantize_chunk(c.t, s, d, c.start, c.nrows, c.nper);
        if (got != c.want) {
            fail(std::string("refusal ") + c.what,
                 "want " + std::to_string(c.want) + " got " + std::to_string(got));
        }
    }
    // row entry geometry
    ++g_cases;
    if (qz2_quantize_row(10, s, d, 255) != -2) { fail("refusal", "row k not a block multiple"); }
    ++g_cases;
    if (qz2_quantize_row(0, s, d, 256) != -1) { fail("refusal", "row unknown type"); }
    ++g_cases;
    if (qz2_quantize_row(15, s, d, 512) != 292 * 2) { fail("refusal", "q8_K row size"); }
}

// ---- main -----------------------------------------------------------------

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    // The two IQ2 table families and both IQ3 grids. The constructors are the
    // native ones; the oracle builds its own copies inside libggml-base.
    tbl_init_iq2(GGML_TYPE_IQ1_M);
    tbl_init_iq2(GGML_TYPE_IQ2_S);
    tbl_init_iq3(256);
    tbl_init_iq3(512);

    const bool quick = (argc > 1 && strcmp(argv[1], "--quick") == 0);
    const int passes = quick ? 1 : 2;
    const char * only_type = getenv("FG612_ONLY_TYPE");
    const char * only_pat  = getenv("FG612_ONLY_PAT");

    run_geometry();
    run_refusals();

    for (int pass = 0; pass < passes; pass++) {
        for (const TypeRow & tr : TYPES) {
            if (only_type && strcmp(only_type, tr.name) != 0) {
                continue;
            }
            const int64_t blck = ggml_blck_size(tr.t);
            for (int64_t nper : LENGTHS) {
                if (nper % blck != 0) {
                    continue;
                }
                if (pass == 1 && nper != 256 && nper != 512) {
                    continue;
                }
                for (int p = 0; p < PATTERN_COUNT; p++) {
                    if (only_pat && atoi(only_pat) != p) {
                        continue;
                    }
                    if (!pattern_supported(tr.t, p)) {
                        continue;
                    }
                    // one and two rows at start 0; the destination offset at a
                    // non-zero start row on every length that has room for it
                    run_case(tr.t, tr.name, p, nper, 1, 0);
                    run_case(tr.t, tr.name, p, nper, 2, 0);
                    run_case(tr.t, tr.name, p, nper, 1, 2);
                    if (pass == 0 && (nper == 256 || nper == 512)) {
                        run_case(tr.t, tr.name, p, nper, 4, 1);
                    }
                }
            }
        }
    }

    tbl_free();

    printf("cases %d, byte/value comparisons %d, failures %d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
