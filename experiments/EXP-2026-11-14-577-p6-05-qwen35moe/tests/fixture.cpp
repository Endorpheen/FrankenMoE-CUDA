// Structure gate for the qwen35moe builder draft (P6-05 architecture 1).
//
// Expected values are the native loader's declarations, line by line in
// src/models/qwen35moe.cpp; the fixture calls the Goose plan/shape exports.
// No native library is linked: q35_shape/q35_kind are pure.
//
// Apex-like parameters: 40 trunk layers + 1 MTP block, 2048 embd, 16 heads,
// 4 KV heads, 256 experts with 8 used, GDN state 128, 4 key groups, 32 value
// heads, conv kernel 4.
//
// Two phases, each with its own summary line:
//   FG579_ARCH1      the original 10-case acceptance set, unchanged;
//   FG579_ARCH1_EXT  the mutation coverage set: the whole 41-layer plan, every
//                    GDN/MTP/output tensor, absent-tensor contracts, the
//                    native flags path and the declared-else-derived widths.
// The exit status combines both phases.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

#include "runtime-gen.h"

struct Counters {
    int cases = 0;
    int values = 0;
    int fails = 0;
};

static Counters *g = nullptr;

static void bad(const char * what, const std::string & detail) {
    ++g->fails;
    if (g->fails <= 100) {
        printf("FAIL %s: %s\n", what, detail.c_str());
    }
}

static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

// The imported model-graph slice declares these; it is not executed with a
// flags pointer here, but the symbols must resolve at link time. ld_u32 reads
// the per-layer marker array when the base is that array, as native does.
static uint32_t g_layer_flags[64];

extern "C" void fg200g_fail(int64_t code) {
    printf("FAIL shim: native abort %lld\n", (long long) code);
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    if (base == (uint64_t) (uintptr_t) g_layer_flags && index >= 0 && index < 64) {
        return (int64_t) g_layer_flags[index];
    }
    return 0;
}

static std::vector<int64_t> params() {
    std::vector<int64_t> p(19, 0);
    p[0]  = 2048;     // n_embd
    p[1]  = 151936;   // n_vocab
    p[2]  = 40;       // n_layer
    p[3]  = 41;       // n_layer_all
    p[4]  = 16;       // n_head
    p[5]  = 4;        // n_head_kv
    p[6]  = 8192;     // n_ff
    p[7]  = 256;      // n_expert
    p[8]  = 8;        // n_expert_used
    p[9]  = 0;        // n_ff_exp declared -> 8192/8
    p[10] = 0;        // n_ff_shexp declared -> n_ff
    p[11] = 4;        // ssm_d_conv
    p[12] = 4096;     // ssm_d_inner
    p[13] = 128;      // ssm_d_state
    p[14] = 32;       // ssm_dt_rank
    p[15] = 4;        // ssm_n_group
    p[16] = 128;      // n_embd_head_k
    p[17] = 4;        // full_attention_interval
    p[18] = 1;        // has output
    return p;
}

static void check_kind(const char * name, int64_t il, int64_t expected, const std::vector<int64_t> & p,
                       int64_t flags_base = 0) {
    ++g->cases;
    std::vector<int64_t> pp = p;
    const int64_t got = q35_kind(sli(pp), il, flags_base);
    ++g->values;
    if (got != expected) {
        bad(name, "kind got " + std::to_string((int) got) + " expected " + std::to_string((int) expected));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t tid,
                        int64_t a, int64_t b, int64_t c) {
    ++g->cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q35_shape(sli(pp), kind, tid, sli(out));
    g->values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((int) out[0]) + "," + std::to_string((int) out[1]) + "," +
                  std::to_string((int) out[2]) + "} ok=" + std::to_string((int) ok) +
                  " expected {" + std::to_string((int) a) + "," + std::to_string((int) b) + "," +
                  std::to_string((int) c) + "}");
    }
}

// A kind that does not own the tensor must report absence: q35_shape returns 0.
static void check_absent(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t tid) {
    ++g->cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q35_shape(sli(pp), kind, tid, sli(out));
    ++g->values;
    if (ok != 0) {
        bad(name, "shape reported present ok=" + std::to_string((int) ok) + " expected absent");
    }
}

// Phase 1: the original acceptance set. Expected line unchanged:
// FG579_ARCH1 cases=10 values=26 failures=0
static Counters run_core(const std::vector<int64_t> & p) {
    Counters c;
    g = &c;

    // 1. token_embd.weight, qwen35moe.cpp:43
    check_shape("token_embd", p, 0, 0, 2048, 151936, 1);
    // 2. output_norm.weight, qwen35moe.cpp:46
    check_shape("output_norm", p, 0, 1, 2048, 1, 1);
    // 3. layer 0 is recurrent: (0+1)%4 != 0, qwen35moe.cpp:19-21
    check_kind("layer0-recurrent", 0, 1, p);
    // 4. layer 3 is full attention: (3+1)%4 == 0
    check_kind("layer3-attention", 3, 0, p);
    // 5. recurrent attn_qkv.weight, qwen35moe.cpp:86
    check_shape("recurrent-wqkv", p, 1, 9, 2048, 5120, 1);
    // 6. recurrent ssm_conv1d.weight, qwen35moe.cpp:88
    check_shape("recurrent-conv1d", p, 1, 11, 4, 5120, 1);
    // 7. attention fused attn_qkv.weight, qwen35moe.cpp:74
    check_shape("attention-qkv", p, 0, 5, 2048, 5120, 1);
    // 8. attention attn_q_norm.weight, qwen35moe.cpp:78
    check_shape("attention-q-norm", p, 0, 7, 128, 1, 1);
    // 9. routed ffn_down_exps.weight, qwen35moe.cpp:96
    check_shape("ffn-down-exps", p, 0, 19, 1024, 2048, 256);
    // 10. MTP nextn.eh_proj.weight, qwen35moe.cpp:133
    check_shape("mtp-eh-proj", p, 2, 27, 4096, 2048, 1);

    printf("FG579_ARCH1 cases=%d values=%d failures=%d\n", c.cases, c.values, c.fails);
    return c;
}

// Phase 2: mutation coverage. Every value is still a native declaration; the
// anchors are the load_block_trunk/load_block_mtp bodies at qwen35moe.cpp:54-146.
static Counters run_ext(const std::vector<int64_t> & p) {
    Counters c;
    g = &c;

    // --- whole plan: 40 trunk layers, every 4th full attention, layer 40 MTP.
    {
        std::vector<int64_t> kinds(41, -1);
        std::vector<int64_t> pp = p;
        const int64_t n = q35_plan(sli(pp), 0, sli(kinds));
        ++g->cases;
        g->values += 41;
        int first_bad = -1;
        for (int i = 0; i < 41; ++i) {
            const int64_t want = (i == 40) ? 2 : (((i + 1) % 4 != 0) ? 1 : 0);
            if (kinds[i] != want && first_bad < 0) {
                first_bad = i;
            }
        }
        if (n != 41 || first_bad >= 0) {
            bad("plan-41", "n=" + std::to_string((int) n) + " first_bad_layer=" +
                               std::to_string(first_bad) + " got=" +
                               std::to_string(first_bad >= 0 ? (int) kinds[first_bad] : -99));
        }
    }
    // MTP block is the layer after the trunk; indexes outside the plan abort.
    check_kind("plan-mtp-layer", 40, 2, p);
    check_kind("plan-negative-layer", -1, -1, p);
    check_kind("plan-past-end", 41, -1, p);

    // --- native flags path: the per-layer u32 marker wins over the interval.
    memset(g_layer_flags, 0, sizeof(g_layer_flags));
    g_layer_flags[1] = 1;   // recurrent marker
    g_layer_flags[2] = 0;   // attention marker
    check_kind("flags-recurrent", 1, 1, p, (int64_t) (uintptr_t) g_layer_flags);
    check_kind("flags-attention", 2, 0, p, (int64_t) (uintptr_t) g_layer_flags);

    // --- trunk tensors shared by every kind, qwen35moe.cpp:44-46,70-71
    check_shape("output", p, 0, 2, 2048, 151936, 1);
    check_shape("attn-norm", p, 0, 3, 2048, 1, 1);
    check_shape("attn-post-norm", p, 0, 4, 2048, 1, 1);

    // --- full-attention trunk layer, qwen35moe.cpp:72-81
    check_shape("attention-out", p, 0, 6, 2048, 2048, 1);
    check_shape("attention-k-norm", p, 0, 8, 128, 1, 1);

    // --- routed and shared experts, qwen35moe.cpp:93-105
    check_shape("ffn-gate-inp", p, 0, 18, 2048, 256, 1);
    check_shape("ffn-gate-up-exps", p, 0, 20, 2048, 2048, 256);
    check_shape("ffn-gate-exps", p, 0, 21, 2048, 1024, 256);
    check_shape("ffn-up-exps", p, 0, 22, 2048, 1024, 256);
    check_shape("ffn-gate-inp-shexp", p, 0, 23, 2048, 1, 1);
    check_shape("ffn-gate-shexp", p, 0, 24, 2048, 8192, 1);
    check_shape("ffn-up-shexp", p, 0, 25, 2048, 8192, 1);
    check_shape("ffn-down-shexp", p, 0, 26, 8192, 2048, 1);

    // --- gated delta net branch, qwen35moe.cpp:83-91
    check_shape("recurrent-wqkv-gate", p, 1, 10, 2048, 4096, 1);
    check_shape("recurrent-ssm-dt", p, 1, 12, 32, 1, 1);
    check_shape("recurrent-ssm-a", p, 1, 13, 32, 1, 1);
    check_shape("recurrent-ssm-beta", p, 1, 14, 2048, 32, 1);
    check_shape("recurrent-ssm-alpha", p, 1, 15, 2048, 32, 1);
    check_shape("recurrent-ssm-norm", p, 1, 16, 128, 1, 1);
    check_shape("recurrent-ssm-out", p, 1, 17, 4096, 2048, 1);

    // --- MTP block: full attention plus NextN tensors, qwen35moe.cpp:108-139
    check_shape("mtp-attn-qkv", p, 2, 5, 2048, 5120, 1);
    check_shape("mtp-attn-out", p, 2, 6, 2048, 2048, 1);
    check_shape("mtp-q-norm", p, 2, 7, 128, 1, 1);
    check_shape("mtp-k-norm", p, 2, 8, 128, 1, 1);
    check_shape("mtp-enorm", p, 2, 28, 2048, 1, 1);
    check_shape("mtp-hnorm", p, 2, 29, 2048, 1, 1);
    check_shape("mtp-embed", p, 2, 30, 2048, 151936, 1);
    check_shape("mtp-head", p, 2, 31, 2048, 151936, 1);
    check_shape("mtp-head-norm", p, 2, 32, 2048, 1, 1);

    // --- ownership contracts: a kind that does not own the tensor reports 0.
    check_absent("attention-lacks-wqkv", p, 0, 9);
    check_absent("recurrent-lacks-attn-qkv", p, 1, 5);
    check_absent("attention-lacks-nextn", p, 0, 27);
    check_absent("recurrent-lacks-nextn", p, 1, 30);

    // --- declared-else-derived rule, qwen35moe.cpp:57-58.
    std::vector<int64_t> pd = p;
    pd[9] = 1536;    // n_ff_exp declared
    pd[10] = 1024;   // n_ff_shexp declared
    check_shape("declared-down-exps", pd, 0, 19, 1536, 2048, 256);
    check_shape("declared-gate-up", pd, 0, 20, 2048, 3072, 256);
    check_shape("declared-gate-shexp", pd, 0, 24, 2048, 1024, 1);
    check_shape("declared-down-shexp", pd, 0, 26, 1024, 2048, 1);

    printf("FG579_ARCH1_EXT cases=%d values=%d failures=%d\n", c.cases, c.values, c.fails);
    return c;
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();

    const Counters core = run_core(p);
    const Counters ext = run_ext(p);

    const int fails = core.fails + ext.fails;
    return fails == 0 ? 0 : 1;
}
