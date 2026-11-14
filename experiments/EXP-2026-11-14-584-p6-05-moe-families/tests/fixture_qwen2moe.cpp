// Structure gate: qwen2moe builder draft (P6-05 architecture 7).
//
// Expected values are the native loader's declarations in
// src/models/qwen2moe.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime-gen-q2m.h"
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
// Qwen2-57B-A14B geometry: n_embd 3584, 28 q heads, 4 kv heads, head 128,
// n_ff 18944, 64 experts, top-8, routed width derived 2368, shared width 2048.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(21, 0);
    p[0]  = 3584;     // n_embd
    p[1]  = 151936;   // n_vocab
    p[2]  = 28;       // n_layer
    p[3]  = 28;       // n_layer_all
    p[4]  = 0;        // n_layer_nextn
    p[5]  = 28;       // n_head
    p[6]  = 4;        // n_head_kv
    p[7]  = 128;      // n_embd_head_k
    p[8]  = 128;      // n_embd_head_v
    p[9]  = 18944;    // n_ff
    p[10] = 64;       // n_expert
    p[11] = 8;        // n_expert_used
    p[12] = 0;        // n_ff_exp declared -> 18944/8 = 2368
    p[13] = 2048;     // n_ff_shexp declared
    p[14] = 0;        // n_expert_shared (unused)
    p[15] = 0;        // n_layer_dense_lead (unused)
    p[16] = 1;        // has output (mandatory)
    p[17] = 0;        // q/k norm absent
    p[18] = 0;        // gating func (softmax)
    p[19] = 0;        // mrope (unused)
    p[20] = 128;      // n_rot
    return p;
}

int main() {
    goose_init(0, nullptr);
    std::vector<int64_t> p = params();

    // 1. plan: 28 routed+shared-expert attention layers, qwen2moe.cpp:25-58.
    std::vector<int64_t> kinds(28, -1);
    const int64_t plan_n = q2m_plan(as_sli(p), as_sli(kinds));
    gate_plan("plan", plan_n, 28, kinds, std::vector<int64_t>(28, 0));
    gate_value("output-optional", q2m_output_optional(), 0);
    // qwen2moe has no q/k norm and no post-attention norm.
    { std::vector<int64_t> out(3, -1);
      gate_value("no-qk-norm", q2m_shape(as_sli(p), 9, as_sli(out)), 0);
      gate_value("no-post-attn-norm", q2m_shape(as_sli(p), 11, as_sli(out)), 0); }

    // 2. token_embd.weight, qwen2moe.cpp:19
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 0, as_sli(out));
      gate_shape("token-embd", ok, out, 3584, 151936, 1); }
    // 3. output_norm.weight, qwen2moe.cpp:22
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 2, as_sli(out));
      gate_shape("output-norm", ok, out, 3584, 1, 1); }
    // 4. fused attn_qkv: q 3584 + k 512 + v 512, qwen2moe.cpp:30
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 4, as_sli(out));
      gate_shape("attn-qkv", ok, out, 3584, 4608, 1); }
    // 5. attn_k: GQA width, qwen2moe.cpp:30
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 6, as_sli(out));
      gate_shape("attn-k", ok, out, 3584, 512, 1); }
    // 6. attn_wo: square, qwen2moe.cpp:31
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 8, as_sli(out));
      gate_shape("attn-wo", ok, out, 3584, 3584, 1); }
    // 7. ffn_gate_inp, qwen2moe.cpp:35
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 13, as_sli(out));
      gate_shape("ffn-gate-inp", ok, out, 3584, 64, 1); }
    // 8. ffn_gate_exps, qwen2moe.cpp:47
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 15, as_sli(out));
      gate_shape("ffn-gate-exps", ok, out, 3584, 2368, 64); }
    // 9. ffn_down_exps, qwen2moe.cpp:48
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 16, as_sli(out));
      gate_shape("ffn-down-exps", ok, out, 2368, 3584, 64); }
    // 10. ffn_down_shexp: declared shexp width, qwen2moe.cpp:56
    { std::vector<int64_t> out(3, -1); int64_t ok = q2m_shape(as_sli(p), 20, as_sli(out));
      gate_shape("ffn-down-shexp", ok, out, 2048, 3584, 1); }

    gate_report("FG-ARCH7");
    return g_fails == 0 ? 0 : 1;
}
