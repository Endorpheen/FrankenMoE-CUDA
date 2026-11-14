// Structure gate for the qwen3next builder draft (P6-05 architecture 5).
//
// Expected values are the native loader's declarations and the native graph's
// view geometry in src/models/qwen3next.cpp. No native library is linked.

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

// The recurrent-flag path reads one 4-byte element per layer
// (llama-hparams.cpp:260, std::array<uint32_t, LLAMA_MAX_LAYERS>), so the shim
// must serve a real array instead of a constant zero.
static uint32_t g_flags[64] = { 0 };

extern "C" void fg200g_fail(int64_t code) {
    printf("FAIL shim: native abort %lld\n", (long long) code);
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base;
    if (index < 0 || index >= (int64_t) (sizeof(g_flags) / sizeof(g_flags[0]))) {
        return 0;
    }
    return (int64_t) g_flags[index];
}

// One 21-slot parameter layout, see tests/cases.md.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(20, 0);
    p[0]  = 2048;     // n_embd
    p[1]  = 151936;   // n_vocab
    p[2]  = 48;       // n_layer
    p[3]  = 49;       // n_layer_all
    p[4]  = 16;       // n_head
    p[5]  = 2;        // n_head_kv
    p[6]  = 128;      // n_embd_head_k
    p[7]  = 8192;     // n_ff
    p[8]  = 512;      // n_expert
    p[9]  = 10;       // n_expert_used
    p[10] = 0;        // n_ff_exp declared
    p[11] = 0;        // n_ff_shexp declared
    p[12] = 4;        // ssm_d_conv
    p[13] = 4096;     // ssm_d_inner
    p[14] = 128;      // ssm_d_state
    p[15] = 32;       // ssm_dt_rank
    p[16] = 16;       // ssm_n_group
    p[17] = 4;        // full_attention_interval
    p[18] = 1;        // has_output
    p[19] = 0;        // expert gating func (softmax)
    return p;
}

// Expected plan of 49 blocks: recurrent on il % 4 != 3, full attention on
// il % 4 == 3 inside the trunk, MTP on the single extra layer.
static std::vector<int64_t> expected_plan() {
    std::vector<int64_t> k(49, 1);
    for (int il = 3; il < 48; il += 4) {
        k[il] = 0;
    }
    k[48] = 2;
    return k;
}

static void gate_value(const char * name, int64_t got, int64_t expected) {
    ++g_cases;
    ++g_values;
    if (got != expected) {
        bad(name, "got " + std::to_string((long long) got) + " expected " +
                      std::to_string((long long) expected));
    }
}

static void gate_plan(const std::vector<int64_t> & p, const std::vector<int64_t> & expected) {
    ++g_cases;
    g_values += (int) expected.size();
    std::vector<int64_t> pp = p;
    std::vector<int64_t> kinds(expected.size(), -1);
    const int64_t n = q3n_plan(sli(pp), 0, sli(kinds));
    if (n != (int64_t) expected.size()) {
        bad("plan", "n_layer got " + std::to_string((long long) n) + " expected " +
                    std::to_string((long long) expected.size()));
        return;
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (kinds[i] != expected[i]) {
            bad("plan", "kind[" + std::to_string(i) + "] got " +
                        std::to_string((long long) kinds[i]) + " expected " +
                        std::to_string((long long) expected[i]));
            return;
        }
    }
}

static void gate_kind(const char * name, const std::vector<int64_t> & p, int64_t il,
                      int64_t flags_base, int64_t expected) {
    ++g_cases;
    ++g_values;
    std::vector<int64_t> pp = p;
    const int64_t got = q3n_kind(sli(pp), il, flags_base);
    if (got != expected) {
        bad(name, "kind(" + std::to_string((long long) il) + ", flags=" +
                      std::to_string((long long) flags_base) + ") got " +
                      std::to_string((long long) got) + " expected " +
                      std::to_string((long long) expected));
    }
}

static void gate_shape(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t tid,
                       int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q3n_shape(sli(pp), kind, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," +
                      std::to_string((long long) out[1]) + "," +
                      std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                      " expected {" + std::to_string((long long) a) + "," +
                      std::to_string((long long) b) + "," + std::to_string((long long) c) + "}");
    }
}

static void gate_absent(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t tid) {
    ++g_cases;
    ++g_values;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q3n_shape(sli(pp), kind, tid, sli(out));
    if (ok != 0) {
        bad(name, "kind " + std::to_string((long long) kind) + " tid " +
                      std::to_string((long long) tid) + " should be absent, got ok=" +
                      std::to_string((long long) ok));
    }
}

static void gate_graph_dims(const std::vector<int64_t> & p, int64_t hv, int64_t cc, int64_t qd) {
    ++g_cases;
    g_values += 3;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q3n_gdn_graph_dims(sli(pp), sli(out));
    if (ok != 1 || out[0] != hv || out[1] != cc || out[2] != qd) {
        bad("gdn-graph-dims", "ok=" + std::to_string((long long) ok) + " got {" +
                      std::to_string((long long) out[0]) + "," +
                      std::to_string((long long) out[1]) + "," +
                      std::to_string((long long) out[2]) + "} expected {" +
                      std::to_string((long long) hv) + "," + std::to_string((long long) cc) + "," +
                      std::to_string((long long) qd) + "}");
    }
}

int main() {
    goose_init(0, nullptr);
    auto p = params();

    // 1. plan: 36 recurrent, 12 attention, 1 MTP (qwen3next.cpp:17-23, :126-128)
    gate_plan(p, expected_plan());

    // 2-8. kind boundaries (qwen3next.cpp:17-23, :135-136, :163)
    gate_kind("kind-layer0-recurrent", p, 0, 0, 1);
    gate_kind("kind-layer3-attention", p, 3, 0, 0);
    gate_kind("kind-layer46-recurrent", p, 46, 0, 1);
    gate_kind("kind-layer47-attention", p, 47, 0, 0);
    gate_kind("kind-layer48-mtp", p, 48, 0, 2);
    gate_kind("kind-below-zero", p, -1, 0, -1);
    gate_kind("kind-past-n-layer-all", p, 49, 0, -1);

    // 9-10. the per-layer array wins over the interval (qwen3next.cpp:17, :163):
    // a layer the interval calls recurrent is attention here and vice versa.
    g_flags[0] = 0;
    g_flags[3] = 1;
    gate_kind("flags-layer0-attention", p, 0, 1, 0);
    gate_kind("flags-layer3-recurrent", p, 3, 1, 1);

    // 11-15. loader policy (qwen3next.cpp:46-51, :72-73, :80-81, :126-128, :570)
    gate_value("output-optional", q3n_output_optional(), 1);
    gate_value("qk-norm-required", q3n_qk_norm_required(), 1);
    gate_value("has-post-norm", q3n_has_post_norm(), 1);
    gate_value("mtp-blocks", q3n_mtp_blocks(sli(p)), 1);
    gate_value("gating-softmax", q3n_gating_kind(sli(p)), 0);

    // 16. GDN view geometry of the graph (qwen3next.cpp:398-404, :457, :474)
    gate_graph_dims(p, 128, 8192, 8192);

    // 17-20. global tensors (qwen3next.cpp:42-51)
    gate_shape("token_embd", p, 0, 0, 2048, 151936, 1);
    gate_shape("output_norm", p, 0, 1, 2048, 1, 1);
    gate_shape("output", p, 0, 2, 2048, 151936, 1);
    gate_shape("attn_norm", p, 0, 3, 2048, 1, 1);
    gate_shape("attn_post_norm", p, 1, 4, 2048, 1, 1);

    // 21-25. attention layer (qwen3next.cpp:77-81): joint Q+gate, GQA K/V
    gate_shape("attn-qkv-fused", p, 0, 5, 2048, 4608, 1);
    gate_shape("attn-wo", p, 0, 6, 2048, 2048, 1);
    gate_shape("attn-q-norm", p, 0, 7, 128, 1, 1);
    gate_shape("attn-k-norm", p, 0, 8, 128, 1, 1);
    gate_shape("recurrent-attn-norm", p, 1, 3, 2048, 1, 1);

    // 26-34. gated delta net layer (qwen3next.cpp:86-94)
    gate_shape("ssm-in-qkvz", p, 1, 9, 2048, 12288, 1);
    gate_shape("wqkv", p, 1, 10, 2048, 8192, 1);
    gate_shape("wqkv-gate", p, 1, 11, 2048, 4096, 1);
    gate_shape("ssm-conv1d", p, 1, 12, 4, 8192, 1);
    gate_shape("ssm-dt", p, 1, 13, 32, 1, 1);
    gate_shape("ssm-a", p, 1, 14, 32, 1, 1);
    gate_shape("ssm-beta-alpha", p, 1, 15, 2048, 64, 1);
    gate_shape("ssm-norm", p, 1, 16, 128, 1, 1);
    gate_shape("ssm-out", p, 1, 17, 4096, 2048, 1);

    // 35-42. routed MoE (qwen3next.cpp:97-99): n_ff_exp = 8192/10 = 819
    gate_shape("ffn-gate-inp", p, 0, 18, 2048, 512, 1);
    gate_shape("ffn-down-exps", p, 0, 19, 819, 2048, 512);
    gate_shape("ffn-gate-up-exps", p, 0, 20, 2048, 1638, 512);
    gate_shape("ffn-gate-exps", p, 0, 21, 2048, 819, 512);
    gate_shape("ffn-up-exps", p, 1, 22, 2048, 819, 512);
    gate_shape("ffn-gate-inp-shexp", p, 0, 23, 2048, 1, 1);
    gate_shape("ffn-gate-shexp", p, 0, 24, 2048, 8192, 1);
    gate_shape("ffn-down-shexp", p, 1, 26, 8192, 2048, 1);

    // 43-48. MTP block (qwen3next.cpp:115-120)
    gate_shape("mtp-eh-proj", p, 2, 27, 4096, 2048, 1);
    gate_shape("mtp-enorm", p, 2, 28, 2048, 1, 1);
    gate_shape("mtp-hnorm", p, 2, 29, 2048, 1, 1);
    gate_shape("mtp-embed-tokens", p, 2, 30, 2048, 151936, 1);
    gate_shape("mtp-shared-head-head", p, 2, 31, 2048, 151936, 1);
    gate_shape("mtp-shared-head-norm", p, 2, 32, 2048, 1, 1);

    // 49-52. the kinds own disjoint tensor sets
    gate_absent("absent-wqkv-on-attention", p, 0, 10);
    gate_absent("absent-attn-qkv-on-recurrent", p, 1, 5);
    gate_absent("absent-qk-norm-on-recurrent", p, 1, 7);
    gate_absent("absent-nextn-on-attention", p, 0, 27);

    printf("FG-ARCH5 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
