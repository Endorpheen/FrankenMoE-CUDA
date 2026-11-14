// Differential gate for P7-04 S5b: the dequant closure and different-type copy.
//
// Native oracle:
//   - valid blocks built by ggml_quantize_chunk(type, src, dst, 0, nrows, n, 0)
//     from libggml-base (q8_K has no chunk case in this tree, so that type is
//     built with quantize_row_q8_K_ref and dequantized through the same traits);
//   - dequantized by ggml_get_type_traits(type)->to_float, the function that
//     ggml_compute_forward_dup_from_q calls;
//   - f16/bf16 conversions by ggml_fp32_to_fp16 / ggml_fp16_to_fp32 /
//     ggml_fp32_to_bf16 / ggml_bf16_to_fp32, the ggml_compute_forward_dup_flt
//     primitives;
//   - q8_0 from_float by quantize_row_q8_0 from libggml-cpu, the function that
//     ggml_compute_forward_dup_to_q calls through type traits.
//
// The port is qd_dequant / qd_copy_to_f32 / qd_copy_from_f32. Floats are
// compared bit for bit with -0 folded to +0 (Goose does not carry the sign of
// zero). Bytes are compared byte for byte.
//
// The port's extern primitives are implemented here: float bit
// reinterpretation and the IQ grid tables. The grids come from
// build/native-tables.c, the literal native tables extracted by
// tests/make-tables.py; the native oracle uses its own copies inside
// libggml-base.
//
// Build and run: tests/build.sh.

#include "runtime-gen.h"
#include "ggml.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
void quantize_row_q8_K_ref(const float * x, void * y, int64_t k);
void dequantize_row_q8_K(const void * x, float * y, int64_t k);
void quantize_row_q8_0(const float * x, void * y, int64_t k);

uint64_t tbl_qd_iq3(int which, int i);
uint64_t tbl_qd_iq2(int which, int i);
}

// ---- the port's extern primitives -----------------------------------------

extern "C" {

int64_t qd_bits(float v) {
    uint32_t b;
    memcpy(&b, &v, sizeof(b));
    return (int64_t) b;
}

float qd_of_bits(int64_t b) {
    uint32_t u = (uint32_t) b;
    float v;
    memcpy(&v, &u, sizeof(v));
    return v;
}

int64_t qd_iq2_grid(int64_t g, int64_t i) {
    return (int64_t) tbl_qd_iq2((int) g, (int) i);
}

int64_t qd_iq3_grid(int64_t g, int64_t i) {
    return (int64_t) tbl_qd_iq3((int) g, (int) i);
}

}  // extern "C"

// ---- helpers --------------------------------------------------------------

static int g_cases = 0;
static int g_fails = 0;
static long long g_values = 0;
static int g_type_cases[64] = {0};
static long long g_type_values[64] = {0};
static int g_copy_cases = 0;

static uint32_t lcg = 987654321u;
static float rnd() {
    lcg = lcg * 1664525u + 1013904223u;
    return ((float) (lcg >> 8) / 16777216.0f) * 2.0f - 1.0f;
}

static void fail(const std::string & tag, const std::string & what) {
    if (g_fails < 24) {
        printf("FAIL %s: %s\n", tag.c_str(), what.c_str());
    }
    g_fails++;
}

static float fold(float v) {
    return v == 0.0f ? 0.0f : v;
}

struct TypeRow {
    const char * name;
    int t;
};

static const TypeRow TYPES[] = {
    {"q1_0",    GGML_TYPE_Q1_0},
    {"q2_0",    GGML_TYPE_Q2_0},
    {"q2_K",    GGML_TYPE_Q2_K},
    {"q3_K",    GGML_TYPE_Q3_K},
    {"q4_K",    GGML_TYPE_Q4_K},
    {"q5_K",    GGML_TYPE_Q5_K},
    {"q6_K",    GGML_TYPE_Q6_K},
    {"q8_K",    GGML_TYPE_Q8_K},
    {"tq1_0",   GGML_TYPE_TQ1_0},
    {"tq2_0",   GGML_TYPE_TQ2_0},
    {"mxfp4",   GGML_TYPE_MXFP4},
    {"nvfp4",   GGML_TYPE_NVFP4},
    {"iq4_nl",  GGML_TYPE_IQ4_NL},
    {"iq4_xs",  GGML_TYPE_IQ4_XS},
    {"iq3_xxs", GGML_TYPE_IQ3_XXS},
    {"iq3_s",   GGML_TYPE_IQ3_S},
    {"iq2_s",   GGML_TYPE_IQ2_S},
    {"iq1_m",   GGML_TYPE_IQ1_M},
};

static const int PATTERNS = 6;

static void fill_pattern(std::vector<float> & v, int pat) {
    const size_t n = v.size();
    for (size_t i = 0; i < n; i++) {
        switch (pat) {
            case 0: v[i] = 0.0f; break;
            case 1: v[i] = (float) ((int) (i % 17) - 8) * 0.5f; break;
            case 2: v[i] = rnd() * 10.0f; break;
            case 3: v[i] = (i == n / 2) ? 50.0f : 0.0f; break;
            case 4: {
                // 40 decades, both signs
                int e = (int) (i % 41) - 20;
                float m = ldexpf(1.0f + (float) (i % 5) * 0.25f, e);
                v[i] = (i & 1) ? -m : m;
            } break;
            default: {
                float m = 1.0e-5f * (float) (1 + (int) (i % 7));
                v[i] = (i % 3 == 0) ? -m : m;
            } break;
        }
    }
}

// Pattern 4 (40 decades) makes two IQ natives abort; pattern 5 is enough to
// exercise the fp16 subnormal block scales. The exclusion is explicit.
static bool pattern_supported(int t, int pat) {
    if (pat != 4) return true;
    if (t == GGML_TYPE_IQ2_S || t == GGML_TYPE_IQ1_M) return false;
    if (t == GGML_TYPE_IQ3_XXS || t == GGML_TYPE_IQ3_S) return false;
    return true;
}

static bool make_blocks(int t, const std::vector<float> & src, int nrows, int n, std::vector<uint8_t> & quant) {
    quant.assign(ggml_row_size((ggml_type) t, n) * nrows, 0);
    if (t == GGML_TYPE_Q8_K) {
        quantize_row_q8_K_ref(src.data(), quant.data(), (int64_t) nrows * n);
        return true;
    }
    int64_t nb = ggml_quantize_chunk((ggml_type) t, src.data(), quant.data(), 0, nrows, n, nullptr);
    return nb > 0;
}

static void compare_floats(const std::string & tag, const std::vector<float> & want, const std::vector<float> & got) {
    if (want.size() != got.size()) {
        fail(tag, "length mismatch");
        return;
    }
    for (size_t i = 0; i < want.size(); i++) {
        g_values++;
        if (memcmp(&want[i], &got[i], sizeof(float)) != 0) {
            // -0 folds to +0, every other value is exact
            if (want[i] == 0.0f && got[i] == 0.0f) {
                continue;
            }
            char buf[160];
            snprintf(buf, sizeof buf, "first diff at %zu want=%g (%08x) got=%g (%08x)", i, want[i],
                     *(uint32_t *) &want[i], got[i], *(uint32_t *) &got[i]);
            fail(tag, buf);
            return;
        }
    }
}

// ---- dequant gate ---------------------------------------------------------

static void run_dequant_case(const TypeRow & tr, int pat, int n, int rows) {
    std::vector<float> row((size_t) n);
    fill_pattern(row, pat);
    std::vector<float> src;
    for (int r = 0; r < rows; r++) {
        std::vector<float> copy = row;
        for (int k = 0; k < r; k++) {
            for (float & v : copy) v = v * 1.0000001f + 1.0f;
        }
        src.insert(src.end(), copy.begin(), copy.end());
    }

    std::vector<uint8_t> quant;
    if (!make_blocks(tr.t, src, rows, n, quant)) {
        fail(tr.name, "native quantize failed");
        return;
    }

    const int64_t nel = (int64_t) rows * n;
    std::vector<float> want((size_t) nel, 0.0f);
    if (tr.t == GGML_TYPE_Q8_K) {
        // q8_K is an intermediate type: this tree registers no to_float trait
        // for it, so the native row decoder is called directly.
        dequantize_row_q8_K(quant.data(), want.data(), nel);
    } else {
        const ggml_type_traits * traits = ggml_get_type_traits((ggml_type) tr.t);
        traits->to_float(quant.data(), want.data(), nel);
    }

    std::vector<float> got((size_t) nel, 0.0f);
    sl_u8 s{quant.data(), (int64_t) quant.size()};
    sl_f32 o{got.data(), (int64_t) got.size()};
    g_cases++;
    int64_t rc = qd_dequant(tr.t, s, nel, o);

    g_type_cases[tr.t]++;
    g_type_values[tr.t] += (long long) nel;
    char tag[96];
    snprintf(tag, sizeof tag, "%s_p%d_n%d_r%d", tr.name, pat, n, rows);
    if (rc != nel) {
        char buf[64];
        snprintf(buf, sizeof buf, "rc=%lld", (long long) rc);
        fail(tag, buf);
        return;
    }
    compare_floats(tag, want, got);
}

static void run_geometry() {
    for (const TypeRow & tr : TYPES) {
        g_cases++;
        if (qd_dequant_elems(tr.t) != ggml_blck_size((ggml_type) tr.t)) {
            fail(tr.name, "block element count");
        }
        g_cases++;
        if (qd_dequant_block_size(tr.t) != (int64_t) ggml_type_size((ggml_type) tr.t)) {
            fail(tr.name, "block byte size");
        }
    }
    // refusals
    std::vector<uint8_t> bytes(4096, 0);
    std::vector<float> out(512, 0.0f);
    sl_u8 s{bytes.data(), (int64_t) bytes.size()};
    sl_f32 o{out.data(), (int64_t) out.size()};
    g_cases++;
    if (qd_dequant(999, s, 256, o) != -1) fail("refusal", "unknown type");
    g_cases++;
    if (qd_dequant(GGML_TYPE_Q6_K, s, 255, o) != -1) fail("refusal", "not a block multiple");
    g_cases++;
    if (qd_dequant(GGML_TYPE_Q6_K, s, 512, o) != 512) fail("refusal", "valid geometry");
}

// ---- copy gate ------------------------------------------------------------

static void run_copy_case(const TypeRow & tr, int pat, int n, int rows) {
    std::vector<float> row((size_t) n);
    fill_pattern(row, pat);
    std::vector<float> src;
    for (int r = 0; r < rows; r++) {
        src.insert(src.end(), row.begin(), row.end());
    }
    std::vector<uint8_t> quant;
    if (!make_blocks(tr.t, src, rows, n, quant)) {
        fail(tr.name, "copy: native quantize failed");
        return;
    }
    const int64_t nel = (int64_t) rows * n;
    // native to_float, one row at a time, as dup_from_q does
    std::vector<float> want((size_t) nel, 0.0f);
    const ggml_type_traits * traits = ggml_get_type_traits((ggml_type) tr.t);
    const size_t row_bytes = ggml_row_size((ggml_type) tr.t, n);
    for (int r = 0; r < rows; r++) {
        traits->to_float(quant.data() + r * row_bytes, want.data() + r * n, n);
    }
    std::vector<float> got((size_t) nel, 0.0f);
    sl_u8 s{quant.data(), (int64_t) quant.size()};
    sl_f32 o{got.data(), (int64_t) got.size()};
    g_cases++;
    g_copy_cases++;
    int64_t rc = qd_copy_to_f32(tr.t, s, nel, o);
    char tag[96];
    snprintf(tag, sizeof tag, "copy_%s_to_f32_p%d_n%d_r%d", tr.name, pat, n, rows);
    if (rc != nel) {
        fail(tag, "rc");
        return;
    }
    compare_floats(tag, want, got);
}

static void run_copy_f32_out(int t, const char * name, int n) {
    std::vector<float> src((size_t) n);
    // values that hit the f16/bf16 rounding ties, subnormals and overflow
    for (int i = 0; i < n; i++) {
        switch (i % 16) {
            case 0: src[i] = 0.0f; break;
            case 1: src[i] = -0.0f; break;
            case 2: src[i] = 1.0f; break;
            case 3: src[i] = 1.0f + ldexpf(1.0f, -11); break;
            case 4: src[i] = 1.0f + ldexpf(1.0f, -12); break;      // f16 tie
            case 5: src[i] = (1.0f + ldexpf(1.0f, -11)) * ldexpf(1.0f, -14); break;
            case 6: src[i] = ldexpf(1.0f, -24); break;              // f16 subnormal
            case 7: src[i] = ldexpf(3.0f, -25); break;              // f16 tie in subnormal
            case 8: src[i] = 65504.0f; break;
            case 9: src[i] = 65520.0f; break;                       // rounds to inf
            case 10: src[i] = 1.0e38f; break;
            // exponent band where the native bias clamp 0x71000000 applies
            case 11: src[i] = ldexpf(1.0f, -15); break;
            case 12: src[i] = ldexpf(1.5f, -15); break;
            case 13: src[i] = ldexpf(3.0f, -16); break;
            case 14: src[i] = -ldexpf(1.0f, -15); break;
            default: src[i] = rnd() * 300.0f; break;
        }
    }
    const int64_t nel = n;
    std::vector<uint8_t> want((size_t) 2 * nel, 0);
    if (t == GGML_TYPE_F16) {
        for (int i = 0; i < n; i++) {
            ggml_fp16_t h = ggml_fp32_to_fp16(src[i]);
            memcpy(want.data() + 2 * i, &h, 2);
        }
    } else {
        for (int i = 0; i < n; i++) {
            ggml_bf16_t h = ggml_fp32_to_bf16(src[i]);
            memcpy(want.data() + 2 * i, &h, 2);
        }
    }
    std::vector<uint8_t> got((size_t) 2 * nel, 0);
    sl_f32 s{src.data(), (int64_t) nel};
    sl_u8 d{got.data(), (int64_t) got.size()};
    g_cases++;
    int64_t rc = qd_copy_from_f32(t, s, nel, d);
    char tag[96];
    snprintf(tag, sizeof tag, "copy_f32_to_%s_n%d", name, n);
    if (rc != nel) {
        fail(tag, "rc");
        return;
    }
    size_t at = 0;
    while (at < want.size() && want[at] == got[at]) at++;
    if (at != want.size()) {
        char buf[128];
        snprintf(buf, sizeof buf, "first byte diff at %zu want=%02x got=%02x", at, want[at], got[at]);
        fail(tag, buf);
        return;
    }
    g_values += (long long) want.size();
}

static void run_copy_16_to_f32(int t, const char * name, int n) {
    std::vector<float> src((size_t) n);
    for (int i = 0; i < n; i++) {
        src[i] = (i % 7 == 0) ? ldexpf(1.0f, -20) : rnd() * 100.0f;
    }
    std::vector<uint8_t> bytes((size_t) 2 * n, 0);
    std::vector<float> want((size_t) n, 0.0f);
    for (int i = 0; i < n; i++) {
        if (t == GGML_TYPE_F16) {
            ggml_fp16_t h = ggml_fp32_to_fp16(src[i]);
            memcpy(bytes.data() + 2 * i, &h, 2);
            want[i] = ggml_fp16_to_fp32(h);
        } else {
            ggml_bf16_t h = ggml_fp32_to_bf16(src[i]);
            memcpy(bytes.data() + 2 * i, &h, 2);
            want[i] = ggml_bf16_to_fp32(h);
        }
    }
    std::vector<float> got((size_t) n, 0.0f);
    sl_u8 s{bytes.data(), (int64_t) bytes.size()};
    sl_f32 o{got.data(), (int64_t) got.size()};
    g_cases++;
    int64_t rc = qd_copy_to_f32(t, s, n, o);
    char tag[96];
    snprintf(tag, sizeof tag, "copy_%s_to_f32_n%d", name, n);
    if (rc != n) {
        fail(tag, "rc");
        return;
    }
    compare_floats(tag, want, got);
}

static void run_copy_q8_0(int n) {
    std::vector<float> src((size_t) n);
    for (int i = 0; i < n; i++) {
        src[i] = (i % 5 == 0) ? (rnd() * 3.0f) : rnd() * 100.0f;
    }
    // one block with amax == 127 makes id exactly 1.0, so the 0.5 grid hits
    // the round-to-nearest-even ties of the q8_0 from_float path
    for (int b = 0; b + 32 <= n; b += 64) {
        src[b] = 127.0f;
        for (int j = 1; j < 32; j++) {
            float mag = (float) (j % 8) + 0.5f;
            src[b + j] = (j & 1) ? -mag : mag;
        }
    }
    std::vector<uint8_t> want((size_t) ggml_row_size(GGML_TYPE_Q8_0, n), 0);
    quantize_row_q8_0(src.data(), want.data(), n);
    std::vector<uint8_t> got(want.size(), 0);
    sl_f32 s{src.data(), (int64_t) n};
    sl_u8 d{got.data(), (int64_t) got.size()};
    g_cases++;
    int64_t rc = qd_copy_from_f32(GGML_TYPE_Q8_0, s, n, d);
    char tag[96];
    snprintf(tag, sizeof tag, "copy_f32_to_q8_0_n%d", n);
    if (rc != n) {
        fail(tag, "rc");
        return;
    }
    size_t at = 0;
    while (at < want.size() && want[at] == got[at]) at++;
    if (at != want.size()) {
        char buf[128];
        snprintf(buf, sizeof buf, "first byte diff at %zu want=%02x got=%02x", at, want[at], got[at]);
        fail(tag, buf);
        return;
    }
    g_values += (long long) want.size();
}

// ---- main -----------------------------------------------------------------

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    // The two IQ2 grids (2048 for iq1_m, 1024 for iq2_s) and both IQ3 grids
    // (256 for iq3_xxs, 512 for iq3_s) come from build/native-tables.c.

    const bool quick = (argc > 1 && strcmp(argv[1], "--quick") == 0);
    const int passes = quick ? 1 : 2;
    const char * only_type = getenv("FG616_ONLY_TYPE");

    run_geometry();

    for (int pass = 0; pass < passes; pass++) {
        for (const TypeRow & tr : TYPES) {
            if (only_type && strcmp(only_type, tr.name) != 0) {
                continue;
            }
            const int blck = ggml_blck_size((ggml_type) tr.t);
            for (int n = 256; n <= 1024; n += 256) {
                if (n % blck != 0) continue;
                for (int pat = 0; pat < PATTERNS; pat++) {
                    if (!pattern_supported(tr.t, pat)) continue;
                    run_dequant_case(tr, pat, n, 1);
                    if (pass == 0) {
                        run_dequant_case(tr, pat, n, 2);
                    }
                }
            }

            // copy: quantized -> f32 on four representative types
            if (!strcmp(tr.name, "q6_K") || !strcmp(tr.name, "iq4_xs") ||
                !strcmp(tr.name, "tq1_0") || !strcmp(tr.name, "iq2_s")) {
                for (int pat = 0; pat < PATTERNS; pat++) {
                    if (!pattern_supported(tr.t, pat)) continue;
                    run_copy_case(tr, pat, 256, 1);
                    run_copy_case(tr, pat, 512, 2);
                }
            }
        }

        run_copy_16_to_f32(GGML_TYPE_F16, "f16", 512);
        run_copy_16_to_f32(GGML_TYPE_BF16, "bf16", 512);
        run_copy_f32_out(GGML_TYPE_F16, "f16", 512);
        run_copy_f32_out(GGML_TYPE_BF16, "bf16", 512);
        run_copy_q8_0(256);
        run_copy_q8_0(512);
    }

    for (const TypeRow & tr : TYPES) {
        printf("type %-8s dequant cases=%d values=%lld\n", tr.name, g_type_cases[tr.t], g_type_values[tr.t]);
    }
    printf("copy quantized->f32 cases=%d (q6_K, iq4_xs, tq1_0, iq2_s)\n", g_copy_cases);
    printf("FG616 cases=%d value/byte comparisons=%lld failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
