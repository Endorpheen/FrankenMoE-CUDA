// Structure gate for the gemma3 builder draft (P6-05 architecture 16).
//
// Expected values are the native loader's declarations in
// src/models/gemma3.cpp plus the SWA pattern rule in src/llama-hparams.cpp:8-25.
// No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include "runtime-arch16.h"

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

// Bit pattern of a float, so the fixture passes the same value the loader
// would pass to the shared decision helpers.
static int64_t fbits(float f) {
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return (int64_t) u;
}

static std::vector<int64_t> params() {
    std::vector<int64_t> p(15, 0);
    p[0]  = 2560;      // n_embd (gemma3 4B)
    p[1]  = 262144;    // n_vocab
    p[2]  = 34;        // n_layer -> LLM_TYPE_4B
    p[3]  = 8;         // n_head
    p[4]  = 4;         // n_head_kv
    p[5]  = 10240;     // n_ff
    p[6]  = 256;       // n_embd_head_k
    p[7]  = 256;       // n_embd_head_v
    p[8]  = 0;         // output absent -> duplicates token_embd
    p[9]  = 1024;      // n_swa -> SWA enabled
    p[10] = 6;         // swa period (loader default)
    p[11] = 0;         // dense_2_feat_out absent
    p[12] = 0;         // dense_3_feat_in absent
    p[13] = fbits(30.0f);  // final logit softcapping = 30.0
    p[14] = fbits(1.0f / sqrtf(2560.0f / 8.0f)); // 27B rule would not apply; see case
    return p;
}

// Derived: q_width = 8*256 = 2048, kv_width = 4*256 = 1024,
//          fused qkv = 2048 + 1024 + 1024 = 4096, wo rows = 2048.

static void check_plan(const char * name, const std::vector<int64_t> & p, int64_t expected_n) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> kinds(expected_n, -1);
    const int64_t n = g3_plan(sli(pp), sli(kinds));
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
    const int64_t n = g3_plan(sli(pp), sli(kinds));
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
    const int64_t ok = g3_shape(sli(pp), tid, sli(out));
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
    const int64_t ok = g3_shape(sli(pp), tid, sli(out));
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

static void check_swa(const char * name, const std::vector<int64_t> & p, int64_t il, int64_t expected) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    ++g_values;
    const int64_t got = g3_is_swa(sli(pp), il);
    if (got != expected) {
        bad(name, "is_swa(" + std::to_string((int) il) + ") got " + std::to_string((int) got) +
                  " expected " + std::to_string((int) expected));
    }
}

int main() {
    goose_init(0, nullptr);
    auto p = params();

    // Plan: every trunk layer is attention (gemma3.cpp:55-72).
    check_plan("plan-all-attention", p, 34);
    check_kind("layer0-attention", p, 0, 0);
    check_kind("layer33-attention", p, 33, 0);

    // Global tensors (gemma3.cpp:39-52).
    check_shape("token_embd", p, 0, 2560, 262144, 1);
    check_shape("output_norm", p, 1, 2560, 1, 1);
    check_shape("output-dup-token-embd", p, 2, 2560, 262144, 1);
    check_absent("dense-2-out-absent", p, 3);
    check_absent("dense-3-out-absent", p, 4);

    auto pd = p;
    pd[11] = 64;   // dense_2_feat_out
    pd[12] = 64;   // dense_3_feat_in
    check_shape("dense-2-out-present", pd, 3, 2560, 64, 1);
    check_shape("dense-3-out-present", pd, 4, 64, 2560, 1);

    // Layer tensors (gemma3.cpp:58-71).
    check_shape("attn_norm", p, 5, 2560, 1, 1);
    check_shape("attn_qkv", p, 6, 2560, 4096, 1);
    check_shape("attn_wo", p, 7, 2048, 2560, 1);
    check_shape("attn_q_norm", p, 8, 256, 1, 1);
    check_shape("attn_k_norm", p, 9, 256, 1, 1);
    check_shape("attn_post_norm", p, 10, 2560, 1, 1);
    check_shape("ffn_norm", p, 11, 2560, 1, 1);
    check_shape("ffn_gate", p, 12, 2560, 10240, 1);
    check_shape("ffn_up", p, 13, 2560, 10240, 1);
    check_shape("ffn_down", p, 14, 10240, 2560, 1);
    check_shape("ffn_post_norm", p, 15, 2560, 1, 1);

    // SWA type (gemma3.cpp:4-14): STANDARD only for a positive window.
    check_marker("swa-type-positive", g3_swa_type(sli(p)), 1);
    auto pw = p;
    pw[9] = 0;
    check_marker("swa-type-none", g3_swa_type(sli(pw)), 0);
    check_marker("graph-iswa-on", g3_graph_iswa(sli(p)), 1);
    check_marker("graph-iswa-off", g3_graph_iswa(sli(pw)), 0);

    // Period: loader default 6, then the document value (gemma3.cpp:7-9).
    check_marker("swa-period-default", g3_swa_period_default(), 6);
    check_marker("swa-period-document", g3_swa_period(sli(p)), 6);

    // Pattern: layers 0-4 SWA, 5 full, 6-10 SWA, 11 full (llama-hparams.cpp:8-25).
    check_swa("swa-layer0", p, 0, 1);
    check_swa("swa-layer4", p, 4, 1);
    check_swa("swa-layer5-full", p, 5, 0);
    check_swa("swa-layer6", p, 6, 1);
    check_swa("swa-layer11-full", p, 11, 0);

    auto pz = p;
    pz[10] = 0;
    check_swa("swa-period0-all", pz, 5, 1);

    // Rope base source per layer (gemma3.cpp:115-121).
    check_marker("rope-base-swa", g3_rope_base_swa(1), 1);
    check_marker("rope-base-full", g3_rope_base_swa(0), 0);

    // Type switch and attention scale rule (gemma3.cpp:20-34).
    check_marker("type-4b", g3_model_type(sli(p)), 4);
    auto p27 = p;
    p27[2] = 62;
    check_marker("type-27b", g3_model_type(sli(p27)), 6);
    auto p270 = p;
    p270[2] = 18;
    check_marker("type-270m", g3_model_type(sli(p270)), 1);
    auto pu = p;
    pu[2] = 40;
    check_marker("type-unknown", g3_model_type(sli(pu)), 0);

    check_marker("scale-kind-per-head", g3_attn_scale_kind(sli(p)), 0);
    check_marker("scale-kind-27b", g3_attn_scale_kind(sli(p27)), 1);

    // Decision markers (gemma3.cpp:93,133-141,184,212).
    check_marker("qk-norm-present", g3_qk_norm(), 1);
    check_marker("ffn-gelu-par", g3_act_gelu(), 1);
    check_marker("softcap-on", g3_softcap_on(sli(p)), 1);
    auto pn = p;
    pn[13] = 0;
    check_marker("softcap-off", g3_softcap_on(sli(pn)), 0);
    check_marker("embd-scale-tokens", g3_embd_scale(1), 1);
    check_marker("embd-scale-raw", g3_embd_scale(0), 0);
    check_marker("kq-scale-explicit", g3_kq_scale_auto(sli(p)), 0);

    // Output source marker (gemma3.cpp:43-48).
    check_marker("output-from-token-embd", g3_output_dup(sli(p)), 1);
    auto po = p;
    po[8] = 1;
    check_marker("output-declared", g3_output_dup(sli(po)), 0);

    printf("FG-ARCH16 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
