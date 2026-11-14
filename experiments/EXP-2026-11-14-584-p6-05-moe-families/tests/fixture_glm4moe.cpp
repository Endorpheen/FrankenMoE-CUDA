// Structure gate: glm4-moe builder draft (P6-05 architecture 13).
//
// Expected values are the native loader's declarations in
// src/models/glm4-moe.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime-gen-g4m.h"
#include "gate_common.h"

extern "C" void fg200g_fail(int64_t code) {
    printf("FAIL shim: native abort %lld\n", (long long) code);
    ++g_fails;
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base; (void) index;
    return 0;
}

// Shared fixture contract (tests/cases.md).
// GLM-4.5-Air geometry: n_embd 4096, 32 q heads, 2 kv heads, head 128,
// n_ff 11264, 128 experts, top-8, routed width derived 1408, two shared
// experts (shexp 2816), one dense lead block, one MTP block, mrope sections.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(21, 0);
    p[0]  = 4096;     // n_embd
    p[1]  = 151552;   // n_vocab
    p[2]  = 46;       // n_layer (trunk)
    p[3]  = 47;       // n_layer_all (trunk + MTP)
    p[4]  = 1;        // n_layer_nextn
    p[5]  = 32;       // n_head
    p[6]  = 2;        // n_head_kv
    p[7]  = 128;      // n_embd_head_k
    p[8]  = 128;      // n_embd_head_v
    p[9]  = 11264;    // n_ff
    p[10] = 128;      // n_expert
    p[11] = 8;        // n_expert_used
    p[12] = 0;        // n_ff_exp declared -> 11264/8 = 1408
    p[13] = 0;        // n_ff_shexp declared (unused)
    p[14] = 2;        // n_expert_shared -> shexp 1408*2 = 2816
    p[15] = 1;        // n_layer_dense_lead
    p[16] = 1;        // has output
    p[17] = 1;        // q/k norm present
    p[18] = 1;        // gating func: sigmoid
    p[19] = 1;        // mrope sections present
    p[20] = 128;      // n_rot
    return p;
}

int main() {
    goose_init(0, nullptr);
    std::vector<int64_t> p = params();

    // 1. plan: layer 0 dense, 1..45 routed MoE, 46 MTP, glm4-moe.cpp:49-51,
    //    :70. Also the mrope decision (:6, :298).
    std::vector<int64_t> kinds(47, -1);
    const int64_t plan_n = g4m_plan(as_sli(p), as_sli(kinds));
    std::vector<int64_t> expected(47, 1);
    expected[0] = 0;
    expected[46] = 2;
    gate_plan("plan", plan_n, 47, kinds, expected);
    gate_value("kind-out-of-range", g4m_kind(as_sli(p), 47), -1);
    gate_value("use-mrope", g4m_use_mrope(as_sli(p)), 1);
    // q/k norm are optional (GLM-4.5 355B omits them); there is no ffn_norm.
    { std::vector<int64_t> out(3, -1);
      std::vector<int64_t> p_noqk = p; p_noqk[17] = 0;
      gate_value("qk-norm-absent", g4m_shape(as_sli(p_noqk), 1, 9, as_sli(out)), 0);
      gate_value("no-ffn-norm", g4m_shape(as_sli(p), 1, 12, as_sli(out)), 0); }

    // 2. token_embd.weight, glm4-moe.cpp:39
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 0, as_sli(out));
      gate_shape("token-embd", ok, out, 4096, 151552, 1); }
    // 3. routed MoE fused attn_qkv: q 4096 + k 256 + v 256, glm4-moe.cpp:56
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 4, as_sli(out));
      gate_shape("moe-attn-qkv", ok, out, 4096, 4608, 1); }
    // 4. attn_post_norm replaces ffn_norm, glm4-moe.cpp:66, :369
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 11, as_sli(out));
      gate_shape("attn-post-norm", ok, out, 4096, 1, 1); }
    // 5. ffn_exp_probs_b, glm4-moe.cpp:76
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 14, as_sli(out));
      gate_shape("ffn-exp-probs-b", ok, out, 128, 1, 1); }
    // 6. ffn_gate_inp, glm4-moe.cpp:74-75
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 13, as_sli(out));
      gate_shape("ffn-gate-inp", ok, out, 4096, 128, 1); }
    // 7. ffn_gate_exps, glm4-moe.cpp:81-82
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 15, as_sli(out));
      gate_shape("ffn-gate-exps", ok, out, 4096, 1408, 128); }
    // 8. ffn_gate_shexp: n_ff_exp * n_expert_shared, glm4-moe.cpp:90-92
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 1, 19, as_sli(out));
      gate_shape("ffn-gate-shexp", ok, out, 4096, 2816, 1); }
    // 9. dense lead ffn_down, glm4-moe.cpp:101
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 0, 23, as_sli(out));
      gate_shape("dense-ffn-down", ok, out, 11264, 4096, 1); }
    // 10. MTP nextn.eh_proj: 2*n_embd rows, glm4-moe.cpp:107
    { std::vector<int64_t> out(3, -1); int64_t ok = g4m_shape(as_sli(p), 2, 25, as_sli(out));
      gate_shape("mtp-eh-proj", ok, out, 8192, 4096, 1); }

    gate_report("FG-ARCH13");
    return g_fails == 0 ? 0 : 1;
}
