// Structure gate for the deepseek4 builder draft (P6-05 architecture 9).
//
// Expected values are the native loader's declarations in
// src/models/deepseek4.cpp. No native library is linked.

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
    std::vector<int64_t> p(23, 0);
    p[0]  = 4096;     // n_embd
    p[1]  = 129280;   // n_vocab
    p[2]  = 43;       // n_layer (trunk)
    p[3]  = 44;       // n_layer_all (trunk + one MTP block)
    p[4]  = 64;       // n_head
    p[5]  = 128;      // n_embd_head_k
    p[6]  = 64;       // n_rot
    p[7]  = 1536;     // q_lora_rank
    p[8]  = 8;        // o_group_count
    p[9]  = 512;      // o_lora_rank
    p[10] = 256;      // n_expert
    p[11] = 8;        // n_expert_used
    p[12] = 2048;     // n_ff_exp (declared expert_feed_forward_length)
    p[13] = 1;        // n_expert_shared
    p[14] = 64;       // indexer_n_head
    p[15] = 128;      // indexer_head_size
    p[16] = 2048;     // indexer_top_k
    p[17] = 4;        // hc_mult
    p[18] = 2;        // hash_layer_count
    p[19] = 2048;     // n_swa
    p[20] = 3;        // expert_gating_func (sqrtsoftplus)
    p[21] = 1;        // MTP probe: nextn.eh_proj present
    p[22] = 1;        // declared n_layer_nextn
    return p;
}

// compress_ratios: layers 0..1 raw, every (il % 8 == 7) layer CSA, the rest
// HCA, and the MTP layer 43 without a compressor.
static std::vector<int64_t> ratios() {
    std::vector<int64_t> r(44, 128);
    r[0] = 0;
    r[1] = 0;
    for (int il = 0; il < 43; ++il) {
        if (il % 8 == 7) {
            r[il] = 4;
        }
    }
    r[43] = 0;
    return r;
}

static void check_val(const char * name, int64_t got, int64_t want) {
    ++g_cases;
    ++g_values;
    if (got != want) {
        bad(name, "got " + std::to_string((long long) got) + " expected " + std::to_string((long long) want));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t il,
                        int64_t tid, int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = ds4_shape(sli(pp), kind, il, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," + std::to_string((long long) out[1]) +
                  "," + std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                  " expected {" + std::to_string((long long) a) + "," + std::to_string((long long) b) +
                  "," + std::to_string((long long) c) + "}");
    }
}

// A tensor the layer kind must not own: the shape call has to report 0.
static void check_absent(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t il,
                         int64_t tid) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = ds4_shape(sli(pp), kind, il, tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "tensor present (ok=" + std::to_string((long long) ok) + "), expected absent");
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();
    const auto rat = ratios();

    // 1. plan: layers 0 and 1 are raw attention (ratio 0)
    {
        std::vector<int64_t> pp = p, rr = rat, kinds(44, -1);
        const int64_t n = ds4_plan(sli(pp), sli(rr), sli(kinds));
        check_val("plan-len", n, 44);
        check_val("plan-layer0-raw", kinds[0], 0);
        check_val("plan-layer1-raw", kinds[1], 0);
    }
    // 2. plan: layer 7 is CSA (ratio 4)
    {
        std::vector<int64_t> pp = p, rr = rat, kinds(44, -1);
        ds4_plan(sli(pp), sli(rr), sli(kinds));
        check_val("plan-layer7-csa", kinds[7], 1);
        check_val("plan-layer15-csa", kinds[15], 1);
    }
    // 3. plan: layer 2 and 8 are HCA (ratio 128)
    {
        std::vector<int64_t> pp = p, rr = rat, kinds(44, -1);
        ds4_plan(sli(pp), sli(rr), sli(kinds));
        check_val("plan-layer2-hca", kinds[2], 2);
        check_val("plan-layer8-hca", kinds[8], 2);
    }
    // 4. plan: the layer past n_layer is the MTP block
    {
        std::vector<int64_t> pp = p, rr = rat, kinds(44, -1);
        ds4_plan(sli(pp), sli(rr), sli(kinds));
        check_val("plan-layer43-mtp", kinds[43], 3);
    }
    // 5. ratio kind: only 0, 4 and 128 are legal (deepseek4.cpp:179-180)
    check_val("ratio-0", ds4_ratio_kind(0), 0);
    check_val("ratio-4", ds4_ratio_kind(4), 1);
    check_val("ratio-128", ds4_ratio_kind(128), 2);
    check_val("ratio-16-rejected", ds4_ratio_kind(16), -1);

    // 6. every layer is MoE: no dense FFN exists in this architecture
    {
        std::vector<int64_t> pp = p;
        check_val("ffn-layer0-moe", ds4_ffn_kind(sli(pp), 0), 1);
        check_val("ffn-layer43-moe", ds4_ffn_kind(sli(pp), 43), 1);
        check_val("ffn-out-of-range", ds4_ffn_kind(sli(pp), 44), -1);
    }

    // 7. hash routing below hash_layer_count, score routing after
    {
        std::vector<int64_t> pp = p;
        check_val("route-layer0-hash", ds4_route_kind(sli(pp), 0), 0);
        check_val("route-layer1-hash", ds4_route_kind(sli(pp), 1), 0);
        check_val("route-layer2-score", ds4_route_kind(sli(pp), 2), 1);
        check_val("mtp-route-ok", ds4_mtp_route_ok(sli(pp), 43), 1);
        check_val("mtp-route-hash-rejected", ds4_mtp_route_ok(sli(pp), 1), 0);
    }

    // 8. SWA: set_swa_pattern(0) marks the trunk, the MTP override marks the rest
    check_val("swa-trunk", ds4_swa(0, 43, 0), 1);
    check_val("swa-mtp", ds4_swa(43, 43, 0), 1);
    check_val("swa-pattern4-layer1", ds4_swa(1, 43, 4), 1);
    check_val("swa-pattern4-layer4", ds4_swa(4, 43, 4), 0);

    // 9. MTP probe (deepseek4.cpp:15-21)
    check_val("nextn-present", ds4_nextn_count(1, 1), 1);
    check_val("nextn-probe-missing", ds4_nextn_count(1, 0), 0);
    check_val("nextn-not-declared", ds4_nextn_count(0, 1), 0);

    // 10. hyper-connection widths
    {
        std::vector<int64_t> pp = p;
        check_val("hc-out-width", ds4_out_width(4096, 4), 16384);
        check_val("hc-out-effective", ds4_out_width_effective(16384, 4096), 16384);
        check_val("hc-out-fallback", ds4_out_width_effective(0, 4096), 4096);
        check_val("hc-out-matches", ds4_out_matches(16384, 4096, 4), 1);
        check_val("hc-out-mismatch", ds4_out_matches(4096, 4096, 4), 0);
        check_val("hc-mix-dim", ds4_hc_mix_dim(4), 24);
        check_val("hc-mult4", ds4_hc_supported(4), 1);
        check_val("hc-mult8-rejected", ds4_hc_supported(8), 0);
        check_val("hc-head-fn-width", ds4_hc_head_fn(4096, 4), 16384);
        (void) pp;
    }

    // main tensor shapes; the HC/MLA block is checked explicitly
    // 11. token embedding and the HC head mixer
    check_shape("token-embd", p, 0, 0, 0, 4096, 129280, 1);
    check_shape("output-norm", p, 0, 0, 1, 4096, 1, 1);
    check_shape("hc-head-fn", p, 0, 0, 3, 16384, 4, 1);
    check_shape("hc-head-base", p, 0, 0, 4, 4, 1, 1);

    // 12. MLA query path and the single KV head
    check_shape("wq-a", p, 0, 0, 8, 4096, 1536, 1);
    check_shape("q-a-norm", p, 0, 0, 9, 1536, 1, 1);
    check_shape("wq-b", p, 0, 0, 10, 1536, 8192, 1);
    check_shape("wkv", p, 0, 0, 11, 4096, 128, 1);
    check_shape("kv-norm", p, 0, 0, 12, 128, 1, 1);

    // 13. grouped low-rank output
    check_shape("wo-a", p, 0, 0, 13, 1024, 512, 8);
    check_shape("wo-b", p, 0, 0, 14, 4096, 4096, 1);
    {
        check_val("o-groups-ok", ds4_o_groups_ok(64, 8), 1);
        check_val("o-groups-rejected", ds4_o_groups_ok(64, 5), 0);
        check_val("o-group-dim", ds4_o_group_dim(64, 8, 128), 1024);
        check_val("head-split-ok", ds4_head_split_ok(128, 64), 1);
        check_val("head-split-rejected", ds4_head_split_ok(64, 128), 0);
        check_val("sinks-width", ds4_sinks_width(64), 64);
    }
    check_shape("attn-sinks", p, 0, 0, 7, 64, 1, 1);

    // 14. hyper-connections per layer, twice (attn and ffn)
    check_shape("hc-attn-fn", p, 0, 0, 15, 16384, 24, 1);
    check_shape("hc-attn-base", p, 0, 0, 16, 24, 1, 1);
    check_shape("hc-attn-scale", p, 0, 0, 17, 3, 1, 1);
    check_shape("hc-ffn-fn", p, 0, 0, 18, 16384, 24, 1);
    check_shape("hc-ffn-base", p, 0, 0, 19, 24, 1, 1);
    check_shape("hc-ffn-scale", p, 0, 0, 20, 3, 1, 1);

    // 15. compressor on a CSA layer: overlapped, twice as wide
    check_shape("csa-comp-wkv", p, 1, 7, 32, 4096, 256, 1);
    check_shape("csa-comp-wgate", p, 1, 7, 33, 4096, 256, 1);
    check_shape("csa-comp-ape", p, 1, 7, 34, 256, 4, 1);
    check_shape("csa-comp-norm", p, 1, 7, 35, 128, 1, 1);

    // 16. compressor on an HCA layer: one block at a time
    check_shape("hca-comp-wkv", p, 2, 8, 32, 4096, 128, 1);
    check_shape("hca-comp-ape", p, 2, 8, 34, 128, 128, 1);

    // 17. no compressor on raw or MTP layers
    check_absent("raw-no-compressor", p, 0, 0, 32);
    check_absent("mtp-no-compressor", p, 3, 43, 32);

    // 18. lightning indexer on CSA layers only
    check_shape("idx-proj", p, 1, 7, 36, 4096, 64, 1);
    check_shape("idx-attn-q-b", p, 1, 7, 37, 1536, 8192, 1);
    check_shape("idx-comp-wkv", p, 1, 7, 38, 4096, 256, 1);
    check_shape("idx-comp-ape", p, 1, 7, 40, 256, 4, 1);
    check_shape("idx-comp-norm", p, 1, 7, 41, 128, 1, 1);
    check_absent("hca-no-indexer", p, 2, 8, 36);
    {
        check_val("idx-topk-clamped", ds4_indexer_top_k(3000, 2048), 2048);
        check_val("idx-topk-short", ds4_indexer_top_k(1000, 2048), 1000);
        check_val("gating-sqrtsoftplus", ds4_gating_ok(3), 1);
        check_val("gating-softmax-rejected", ds4_gating_ok(0), 0);
        check_val("clamp-shexp-fallback", ds4_clamp_shexp(0, 0, 7), 7);
        check_val("clamp-shexp-declared", ds4_clamp_shexp(1, 9, 7), 9);
        check_val("mtp-ratio-ok", ds4_mtp_ratio_ok(0), 1);
        check_val("mtp-ratio-rejected", ds4_mtp_ratio_ok(4), 0);
    }

    // 19. routing tensors: token-id table on hash layers, bias elsewhere
    check_shape("hash-tid2eid", p, 0, 0, 22, 8, 129280, 1);
    check_absent("score-layer-no-tid2eid", p, 0, 2, 22);
    check_shape("score-exp-probs-b", p, 2, 2, 23, 256, 1, 1);
    check_absent("hash-layer-no-probs-b", p, 0, 0, 23);

    // 20. routed and shared experts
    check_shape("router", p, 0, 0, 21, 4096, 256, 1);
    check_shape("ffn-gate-exps", p, 0, 0, 26, 4096, 2048, 256);
    check_shape("ffn-down-exps", p, 0, 0, 27, 2048, 4096, 256);
    check_shape("ffn-up-exps", p, 0, 0, 28, 4096, 2048, 256);
    check_shape("ffn-gate-shexp", p, 0, 0, 29, 4096, 2048, 1);
    check_shape("ffn-down-shexp", p, 0, 0, 30, 2048, 4096, 1);
    check_shape("ffn-up-shexp", p, 0, 0, 31, 4096, 2048, 1);

    // 21. MTP block
    check_shape("mtp-eh-proj", p, 3, 43, 42, 8192, 4096, 1);
    check_shape("mtp-enorm", p, 3, 43, 43, 4096, 1, 1);
    check_shape("mtp-hnorm", p, 3, 43, 44, 4096, 1, 1);
    check_shape("mtp-embed", p, 3, 43, 45, 4096, 129280, 1);
    check_shape("mtp-shared-head", p, 3, 43, 46, 4096, 129280, 1);
    check_shape("mtp-shared-head-norm", p, 3, 43, 47, 4096, 1, 1);
    check_absent("mtp-no-indexer", p, 3, 43, 36);

    printf("FG-ARCH9 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
