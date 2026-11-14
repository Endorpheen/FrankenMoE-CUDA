// Differential gate for P7-04 S4: the weighted (imatrix) row quantizers.
//
// The native oracle is ggml_quantize_chunk(type, src, dst, start, nrows,
// n_per_row, imatrix) from libggml-base (libggml-cpu supplies
// ggml_quantize_init). The port is qz_quantize_chunk_im with the same imatrix
// address. The whole destination buffer (including the region before
// start_row) and the returned byte count are compared byte for byte.
//
// Three corpora:
//   - weighted: 18 types x 4 imatrix patterns x 17 value patterns x block
//     multiple lengths 32..1024 x 1-4 rows x start offsets.
//   - null-arm: the 11 types of this port that have an unweighted native arm,
//     against the same oracle with imatrix == nullptr. The full S3b corpus is
//     compared against the accepted S3b port by tests/null-regress.cpp.
//   - refusals: the port's -4/-5 answers for NULL arms it does not carry and
//     the chunk geometry the native asserts on.
//
// The port's extern primitives are implemented here: fp16 load/store, f32
// loads/stores, byte reads/writes, fabsf/sqrtf/log2f/floorf/lroundf/ldexpf,
// float <-> bit reinterpretation, the native nearest_int, a 4 KiB scratch
// block, and the IQ grid/map/neighbour tables. The tables come from
// build/native-tables.c, the native constructor code extracted by
// tests/make-tables.py; the native oracle builds its own copy inside
// libggml-base.
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

static uint8_t g_scratch[8192] __attribute__((aligned(16)));

extern "C" {

float qz_ld_f32(uint64_t a) {
    float v;
    memcpy(&v, (const void *) (uintptr_t) a, sizeof(v));
    return v;
}

void qz_st_f32(uint64_t a, float v) {
    memcpy((void *) (uintptr_t) a, &v, sizeof(v));
}

float qz_ld_f16(uint64_t a) {
    ggml_fp16_t h;
    memcpy(&h, (const void *) (uintptr_t) a, sizeof(h));
    return ggml_fp16_to_fp32(h);
}

void qz_st_f16(uint64_t a, float v) {
    ggml_fp16_t h = ggml_fp32_to_fp16(v);
    memcpy((void *) (uintptr_t) a, &h, sizeof(h));
}

void qz_st_u8(uint64_t a, int64_t v) {
    uint8_t b = (uint8_t) v;
    memcpy((void *) (uintptr_t) a, &b, sizeof(b));
}

int64_t qz_ld_u8(uint64_t a) {
    uint8_t b;
    memcpy(&b, (const void *) (uintptr_t) a, sizeof(b));
    return b;
}

void qz_st_u16(uint64_t a, int64_t v) {
    uint8_t b[2] = { (uint8_t) v, (uint8_t) (v >> 8) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

void qz_st_u32(uint64_t a, int64_t v) {
    uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

float qz_abs(float v) { return fabsf(v); }
float qz_sqrtf(float v) { return sqrtf(v); }
float qz_log2f(float v) { return log2f(v); }
float qz_floorf(float v) { return floorf(v); }
int64_t qz_lroundf(float v) { return (int64_t) lroundf(v); }
float qz_ldexpf(float v, int64_t e) { return ldexpf(v, (int) e); }

int64_t qz_bits(float v) {
    uint32_t b;
    memcpy(&b, &v, sizeof(b));
    return (int64_t) b;
}

float qz_of_bits(int64_t b) {
    uint32_t u = (uint32_t) b;
    float v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

// The native nearest_int, without its debug-only range assert: the reference
// library is built with NDEBUG, so the port must see the same conversion.
int64_t qz_nearest(float fval) {
    float val = fval + 12582912.0f;
    int32_t i;
    memcpy(&i, &val, sizeof(i));
    return (int64_t) ((i & 0x007fffff) - 0x00400000);
}

uint64_t qz_tmp(int64_t off) {
    return (uint64_t) (uintptr_t) (g_scratch + off);
}

void qz_fail(int64_t code) {
    fprintf(stderr, "port failure %lld\n", (long long) code);
    abort();
}

int64_t qz_iq2_grid(int64_t g, int64_t i) { return (int64_t) tbl_iq2_grid((int) g, (int) i); }
int64_t qz_iq2_map(int64_t g, int64_t u) { return tbl_iq2_map((int) g, (int) u); }
int64_t qz_iq2_nb(int64_t g, int64_t off) { return tbl_iq2_nb((int) g, (int) off); }
int64_t qz_iq3_grid(int64_t g, int64_t i) { return (int64_t) tbl_iq3_grid((int) g, (int) i); }
int64_t qz_iq3_map(int64_t g, int64_t u) { return tbl_iq3_map((int) g, (int) u); }
int64_t qz_iq3_nb(int64_t g, int64_t off) { return tbl_iq3_nb((int) g, (int) off); }

// The IQ1 sort orders n records of {value, index} by value only, as
// iq1_sort_helper does. Equal keys are ordered by the C library; the fixture
// runs the same qsort call as the oracle, so both see the same permutation.
static int iq1_cmp(const void * l, const void * r) {
    const float * a = (const float *) l;
    const float * b = (const float *) r;
    return *a < *b ? -1 : *a > *b ? 1 : 0;
}

void qz_sort16(uint64_t pairs, int64_t n) {
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
// code (iq*_find_best_neighbour) when every candidate distance is inf, which
// happens when the weights or x*x overflow: pm_fltmax and decade_mix. Those
// pairs are excluded for those types and called out in README.md. The same
// exclusions were used by the S3b gate for the types it carried.
static bool pattern_supported(ggml_type tt, int pp) {
    if (getenv("FG615_ALL_PATTERNS")) {
        return true;
    }
    if (tt == GGML_TYPE_IQ3_XXS || tt == GGML_TYPE_IQ3_S ||
        tt == GGML_TYPE_IQ2_XXS || tt == GGML_TYPE_IQ2_XS || tt == GGML_TYPE_IQ1_S) {
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

// ---- imatrix patterns -----------------------------------------------------
// Deterministic positive weights: constant, ramp, exponential, sparse large.
// The imatrix is per column and the same for every row, exactly as
// ggml_quantize_chunk hands it to the quantizers.

static const int IW_COUNT = 4;

static const char * iw_name(int w) {
    static const char * names[IW_COUNT] = { "const", "ramp", "exp", "sparse" };
    return names[w];
}

static float iw_value(int p, int64_t j) {
    switch (p) {
        case 0: return 1.0f;
        case 1: return 0.25f + (float) (j % 64) * 0.125f;
        case 2: {
                    const int e = (int) (j % 21) - 10;
                    const int k = e < 0 ? -e : e;
                    float s = 1.0f;
                    for (int i = 0; i < k; i++) {
                        s = s * 2.0f;
                    }
                    return e < 0 ? 1.0f / s : s;
                }
        case 3: if (j % 97 == 31) return 1000.0f;
                if (j % 13 == 5) return 0.001f;
                return 1.0f;
        default: return 1.0f;
    }
}

// ---- types and shapes -----------------------------------------------------

struct TypeRow {
    ggml_type   t;
    const char * name;
    bool         null_ok;
};

// The 18 weighted quantizers of this slice. null_ok marks the types whose
// unweighted native arm exists (not the S3a q4_0 family, not the three IQ
// types whose native impls assert on a NULL imatrix).
static const TypeRow TYPES[] = {
    { GGML_TYPE_Q4_0,    "q4_0",    false },
    { GGML_TYPE_Q4_1,    "q4_1",    false },
    { GGML_TYPE_Q5_0,    "q5_0",    false },
    { GGML_TYPE_Q5_1,    "q5_1",    false },
    { GGML_TYPE_Q2_K,    "q2_K",    true  },
    { GGML_TYPE_Q3_K,    "q3_K",    true  },
    { GGML_TYPE_Q4_K,    "q4_K",    true  },
    { GGML_TYPE_Q5_K,    "q5_K",    true  },
    { GGML_TYPE_Q6_K,    "q6_K",    true  },
    { GGML_TYPE_IQ2_XXS, "iq2_xxs", false },
    { GGML_TYPE_IQ2_XS,  "iq2_xs",  false },
    { GGML_TYPE_IQ3_XXS, "iq3_xxs", true  },
    { GGML_TYPE_IQ3_S,   "iq3_s",   true  },
    { GGML_TYPE_IQ2_S,   "iq2_s",   true  },
    { GGML_TYPE_IQ1_S,   "iq1_s",   false },
    { GGML_TYPE_IQ1_M,   "iq1_m",   true  },
    { GGML_TYPE_IQ4_NL,  "iq4_nl",  true  },
    { GGML_TYPE_IQ4_XS,  "iq4_xs",  true  },
};

static const int64_t LENGTHS[] = { 32, 64, 96, 128, 160, 256, 384, 512, 1024 };

// ---- one case -------------------------------------------------------------

// imat == 0 selects the NULL arm, otherwise the weighted arm with the same
// per-column weights for every row.
static void run_case(ggml_type t, const char * tname, int wp, int iwp, int64_t nper,
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
    const bool weighted = (iwp >= 0);

    std::vector<float> src((size_t) n_src);
    uint64_t seed = UINT64_C(0x1234567890ABCDEF) ^ (uint64_t) t * 1315423911u
                    ^ (uint64_t) wp * 2654435761u ^ (uint64_t) nper * 97u
                    ^ (uint64_t) nrows * 13u ^ (uint64_t) start_rows * 7u;
    for (int64_t j = 0; j < n_src; j++) {
        src[(size_t) j] = pattern_value(wp, seed, j, nper);
    }

    std::vector<float> imat;
    if (weighted) {
        imat.resize((size_t) nper);
        for (int64_t j = 0; j < nper; j++) {
            imat[(size_t) j] = iw_value(iwp, j);
        }
    }
    const float * imp = weighted ? imat.data() : nullptr;

    std::vector<uint8_t> dst_ref((size_t) dst_bytes, 0xA5);
    std::vector<uint8_t> dst_port((size_t) dst_bytes, 0xA5);

    if (getenv("FG615_TRACE")) {
        fprintf(stderr, "TRACE %s/%s/%s/nper=%lld/rows=%lld/start=%lld\n",
                tname, pattern_name(wp), weighted ? iw_name(iwp) : "null",
                (long long) nper, (long long) nrows, (long long) start_rows);
    }

    const int64_t r_ref = (int64_t) ggml_quantize_chunk(t, src.data(), dst_ref.data(),
                                                        start, nrows, nper, imp);
    const int64_t r_port = qw_quantize_chunk_im((int64_t) t,
                                                (uint64_t) (uintptr_t) src.data(),
                                                (uint64_t) (uintptr_t) dst_port.data(),
                                                start, nrows, nper,
                                                (uint64_t) (uintptr_t) imp);

    ++g_cases;
    char tag[256];
    snprintf(tag, sizeof(tag), "%s/%s/%s/nper=%lld/rows=%lld/start=%lld",
             tname, pattern_name(wp), weighted ? iw_name(iwp) : "null",
             (long long) nper, (long long) nrows, (long long) start_rows);

    if (r_ref != r_port) {
        char d[128];
        snprintf(d, sizeof(d), "byte count ref=%lld port=%lld", (long long) r_ref, (long long) r_port);
        fail(tag, d);
        return;
    }
    if (memcmp(dst_ref.data(), dst_port.data(), (size_t) dst_bytes) != 0) {
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
        if (qw_blck((int64_t) tr.t) != b_ref || qw_type_size((int64_t) tr.t) != ts_ref) {
            fail("geometry", std::string(tr.name) + " blck/type_size mismatch");
            continue;
        }
        for (int64_t ne : LENGTHS) {
            int64_t want = -2;
            if (ne % b_ref == 0) {
                want = (int64_t) ggml_row_size(tr.t, ne);
            }
            ++g_cases;
            if (qw_row_size((int64_t) tr.t, ne) != want) {
                fail("row_size", std::string(tr.name) + " ne=" + std::to_string(ne));
            }
        }
    }
    // types outside the slice
    if (qw_blck(0) != -1 || qw_type_size(0) != -1 || qw_row_size(0, 32) != -1) {
        fail("outside", "type 0 is not refused");
    }
    if (qw_blck(8) != -1) { fail("outside", "q8_0 is not a weighted type"); }
}

// ---- refusals -------------------------------------------------------------

static void run_refusals(void) {
    std::vector<float>   src(8192, 0.25f);
    std::vector<uint8_t> dst(1 << 16, 0xA5);
    std::vector<float>   imat(1024, 1.5f);
    const uint64_t s = (uint64_t) (uintptr_t) src.data();
    const uint64_t d = (uint64_t) (uintptr_t) dst.data();
    const uint64_t iw = (uint64_t) (uintptr_t) imat.data();

    struct Case { int64_t want; int64_t t, start, nrows, nper; uint64_t im; const char * what; };
    const Case cases[] = {
        { -1, 0,   0, 1, 256, iw, "unknown type" },
        { -1, 8,   0, 1, 256, iw, "q8_0 is outside the slice" },
        { -3, 15,  0, 1, 256, iw, "q8_K is not a chunk type" },
        { -2, 10,  0, 1, 128, iw, "q2_K n_per_row not a block multiple" },
        { -2, 10,  0, 0, 256, iw, "zero rows" },
        { -2, 10,  0, 1,   0, iw, "zero columns" },
        { -2, 10, -1, 1, 256, iw, "negative start" },
        { -2, 10,  1, 1, 256, iw, "start not a block multiple" },
        { -2, 10, 32, 1, 256, iw, "start not a row multiple" },
        { -2, 20,  0, 1, 48,  iw, "iq4_nl n_per_row not a block multiple" },
        { -4, 2,   0, 1, 256, 0,  "q4_0 NULL arm is S3a" },
        { -5, 16,  0, 1, 256, 0,  "iq2_xxs NULL arm asserts natively" },
        { -5, 19,  0, 1, 256, 0,  "iq1_s NULL arm asserts natively" },
    };
    for (const Case & c : cases) {
        ++g_cases;
        const int64_t got = qw_quantize_chunk_im(c.t, s, d, c.start, c.nrows, c.nper, c.im);
        if (got != c.want) {
            fail(std::string("refusal ") + c.what,
                 "want " + std::to_string(c.want) + " got " + std::to_string(got));
        }
    }
    // weighted arms accepted where the NULL arm is refused
    struct Ok { int64_t t, nper; const char * what; };
    const Ok oks[] = {
        { 2,  256, "q4_0 weighted" },
        { 16, 256, "iq2_xxs weighted" },
        { 19, 256, "iq1_s weighted" },
    };
    for (const Ok & c : oks) {
        ++g_cases;
        const int64_t got = qw_quantize_chunk_im(c.t, s, d, 0, 1, c.nper, iw);
        if (got <= 0) {
            fail(std::string("accept ") + c.what, "got " + std::to_string(got));
        }
    }
    // row entry geometry and its NULL-arm refusals
    ++g_cases;
    if (qw_quantize_row(10, s, d, 255) != -2) { fail("refusal", "row k not a block multiple"); }
    ++g_cases;
    if (qw_quantize_row(0, s, d, 256) != -1) { fail("refusal", "row unknown type"); }
    ++g_cases;
    if (qw_quantize_row(2, s, d, 256) != -4) { fail("refusal", "row q4_0 NULL arm"); }
    ++g_cases;
    if (qw_quantize_row(16, s, d, 256) != -5) { fail("refusal", "row iq2_xxs NULL arm"); }
}

// ---- main -----------------------------------------------------------------

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    // Both IQ2 table families (index 0..3: IQ2_XXS, IQ2_XS, IQ1_S/IQ1_M,
    // IQ2_S) and both IQ3 grids. The constructors are the native ones; the
    // oracle builds its own copies inside libggml-base.
    tbl_init_iq2(GGML_TYPE_IQ2_XXS);
    tbl_init_iq2(GGML_TYPE_IQ2_XS);
    tbl_init_iq2(GGML_TYPE_IQ1_M);
    tbl_init_iq2(GGML_TYPE_IQ2_S);
    tbl_init_iq3(256);
    tbl_init_iq3(512);

    const bool quick = (argc > 1 && strcmp(argv[1], "--quick") == 0);
    const int passes = quick ? 1 : 2;
    const char * only_type = getenv("FG615_ONLY_TYPE");
    const char * only_pat  = getenv("FG615_ONLY_PAT");

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
                    for (int iw = 0; iw < IW_COUNT; iw++) {
                        run_case(tr.t, tr.name, p, iw, nper, 1, 0);
                        run_case(tr.t, tr.name, p, iw, nper, 2, 0);
                        run_case(tr.t, tr.name, p, iw, nper, 1, 2);
                        if (pass == 0 && (nper == 256 || nper == 512)) {
                            run_case(tr.t, tr.name, p, iw, nper, 4, 1);
                        }
                    }
                    if (tr.null_ok) {
                        // NULL arm of the same type, one geometry per length
                        run_case(tr.t, tr.name, p, -1, nper, 1, 0);
                        run_case(tr.t, tr.name, p, -1, nper, 2, 1);
                    }
                }
            }
        }
    }

    tbl_free();

    printf("cases %d, byte/value comparisons %d, failures %d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
