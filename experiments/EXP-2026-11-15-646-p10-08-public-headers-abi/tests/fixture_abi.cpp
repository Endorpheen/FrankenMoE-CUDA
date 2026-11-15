// Differential gate for P10-08 S1 (ggml quantization type table).
//
// Native arm: tests/extract_native.py parses type_traits[GGML_TYPE_COUNT] out of
// ggml/src/ggml.c on every build and this file resolves the block expressions
// (QK_K, sizeof(block_q4_K), ...) against the real ggml.h and ggml-common.h. No
// table is transcribed here.
//
// Port arm: the abi_ exports of source/ggml-type-table.goose.
//
// Compile-time part: the canon freeze block below pins GGML_TYPE_COUNT and the
// twelve QK* block sizes. A header change that keeps the table self-consistent
// but moves the ABI still fails the build here.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "abi.h"
#include "native_abi.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-28s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const char * want, const std::string & got) {
    values++;
    const char * w = want == nullptr ? "" : want;
    if (got != w) {
        fails++;
        printf("FAIL %-28s want=[%s] got=[%s]\n", what, w, got.c_str());
    }
}

static std::string port_name(int64_t t) {
    char buf[128];
    memset(buf, 0x5a, sizeof buf);
    int64_t n = abi_type_name(t, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf});
    if (n < 0) {
        return "!";
    }
    return std::string(buf, (size_t) n);
}

// ── canon freeze: the public geometry constants this port is written against ──
static_assert(GGML_TYPE_COUNT == 43, "GGML_TYPE_COUNT moved");
static_assert(QK_K == 256, "QK_K moved");
static_assert(QK1_0 == 128, "QK1_0 moved");
static_assert(QK2_0 == 64, "QK2_0 moved");
static_assert(QK4_0 == 32, "QK4_0 moved");
static_assert(QK4_1 == 32, "QK4_1 moved");
static_assert(QK5_0 == 32, "QK5_0 moved");
static_assert(QK5_1 == 32, "QK5_1 moved");
static_assert(QK8_0 == 32, "QK8_0 moved");
static_assert(QK8_1 == 32, "QK8_1 moved");
static_assert(QK_MXFP4 == 32, "QK_MXFP4 moved");
static_assert(QK_NVFP4 == 64, "QK_NVFP4 moved");
static_assert(QK4_NL == 32, "QK4_NL moved");

int main() {
    goose_init(0, nullptr);

    // ── the size of the type space ───────────────────────────────────────────
    cases++;
    eq_int("abi_type_count", (int64_t) GGML_TYPE_COUNT, abi_type_count());

    // ── every slot: name, block size, block bytes, quantized flag ────────────
    for (int64_t t = 0; t < (int64_t) GGML_TYPE_COUNT; t++) {
        cases++;
        const gs_nat_type & n = NAT_ABI[t];
        char what[64];
        snprintf(what, sizeof what, "type %lld blck", (long long) t);
        eq_int(what, n.blck, abi_blck_size(t));
        snprintf(what, sizeof what, "type %lld size", (long long) t);
        eq_int(what, n.tsize, abi_type_size(t));
        snprintf(what, sizeof what, "type %lld isq", (long long) t);
        eq_int(what, n.isq, abi_is_quantized(t));
        snprintf(what, sizeof what, "type %lld name", (long long) t);
        eq_str(what, n.name, port_name(t));
    }

    // ── row size: type_size * ne / blck_size, multiplication first ───────────
    struct R {
        int64_t t;
        int64_t ne;
        const char * name;
    };
    const R rows[] = {
        // unquantized types: one element is one block
        {GGML_TYPE_F32, 0, "f32 ne=0"},
        {GGML_TYPE_F32, 1, "f32 ne=1"},
        {GGML_TYPE_F32, 4096, "f32 ne=4096"},
        {GGML_TYPE_F16, 4096, "f16 ne=4096"},
        {GGML_TYPE_BF16, 14336, "bf16 ne=14336"},
        {GGML_TYPE_I8, 7, "i8 ne=7"},
        {GGML_TYPE_I64, 3, "i64 ne=3"},
        {GGML_TYPE_F64, 2, "f64 ne=2"},
        // legacy 32-element blocks
        {GGML_TYPE_Q4_0, 32, "q4_0 ne=32"},
        {GGML_TYPE_Q4_0, 4096, "q4_0 ne=4096"},
        {GGML_TYPE_Q4_1, 64, "q4_1 ne=64"},
        {GGML_TYPE_Q5_0, 4096, "q5_0 ne=4096"},
        {GGML_TYPE_Q5_1, 32, "q5_1 ne=32"},
        {GGML_TYPE_Q8_0, 4096, "q8_0 ne=4096"},
        {GGML_TYPE_Q8_1, 96, "q8_1 ne=96"},
        {GGML_TYPE_IQ4_NL, 4096, "iq4_nl ne=4096"},
        {GGML_TYPE_MXFP4, 4096, "mxfp4 ne=4096"},
        // 256-element K blocks
        {GGML_TYPE_Q2_K, 256, "q2_K ne=256"},
        {GGML_TYPE_Q2_K, 14336, "q2_K ne=14336"},
        {GGML_TYPE_Q3_K, 512, "q3_K ne=512"},
        {GGML_TYPE_Q4_K, 4096, "q4_K ne=4096"},
        {GGML_TYPE_Q5_K, 14336, "q5_K ne=14336"},
        {GGML_TYPE_Q6_K, 4096, "q6_K ne=4096"},
        {GGML_TYPE_Q8_K, 2560, "q8_K ne=2560"},
        {GGML_TYPE_IQ2_XXS, 4096, "iq2_xxs ne=4096"},
        {GGML_TYPE_IQ2_XS, 4096, "iq2_xs ne=4096"},
        {GGML_TYPE_IQ3_XXS, 4096, "iq3_xxs ne=4096"},
        {GGML_TYPE_IQ3_S, 4096, "iq3_s ne=4096"},
        {GGML_TYPE_IQ2_S, 4096, "iq2_s ne=4096"},
        {GGML_TYPE_IQ1_S, 4096, "iq1_s ne=4096"},
        {GGML_TYPE_IQ1_M, 4096, "iq1_m ne=4096"},
        {GGML_TYPE_IQ4_XS, 4096, "iq4_xs ne=4096"},
        {GGML_TYPE_TQ1_0, 256, "tq1_0 ne=256"},
        {GGML_TYPE_TQ2_0, 512, "tq2_0 ne=512"},
        // the new small-block types
        {GGML_TYPE_Q1_0, 128, "q1_0 ne=128"},
        {GGML_TYPE_Q1_0, 4096, "q1_0 ne=4096"},
        {GGML_TYPE_Q2_0, 64, "q2_0 ne=64"},
        {GGML_TYPE_NVFP4, 64, "nvfp4 ne=64"},
        {GGML_TYPE_NVFP4, 4096, "nvfp4 ne=4096"},
    };
    for (const R & r : rows) {
        cases++;
        eq_int(r.name, NAT_ABI[r.t].tsize * r.ne / NAT_ABI[r.t].blck, abi_row_size(r.t, r.ne));
    }

    // ── two-dimensional nbytes: one row of ne0, ne1 rows ─────────────────────
    struct N {
        int64_t t, ne0, ne1;
        const char * name;
    };
    const N twod[] = {
        {GGML_TYPE_F32, 4096, 4096, "f32 4096x4096"},
        {GGML_TYPE_F16, 4096, 14336, "f16 4096x14336"},
        {GGML_TYPE_Q4_K, 4096, 14336, "q4_K 4096x14336"},
        {GGML_TYPE_Q6_K, 4096, 4096, "q6_K 4096x4096"},
        {GGML_TYPE_Q2_K, 14336, 4096, "q2_K 14336x4096"},
        {GGML_TYPE_Q8_0, 32, 1, "q8_0 32x1"},
        {GGML_TYPE_BF16, 8192, 2048, "bf16 8192x2048"},
        {GGML_TYPE_I32, 100, 0, "i32 100x0"},
    };
    for (const N & c : twod) {
        cases++;
        eq_int(c.name, NAT_ABI[c.t].tsize * c.ne0 / NAT_ABI[c.t].blck * c.ne1,
               abi_nbytes_2d(c.t, c.ne0, c.ne1));
    }

    // ── the refusal boundary: what the native asserts refuse, the port answers ─
    const int64_t bad[] = {-1, -7, GGML_TYPE_COUNT, GGML_TYPE_COUNT + 1, 1000};
    for (int64_t t : bad) {
        cases++;
        char what[48];
        snprintf(what, sizeof what, "oor %lld blck", (long long) t);
        eq_int(what, -1, abi_blck_size(t));
        snprintf(what, sizeof what, "oor %lld size", (long long) t);
        eq_int(what, -1, abi_type_size(t));
        snprintf(what, sizeof what, "oor %lld isq", (long long) t);
        eq_int(what, -1, abi_is_quantized(t));
        snprintf(what, sizeof what, "oor %lld name", (long long) t);
        eq_str(what, "!", port_name(t));
        snprintf(what, sizeof what, "oor %lld row", (long long) t);
        eq_int(what, -1, abi_row_size(t, 256));
    }

    // a block size of zero: native divides by zero, the port refuses
    const int64_t zero_blck[] = {4, 5, 31, 32, 33, 36, 37, 38};
    for (int64_t t : zero_blck) {
        cases++;
        eq_int("removed slot blck=0", 0, abi_blck_size(t));
        eq_int("removed slot row size", -1, abi_row_size(t, 32));
        eq_int("removed slot nbytes", -1, abi_nbytes_2d(t, 32, 2));
    }

    // a row that is not a whole number of blocks, and a negative row
    cases++;
    eq_int("q4_K ne=100 partial", -1, abi_row_size(GGML_TYPE_Q4_K, 100));
    eq_int("f32 ne=-1", -1, abi_row_size(GGML_TYPE_F32, -1));
    eq_int("f32 4096x-1", -1, abi_nbytes_2d(GGML_TYPE_F32, 4096, -1));

    // a buffer smaller than the name: the port reports what it wrote, never more
    cases++;
    {
        uint8_t small[4];
        memset(small, 0, sizeof small);
        int64_t n = abi_type_name(GGML_TYPE_Q4_0, sl_u8{small, (int64_t) sizeof small});
        eq_int("short buffer writes 4", 4, n);
        eq_int("short buffer content", 0, memcmp(small, "q4_0", 4));
    }

    printf("FG645_ABI cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
