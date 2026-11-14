// Structure gate: gemma4 builder draft (P6-05 architecture 18).
//
// Expected values are the native loader's declarations in
// src/models/gemma4.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime-gen-g4.h"
#include "gate_common.h"

extern "C" void fg200g_fail(int64_t code) {
    printf("FAIL shim: native abort %lld\n", (long long) code);
    ++g_fails;
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
// The per-layer patterns are passed as raw arrays (is_swa_impl and the loader's
// gate_inp presence), the same way the base module reads is_recr_impl.
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    return (int64_t) ((const uint32_t *) (uintptr_t) base)[index];
}

// Shared fixture contract (tests/cases.md): 30 layers, SWA pattern 6, MoE from
// layer 4 on, 2048 hidden, GQA 8/2 heads, head 256 full / 128 SWA, 4 shared-KV
// layers at the end, PLE with 256 per-layer width, declared expert width 1024.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(25, 0);
    p[0]  = 2048;      // n_embd
    p[1]  = 262144;    // n_vocab
    p[2]  = 30;        // n_layer
    p[3]  = 30;        // n_layer_all
    p[4]  = 0;         // n_layer_nextn
    p[5]  = 8;         // n_head
    p[6]  = 2;         // n_head_kv
    p[7]  = 256;       // n_embd_head_k
    p[8]  = 256;       // n_embd_head_v
    p[9]  = 128;       // n_embd_head_k_swa
    p[10] = 128;       // n_embd_head_v_swa
    p[11] = 256;       // n_rot
    p[12] = 16384;     // n_ff
    p[13] = 128;       // n_expert
    p[14] = 8;         // n_expert_used
    p[15] = 1024;      // n_ff_exp (declared; no fallback in this architecture)
    p[16] = 0;         // n_ff_shexp (unused: the dense FFN is the shared expert)
    p[17] = 256;       // n_embd_per_layer (PLE on)
    p[18] = 4;         // n_kv_shared_layers
    p[19] = 0;         // n_embd_inp (unused)
    p[20] = 0;         // has separate output (optional -> duplicated)
    p[21] = 1065353216; // f_attention_scale = 1.0f bits
    p[22] = 0;         // attention temperature (unused)
    p[23] = 0;         // longrope (unused)
    p[24] = 0;         // final logit softcapping off
    return p;
}

// sliding window pattern: is_swa = il % 6 < 5 (llama-hparams.cpp:11).
static std::vector<uint32_t> swa_pattern(int64_t n) {
    std::vector<uint32_t> v((size_t) n, 0);
    for (int64_t i = 0; i < n; ++i) {
        v[(size_t) i] = (uint32_t) ((i % 6) < 5);
    }
    return v;
}

// loader-level router presence: the fixture models a dense lead of four layers
// followed by routed-expert layers (gemma4.cpp:101-102 is per-layer presence).
static std::vector<uint32_t> moe_pattern(int64_t n) {
    std::vector<uint32_t> v((size_t) n, 0);
    for (int64_t i = 0; i < n; ++i) {
        v[(size_t) i] = (uint32_t) (i >= 4);
    }
    return v;
}

static int64_t base_of(std::vector<uint32_t> & v) {
    return (int64_t) (uintptr_t) v.data();
}

int main() {
    goose_init(0, nullptr);
    std::vector<int64_t> p = params();
    std::vector<uint32_t> swa = swa_pattern(p[2]);
    std::vector<uint32_t> moe = moe_pattern(p[2]);
    const int64_t swa_b = base_of(swa);
    const int64_t moe_b = base_of(moe);

    // 1. plan: 30 layers, kinds = SWA/full x dense/MoE (gemma4.cpp:84, :264).
    {
        std::vector<int64_t> kinds(30, -1);
        std::vector<int64_t> exp = {
            1, 1, 1, 1, 3, 2,
            3, 3, 3, 3, 3, 2,
            3, 3, 3, 3, 3, 2,
            3, 3, 3, 3, 3, 2,
            3, 3, 3, 3, 3, 2,
        };
        const int64_t n = g4_plan(as_sli(p), swa_b, moe_b, as_sli(kinds));
        gate_plan("plan", n, 30, kinds, exp);
    }

    // 2. global tensors, gemma4.cpp:44-58
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 0, swa_b, moe_b, as_sli(out));
      gate_shape("token-embd", ok, out, 2048, 262144, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 1, swa_b, moe_b, as_sli(out));
      gate_shape("output", ok, out, 2048, 262144, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 2, swa_b, moe_b, as_sli(out));
      gate_shape("output-norm", ok, out, 2048, 1, 1); }

    // 3. PLE globals, gemma4.cpp:52-56: 256*30 rows
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 3, swa_b, moe_b, as_sli(out));
      gate_shape("per-layer-tok-embd", ok, out, 7680, 262144, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 4, swa_b, moe_b, as_sli(out));
      gate_shape("per-layer-model-proj", ok, out, 2048, 7680, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 5, swa_b, moe_b, as_sli(out));
      gate_shape("per-layer-proj-norm", ok, out, 256, 1, 1); }

    // 4. attention shapes follow the per-layer head width (llama-hparams.cpp:115)
    //    SWA layer 0: head 128 -> q 1024, k/v 256; full layer 5: head 256.
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 7, swa_b, moe_b, as_sli(out));
      gate_shape("attn-q-swa", ok, out, 2048, 1024, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 8, swa_b, moe_b, as_sli(out));
      gate_shape("attn-k-swa", ok, out, 2048, 256, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 5, 7, swa_b, moe_b, as_sli(out));
      gate_shape("attn-q-full", ok, out, 2048, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 5, 8, swa_b, moe_b, as_sli(out));
      gate_shape("attn-k-full", ok, out, 2048, 512, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 5, 9, swa_b, moe_b, as_sli(out));
      gate_shape("attn-v-full", ok, out, 2048, 512, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 10, swa_b, moe_b, as_sli(out));
      gate_shape("attn-wo-swa", ok, out, 1024, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 11, swa_b, moe_b, as_sli(out));
      gate_shape("attn-q-norm-swa", ok, out, 128, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 5, 11, swa_b, moe_b, as_sli(out));
      gate_shape("attn-q-norm-full", ok, out, 256, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 13, swa_b, moe_b, as_sli(out));
      gate_shape("attn-post-norm", ok, out, 2048, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 14, swa_b, moe_b, as_sli(out));
      gate_shape("layer-out-scale", ok, out, 1, 1, 1); }

    // 5. rope_freqs only on full layers, head/2 wide (gemma4.cpp:84-88)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 5, 15, swa_b, moe_b, as_sli(out));
      gate_shape("rope-freqs-full", ok, out, 128, 1, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("rope-freqs-swa",
          g4_shape(as_sli(p), 0, 15, swa_b, moe_b, as_sli(out))); }

    // 6. dense FFN on every layer (it is the shared expert on MoE layers)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 17, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-gate", ok, out, 2048, 16384, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 19, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-down", ok, out, 16384, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 20, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-post-norm", ok, out, 2048, 1, 1); }

    // 7. router and expert group: absent on the dense lead, present from layer 4
    { std::vector<int64_t> out(3, -1); gate_absent("gate-inp-dense",
          g4_shape(as_sli(p), 3, 21, swa_b, moe_b, as_sli(out))); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 21, swa_b, moe_b, as_sli(out));
      gate_shape("gate-inp-moe", ok, out, 2048, 128, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 22, swa_b, moe_b, as_sli(out));
      gate_shape("gate-inp-scale", ok, out, 2048, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 23, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-pre-norm-2", ok, out, 2048, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 24, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-post-norm-1", ok, out, 2048, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 25, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-post-norm-2", ok, out, 2048, 1, 1); }
    // merged gate_up doubles the declared expert width (gemma4.cpp:113)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 26, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-gate-up-exps", ok, out, 2048, 2048, 128); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 27, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-gate-exps", ok, out, 2048, 1024, 128); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 28, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-up-exps", ok, out, 2048, 1024, 128); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 4, 29, swa_b, moe_b, as_sli(out));
      gate_shape("ffn-down-exps", ok, out, 1024, 2048, 128); }
    { std::vector<int64_t> out(3, -1); gate_absent("exps-absent-on-dense",
          g4_shape(as_sli(p), 3, 29, swa_b, moe_b, as_sli(out))); }

    // 8. PLE per layer (gemma4.cpp:126-130)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 30, swa_b, moe_b, as_sli(out));
      gate_shape("per-layer-inp-gate", ok, out, 2048, 256, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 31, swa_b, moe_b, as_sli(out));
      gate_shape("per-layer-proj", ok, out, 256, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4_shape(as_sli(p), 0, 32, swa_b, moe_b, as_sli(out));
      gate_shape("per-layer-post-norm", ok, out, 2048, 1, 1); }

    // 9. decisions
    gate_value("has-kv-last-owned", g4_has_kv(as_sli(p), 25), 1);
    gate_value("has-kv-first-shared", g4_has_kv(as_sli(p), 26), 0);
    gate_value("head-dim-swa", g4_head_dim(as_sli(p), 0, swa_b), 128);
    gate_value("head-dim-full", g4_head_dim(as_sli(p), 5, swa_b), 256);
    gate_value("rope-need-swa", g4_rope_freqs_need(as_sli(p), 0, swa_b), 0);
    gate_value("rope-need-first-full", g4_rope_freqs_need(as_sli(p), 5, swa_b), 1);
    gate_value("rope-need-later-full", g4_rope_freqs_need(as_sli(p), 11, swa_b), 2);
    gate_value("has-expert-dense", g4_has_expert(as_sli(p), 3, moe_b), 0);
    gate_value("has-expert-moe", g4_has_expert(as_sli(p), 4, moe_b), 1);
    gate_value("ple-active", g4_ple_active(as_sli(p)), 1);
    gate_value("output-optional", g4_output_optional(), 1);
    gate_value("kq-scale-fixed", g4_kq_scale_auto(), 0);
    gate_value("softcap-off", g4_softcap_active(as_sli(p)), 0);
    gate_value("layer-kind-last", g4_layer_kind(as_sli(p), 29, swa_b, moe_b), 2);

    gate_report("FG-ARCH18");
    return g_fails == 0 ? 0 : 1;
}
