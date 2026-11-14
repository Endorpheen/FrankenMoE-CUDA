// Structure gate: mistral3 builder draft (P6-05 architecture 20).
//
// Expected values are the native loader's declarations in
// src/models/mistral3.cpp. No native library is linked. Two parameter sets are
// exercised: a routed-expert file with a shared expert, and a dense file with
// longrope factors.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime-gen-m3.h"
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

// Shared fixture contract (tests/cases.md): 40 layers, 5120 hidden, GQA 40/8
// heads of 128, n_ff 14336. Set A is a MoE file with a 2048-wide shared
// expert; set B is the dense file with no router and longrope factors.
static std::vector<int64_t> params_moe() {
    std::vector<int64_t> p(25, 0);
    p[0]  = 5120;      // n_embd
    p[1]  = 131072;    // n_vocab
    p[2]  = 40;        // n_layer
    p[3]  = 40;        // n_layer_all
    p[4]  = 0;         // n_layer_nextn
    p[5]  = 40;        // n_head
    p[6]  = 8;         // n_head_kv
    p[7]  = 128;       // n_embd_head_k
    p[8]  = 128;       // n_embd_head_v
    p[9]  = 0;         // n_embd_head_k_swa (unused: no SWA in mistral3)
    p[10] = 0;         // n_embd_head_v_swa (unused)
    p[11] = 128;       // n_rot
    p[12] = 14336;     // n_ff
    p[13] = 128;       // n_expert
    p[14] = 8;         // n_expert_used
    p[15] = 0;         // n_ff_exp (unused: no fallback in this architecture)
    p[16] = 2048;      // n_ff_shexp
    p[17] = 0;         // n_embd_per_layer (unused)
    p[18] = 0;         // n_kv_shared_layers (unused)
    p[19] = 0;         // n_embd_inp (unused)
    p[20] = 0;         // has separate output (optional -> duplicated)
    p[21] = 0;         // f_attention_scale = 0.0f -> 1/sqrt(head)
    p[22] = 0;         // attention temperature off
    p[23] = 0;         // longrope off
    p[24] = 0;         // softcapping (unused)
    return p;
}

static std::vector<int64_t> params_dense() {
    std::vector<int64_t> p = params_moe();
    p[13] = 0;          // n_expert -> dense FFN branch
    p[14] = 0;
    p[16] = 0;          // no shared expert outside the MoE branch
    p[20] = 1;          // output present
    p[21] = 1065353216; // f_attention_scale = 1.0f
    p[23] = 1;          // rope_scaling_type == LONGROPE
    return p;
}

int main() {
    goose_init(0, nullptr);
    std::vector<int64_t> pm = params_moe();
    std::vector<int64_t> pd = params_dense();

    // 1. plan, set A: every layer is MoE because n_expert != 0 (mistral3.cpp:64).
    {
        std::vector<int64_t> kinds(40, -1);
        const int64_t n = m3_plan(as_sli(pm), as_sli(kinds));
        gate_plan("plan-moe", n, 40, kinds, std::vector<int64_t>(40, 1));
    }
    // 2. plan, set B: every layer is dense (mistral3.cpp:65-67).
    {
        std::vector<int64_t> kinds(40, -1);
        const int64_t n = m3_plan(as_sli(pd), as_sli(kinds));
        gate_plan("plan-dense", n, 40, kinds, std::vector<int64_t>(40, 0));
    }

    // 3. globals (mistral3.cpp:32-41)
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 0, as_sli(out));
      gate_shape("token-embd", ok, out, 5120, 131072, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 1, as_sli(out));
      gate_shape("output-norm", ok, out, 5120, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 2, as_sli(out));
      gate_shape("output-duplicated", ok, out, 5120, 131072, 1); }

    // 4. GQA attention: q 5120, kv 1024, fused 7168 (mistral3.cpp:48-52)
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 3, as_sli(out));
      gate_shape("attn-norm", ok, out, 5120, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 4, as_sli(out));
      gate_shape("attn-qkv-fused", ok, out, 5120, 7168, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 5, as_sli(out));
      gate_shape("attn-q", ok, out, 5120, 5120, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 6, as_sli(out));
      gate_shape("attn-k", ok, out, 5120, 1024, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 7, as_sli(out));
      gate_shape("attn-v", ok, out, 5120, 1024, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 8, as_sli(out));
      gate_shape("attn-wo", ok, out, 5120, 5120, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 9, as_sli(out));
      gate_shape("attn-wo-bias", ok, out, 5120, 1, 1); }

    // 5. rope group, set A: single freq tensor, n_rot/2 wide (mistral3.cpp:61)
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 11, as_sli(out));
      gate_shape("rope-freqs", ok, out, 64, 1, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("rope-long-absent",
          m3_shape(as_sli(pm), 0, 12, as_sli(out))); }
    // set B: longrope pair instead (mistral3.cpp:56-59)
    { std::vector<int64_t> out(3, -1); gate_absent("rope-freqs-absent-longrope",
          m3_shape(as_sli(pd), 0, 11, as_sli(out))); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 12, as_sli(out));
      gate_shape("rope-long", ok, out, 64, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 13, as_sli(out));
      gate_shape("rope-short", ok, out, 64, 1, 1); }

    // 6. MoE branch: expert width is n_ff itself, with the shared expert
    //    beside it (mistral3.cpp:74-84)
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 20, as_sli(out));
      gate_shape("ffn-gate-inp", ok, out, 5120, 128, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 21, as_sli(out));
      gate_shape("ffn-gate-exps", ok, out, 5120, 14336, 128); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 22, as_sli(out));
      gate_shape("ffn-down-exps", ok, out, 14336, 5120, 128); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 23, as_sli(out));
      gate_shape("ffn-up-exps", ok, out, 5120, 14336, 128); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 24, as_sli(out));
      gate_shape("ffn-gate-shexp", ok, out, 5120, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 25, as_sli(out));
      gate_shape("ffn-up-shexp", ok, out, 5120, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 0, 26, as_sli(out));
      gate_shape("ffn-down-shexp", ok, out, 2048, 5120, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("dense-ffn-absent-in-moe",
          m3_shape(as_sli(pm), 0, 14, as_sli(out))); }

    // 7. dense branch: gate/up/down with their optional biases
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 10, as_sli(out));
      gate_shape("ffn-norm-dense", ok, out, 5120, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 14, as_sli(out));
      gate_shape("ffn-gate-dense", ok, out, 5120, 14336, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 15, as_sli(out));
      gate_shape("ffn-down-dense", ok, out, 14336, 5120, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 16, as_sli(out));
      gate_shape("ffn-up-dense", ok, out, 5120, 14336, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 17, as_sli(out));
      gate_shape("ffn-gate-bias", ok, out, 14336, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 18, as_sli(out));
      gate_shape("ffn-down-bias", ok, out, 5120, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pd), 0, 19, as_sli(out));
      gate_shape("ffn-up-bias", ok, out, 14336, 1, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("moe-absent-in-dense",
          m3_shape(as_sli(pd), 0, 20, as_sli(out))); }
    { std::vector<int64_t> out(3, -1); gate_absent("shexp-absent-in-dense",
          m3_shape(as_sli(pd), 0, 24, as_sli(out))); }

    // 8. layer bound
    { std::vector<int64_t> out(3, -1); int64_t ok = m3_shape(as_sli(pm), 39, 5, as_sli(out));
      gate_shape("attn-q-last-layer", ok, out, 5120, 5120, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("shape-past-last-layer",
          m3_shape(as_sli(pm), 40, 5, as_sli(out))); }

    // 9. decisions
    gate_value("kind-moe", m3_layer_kind(as_sli(pm), 0), 1);
    gate_value("kind-dense", m3_layer_kind(as_sli(pd), 0), 0);
    gate_value("kind-past-last", m3_layer_kind(as_sli(pm), 40), -1);
    gate_value("kq-auto-when-zero", m3_kq_scale_auto(as_sli(pm)), 1);
    gate_value("kq-fixed-when-set", m3_kq_scale_auto(as_sli(pd)), 0);
    gate_value("fused-qkv-optional", m3_fused_qkv_optional(), 1);
    gate_value("output-optional", m3_output_optional(), 1);
    gate_value("shexp-present", m3_has_shexp(as_sli(pm)), 1);
    gate_value("shexp-absent", m3_has_shexp(as_sli(pd)), 0);
    gate_value("attn-temp-off", m3_attn_temperature(as_sli(pm)), 0);
    gate_value("rope-kind-freqs", m3_rope_tensor_kind(as_sli(pm)), 0);
    gate_value("rope-kind-longrope", m3_rope_tensor_kind(as_sli(pd)), 1);
    {   std::vector<int64_t> pt = params_dense();
        pt[22] = 1;
        gate_value("attn-temp-on", m3_attn_temperature(as_sli(pt)), 1); }

    gate_report("FG-ARCH20");
    return g_fails == 0 ? 0 : 1;
}
