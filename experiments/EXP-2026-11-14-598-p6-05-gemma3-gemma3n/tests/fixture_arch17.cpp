// Structure gate for the gemma3n builder draft (P6-05 architecture 17).
//
// Expected values are the native loader's declarations in
// src/models/gemma3n.cpp, the altup/laurel shapes at :26-29 and :65-76, and the
// SWA/KV rules in src/llama-hparams.cpp:8-25 and :295-304.
// No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-arch17.h"

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
    std::vector<int64_t> p(14, 0);
    p[0]  = 2048;      // n_embd (gemma3n E4B)
    p[1]  = 262144;    // n_vocab
    p[2]  = 35;        // n_layer -> LLM_TYPE_E4B
    p[3]  = 8;         // n_head
    p[4]  = 2;         // n_head_kv
    p[5]  = 16384;     // n_ff
    p[6]  = 256;       // n_embd_head_k
    p[7]  = 256;       // n_embd_head_v
    p[8]  = 0;         // output absent -> duplicates token_embd
    p[9]  = 5;         // swa period (loader default)
    p[10] = 4;         // n_altup
    p[11] = 0;         // i_altup_act
    p[12] = 64;        // laurel_rank
    p[13] = 256;       // n_embd_altup
    return p;
}

// Derived: q_width = 8*256 = 2048, kv_width = 2*256 = 512,
//          fused qkv = 2048 + 512 + 512 = 3072, wo = {2048, 2048},
//          per_layer_tok_embd = {256*35, 262144} = {8960, 262144}.

static void check_plan(const char * name, const std::vector<int64_t> & p, int64_t expected_n) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> kinds(expected_n, -1);
    const int64_t n = g3n_plan(sli(pp), sli(kinds));
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
    const int64_t n = g3n_plan(sli(pp), sli(kinds));
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
    const int64_t ok = g3n_shape(sli(pp), tid, sli(out));
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
    const int64_t ok = g3n_shape(sli(pp), tid, sli(out));
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

static void check_kv(const char * name, int64_t from_start, int64_t il, int64_t expected) {
    ++g_cases;
    ++g_values;
    const int64_t got = g3n_has_kv(from_start, il);
    if (got != expected) {
        bad(name, "has_kv(" + std::to_string((int) from_start) + "," + std::to_string((int) il) +
                  ") got " + std::to_string((int) got) + " expected " + std::to_string((int) expected));
    }
}

static void check_swa(const char * name, const std::vector<int64_t> & p, int64_t il, int64_t expected) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    ++g_values;
    const int64_t got = g3n_is_swa(sli(pp), il);
    if (got != expected) {
        bad(name, "is_swa(" + std::to_string((int) il) + ") got " + std::to_string((int) got) +
                  " expected " + std::to_string((int) expected));
    }
}

int main() {
    goose_init(0, nullptr);
    auto p = params();

    // Plan: every trunk layer is attention (gemma3n.cpp:47-77).
    check_plan("plan-all-attention", p, 35);
    check_kind("layer0-attention", p, 0, 0);
    check_kind("layer34-attention", p, 34, 0);

    // Global tensors (gemma3n.cpp:30-45).
    check_shape("token_embd", p, 0, 2048, 262144, 1);
    check_shape("output_norm", p, 1, 2048, 1, 1);
    check_shape("output-dup-token-embd", p, 2, 2048, 262144, 1);
    check_shape("altup_proj", p, 3, 2048, 2048, 3);
    check_shape("altup_unembd_proj", p, 4, 2048, 2048, 3);
    check_shape("per_layer_tok_embd", p, 5, 8960, 262144, 1);
    check_shape("per_layer_model_proj", p, 6, 2048, 8960, 1);
    check_shape("per_layer_proj_norm", p, 7, 256, 1, 1);

    // Layer tensors (gemma3n.cpp:50-63).
    check_shape("attn_norm", p, 8, 2048, 1, 1);
    check_shape("attn_qkv", p, 9, 2048, 3072, 1);
    check_shape("attn_wo", p, 10, 2048, 2048, 1);
    check_shape("attn_q_norm", p, 11, 256, 1, 1);
    check_shape("attn_k_norm", p, 12, 256, 1, 1);
    check_shape("attn_post_norm", p, 13, 2048, 1, 1);
    check_shape("ffn_norm", p, 14, 2048, 1, 1);
    check_shape("ffn_gate", p, 15, 2048, 16384, 1);
    check_shape("ffn_up", p, 16, 2048, 16384, 1);
    check_shape("ffn_down", p, 17, 16384, 2048, 1);
    check_shape("ffn_post_norm", p, 18, 2048, 1, 1);

    // Altup and laurel layer tensors (gemma3n.cpp:66-76).
    check_shape("per_layer_inp_gate", p, 19, 2048, 256, 1);
    check_shape("per_layer_proj", p, 20, 256, 2048, 1);
    check_shape("per_layer_post_norm", p, 21, 2048, 1, 1);
    check_shape("altup_correct_coef", p, 22, 4, 4, 1);
    check_shape("altup_correct_scale", p, 23, 2048, 1, 1);
    check_shape("altup_predict_coef", p, 24, 4, 16, 1);
    check_shape("altup_router", p, 25, 2048, 4, 1);
    check_shape("altup_router_norm", p, 26, 2048, 1, 1);
    check_shape("laurel_l", p, 27, 2048, 64, 1);
    check_shape("laurel_r", p, 28, 64, 2048, 1);
    check_shape("laurel_post_norm", p, 29, 2048, 1, 1);

    // KV prefix (gemma3n.cpp:9, llama-hparams.cpp:295-304).
    check_marker("kv-from-start", g3n_kv_from_start(), 20);
    check_kv("kv-layer19", 20, 19, 1);
    check_kv("kv-layer20", 20, 20, 0);
    check_kv("kv-layer34", 20, 34, 0);
    check_kv("kv-all-layers", -1, 34, 1);
    check_kv("kv-none", 0, 0, 0);

    // SWA is unconditional, period defaults to 5 (gemma3n.cpp:4-7).
    check_marker("swa-type-standard", g3n_swa_type(), 1);
    check_marker("swa-period-default", g3n_swa_period_default(), 5);
    check_marker("swa-period-document", g3n_swa_period(sli(p)), 5);
    check_swa("swa-layer0", p, 0, 1);
    check_swa("swa-layer4-full", p, 4, 0);
    check_swa("swa-layer5", p, 5, 1);
    check_swa("swa-layer9-full", p, 9, 0);

    // Fixed attention scale and shared auto branch (gemma3n.cpp:10).
    check_marker("attn-scale-fixed", g3n_attn_scale_bits(), 1065353216);
    check_marker("attn-scale-not-auto", g3n_attn_scale_auto(), 0);

    // Norm decisions (gemma3n.cpp:157-159).
    check_marker("qk-norm-present", g3n_qk_norm(), 1);
    check_marker("v-norm-weightless", g3n_v_norm(), 1);

    // Activation sparsity on the leading layers (models.h:836, gemma3n.cpp:212).
    check_marker("sparsity-layers", g3n_sparsity_layers(), 10);
    check_marker("sparsity-layer9", g3n_uses_sparsity(9), 1);
    check_marker("sparsity-layer10", g3n_uses_sparsity(10), 0);

    // Unconditional softcap (gemma3n.cpp:301-306).
    check_marker("softcap-always", g3n_softcap_always(), 1);

    // Altup decisions (gemma3n.cpp:38-39,123-128,232,283,448).
    check_marker("altup-extra", g3n_altup_extra(sli(p)), 3);
    check_marker("altup-merge-div", g3n_altup_merge_div(sli(p)), 4);
    check_marker("altup-active-idx", g3n_altup_active(sli(p)), 0);
    check_marker("altup-correct-bias", g3n_altup_correct_bias(), 1);

    // Input embedding scale decision (gemma3n.cpp:104).
    check_marker("embd-scale-tokens", g3n_embd_scale(1), 1);
    check_marker("embd-scale-raw", g3n_embd_scale(0), 0);

    // Output source marker (gemma3n.cpp:30-34).
    check_marker("output-from-token-embd", g3n_output_dup(sli(p)), 1);
    auto po = p;
    po[8] = 1;
    check_marker("output-declared", g3n_output_dup(sli(po)), 0);

    printf("FG-ARCH17 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
