// Structure gate for the glm5-next builder draft (P6-05 architecture 12).
//
// Expected values are the native loader's declarations in
// src/models/glm5-next.cpp. No native library is linked.

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

static std::vector<int64_t> params() {
    std::vector<int64_t> p(26, 0);
    p[0]  = 5120;     // n_embd
    p[1]  = 154880;   // n_vocab
    p[2]  = 45;       // n_layer (trunk)
    p[3]  = 46;       // n_layer_all (trunk + one MTP block)
    p[4]  = 64;       // n_head (query and KDA heads)
    p[5]  = 192;      // n_embd_head_k_mla
    p[6]  = 128;      // n_embd_head_v_mla
    p[7]  = 0;        // n_rot (nope-only MLA)
    p[8]  = 1536;     // q_lora_rank
    p[9]  = 512;      // kv_lora_rank
    p[10] = 128;      // n_embd_head_kda
    p[11] = 4;        // ssm_d_conv
    p[12] = 256;      // n_expert
    p[13] = 8;        // n_expert_used
    p[14] = 2;        // n_expert_shared
    p[15] = 12288;    // n_ff (dense width)
    p[16] = 32;       // indexer_n_head
    p[17] = 128;      // indexer_head_size
    p[18] = 2048;     // indexer_top_k
    p[19] = 4;        // indexer_kpool
    p[20] = 1;        // indexer_kpool_select_tail
    p[21] = 4;        // dsv4_hc_mult
    p[22] = 3;        // n_layer_dense_lead
    p[23] = 0;        // expert_gating_func NONE -> SIGMOID
    p[24] = 1;        // declared n_layer_nextn
    p[25] = 1;        // MTP probe: nextn.eh_proj present
    return p;
}

// kv_heads: zero means a KDA layer, positive means nope MLA. Every fourth
// trunk layer (3, 7, ..., 43) is MLA; the MTP layer 45 is MLA too, although
// the plan marks it as MTP before reading this array.
static std::vector<int64_t> kv_heads() {
    std::vector<int64_t> kv(46, 0);
    for (int il = 3; il < 45; il += 4) {
        kv[il] = 8;
    }
    kv[45] = 8;
    return kv;
}

// expert_feed_forward_length per layer: zero on the three dense-lead layers,
// 1536 on every MoE layer.
static std::vector<int64_t> ff_exp() {
    std::vector<int64_t> ff(46, 1536);
    ff[0] = 0;
    ff[1] = 0;
    ff[2] = 0;
    return ff;
}

// indexer types per trunk layer: layer 7 is a shared (non-full) indexer that
// reuses layer 3's selection.
static std::vector<int64_t> indexer_full() {
    std::vector<int64_t> f(45, 1);
    f[7] = 0;
    return f;
}

static void check_val(const char * name, int64_t got, int64_t want) {
    ++g_cases;
    ++g_values;
    if (got != want) {
        bad(name, "got " + std::to_string((long long) got) + " expected " + std::to_string((long long) want));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, const std::vector<int64_t> & ff,
                        const std::vector<int64_t> & flags, int64_t kind, int64_t il,
                        int64_t tid, int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p, f = ff, fl = flags;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = g5_shape(sli(pp), sli(f), sli(fl), kind, il, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," + std::to_string((long long) out[1]) +
                  "," + std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                  " expected {" + std::to_string((long long) a) + "," + std::to_string((long long) b) +
                  "," + std::to_string((long long) c) + "}");
    }
}

// A tensor the plan must not own: the shape call has to report 0.
static void check_absent(const char * name, const std::vector<int64_t> & p, const std::vector<int64_t> & ff,
                         const std::vector<int64_t> & flags, int64_t kind, int64_t il, int64_t tid) {
    ++g_cases;
    std::vector<int64_t> pp = p, f = ff, fl = flags;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = g5_shape(sli(pp), sli(f), sli(fl), kind, il, tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "tensor present (ok=" + std::to_string((long long) ok) + "), expected absent");
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();
    const auto kv = kv_heads();
    const auto ff = ff_exp();
    const auto fl = indexer_full();

    // 1. plan: zero KV heads select KDA, positive select nope MLA
    {
        std::vector<int64_t> pp = p, k = kv, kinds(46, -1);
        const int64_t n = g5_plan(sli(pp), sli(k), sli(kinds));
        check_val("plan-len", n, 46);
        check_val("plan-layer0-kda", kinds[0], 0);
        check_val("plan-layer1-kda", kinds[1], 0);
        check_val("plan-layer2-kda", kinds[2], 0);
        check_val("plan-layer3-mla", kinds[3], 1);
        check_val("plan-layer7-mla", kinds[7], 1);
        check_val("plan-layer43-mla", kinds[43], 1);
        check_val("plan-layer45-mtp", kinds[45], 2);
    }
    // 2. attention kind and the out-of-range guard
    {
        std::vector<int64_t> k = kv;
        check_val("attn-kv0-kda", g5_attn_kind(sli(k), 0), 0);
        check_val("attn-kv8-mla", g5_attn_kind(sli(k), 3), 1);
        std::vector<int64_t> pp = p;
        check_val("kind-out-of-range", g5_layer_kind(sli(pp), sli(k), 46), -1);
        check_val("kind-negative", g5_layer_kind(sli(pp), sli(k), -1), -1);
    }

    // 3. dense lead then MoE; MTP is always MoE
    {
        std::vector<int64_t> pp = p;
        check_val("ffn-layer0-dense", g5_ffn_kind(sli(pp), 0), 0);
        check_val("ffn-layer2-dense", g5_ffn_kind(sli(pp), 2), 0);
        check_val("ffn-layer3-moe", g5_ffn_kind(sli(pp), 3), 1);
        check_val("ffn-layer44-moe", g5_ffn_kind(sli(pp), 44), 1);
        check_val("ffn-layer45-moe", g5_ffn_kind(sli(pp), 45), 1);
        check_val("ffn-out-of-range", g5_ffn_kind(sli(pp), 46), -1);
    }

    // 4. indexer ownership: full flag, shared layer, MTP always full
    {
        std::vector<int64_t> pp = p, f = fl;
        check_val("idx-full-layer3", g5_indexer_full(sli(pp), sli(f), 3), 1);
        check_val("idx-full-layer7-shared", g5_indexer_full(sli(pp), sli(f), 7), 0);
        check_val("idx-full-layer11", g5_indexer_full(sli(pp), sli(f), 11), 1);
        check_val("idx-full-mtp", g5_indexer_full(sli(pp), sli(f), 45), 1);
        check_val("idx-full-out-of-range", g5_indexer_full(sli(pp), sli(f), 46), -1);
        check_val("has-indexer-kda", g5_has_indexer(0), 0);
        check_val("has-indexer-mla", g5_has_indexer(1), 1);
        check_val("has-indexer-mtp", g5_has_indexer(2), 1);
    }

    // 5. k-pool legality, selection width and sparse mask width
    check_val("kpool-ok", g5_kpool_ok(4, 2048), 1);
    check_val("kpool-one-rejected", g5_kpool_ok(1, 2048), 0);
    check_val("kpool-remainder-rejected", g5_kpool_ok(4, 2047), 0);
    check_val("top-pool-short", g5_n_top_pool(64, 2048, 4), 64);
    check_val("top-pool-capped", g5_n_top_pool(1000, 2048, 4), 512);
    check_val("top-pool-kpool8", g5_n_top_pool(64, 2048, 8), 64);
    check_val("n-sel-tail", g5_n_sel(64, 2048, 4, 1), 259);
    check_val("n-sel-no-tail", g5_n_sel(64, 2048, 4, 0), 256);
    check_val("n-sel-capped-tail", g5_n_sel(1000, 2048, 4, 1), 2051);
    check_val("dsa-mask-len", g5_dsa_mask_len(1000, 259), 1259);
    check_val("dsa-mask-zero", g5_dsa_mask_len(0, 0), 0);

    // 6. nope-only MLA geometry
    check_val("mla-nope-ok", g5_mla_nope_ok(0), 1);
    check_val("mla-rope-rejected", g5_mla_nope_ok(64), 0);
    check_val("qk-nope-full", g5_qk_nope(192, 0), 192);
    check_val("qk-nope-split", g5_qk_nope(192, 64), 128);
    check_val("mla-cache-width", g5_mla_cache_width(512), 512);
    check_val("wkv-a-nope", g5_wkv_a_width(512, 0), 512);
    check_val("wkv-a-rope", g5_wkv_a_width(512, 64), 576);

    // 7. KDA geometry and the decay-gate variant
    check_val("kda-d-inner", g5_d_inner(128, 64), 8192);
    check_val("kda-conv-state", g5_kda_conv_state(4, 8192), 24576);
    check_val("kda-gate-bounded", g5_kda_gate_kind(1), 1);
    check_val("kda-gate-softplus", g5_kda_gate_kind(0), 0);

    // 8. hyper-connection widths and the flat MTP output width
    check_val("hc-mix-dim", g5_hc_mix_dim(4), 24);
    check_val("hc-mix-dim2", g5_hc_mix_dim(2), 8);
    check_val("hc-mult4", g5_hc_supported(4), 1);
    check_val("hc-mult8-rejected", g5_hc_supported(8), 0);
    check_val("hc-fn-width", g5_hc_fn_width(5120, 4), 20480);
    check_val("out-effective", g5_out_width_effective(0, 5120), 5120);
    check_val("out-declared", g5_out_width_effective(4096, 5120), 4096);
    check_val("out-matches-flat", g5_out_matches(5120, 5120), 1);
    check_val("out-hc-rejected", g5_out_matches(20480, 5120), 0);

    // 9. expert widths, gating default and the shexp clamp fallback
    {
        std::vector<int64_t> pp = p, f = ff;
        check_val("ff-exp-dense-layer", g5_ff_exp(sli(pp), sli(f), 0), 0);
        check_val("ff-exp-moe-layer", g5_ff_exp(sli(pp), sli(f), 3), 1536);
        check_val("ff-exp-mtp", g5_ff_exp(sli(pp), sli(f), 45), 1536);
    }
    check_val("shexp-width", g5_shexp_width(1536, 2), 3072);
    check_val("shexp-width-single", g5_shexp_width(1536, 1), 1536);
    check_val("gating-default-sigmoid", g5_gating_effective(0), 1);
    check_val("gating-sigmoid", g5_gating_effective(1), 1);
    check_val("gating-kept", g5_gating_effective(3), 3);
    check_val("clamp-shexp-fallback", g5_clamp_shexp(0, 0, 7), 7);
    check_val("clamp-shexp-declared", g5_clamp_shexp(1, 9, 7), 9);

    // 10. MTP probe, single-block rule and the doubled input projection
    check_val("nextn-present", g5_nextn_count(1, 1), 1);
    check_val("nextn-probe-missing", g5_nextn_count(1, 0), 0);
    check_val("nextn-not-declared", g5_nextn_count(0, 1), 0);
    check_val("nextn-single", g5_nextn_single(1), 1);
    check_val("nextn-double-rejected", g5_nextn_single(2), 0);
    check_val("eh-proj-width", g5_eh_proj_width(5120), 10240);

    // 11. global tensors
    check_shape("token-embd", p, ff, fl, 0, 0, 0, 5120, 154880, 1);
    check_shape("output-norm", p, ff, fl, 0, 0, 1, 5120, 1, 1);
    check_shape("output", p, ff, fl, 0, 0, 2, 5120, 154880, 1);

    // 12. KDA layer 0 (dense lead): common norms and the two HC modules
    check_shape("kda-attn-norm", p, ff, fl, 0, 0, 3, 5120, 1, 1);
    check_shape("kda-ffn-norm", p, ff, fl, 0, 0, 4, 5120, 1, 1);
    check_shape("hc-attn-fn", p, ff, fl, 0, 0, 5, 20480, 24, 1);
    check_shape("hc-attn-base", p, ff, fl, 0, 0, 6, 24, 1, 1);
    check_shape("hc-attn-scale", p, ff, fl, 0, 0, 7, 3, 1, 1);
    check_shape("hc-ffn-fn", p, ff, fl, 0, 0, 8, 20480, 24, 1);
    check_shape("hc-ffn-base", p, ff, fl, 0, 0, 9, 24, 1, 1);
    check_shape("hc-ffn-scale", p, ff, fl, 0, 0, 10, 3, 1, 1);

    // 13. KDA projections, convolutions and gates
    check_shape("kda-wq", p, ff, fl, 0, 0, 11, 5120, 8192, 1);
    check_shape("kda-wk", p, ff, fl, 0, 0, 12, 5120, 8192, 1);
    check_shape("kda-wv", p, ff, fl, 0, 0, 13, 5120, 8192, 1);
    check_shape("kda-q-conv", p, ff, fl, 0, 0, 14, 4, 1, 8192);
    check_shape("kda-k-conv", p, ff, fl, 0, 0, 15, 4, 1, 8192);
    check_shape("kda-v-conv", p, ff, fl, 0, 0, 16, 4, 1, 8192);
    check_shape("kda-f-a", p, ff, fl, 0, 0, 17, 5120, 128, 1);
    check_shape("kda-f-b", p, ff, fl, 0, 0, 18, 128, 8192, 1);
    check_shape("kda-beta", p, ff, fl, 0, 0, 19, 5120, 64, 1);
    check_shape("kda-a", p, ff, fl, 0, 0, 20, 64, 1, 1);
    check_shape("kda-dt-bias", p, ff, fl, 0, 0, 21, 8192, 1, 1);
    check_shape("kda-g-a", p, ff, fl, 0, 0, 22, 5120, 128, 1);
    check_shape("kda-g-b", p, ff, fl, 0, 0, 23, 128, 8192, 1);
    check_shape("kda-o-norm", p, ff, fl, 0, 0, 24, 128, 1, 1);
    check_shape("kda-wo", p, ff, fl, 0, 0, 25, 8192, 5120, 1);

    // 14. dense lead FFN on layer 0, routed experts absent there
    check_shape("dense-gate", p, ff, fl, 0, 0, 41, 5120, 12288, 1);
    check_shape("dense-down", p, ff, fl, 0, 0, 42, 12288, 5120, 1);
    check_shape("dense-up", p, ff, fl, 0, 0, 43, 5120, 12288, 1);
    check_absent("dense-no-gate-inp", p, ff, fl, 0, 0, 44);
    check_absent("dense-no-gate-exps", p, ff, fl, 0, 0, 46);
    check_absent("kda-no-mla-wq-a", p, ff, fl, 0, 0, 28);
    check_absent("kda-no-ssm-on-mla", p, ff, fl, 1, 3, 17);
    check_absent("trunk-no-nextn", p, ff, fl, 0, 0, 52);

    // 15. KDA layer 4 is MoE: the attention family does not decide the FFN
    check_shape("kda-moe-router", p, ff, fl, 0, 4, 44, 5120, 256, 1);
    check_shape("kda-moe-probs", p, ff, fl, 0, 4, 45, 256, 1, 1);
    check_shape("kda-moe-gate-exps", p, ff, fl, 0, 4, 46, 5120, 1536, 256);
    check_shape("kda-moe-down-exps", p, ff, fl, 0, 4, 47, 1536, 5120, 256);
    check_shape("kda-moe-up-exps", p, ff, fl, 0, 4, 48, 5120, 1536, 256);
    check_shape("kda-moe-gate-shexp", p, ff, fl, 0, 4, 49, 5120, 3072, 1);
    check_shape("kda-moe-down-shexp", p, ff, fl, 0, 4, 50, 3072, 5120, 1);
    check_shape("kda-moe-up-shexp", p, ff, fl, 0, 4, 51, 5120, 3072, 1);
    check_absent("kda-moe-no-dense", p, ff, fl, 0, 4, 41);

    // 16. nope MLA layer 3: low-rank query, compressed KV and the O projection
    check_shape("mla-q-a-norm", p, ff, fl, 1, 3, 26, 1536, 1, 1);
    check_shape("mla-kv-a-norm", p, ff, fl, 1, 3, 27, 512, 1, 1);
    check_shape("mla-wq-a", p, ff, fl, 1, 3, 28, 5120, 1536, 1);
    check_shape("mla-wq-b", p, ff, fl, 1, 3, 29, 1536, 12288, 1);
    check_shape("mla-wkv-a-mqa", p, ff, fl, 1, 3, 30, 5120, 512, 1);
    check_shape("mla-wk-b", p, ff, fl, 1, 3, 31, 192, 512, 64);
    check_shape("mla-wv-b", p, ff, fl, 1, 3, 32, 512, 128, 64);
    check_shape("mla-attn-out", p, ff, fl, 1, 3, 33, 8192, 5120, 1);
    check_absent("mla-no-kda", p, ff, fl, 1, 3, 11);

    // 17. full k-pool indexer on layer 3
    check_shape("idx-k-norm", p, ff, fl, 1, 3, 34, 128, 1, 1);
    check_shape("idx-k-norm-bias", p, ff, fl, 1, 3, 35, 128, 1, 1);
    check_shape("idx-proj", p, ff, fl, 1, 3, 36, 5120, 32, 1);
    check_shape("idx-attn-k", p, ff, fl, 1, 3, 37, 5120, 128, 1);
    check_shape("idx-attn-q-b", p, ff, fl, 1, 3, 38, 1536, 4096, 1);
    check_shape("idx-kpool-gate", p, ff, fl, 1, 3, 39, 5120, 128, 1);
    check_shape("idx-kpool-ape", p, ff, fl, 1, 3, 40, 128, 4, 1);

    // 18. shared indexer layer 7: the plan does not own the selection tensors
    check_absent("shared-idx-proj", p, ff, fl, 1, 7, 36);
    check_absent("shared-idx-ape", p, ff, fl, 1, 7, 40);
    check_absent("shared-idx-q-b", p, ff, fl, 1, 7, 38);

    // 19. MTP layer 45: flat MLA, full indexer, MoE FFN, no HC tensors
    check_shape("mtp-attn-norm", p, ff, fl, 2, 45, 3, 5120, 1, 1);
    check_shape("mtp-wq-a", p, ff, fl, 2, 45, 28, 5120, 1536, 1);
    check_shape("mtp-wq-b", p, ff, fl, 2, 45, 29, 1536, 12288, 1);
    check_shape("mtp-wk-b", p, ff, fl, 2, 45, 31, 192, 512, 64);
    check_shape("mtp-idx-proj", p, ff, fl, 2, 45, 36, 5120, 32, 1);
    check_shape("mtp-idx-ape", p, ff, fl, 2, 45, 40, 128, 4, 1);
    check_shape("mtp-router", p, ff, fl, 2, 45, 44, 5120, 256, 1);
    check_shape("mtp-gate-exps", p, ff, fl, 2, 45, 46, 5120, 1536, 256);
    check_shape("mtp-gate-shexp", p, ff, fl, 2, 45, 49, 5120, 3072, 1);
    check_shape("mtp-eh-proj", p, ff, fl, 2, 45, 52, 10240, 5120, 1);
    check_shape("mtp-enorm", p, ff, fl, 2, 45, 53, 5120, 1, 1);
    check_shape("mtp-hnorm", p, ff, fl, 2, 45, 54, 5120, 1, 1);
    check_shape("mtp-embed", p, ff, fl, 2, 45, 55, 5120, 154880, 1);
    check_shape("mtp-shared-head", p, ff, fl, 2, 45, 56, 5120, 154880, 1);
    check_shape("mtp-shared-head-norm", p, ff, fl, 2, 45, 57, 5120, 1, 1);
    check_absent("mtp-no-hc-attn", p, ff, fl, 2, 45, 5);
    check_absent("mtp-no-hc-ffn", p, ff, fl, 2, 45, 8);
    check_absent("mtp-no-dense", p, ff, fl, 2, 45, 41);

    printf("FG-ARCH12 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
