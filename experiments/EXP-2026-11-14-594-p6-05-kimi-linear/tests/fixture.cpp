// Structure gate for the kimi-linear builder draft (P6-05 architecture 11).
//
// Expected values are the native loader's declarations and the native graph's
// view geometry in src/models/kimi-linear.cpp plus the Kimi KDA branches of
// llama-hparams.cpp. No native library is linked.

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

// One 21-slot parameter layout, see tests/cases.md.
// Default file shape: 48B-A3B geometry, no q lora, split wk_b/wv_b, one dense
// leading layer, one shared expert.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(21, 0);
    p[0]  = 2304;        // n_embd
    p[1]  = 163840;      // n_vocab
    p[2]  = 27;          // n_layer
    p[3]  = 32;          // n_head
    p[4]  = 9216;        // n_ff
    p[5]  = 256;         // n_expert
    p[6]  = 8;           // n_expert_used
    p[7]  = 1024;        // n_ff_exp declared (per-layer array in the file)
    p[8]  = 1;           // n_expert_shared
    p[9]  = 4;           // ssm_d_conv
    p[10] = 128;         // n_embd_head_kda
    p[11] = 192;         // n_embd_head_k_mla
    p[12] = 128;         // n_embd_head_v_mla
    p[13] = 512;         // kv_lora_rank
    p[14] = 0;           // q_lora_rank (absent)
    p[15] = 64;          // qk_rope_head_dim (n_rot, width only)
    p[16] = 1;           // n_layer_dense_lead
    p[17] = 0;           // attn_q_a_norm absent -> one wq
    p[18] = 0;           // combined wkv_b absent -> split wk_b/wv_b
    p[19] = 0;           // expert_gating_func: softmax
    p[20] = 1075628868;  // expert_weights_scale 2.446f = 0x401C8B44
    return p;
}

// Legacy / compressed variant: q lora present, combined wkv_b, two shared
// experts and two dense leading layers.
static std::vector<int64_t> params_legacy() {
    std::vector<int64_t> p = params();
    p[8]  = 2;    // n_expert_shared
    p[14] = 1024; // q_lora_rank
    p[16] = 2;    // n_layer_dense_lead
    p[17] = 1;    // attn_q_a_norm present -> wq_a/wq_b
    p[18] = 1;    // combined wkv_b present -> MHA fallback
    return p;
}

// n_head_kv per layer: 0 marks a KDA (recurrent) layer, n_head marks MLA.
// The 48B-A3B checkpoint keeps MLA on every fourth layer.
static std::vector<int64_t> kv_array() {
    std::vector<int64_t> kv(27, 0);
    for (int il = 3; il < 27; il += 4) {
        kv[il] = 32;
    }
    return kv;
}

static void check_val(const char * name, int64_t got, int64_t want) {
    ++g_cases;
    ++g_values;
    if (got != want) {
        bad(name, "got " + std::to_string((long long) got) + " expected " +
                  std::to_string((long long) want));
    }
}

static void check_shape(const std::vector<int64_t> & p, int64_t kind, int64_t il, int64_t tid,
                        int64_t a, int64_t b, int64_t c, const char * name) {
    ++g_cases;
    g_values += 3;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = kl_shape(sli(pp), kind, il, tid, sli(out));
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," +
                  std::to_string((long long) out[1]) + "," +
                  std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                  " expected {" + std::to_string((long long) a) + "," +
                  std::to_string((long long) b) + "," + std::to_string((long long) c) + "}");
    }
}

// A tensor the layer must not own: kl_shape has to report 0.
static void check_absent(const std::vector<int64_t> & p, int64_t kind, int64_t il, int64_t tid,
                         const char * name) {
    ++g_cases;
    ++g_values;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = kl_shape(sli(pp), kind, il, tid, sli(out));
    if (ok != 0) {
        bad(name, "tensor present (ok=" + std::to_string((long long) ok) + "), expected absent");
    }
}

int main() {
    goose_init(0, nullptr);

    auto p = params();
    auto pl = params_legacy();
    const auto kv = kv_array();

    // 1-2. per-layer marking and plan (kimi-linear.cpp:15-19, :18)
    {
        std::vector<int64_t> kk = kv, fl(27, -1), kinds(27, -1);
        const int64_t n = kl_recr_flags(sli(kk), sli(fl));
        check_val("flags-len", n, 27);
        check_val("flags-layer0-kda", fl[0], 1);
        check_val("flags-layer3-mla", fl[3], 0);
        check_val("flags-layer26-kda", fl[26], 1);

        const int64_t np = kl_plan(sli(fl), sli(kinds));
        check_val("plan-len", np, 27);
        int plan_bad = 0;
        for (int il = 0; il < 27; ++il) {
            const int64_t want = (il % 4 == 3) ? 0 : 1;
            if (kinds[il] != want) {
                plan_bad = 1;
            }
        }
        check_val("plan-kinds", plan_bad, 0);
    }

    // 3-8. kind boundaries through the flag array (llama-hparams.cpp:258-265)
    {
        std::vector<int64_t> kk = kv, fl(27, -1);
        kl_recr_flags(sli(kk), sli(fl));
        check_val("kind-layer0-kda", kl_kind(sli(fl), 0), 1);
        check_val("kind-layer3-mla", kl_kind(sli(fl), 3), 0);
        check_val("kind-layer23-mla", kl_kind(sli(fl), 23), 0);
        check_val("kind-layer26-kda", kl_kind(sli(fl), 26), 1);
        check_val("kind-below-zero", kl_kind(sli(fl), -1), -1);
        check_val("kind-past-last", kl_kind(sli(fl), 27), -1);
    }

    // 9-13. loader policy: required output, no post norm, no q/k norm (:37-41, :46, :113)
    check_val("output-required", kl_output_required(), 1);
    check_val("no-post-norm", kl_has_post_norm(), 0);
    check_val("no-qk-norm", kl_qk_norm_required(), 0);
    // the rotary width is a slice width only (:125, :273, :396)
    check_val("no-rope", kl_rope_applied(), 0);
    // MoE renormalises the routed scores (:519)
    check_val("moe-norm-w", kl_moe_norm_w(), 1);

    // 14-16. KDA gate ops are sigmoid, applied after the norm (:334, :376-379)
    check_val("kda-beta-sigmoid", kl_kda_beta_gate(), 2);
    check_val("kda-out-sigmoid", kl_kda_out_gate(), 2);
    check_val("kda-norm-before-gate", kl_kda_norm_before_gate(), 1);

    // 17-20. routing kind passes through, score scale from the file, MLA scale computed
    check_val("gating-softmax", kl_gating_kind(sli(p)), 0);
    {
        std::vector<int64_t> g = p;
        g[19] = 1;
        check_val("gating-sigmoid", kl_gating_kind(sli(g)), 1);
    }
    check_val("weights-scale-2.446", kl_weights_scale_used(sli(p)), 1);
    check_val("kq-scale-computed", kl_kq_scale_auto_kind(), 1);

    // 21-24. type table and per-file layout switches (:27-30, :112-135)
    check_val("type-27-layers", kl_type_known(27), 1);
    check_val("type-28-unknown", kl_type_known(28), 0);
    check_val("q-lora-absent", kl_q_lora_used(sli(p)), 0);
    check_val("cache-split", kl_mla_cache_path(sli(p)), 1);
    check_val("q-lora-present", kl_q_lora_used(sli(pl)), 1);
    check_val("cache-combined", kl_mla_cache_path(sli(pl)), 0);

    // 25-32. KDA / MLA derived geometry (llama-hparams.cpp:219-224, :242-247, kimi-linear.cpp:274, :129, :159)
    check_val("d-inner", kl_d_inner(sli(p)), 4096);
    check_val("conv-state", kl_kda_conv_state(sli(p)), 12288);
    check_val("conv-total", kl_kda_conv_total(sli(p)), 36864);
    check_val("kda-state", kl_kda_state(sli(p)), 524288);
    check_val("mla-nope", kl_nope(sli(p)), 128);
    check_val("wkv-b-width", kl_mla_wkv_b_width(sli(p)), 8192);
    check_val("shexp-width-1", kl_shexp_width(1024, 1), 1024);
    check_val("shexp-width-2", kl_shexp_width(1024, 2), 2048);
    check_val("shexp-width-0", kl_shexp_width(1024, 0), 1024);

    // 33-36. FFN branch boundary (:144, :501)
    check_val("ffn-layer0-dense", kl_ffn_kind(sli(p), 0), 0);
    check_val("ffn-layer1-moe", kl_ffn_kind(sli(p), 1), 1);
    check_val("ffn-below-zero", kl_ffn_kind(sli(p), -1), -1);
    check_val("ffn-past-last", kl_ffn_kind(sli(p), 27), -1);
    check_val("ffn-lead2-layer1-dense", kl_ffn_kind(sli(pl), 1), 0);
    check_val("ffn-lead2-layer2-moe", kl_ffn_kind(sli(pl), 2), 1);

    // 37-42. globals and per-layer norms (:39-41, :48, :138)
    check_shape(p, 1, 1, 0, 2304, 163840, 1, "token_embd");
    check_shape(p, 1, 1, 1, 2304, 1, 1, "output_norm");
    check_shape(p, 1, 1, 2, 2304, 163840, 1, "output");
    check_shape(p, 1, 1, 3, 2304, 1, 1, "attn_norm-kda");
    check_shape(p, 0, 3, 3, 2304, 1, 1, "attn_norm-mla");
    check_shape(p, 1, 1, 4, 2304, 1, 1, "ffn_norm");

    // 43-50. MLA layer, default layout: no q lora, split kv_b (:105-134)
    check_shape(p, 0, 3, 5, 2304, 6144, 1, "mla-wq");
    check_shape(p, 0, 3, 9, 2304, 576, 1, "mla-wkv-a-mqa");
    check_shape(p, 0, 3, 10, 512, 1, 1, "mla-kv-a-norm");
    check_shape(p, 0, 3, 12, 128, 512, 32, "mla-wk-b");
    check_shape(p, 0, 3, 13, 512, 128, 32, "mla-wv-b");
    check_shape(p, 0, 3, 14, 4096, 2304, 1, "mla-wo");
    check_absent(p, 0, 3, 6, "mla-wq-a-absent");
    check_absent(p, 0, 3, 7, "mla-q-a-norm-absent");
    check_absent(p, 0, 3, 11, "mla-wkv-b-absent");

    // 51-65. KDA layer: three convs, inner-width q/k/v, per-head beta/A_log
    check_shape(p, 1, 1, 15, 4, 1, 4096, "kda-q-conv");
    check_shape(p, 1, 1, 16, 4, 1, 4096, "kda-k-conv");
    check_shape(p, 1, 1, 17, 4, 1, 4096, "kda-v-conv");
    check_shape(p, 1, 1, 18, 2304, 4096, 1, "kda-wq");
    check_shape(p, 1, 1, 19, 2304, 4096, 1, "kda-wk");
    check_shape(p, 1, 1, 20, 2304, 4096, 1, "kda-wv");
    check_shape(p, 1, 1, 21, 2304, 128, 1, "kda-f-a");
    check_shape(p, 1, 1, 22, 128, 4096, 1, "kda-f-b");
    check_shape(p, 1, 1, 23, 2304, 32, 1, "kda-beta");
    check_shape(p, 1, 1, 24, 1, 32, 1, "kda-a-log");
    check_shape(p, 1, 1, 25, 4096, 1, 1, "kda-dt-bias");
    check_shape(p, 1, 1, 26, 2304, 128, 1, "kda-g-a");
    check_shape(p, 1, 1, 27, 128, 4096, 1, "kda-g-b");
    check_shape(p, 1, 1, 28, 128, 1, 1, "kda-o-norm");
    check_shape(p, 1, 1, 14, 4096, 2304, 1, "kda-wo");

    // 66-77. dense leading layer and MoE layers (:144-164)
    check_shape(p, 1, 0, 29, 2304, 9216, 1, "dense-gate");
    check_shape(p, 1, 0, 30, 9216, 2304, 1, "dense-down");
    check_shape(p, 1, 0, 31, 2304, 9216, 1, "dense-up");
    check_absent(p, 1, 1, 29, "dense-gate-absent-on-moe");
    check_absent(p, 1, 0, 32, "moe-router-absent-on-dense");
    check_shape(p, 1, 1, 32, 2304, 256, 1, "moe-gate-inp");
    check_shape(p, 1, 1, 33, 2304, 1024, 256, "moe-gate-exps");
    check_shape(p, 1, 1, 34, 1024, 2304, 256, "moe-down-exps");
    check_shape(p, 1, 1, 35, 2304, 1024, 256, "moe-up-exps");
    check_shape(p, 1, 1, 36, 2304, 1024, 1, "moe-gate-shexp");
    check_shape(p, 1, 1, 37, 1024, 2304, 1, "moe-down-shexp");
    check_shape(p, 1, 1, 38, 2304, 1024, 1, "moe-up-shexp");
    check_shape(p, 1, 1, 39, 256, 1, 1, "moe-exp-probs-b");

    // 78-82. the kinds and the FFN branches own disjoint tensor sets
    check_absent(p, 0, 3, 15, "kda-conv-absent-on-mla");
    check_absent(p, 0, 3, 18, "kda-wq-absent-on-mla");
    check_absent(p, 1, 1, 9, "mla-wkv-a-absent-on-kda");
    check_absent(p, 1, 1, 12, "mla-wk-b-absent-on-kda");
    check_absent(p, 1, 0, 39, "moe-probs-absent-on-dense");

    // 83-90. legacy / compressed variant (:115-121, :128-132, :159, :144)
    check_shape(pl, 0, 3, 6, 2304, 1024, 1, "legacy-wq-a");
    check_shape(pl, 0, 3, 7, 1024, 1, 1, "legacy-q-a-norm");
    check_shape(pl, 0, 3, 8, 1024, 6144, 1, "legacy-wq-b");
    check_absent(pl, 0, 3, 5, "legacy-wq-absent");
    check_shape(pl, 0, 3, 11, 512, 8192, 1, "legacy-wkv-b");
    check_absent(pl, 0, 3, 12, "legacy-wk-b-absent");
    check_shape(pl, 1, 2, 36, 2304, 2048, 1, "legacy-shexp-2");
    check_shape(pl, 1, 2, 37, 2048, 2304, 1, "legacy-shexp-down-2");
    check_shape(pl, 1, 1, 29, 2304, 9216, 1, "legacy-dense-layer1");
    check_shape(pl, 1, 1, 30, 9216, 2304, 1, "legacy-dense-down-layer1");

    printf("FG-ARCH11 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
