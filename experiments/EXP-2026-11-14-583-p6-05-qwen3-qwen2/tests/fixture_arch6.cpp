// Structure gate for the qwen2 builder draft (P6-05 architecture 6).
//
// Expected values are the native loader's declarations in
// src/models/qwen2.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-arch6.h"

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
    std::vector<int64_t> p(11, 0);
    p[0]  = 3584;     // n_embd
    p[1]  = 152064;   // n_vocab
    p[2]  = 28;       // n_layer (7B)
    p[3]  = 28;       // n_head
    p[4]  = 4;        // n_head_kv
    p[5]  = 18944;    // n_ff
    p[6]  = 128;      // n_embd_head_k
    p[7]  = 128;      // n_embd_head_v
    p[8]  = 1;        // output present
    p[9]  = 1;        // fused attention bias present
    p[10] = 1;        // output bias present
    return p;
}

// Derived: q_width = n_embd = 3584, kv_width = 4*128 = 512,
//          fused qkv = 3584 + 512 + 512 = 4608.

static void check_plan(const char * name, const std::vector<int64_t> & p, int64_t expected_n) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> kinds(expected_n, -1);
    const int64_t n = q2_plan(sli(pp), sli(kinds));
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
    std::vector<int64_t> kinds((size_t) p[2], -1);
    q2_plan(sli(pp), sli(kinds));
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
    const int64_t ok = q2_shape(sli(pp), tid, sli(out));
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
    const int64_t ok = q2_shape(sli(pp), tid, sli(out));
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

    // qwen2.cpp:22,25,26,27
    check_shape("token_embd", p, 0, 3584, 152064, 1);
    check_shape("output_norm", p, 1, 3584, 1, 1);
    check_shape("output", p, 2, 3584, 152064, 1);
    check_shape("output_bias", p, 3, 152064, 1, 1);

    // qwen2.cpp:36,38,39,41,43-45
    check_shape("attn_norm", p, 4, 3584, 1, 1);
    check_shape("ffn_norm", p, 12, 3584, 1, 1);
    check_shape("attn_qkv", p, 5, 3584, 4608, 1);
    check_shape("attn_qkv_bias", p, 6, 4608, 1, 1);
    check_shape("attn_wo", p, 10, 3584, 3584, 1);
    check_shape("ffn_gate", p, 13, 3584, 18944, 1);
    check_shape("ffn_up", p, 14, 3584, 18944, 1);
    check_shape("ffn_down", p, 15, 18944, 3584, 1);

    // No qk-norm anywhere (qwen2.cpp:81-98 goes straight to RoPE).
    check_absent("attn_q_norm-absent", p, 16);
    check_absent("attn_k_norm-absent", p, 17);
    check_marker("qk-norm-marker", q2_has_qk_norm(), 0);

    // No attn_out bias: the loader never declares one (qwen2.cpp:38-50).
    check_absent("attn_wo_bias-absent", p, 11);

    // Legacy split-bias direction: fused bias off.
    auto pl = p;
    pl[9] = 0;
    check_shape("legacy-q-bias", pl, 7, 3584, 1, 1);
    check_shape("legacy-k-bias", pl, 8, 512, 1, 1);
    check_shape("legacy-v-bias", pl, 9, 512, 1, 1);
    check_absent("fused-qkv-bias-suppressed", pl, 6);

    // Output bias is optional (qwen2.cpp:27).
    auto pn = p;
    pn[10] = 0;
    check_absent("output-bias-optional", pn, 3);

    // The lm_head weight is optional and falls back to token_embd
    // (qwen2.cpp:28-31).
    auto pd = p;
    check_marker("output-declared", q2_output_dup(sli(pd)), 0);
    pd[8] = 0;
    check_marker("output-from-token-embd", q2_output_dup(sli(pd)), 1);

    printf("FG-ARCH6 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
