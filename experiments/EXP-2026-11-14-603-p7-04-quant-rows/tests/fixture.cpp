// Differential gate for P7-04 S3a: the unweighted row quantizers.
//
// The native oracle is the library entry point the loader uses:
// ggml_quantize_chunk(type, src, dst, start, nrows, n_per_row, nullptr) from
// libggml-base, with libggml-cpu supplying ggml_quantize_init. The port is
// source/quant-rows.goose (qz_quantize_chunk, qz_quantize_row, qz_row_size,
// qz_blck, qz_type_size).
//
// Compared byte for byte: the whole destination buffer, including the region
// before start_row and the returned byte count. Coverage:
//   - seven types (q4_0, q4_1, q5_0, q5_1, q8_0, q1_0, q2_0);
//   - sixteen row patterns: +0, -0, +/-FLT_MAX, minimum subnormals, random
//     [-1,1] and [-10,10], exact 1/8 grid, exact 0.5 ties, a block whose first
//     maximum is positive and whose opposite extreme is negative (the only way
//     the reference nibble clamp is reached), a single peak, a constant block,
//     a ramp, a 60-decade mix, a q8_0 tie grid at amax = 254 (x*id = k + 0.5
//     exactly), subnormal random and a negative-only block;
//   - shapes 32..1024 per row, one to four rows, and a start offset of one or
//     two rows so the start_row * row_size destination arithmetic is exercised;
//   - the geometry refusals: lengths and starts that are not block multiples,
//     and unknown types. Native asserts on those, so the gate checks the port
//     refuses them (-1 unknown type, -2 geometry) instead of comparing bytes;
//   - qz_blck, qz_type_size and qz_row_size against ggml_blck_size,
//     ggml_type_size and ggml_row_size for every gated type and row length.
//
// NaN and inf rows are deliberately absent: the native (int8_t)/(uint8_t)
// float casts are undefined behaviour there, so no byte-identical claim can be
// made. See README.md.
//
// built and run by tests/build.sh.

#include "runtime-gen.h"
#include "ggml.h"

#include <cfloat>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ---- the port's extern primitives -----------------------------------------

extern "C" {

float qz_ld_f32(uint64_t a) {
    float v;
    memcpy(&v, (const void *) (uintptr_t) a, sizeof(v));
    return v;
}

void qz_st_f16(uint64_t a, float v) {
    // The native scale store: quantize_row_q*_ref uses GGML_FP32_TO_FP16, and
    // the public wrapper is the same computation.
    ggml_fp16_t h = ggml_fp32_to_fp16(v);
    memcpy((void *) (uintptr_t) a, &h, sizeof(h));
}

void qz_st_u8(uint64_t a, int64_t v) {
    uint8_t b = (uint8_t) v;
    memcpy((void *) (uintptr_t) a, &b, sizeof(b));
}

void qz_st_u32(uint64_t a, int64_t v) {
    // ggml stores qh with memcpy of a uint32_t; the format is little-endian.
    uint8_t b[4] = { (uint8_t) (v), (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) };
    memcpy((void *) (uintptr_t) a, b, sizeof(b));
}

int64_t qz_ld_u8(uint64_t a) {
    uint8_t b;
    memcpy(&b, (const void *) (uintptr_t) a, sizeof(b));
    return b;
}

float qz_abs(float v) {
    return v < 0.0f ? -v : v;
}

} // extern "C"

// ---- small helpers --------------------------------------------------------

static int g_cases  = 0;
static int g_values = 0;
static int g_fails  = 0;

static void fail(const std::string & what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 40) {
        printf("FAIL %s: %s\n", what.c_str(), detail.c_str());
    }
}

static uint64_t sm64(uint64_t & s) {
    s += UINT64_C(0x9E3779B97F4A7C15);
    uint64_t z = s;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

// [0,1) with 24 significant bits, so the value is exactly representable.
static float frand01(uint64_t & s) {
    return (float) ((sm64(s) >> 40)) * (1.0f / 16777216.0f);
}

static float f32_from_bits(uint32_t bits) {
    float f;
    memcpy(&f, &bits, sizeof(f));
    return f;
}

// ---- row patterns ---------------------------------------------------------

static const int PATTERN_COUNT = 17;

static const char * pattern_name(int p) {
    static const char * names[PATTERN_COUNT] = {
        "zeros", "neg_zero", "pm_fltmax", "min_subnormal", "rand_pm1", "rand_pm10",
        "grid_1_8", "ties_half", "first_max_tie", "single_peak", "constant", "ramp",
        "decade_mix", "q8_tie_grid", "subnormal_rand", "negative_only",
        "rand_exponent",
    };
    return names[p];
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
        case 8:  // first maximum positive, opposite extreme negative
                 if (j == 0) return 1.0f;
                 if (j == 1) return -1.0f;
                 return (float) (j % 7) * 0.001f;
        case 9:  return j == 3 ? 1.0f : 0.0f;
        case 10: return 0.75f;
        case 11: return (float) (j - n / 2) / (float) n;
        case 12: return (j & 1) ? 1.0e30f : 1.0e-30f;
        case 13: // amax = 254 and odd integers: x * id lands on k + 0.5 exactly
                 if (j == 0) return 254.0f;
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
                     // random exponent per element: 2^-30 .. 2^30, no library call
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
    { GGML_TYPE_Q4_0, "q4_0" },
    { GGML_TYPE_Q4_1, "q4_1" },
    { GGML_TYPE_Q5_0, "q5_0" },
    { GGML_TYPE_Q5_1, "q5_1" },
    { GGML_TYPE_Q8_0, "q8_0" },
    { GGML_TYPE_Q1_0, "q1_0" },
    { GGML_TYPE_Q2_0, "q2_0" },
};
static const int TYPE_COUNT = (int) (sizeof(TYPES) / sizeof(TYPES[0]));

struct Shape {
    int64_t n_per_row;
    int64_t nrows;
    int64_t start_rows; // start = start_rows * n_per_row
};

static const Shape SHAPES[] = {
    {  32, 1, 0 }, {  32, 4, 0 }, {  64, 1, 0 }, {  64, 3, 0 },
    {  96, 1, 0 }, { 128, 1, 0 }, { 128, 2, 0 }, { 160, 2, 0 },
    { 256, 1, 0 }, { 256, 3, 0 }, { 384, 1, 0 }, { 512, 1, 0 },
    { 1024, 2, 0 }, {  64, 1, 2 }, { 128, 1, 1 }, { 256, 2, 1 },
};
static const int SHAPE_COUNT = (int) (sizeof(SHAPES) / sizeof(SHAPES[0]));

// ---- one comparison -------------------------------------------------------

static void compare_case(const TypeRow & tr, const Shape & sh, int pattern, int64_t case_no) {
    const int64_t n_per_row = sh.n_per_row;
    const int64_t start     = sh.start_rows * n_per_row;
    const int64_t nrows     = sh.nrows;
    const int64_t row_size  = ggml_row_size(tr.t, n_per_row);
    const int64_t n_floats  = start + nrows * n_per_row;
    const size_t  dst_size  = (size_t) ((sh.start_rows + nrows) * row_size);

    std::vector<float> src((size_t) n_floats);
    uint64_t seed = UINT64_C(0x5EED0000) + (uint64_t) case_no * UINT64_C(0x10001);
    for (int64_t j = 0; j < n_floats; j++) {
        // Rows before start belong to neighbours; they are still filled so the
        // buffer is defined for both sides.
        src[(size_t) j] = pattern_value(pattern, seed, j, n_per_row);
    }

    std::vector<uint8_t> native_dst(dst_size, 0xA5);
    std::vector<uint8_t> port_dst(dst_size, 0xA5);

    const size_t native_n = ggml_quantize_chunk(tr.t, src.data(), native_dst.data(), start, nrows, n_per_row, nullptr);
    const int64_t port_n = qz_quantize_chunk((int64_t) tr.t, (uint64_t) (uintptr_t) src.data(),
                                            (uint64_t) (uintptr_t) port_dst.data(),
                                            start, nrows, n_per_row);

    char what[160];
    snprintf(what, sizeof(what), "%s pattern=%s n_per_row=%lld nrows=%lld start_rows=%lld",
             tr.name, pattern_name(pattern), (long long) n_per_row, (long long) nrows,
             (long long) sh.start_rows);
    ++g_cases;

    if ((int64_t) native_n != port_n) {
        fail(what, "byte count native=" + std::to_string(native_n) + " port=" + std::to_string(port_n));
        return;
    }
    for (size_t b = 0; b < dst_size; b++) {
        ++g_values;
        if (native_dst[b] != port_dst[b]) {
            char det[160];
            snprintf(det, sizeof(det), "offset %zu native=%02x port=%02x", b,
                     (unsigned) native_dst[b], (unsigned) port_dst[b]);
            fail(what, det);
            return;
        }
    }
}

// ---- geometry refusals and size helpers -----------------------------------

static void compare_sizes() {
    const int64_t lengths[] = { 32, 64, 96, 128, 160, 256, 384, 512, 1024 };
    for (int i = 0; i < TYPE_COUNT; i++) {
        const TypeRow & tr = TYPES[i];
        ++g_cases;
        ++g_values;
        if (qz_blck((int64_t) tr.t) != (int64_t) ggml_blck_size(tr.t)) {
            fail(std::string(tr.name) + " blck", std::to_string(qz_blck((int64_t) tr.t)));
        }
        ++g_values;
        if (qz_type_size((int64_t) tr.t) != (int64_t) ggml_type_size(tr.t)) {
            fail(std::string(tr.name) + " type_size", std::to_string(qz_type_size((int64_t) tr.t)));
        }
        for (int64_t ne : lengths) {
            ++g_cases;
            ++g_values;
            const int64_t n = qz_row_size((int64_t) tr.t, ne);
            const int64_t want = (ne % ggml_blck_size(tr.t) == 0) ? (int64_t) ggml_row_size(tr.t, ne) : -2;
            if (n != want) {
                fail(std::string(tr.name) + " row_size ne=" + std::to_string(ne),
                     "port=" + std::to_string(n) + " native=" + std::to_string(want));
            }
        }
    }
}

static void check_refusals() {
    struct Bad {
        int64_t t;
        int64_t start;
        int64_t nrows;
        int64_t n_per_row;
        int64_t want;
    };
    // -1 unknown type, -2 geometry. The native entry point asserts on all of
    // these, and the asserts are compiled out in the release library, so the
    // contract is checked here instead of being fed to the oracle.
    const Bad bad[] = {
        { 9,   0, 1,   32, -1 },  // GGML_TYPE_Q8_1, outside S3a
        { 0,   0, 1,   32, -1 },  // F32
        { GGML_TYPE_Q4_0,   0, 1,  100, -2 },  // row length not a block multiple
        { GGML_TYPE_Q4_0,  33, 1,   32, -2 },  // start not a block multiple
        { GGML_TYPE_Q4_0,  32, 1,   64, -2 },  // start not a row multiple
        { GGML_TYPE_Q1_0,   0, 1,   96, -2 },  // 96 % 128 != 0
        { GGML_TYPE_Q2_0,   0, 1,   32, -2 },  // 32 % 64 != 0
        { GGML_TYPE_Q8_0,   0, 0,   32, -2 },  // no rows
        { GGML_TYPE_Q8_0,   0, 1,    0, -2 },  // no columns
        { GGML_TYPE_Q8_0,  -32, 1,  32, -2 },  // negative start
    };
    uint8_t sink[64];
    std::vector<float> src(64, 1.0f);
    for (const Bad & b : bad) {
        ++g_cases;
        ++g_values;
        const int64_t got = qz_quantize_chunk(b.t, (uint64_t) (uintptr_t) src.data(),
                                              (uint64_t) (uintptr_t) sink,
                                              b.start, b.nrows, b.n_per_row);
        if (got != b.want) {
            fail("refusal", "t=" + std::to_string(b.t) + " n_per_row=" + std::to_string(b.n_per_row) +
                            " start=" + std::to_string(b.start) + " got=" + std::to_string(got) +
                            " want=" + std::to_string(b.want));
        }
    }
    ++g_cases;
    ++g_values;
    const int64_t row_bad = qz_quantize_row((int64_t) GGML_TYPE_Q4_0, (uint64_t) (uintptr_t) src.data(),
                                            (uint64_t) (uintptr_t) sink, 100);
    if (row_bad != -2) {
        fail("row refusal", "k=100 got=" + std::to_string(row_bad));
    }
}

// ---- main -----------------------------------------------------------------

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    compare_sizes();
    check_refusals();

    int64_t case_no = 0;
    for (int i = 0; i < TYPE_COUNT; i++) {
        const TypeRow & tr = TYPES[i];
        const int64_t blck = ggml_blck_size(tr.t);
        for (int s = 0; s < SHAPE_COUNT; s++) {
            if (SHAPES[s].n_per_row % blck != 0) {
                continue;
            }
            for (int p = 0; p < PATTERN_COUNT; p++) {
                compare_case(tr, SHAPES[s], p, case_no++);
            }
        }
    }

    printf("FG607_QROWS cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
