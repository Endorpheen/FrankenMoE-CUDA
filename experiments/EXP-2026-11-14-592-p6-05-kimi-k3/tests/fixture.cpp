// Structure gate for the kimi-k3 builder draft (P6-05 architecture 10).
//
// Expected values are the native loader's declarations in
// src/models/kimi-k3.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-gen.h"

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

// Kimi-K3 sized: 2.8T-A50B is 93 layers, 7168 hidden, 384 experts.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(22, 0);
    p[0]  = 7168;      // n_embd
    p[1]  = 163840;    // n_vocab
    p[2]  = 93;        // n_layer (trunk; no nextn block in this family)
    p[3]  = 93;        // n_layer_all
    p[4]  = 64;        // n_head
    p[5]  = 128;       // n_embd_head_kda
    p[6]  = 4;         // ssm_d_conv
    p[7]  = 192;       // n_embd_head_k_mla
    p[8]  = 128;       // n_embd_head_v_mla
    p[9]  = 64;        // n_rot (qk_rope_head_dim)
    p[10] = 1536;      // n_lora_q
    p[11] = 512;       // n_lora_kv
    p[12] = 18432;     // n_ff (dense lead width)
    p[13] = 384;       // n_expert
    p[14] = 8;         // n_expert_used
    p[15] = 2048;      // n_ff_exp fallback (the array carries the real width)
    p[16] = 1;         // n_expert_shared
    p[17] = 1;         // n_layer_dense_lead
    p[18] = 1536;      // n_expert_latent
    p[19] = 8;         // attn_res_block_size
    p[20] = 1;         // expert_gating_func (sigmoid)
    p[21] = 1065353216; // situ_linear_beta bits, 1.0f
    return p;
}

// per-layer expert feed-forward length: one entry per declared block
static std::vector<int64_t> ffexp() {
    std::vector<int64_t> f(93, 2048);
    f[1] = 1024;       // the array is per layer, not a single value
    f[0] = 0;          // layer 0 is the dense lead and never reads it
    return f;
}

// n_head_kv marker per layer: zero marks KDA, one marks MLA (kimi-k3.cpp:31)
static std::vector<int64_t> n_head_kv_arr() {
    std::vector<int64_t> r(93, 0);
    for (int i = 0; i < 93; ++i) {
        r[i] = (i % 4 == 3) ? 1 : 0;
    }
    return r;
}

static void check_val(const char * name, int64_t got, int64_t want) {
    ++g_cases;
    ++g_values;
    if (got != want) {
        bad(name, "got " + std::to_string((long long) got) + " expected " + std::to_string((long long) want));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, const std::vector<int64_t> & f,
                        int64_t kind, int64_t attn, int64_t il, int64_t tid,
                        int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p, ff = f;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = mk_shape(sli(pp), sli(ff), kind, attn, il, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," + std::to_string((long long) out[1]) +
                  "," + std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                  " expected {" + std::to_string((long long) a) + "," + std::to_string((long long) b) +
                  "," + std::to_string((long long) c) + "}");
    }
}

// A tensor the plan or attention kind must not own: the call has to report 0.
static void check_absent(const char * name, const std::vector<int64_t> & p, const std::vector<int64_t> & f,
                         int64_t kind, int64_t attn, int64_t il, int64_t tid) {
    ++g_cases;
    std::vector<int64_t> pp = p, ff = f;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = mk_shape(sli(pp), sli(ff), kind, attn, il, tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "tensor present (ok=" + std::to_string((long long) ok) + "), expected absent");
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();
    const auto ff = ffexp();
    const auto recr = n_head_kv_arr();

    // 1. the attention marker: n_head_kv == 0 selects KDA (kimi-k3.cpp:31-33)
    check_val("recr-kv0-kda", mk_is_recr(0), 1);
    check_val("recr-kv1-mla", mk_is_recr(1), 0);
    check_val("recr-kv8-mla", mk_is_recr(8), 0);
    check_val("attn-kv0-kda", mk_attn_kind(0), 0);
    check_val("attn-kv1-mla", mk_attn_kind(1), 1);
    {
        std::vector<int64_t> pp = p;
        check_val("fixture-kv-len", (int64_t) recr.size(), 93);
        check_val("fixture-attn0-kda", mk_attn_kind(recr[0]), 0);
        check_val("fixture-attn3-mla", mk_attn_kind(recr[3]), 1);
        (void) pp;
    }

    // 2. the marker is a boolean, not a grouped-query width
    check_val("gqa-1group-ok", mk_gqa_ok(64, 1), 1);
    check_val("gqa-8groups-ok", mk_gqa_ok(64, 8), 1);
    check_val("gqa-5groups-rejected", mk_gqa_ok(64, 5), 0);
    check_val("gqa-zero-rejected", mk_gqa_ok(64, 0), 0);
    check_val("gqa-more-kv-than-q", mk_gqa_ok(8, 9), 0);
    check_val("kv-rows-mla", mk_kv_rows(1), 1);
    check_val("kv-rows-kda", mk_kv_rows(0), -1);

    // 3. plan: dense lead, then latent MoE, no MTP
    {
        std::vector<int64_t> pp = p;
        check_val("kind-layer0-dense", mk_layer_kind(sli(pp), 0), 0);
        check_val("kind-layer1-moe", mk_layer_kind(sli(pp), 1), 1);
        check_val("kind-layer92-moe", mk_layer_kind(sli(pp), 92), 1);
        check_val("kind-layer93-out", mk_layer_kind(sli(pp), 93), -1);
        check_val("kind-negative", mk_layer_kind(sli(pp), -1), -1);
    }
    {
        std::vector<int64_t> pp = p, kinds(93, -1);
        const int64_t n = mk_plan(sli(pp), sli(kinds));
        check_val("plan-len", n, 93);
        check_val("plan-kind0", kinds[0], 0);
        check_val("plan-kind1", kinds[1], 1);
        check_val("plan-kind92", kinds[92], 1);
    }
    {
        // a declared nextn layer is refused, never silently dropped
        std::vector<int64_t> pp = p;
        pp[3] = 94;
        check_val("plan-nextn-rejected", mk_plan_ok(sli(pp)), 0);
    }
    {
        std::vector<int64_t> pp = p;
        check_val("plan-trunk-ok", mk_plan_ok(sli(pp)), 1);
        check_val("mtp-unsupported", mk_mtp_supported(), 0);
    }

    // 4. cross-layer residual attention
    {
        std::vector<int64_t> pp = p;
        check_val("res-on", mk_has_res(sli(pp)), 1);
        pp[19] = 0;
        check_val("res-off", mk_has_res(sli(pp)), 0);
        check_val("res-push-off", mk_res_push(sli(pp), 0), 0);
    }
    {
        std::vector<int64_t> pp = p;
        check_val("res-push-layer0", mk_res_push(sli(pp), 0), 1);
        check_val("res-push-layer8", mk_res_push(sli(pp), 8), 1);
        check_val("res-push-layer7", mk_res_push(sli(pp), 7), 0);
        check_val("res-push-layer92", mk_res_push(sli(pp), 92), 0);
        check_val("res-points-93-8", mk_res_points(93, 8), 12);
        check_val("res-points-no-res", mk_res_points(93, 0), 0);
        check_val("res-points-empty", mk_res_points(0, 8), 0);
        check_val("res-scores-width", mk_res_scores_width(11), 12);
        check_val("res-mix-no-ckpt", mk_res_mix_active(sli(pp), 0), 0);
        check_val("res-mix-first-ckpt", mk_res_mix_active(sli(pp), 1), 1);
        pp[19] = 0;
        check_val("res-mix-off", mk_res_mix_active(sli(pp), 4), 0);
    }

    // 5. latent MoE widths
    check_val("embd-latent-set", mk_embd_latent(7168, 1536), 1536);
    check_val("embd-latent-fallback", mk_embd_latent(7168, 0), 7168);
    check_val("shexp-width-one", mk_shexp_width(2048, 1), 2048);
    check_val("shexp-width-zero", mk_shexp_width(2048, 0), 2048);
    check_val("shexp-width-two", mk_shexp_width(2048, 2), 4096);
    check_val("d-inner", mk_d_inner(64, 128), 8192);
    check_val("qkv-width", mk_qkv_width(8192, 8192, 8192), 24576);

    // 6. MLA head split and the joint latent projection
    check_val("nope-192-64", mk_nope(192, 64), 128);
    check_val("nope-192-0", mk_nope(192, 0), 192);
    check_val("kv-per-head", mk_kv_per_head(128, 128), 256);
    check_val("wkv-a-width", mk_wkv_a_width(512, 64), 576);

    // 7. activation/gating decisions
    check_val("gating-sigmoid", mk_gating_ok(1), 1);
    check_val("gating-sqrtsoftplus", mk_gating_ok(3), 1);
    check_val("gating-4-rejected", mk_gating_ok(4), 0);
    check_val("gating-negative-rejected", mk_gating_ok(-1), 0);
    check_val("situ-up-on", mk_situ_up(1065353216), 1);
    check_val("situ-up-off", mk_situ_up(0), 0);
    check_val("situ-up-negative", mk_situ_up(-1082130432), 0);
    check_val("kda-gate-bound", mk_kda_gate_low(1), 1);
    check_val("kda-gate-softplus", mk_kda_gate_low(0), 0);

    // main tensor shapes
    // 8. globals and the residual score vectors
    check_shape("token-embd", p, ff, 0, 0, 0, 0, 7168, 163840, 1);
    check_shape("output-norm", p, ff, 0, 0, 0, 1, 7168, 1, 1);
    check_shape("output", p, ff, 0, 0, 0, 2, 7168, 163840, 1);
    check_shape("output-res-score", p, ff, 0, 0, 0, 3, 7168, 1, 1);
    check_shape("attn-norm", p, ff, 1, 0, 1, 4, 7168, 1, 1);
    check_shape("ffn-norm", p, ff, 1, 0, 1, 5, 7168, 1, 1);
    check_shape("attn-res-score", p, ff, 1, 0, 1, 6, 7168, 1, 1);
    check_shape("ffn-res-score", p, ff, 1, 0, 1, 7, 7168, 1, 1);
    {
        std::vector<int64_t> pp = p;
        pp[19] = 0;
        check_absent("no-output-res-score", pp, ff, 0, 0, 0, 3);
        check_absent("no-attn-res-score", pp, ff, 1, 0, 1, 6);
        check_absent("no-ffn-res-score", pp, ff, 1, 0, 1, 7);
    }

    // 9. dense lead FFN
    check_shape("dense-gate", p, ff, 0, 0, 0, 30, 7168, 18432, 1);
    check_shape("dense-down", p, ff, 0, 0, 0, 31, 18432, 7168, 1);
    check_shape("dense-up", p, ff, 0, 0, 0, 32, 7168, 18432, 1);
    check_absent("dense-no-router", p, ff, 0, 0, 0, 33);
    check_absent("moe-no-dense-gate", p, ff, 1, 0, 1, 30);

    // 10. KDA layer
    check_shape("kda-conv-q", p, ff, 1, 0, 1, 8, 4, 1, 8192);
    check_shape("kda-conv-v", p, ff, 1, 0, 1, 10, 4, 1, 8192);
    check_shape("kda-qkv", p, ff, 1, 0, 1, 11, 7168, 24576, 1);
    check_shape("kda-f-a", p, ff, 1, 0, 1, 12, 7168, 128, 1);
    check_shape("kda-f-b", p, ff, 1, 0, 1, 13, 128, 8192, 1);
    check_shape("kda-beta", p, ff, 1, 0, 1, 14, 7168, 64, 1);
    check_shape("kda-a", p, ff, 1, 0, 1, 15, 64, 1, 1);
    check_shape("kda-dt-b", p, ff, 1, 0, 1, 16, 8192, 1, 1);
    check_shape("kda-g", p, ff, 1, 0, 1, 17, 7168, 8192, 1);
    check_shape("kda-o-norm", p, ff, 1, 0, 1, 18, 128, 1, 1);
    check_shape("kda-wo", p, ff, 1, 0, 1, 19, 8192, 7168, 1);
    check_absent("kda-no-mla-q", p, ff, 1, 0, 1, 20);
    check_absent("mla-no-kda-gate", p, ff, 1, 1, 3, 17);

    // 11. MLA layer, low-rank query branch
    check_shape("mla-wq-a", p, ff, 1, 1, 3, 20, 7168, 1536, 1);
    check_shape("mla-q-a-norm", p, ff, 1, 1, 3, 21, 1536, 1, 1);
    check_shape("mla-wq-b", p, ff, 1, 1, 3, 22, 1536, 12288, 1);
    check_absent("mla-no-plain-q", p, ff, 1, 1, 3, 23);
    check_shape("mla-wkv-a", p, ff, 1, 1, 3, 24, 7168, 576, 1);
    check_shape("mla-kv-a-norm", p, ff, 1, 1, 3, 25, 512, 1, 1);
    check_shape("mla-wkv-b", p, ff, 1, 1, 3, 26, 512, 16384, 1);
    check_shape("mla-wk-b", p, ff, 1, 1, 3, 27, 128, 512, 64);
    check_shape("mla-wv-b", p, ff, 1, 1, 3, 28, 512, 128, 64);
    check_shape("mla-gate", p, ff, 1, 1, 3, 29, 7168, 8192, 1);
    check_shape("mla-wo", p, ff, 1, 1, 3, 19, 8192, 7168, 1);
    check_absent("mla-no-kda-qkv", p, ff, 1, 1, 3, 11);

    // 12. MLA layer, plain query branch (q_lora == 0)
    {
        std::vector<int64_t> pp = p;
        pp[10] = 0;
        check_shape("plain-wq", pp, ff, 1, 1, 3, 23, 7168, 12288, 1);
        check_absent("plain-no-wq-a", pp, ff, 1, 1, 3, 20);
        check_absent("plain-no-q-a-norm", pp, ff, 1, 1, 3, 21);
        check_absent("plain-no-wq-b", pp, ff, 1, 1, 3, 22);
    }

    // 13. latent MoE: router on the full width, experts on the latent one
    check_shape("router", p, ff, 1, 0, 1, 33, 7168, 384, 1);
    check_shape("exp-probs-b", p, ff, 1, 0, 1, 34, 384, 1, 1);
    check_shape("gate-exps-latent", p, ff, 1, 0, 1, 35, 1536, 1024, 384);
    check_shape("down-exps-latent", p, ff, 1, 0, 1, 36, 1024, 1536, 384);
    check_shape("up-exps-latent", p, ff, 1, 0, 1, 37, 1536, 1024, 384);
    check_shape("routed-down", p, ff, 1, 0, 1, 38, 7168, 1536, 1);
    check_shape("routed-up", p, ff, 1, 0, 1, 39, 1536, 7168, 1);
    check_shape("routed-norm", p, ff, 1, 0, 1, 40, 1536, 1, 1);
    check_shape("shexp-gate", p, ff, 1, 0, 1, 41, 7168, 1024, 1);
    check_shape("shexp-down", p, ff, 1, 0, 1, 42, 1024, 7168, 1);
    check_shape("shexp-up", p, ff, 1, 0, 1, 43, 7168, 1024, 1);
    {
        // the per-layer array is read, not one global width
        check_shape("exps-layer2-width", p, ff, 1, 0, 2, 35, 1536, 2048, 384);
        check_shape("shexp-layer2-width", p, ff, 1, 0, 2, 41, 7168, 2048, 1);
    }
    {
        // no latent length: experts and the router meet at n_embd, no adapters
        std::vector<int64_t> pp = p;
        pp[18] = 0;
        check_shape("exps-full-width", pp, ff, 1, 0, 1, 35, 7168, 1024, 384);
        check_absent("no-routed-down", pp, ff, 1, 0, 1, 38);
        check_absent("no-routed-up", pp, ff, 1, 0, 1, 39);
        check_absent("no-routed-norm", pp, ff, 1, 0, 1, 40);
    }
    {
        // shared experts at a zero count keep the single-expert width
        std::vector<int64_t> pp = p;
        pp[16] = 0;
        check_shape("shexp-zero-count", pp, ff, 1, 0, 1, 41, 7168, 1024, 1);
        pp[16] = 2;
        check_shape("shexp-two-count", pp, ff, 1, 0, 1, 41, 7168, 2048, 1);
    }

    // 14. an out-of-range layer index falls back to the declared width
    check_shape("exps-width-fallback", p, ff, 1, 0, -1, 35, 1536, 2048, 384);

    printf("FG-ARCH10 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
