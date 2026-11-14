// Structure gate for the glm-dsa builder draft (P6-05 architecture 15).
//
// Expected values are the native loader's declarations in
// src/models/glm-dsa.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-arch15.h"

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
    std::vector<int64_t> p(22, 0);
    p[0]  = 5120;     // n_embd
    p[1]  = 154880;   // n_vocab
    p[2]  = 78;       // n_layer (glm-dsa.cpp:73-75)
    p[3]  = 1;        // n_layer_nextn
    p[4]  = 64;       // n_head
    p[5]  = 64;       // n_rot
    p[6]  = 1536;     // q_lora_rank
    p[7]  = 512;      // kv_lora_rank
    p[8]  = 192;      // n_embd_head_k_mla
    p[9]  = 128;      // n_embd_head_v_mla
    p[10] = 13312;    // n_ff (dense layers)
    p[11] = 1536;     // n_ff_exp
    p[12] = 256;      // n_expert
    p[13] = 8;        // n_expert_used
    p[14] = 2;        // n_expert_shared (chosen so the multiplier is visible)
    p[15] = 3;        // n_layer_dense_lead
    p[16] = 32;       // indexer_n_head
    p[17] = 128;      // indexer_head_size
    p[18] = 2048;     // indexer_top_k
    p[19] = 2;        // expert_gating_func (sigmoid)
    p[20] = 0;        // output absent -> duplicates token_embd
    p[21] = 1048576;  // n_ctx_train: the GLM 5.2 indexer pattern applies
    return p;
}

// is_indexer_full_impl, the GLM_5_2_DEFAULT_INDEXER_TYPES table of
// glm-dsa.cpp:6-27: full at 0 and 1, then every fourth layer from 2 on.
static std::vector<int64_t> types() {
    std::vector<int64_t> t(78, 0);
    for (int il = 0; il < 78; ++il) {
        if (il < 2 || il % 4 == 2) {
            t[il] = 1;
        }
    }
    return t;
}

// Derived: nope = 192-64 = 128, wq_b columns = 64*192 = 12288,
// wkv_a_mqa = 512+64 = 576, wo rows = 64*128 = 8192,
// shexp width = 1536*2 = 3072, indexer q columns = 32*128 = 4096.

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
    const int64_t ok = dsa_shape(sli(pp), kind, il, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," + std::to_string((long long) out[1]) +
                  "," + std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                  " expected {" + std::to_string((long long) a) + "," + std::to_string((long long) b) +
                  "," + std::to_string((long long) c) + "}");
    }
}

static void check_absent(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t il,
                         int64_t tid) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = dsa_shape(sli(pp), kind, il, tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "tensor present (ok=" + std::to_string((long long) ok) + "), expected absent");
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();
    const auto ty = types();

    // 1. plan: 78 trunk blocks plus one NextN block (glm-dsa.cpp:60-61, :125)
    {
        std::vector<int64_t> pp = p, kinds(79, -1);
        const int64_t n = dsa_plan(sli(pp), sli(kinds));
        check_val("plan-len", n, 79);
        check_val("plan-layer0-trunk", kinds[0], 0);
        check_val("plan-layer77-trunk", kinds[77], 0);
        check_val("plan-layer78-nextn", kinds[78], 1);
        check_val("n-layer-all", dsa_n_layer_all(sli(pp)), 79);
    }
    // 2. per-block kind, including the out-of-range answers
    {
        std::vector<int64_t> pp = p;
        check_val("kind-layer0", dsa_kind(sli(pp), 0), 0);
        check_val("kind-layer78", dsa_kind(sli(pp), 78), 1);
        check_val("kind-layer79-rejected", dsa_kind(sli(pp), 79), -1);
        check_val("kind-negative-rejected", dsa_kind(sli(pp), -1), -1);
    }
    // 3. NextN constraints: below block_count, and exactly one draft block
    {
        std::vector<int64_t> pp = p;
        check_val("nextn-ok", dsa_nextn_ok(sli(pp)), 1);
        check_val("nextn-single", dsa_nextn_single(sli(pp)), 1);
        std::vector<int64_t> pm = p;
        pm[3] = 2;
        check_val("nextn-two-rejected", dsa_nextn_single(sli(pm)), 0);
    }

    // 4. dense lead vs MoE is an index decision, NextN included (glm-dsa.cpp:155)
    {
        std::vector<int64_t> pp = p;
        check_val("ffn-layer0-dense", dsa_ffn_kind(sli(pp), 0), 0);
        check_val("ffn-layer2-dense", dsa_ffn_kind(sli(pp), 2), 0);
        check_val("ffn-layer3-moe", dsa_ffn_kind(sli(pp), 3), 1);
        check_val("ffn-layer77-moe", dsa_ffn_kind(sli(pp), 77), 1);
        check_val("ffn-layer78-moe", dsa_ffn_kind(sli(pp), 78), 1);
        check_val("ffn-out-of-range", dsa_ffn_kind(sli(pp), 79), -1);
        check_val("mtp-ffn-ok", dsa_mtp_ffn_ok(sli(pp)), 1);
        std::vector<int64_t> pd = p;
        pd[15] = 79;   // dense lead reaches the NextN block
        check_val("mtp-ffn-dense-lead-rejected", dsa_mtp_ffn_ok(sli(pd)), 0);
    }

    // 5. MLA requirement and the head split (glm-dsa.cpp:98-108)
    {
        std::vector<int64_t> pp = p;
        check_val("is-mla", dsa_is_mla(sli(pp)), 1);
        std::vector<int64_t> pz = p;
        pz[9] = 0;
        check_val("is-mla-half-declared", dsa_is_mla(sli(pz)), 0);
        check_val("mla-pair-ok", dsa_mla_pair_ok(sli(pp)), 1);
        check_val("mla-pair-half-declared", dsa_mla_pair_ok(sli(pz)), 0);
        std::vector<int64_t> pzz = p;
        pzz[8] = 0;
        pzz[9] = 0;
        check_val("mla-pair-both-zero", dsa_mla_pair_ok(sli(pzz)), 1);
        check_val("nope-width", dsa_nope_width(sli(pp)), 128);
        check_val("wo-width", dsa_wo_width(sli(pp)), 8192);
        check_val("wq-b-width", dsa_wq_b_width(sli(pp)), 12288);
    }

    // 6. DSA indexer types and the top-k reuse rule (:220, :364, :370)
    {
        std::vector<int64_t> tt = ty;
        check_val("indexer-default-small-ctx", dsa_indexer_default(131072), 1);
        check_val("indexer-default-1m-ctx", dsa_indexer_default(1048576), 0);
        check_val("idx52-layer0-full", dsa_indexer_5_2(0), 1);
        check_val("idx52-layer2-full", dsa_indexer_5_2(2), 1);
        check_val("idx52-layer3-shared", dsa_indexer_5_2(3), 0);
        check_val("idx52-layer6-full", dsa_indexer_5_2(6), 1);
        check_val("indexer-kind-layer0", dsa_indexer_kind(sli(tt), 78, 0), 1);
        check_val("indexer-kind-layer3", dsa_indexer_kind(sli(tt), 78, 3), 0);
        check_val("indexer-kind-layer78-rejected", dsa_indexer_kind(sli(tt), 78, 78), -1);
        check_val("indexer-ok-full", dsa_indexer_ok(sli(tt), 78, 0), 1);
        check_val("indexer-ok-shared-after-full", dsa_indexer_ok(sli(tt), 78, 3), 1);
        std::vector<int64_t> ts(78, 0);
        check_val("indexer-ok-first-shared", dsa_indexer_ok(sli(ts), 78, 0), 0);
        check_val("topk-clamped", dsa_indexer_top_k(3000, 2048), 2048);
        check_val("topk-short", dsa_indexer_top_k(1000, 2048), 1000);
        check_val("rope-in-indexer-ok", dsa_rope_in_indexer_ok(sli(tt)), 1);
        std::vector<int64_t> pr = p;
        pr[5] = 192;
        check_val("rope-in-indexer-rejected", dsa_rope_in_indexer_ok(sli(pr)), 0);
    }

    // 7. gating default, shared-expert width, MoE dimension checks (:53-57, :163-178)
    {
        std::vector<int64_t> pp = p;
        check_val("gating-none-becomes-sigmoid", dsa_gating_func(0), 2);
        check_val("gating-declared-kept", dsa_gating_func(1), 1);
        check_val("shexp-width", dsa_shexp_width(sli(pp)), 3072);
        check_val("moe-dims-ok", dsa_moe_dims_ok(sli(pp)), 1);
        std::vector<int64_t> pe = p;
        pe[13] = 0;
        check_val("moe-dims-usage-zero", dsa_moe_dims_ok(sli(pe)), 0);
        check_val("output-from-token-embd", dsa_output_dup(sli(pp)), 1);
        std::vector<int64_t> po = p;
        po[20] = 1;
        check_val("output-declared", dsa_output_dup(sli(po)), 0);
        check_val("nextn-eh-proj", dsa_nextn_eh_proj(sli(pp)), 10240);
    }

    // 8. global tensors (glm-dsa.cpp:115-123)
    check_shape("token-embd", p, 0, 0, 0, 5120, 154880, 1);
    check_shape("output-norm", p, 0, 0, 1, 5120, 1, 1);
    check_shape("output-dup-token-embd", p, 0, 0, 2, 5120, 154880, 1);

    // 9. MLA attention rows (glm-dsa.cpp:132-147)
    check_shape("attn-norm", p, 0, 0, 3, 5120, 1, 1);
    check_shape("attn-q-a-norm", p, 0, 0, 4, 1536, 1, 1);
    check_shape("attn-kv-a-norm", p, 0, 0, 5, 512, 1, 1);
    check_shape("wq-a", p, 0, 0, 6, 5120, 1536, 1);
    check_shape("wq-b", p, 0, 0, 7, 1536, 12288, 1);
    check_shape("wkv-a-mqa", p, 0, 0, 8, 5120, 576, 1);
    check_shape("wk-b", p, 0, 0, 9, 128, 512, 64);
    check_shape("wv-b", p, 0, 0, 10, 512, 128, 64);
    check_shape("attn-wo", p, 0, 0, 11, 8192, 5120, 1);

    // 10. DSA indexer rows, declared on dense lead layers too (:149-154)
    check_shape("indexer-k-norm", p, 0, 0, 13, 128, 1, 1);
    check_shape("indexer-k-norm-bias", p, 0, 0, 14, 128, 1, 1);
    check_shape("indexer-proj", p, 0, 0, 15, 5120, 32, 1);
    check_shape("indexer-attn-k", p, 0, 0, 16, 5120, 128, 1);
    check_shape("indexer-attn-q-b", p, 0, 0, 17, 1536, 4096, 1);
    check_shape("indexer-on-dense-lead", p, 0, 1, 15, 5120, 32, 1);

    // 11. dense lead FFN: SILU-PAR with three separate rows (:155-158)
    check_shape("dense-ffn-norm", p, 0, 0, 12, 5120, 1, 1);
    check_shape("dense-ffn-gate", p, 0, 0, 18, 5120, 13312, 1);
    check_shape("dense-ffn-up", p, 0, 0, 20, 5120, 13312, 1);
    check_shape("dense-ffn-down", p, 0, 2, 19, 13312, 5120, 1);
    check_absent("dense-no-router", p, 0, 0, 21);

    // 12. MoE + shared expert (:160-178)
    check_shape("router", p, 0, 3, 21, 5120, 256, 1);
    check_shape("exp-probs-b", p, 0, 3, 22, 256, 1, 1);
    check_shape("ffn-gate-exps", p, 0, 3, 23, 5120, 1536, 256);
    check_shape("ffn-down-exps", p, 0, 3, 24, 1536, 5120, 256);
    check_shape("ffn-up-exps", p, 0, 3, 25, 5120, 1536, 256);
    check_shape("ffn-gate-shexp", p, 0, 3, 26, 5120, 3072, 1);
    check_shape("ffn-down-shexp", p, 0, 3, 27, 3072, 5120, 1);
    check_shape("ffn-up-shexp", p, 0, 3, 28, 5120, 3072, 1);
    check_absent("moe-no-dense-gate", p, 0, 3, 18);

    // 13. NextN block: trunk rows plus the nextn wiring (:181-190).
    //     It is MoE because its index is past the dense lead.
    check_shape("nextn-attn-norm", p, 1, 78, 3, 5120, 1, 1);
    check_shape("nextn-router", p, 1, 78, 21, 5120, 256, 1);
    check_shape("nextn-eh-proj", p, 1, 78, 29, 10240, 5120, 1);
    check_shape("nextn-enorm", p, 1, 78, 30, 5120, 1, 1);
    check_shape("nextn-hnorm", p, 1, 78, 31, 5120, 1, 1);
    check_shape("nextn-embed", p, 1, 78, 32, 5120, 154880, 1);
    check_shape("nextn-shared-head", p, 1, 78, 33, 5120, 154880, 1);
    check_shape("nextn-shared-head-norm", p, 1, 78, 34, 5120, 1, 1);
    check_absent("trunk-no-eh-proj", p, 0, 0, 29);
    check_absent("trunk-no-shared-head", p, 0, 77, 33);

    printf("FG-ARCH15 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
