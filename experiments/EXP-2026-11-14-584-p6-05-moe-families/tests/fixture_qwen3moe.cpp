// Structure gate: qwen3moe builder draft (P6-05 architecture 4).
//
// Expected values are the native loader's declarations in
// src/models/qwen3moe.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime-gen-q3m.h"
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
// P_N_EMBD 2048, vocab 151936, n_layer 48 == n_layer_all, n_head 32,
// n_head_kv 4, head 128, n_ff 6144, experts 128, used 8, n_ff_exp derived,
// output optional, q/k norm present, softmax gating.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(21, 0);
    p[0]  = 2048;     // n_embd
    p[1]  = 151936;   // n_vocab
    p[2]  = 48;       // n_layer
    p[3]  = 48;       // n_layer_all
    p[4]  = 0;        // n_layer_nextn
    p[5]  = 32;       // n_head
    p[6]  = 4;        // n_head_kv
    p[7]  = 128;      // n_embd_head_k
    p[8]  = 128;      // n_embd_head_v
    p[9]  = 6144;     // n_ff
    p[10] = 128;      // n_expert
    p[11] = 8;        // n_expert_used
    p[12] = 0;        // n_ff_exp declared -> 6144/8 = 768
    p[13] = 0;        // n_ff_shexp (unused)
    p[14] = 0;        // n_expert_shared (unused)
    p[15] = 0;        // n_layer_dense_lead (unused)
    p[16] = 1;        // has output
    p[17] = 1;        // q/k norm present
    p[18] = 0;        // gating func (unused: softmax)
    p[19] = 0;        // mrope (unused)
    p[20] = 128;      // n_rot
    return p;
}

int main() {
    goose_init(0, nullptr);
    std::vector<int64_t> p = params();

    // 1. plan: 48 routed-expert attention layers, qwen3moe.cpp:27-55.
    std::vector<int64_t> kinds(48, -1);
    const int64_t plan_n = q3m_plan(as_sli(p), as_sli(kinds));
    gate_plan("plan", plan_n, 48, kinds, std::vector<int64_t>(48, 0));
    gate_value("output-optional", q3m_output_optional(), 1);
    // qwen3moe has no shared expert and no post-attention norm.
    { std::vector<int64_t> out(3, -1);
      gate_value("no-shared-expert", q3m_shape(as_sli(p), 19, as_sli(out)), 0);
      gate_value("no-post-attn-norm", q3m_shape(as_sli(p), 11, as_sli(out)), 0); }

    // 2. token_embd.weight, qwen3moe.cpp:17
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 0, as_sli(out));
      gate_shape("token-embd", ok, out, 2048, 151936, 1); }
    // 3. output_norm.weight, qwen3moe.cpp:20
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 2, as_sli(out));
      gate_shape("output-norm", ok, out, 2048, 1, 1); }
    // 4. fused attn_qkv: q 4096 + k 512 + v 512, qwen3moe.cpp:32
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 4, as_sli(out));
      gate_shape("attn-qkv", ok, out, 2048, 5120, 1); }
    // 5. attn_wo: q width x n_embd, qwen3moe.cpp:33
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 8, as_sli(out));
      gate_shape("attn-wo", ok, out, 4096, 2048, 1); }
    // 6. attn_q_norm: head width, qwen3moe.cpp:36
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 9, as_sli(out));
      gate_shape("attn-q-norm", ok, out, 128, 1, 1); }
    // 7. ffn_gate_inp: n_embd x n_expert, qwen3moe.cpp:40
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 13, as_sli(out));
      gate_shape("ffn-gate-inp", ok, out, 2048, 128, 1); }
    // 8. ffn_gate_exps: n_embd x n_ff_exp x n_expert, qwen3moe.cpp:52
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 15, as_sli(out));
      gate_shape("ffn-gate-exps", ok, out, 2048, 768, 128); }
    // 9. ffn_down_exps: n_ff_exp x n_embd x n_expert, qwen3moe.cpp:53
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 16, as_sli(out));
      gate_shape("ffn-down-exps", ok, out, 768, 2048, 128); }
    // 10. ffn_up_exps: n_embd x n_ff_exp x n_expert, qwen3moe.cpp:54
    { std::vector<int64_t> out(3, -1); int64_t ok = q3m_shape(as_sli(p), 17, as_sli(out));
      gate_shape("ffn-up-exps", ok, out, 2048, 768, 128); }

    gate_report("FG-ARCH4");
    return g_fails == 0 ? 0 : 1;
}
