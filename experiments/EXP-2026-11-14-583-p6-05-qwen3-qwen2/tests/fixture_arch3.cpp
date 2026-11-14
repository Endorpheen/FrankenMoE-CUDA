// Structure gate for the qwen3 builder draft (P6-05 architecture 3).
//
// Expected values are the native loader's declarations in
// src/models/qwen3.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-arch3.h"

static int g_cases = 0, g_values = 0, g_fails = 0;

static void bad(const char * what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 100) {
        printf("FAIL %s: %s\n", what, detail.c_str());
    }
}

static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

extern "C" void fg200g_fail(int64_t code) {
    printf("FAIL shim: native abort %lld\n", (long long) code);
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base; (void) index;
    return 0;
}

static std::vector<int64_t> params() {
    std::vector<int64_t> p(12, 0);
    p[0]  = 1024;     // n_embd
    p[1]  = 151936;   // n_vocab
    p[2]  = 28;       // n_layer (0.6B)
    p[3]  = 16;       // n_head
    p[4]  = 8;        // n_head_kv
    p[5]  = 3072;     // n_ff
    p[6]  = 128;      // n_embd_head_k
    p[7]  = 128;      // n_embd_head_v
    p[8]  = 0;        // output absent -> duplicates token_embd
    p[9]  = 0;        // no attention bias
    p[10] = 0;        // no cls_out
    p[11] = 0;        // no dense_2_out
    return p;
}

// Derived: q_width = 16*128 = 2048, kv_width = 8*128 = 1024,
//          fused qkv = 2048 + 1024 + 1024 = 4096, wo rows = 2048.

static void check_plan(const char * name, const std::vector<int64_t> & p, int64_t expected_n) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> kinds(expected_n, -1);
    const int64_t n = q3_plan(sli(pp), sli(kinds));
    ++g_values;
    if (n != expected_n) {
        bad(name, "plan n got " + std::to_string((int) n) + " expected " + std::to_string((int) expected_n));
        return;
    }
    for (int64_t i = 0; i < expected_n; ++i) {
        ++g_values;
        if (kinds[i] != 0) {
            bad(name, "kind[" + std::to_string((int) i) + "] got " + std::to_string((int) kinds[i]) +
                      " expected 0");
            return;
        }
    }
}

static void check_kind(const char * name, const std::vector<int64_t> & p, int64_t il, int64_t expected) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> kinds(expected == -1 ? 1 : (size_t) p[2], -1);
    const int64_t n = q3_plan(sli(pp), sli(kinds));
    (void) n;
    ++g_values;
    if (il < 0 || il >= (int64_t) kinds.size() || kinds[(size_t) il] != expected) {
        bad(name, "kind[" + std::to_string((int) il) + "] expected " + std::to_string((int) expected));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, int64_t tid,
                        int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q3_shape(sli(pp), tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((int) out[0]) + "," + std::to_string((int) out[1]) + "," +
                  std::to_string((int) out[2]) + "} ok=" + std::to_string((int) ok) +
                  " expected {" + std::to_string((int) a) + "," + std::to_string((int) b) + "," +
                  std::to_string((int) c) + "}");
    }
}

static void check_absent(const char * name, const std::vector<int64_t> & p, int64_t tid) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q3_shape(sli(pp), tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "expected absent, got ok=" + std::to_string((int) ok));
    }
}

static void check_marker(const char * name, int64_t got, int64_t expected) {
    ++g_cases;
    ++g_values;
    if (got != expected) {
        bad(name, "got " + std::to_string((int) got) + " expected " + std::to_string((int) expected));
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();

    // Plan: every trunk layer is attention, dense.goose:d_plan.
    check_plan("plan-all-attention", p, 28);
    check_kind("layer0-attention", p, 0, 0);
    check_kind("layer27-attention", p, 27, 0);

    // qwen3.cpp:18,21,22
    check_shape("token_embd", p, 0, 1024, 151936, 1);
    check_shape("output_norm", p, 1, 1024, 1, 1);
    check_shape("output-dup-token-embd", p, 2, 1024, 151936, 1);
    // qwen3.cpp:29,32,33 absent in this fixture (n_cls_out = 0, n_embd_out = 0)
    check_absent("cls-out-absent", p, 3);
    check_absent("dense-2-out-absent", p, 4);
    check_absent("dense-2-out-bias-absent", p, 5);

    // Positive optional heads (qwen3.cpp:29-33): reranker and embedding outputs.
    auto pc = p;
    pc[10] = 64;      // n_cls_out
    pc[11] = 256;     // n_embd_out
    check_shape("cls-out-present", pc, 3, 1024, 64, 1);
    check_shape("dense-2-out-present", pc, 4, 1024, 256, 1);
    check_shape("dense-2-out-bias-present", pc, 5, 256, 1, 1);

    // qwen3.cpp:38,40,41,43,44,46-49
    check_shape("attn_norm", p, 6, 1024, 1, 1);
    check_shape("ffn_norm", p, 7, 1024, 1, 1);
    check_shape("attn_qkv", p, 8, 1024, 4096, 1);
    check_shape("attn_q_norm", p, 14, 128, 1, 1);
    check_shape("attn_k_norm", p, 15, 128, 1, 1);
    check_shape("attn_wo", p, 13, 2048, 1024, 1);
    check_shape("ffn_gate", p, 16, 1024, 3072, 1);
    check_shape("ffn_up", p, 17, 1024, 3072, 1);
    check_shape("ffn_down", p, 18, 3072, 1024, 1);

    // qwen3 normalises Q and K per head before RoPE (qwen3.cpp:102,111).
    check_marker("qk-norm-present", q3_qk_norm(), 1);

    // output is optional and falls back to the token embedding (qwen3.cpp:23-26).
    auto po = p;
    check_marker("output-from-token-embd", q3_output_dup(sli(po)), 1);
    po[8] = 1;
    check_marker("output-declared", q3_output_dup(sli(po)), 0);

    // Bias set: with no fused bias the legacy q/k/v biases are declared
    // (llama-model.cpp:3888-3895); qwen3 has no output bias.
    check_absent("attn_qkv_bias-absent", p, 9);
    check_shape("legacy-q-bias", p, 10, 2048, 1, 1);
    check_shape("legacy-k-bias", p, 11, 1024, 1, 1);
    check_shape("legacy-v-bias", p, 12, 1024, 1, 1);

    // Positive fused-bias direction: one flag flipped on a copy.
    auto pb = p;
    pb[9] = 1;
    check_shape("fused-qkv-bias", pb, 9, 4096, 1, 1);
    check_absent("legacy-q-bias-suppressed", pb, 10);

    printf("FG-ARCH3 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
