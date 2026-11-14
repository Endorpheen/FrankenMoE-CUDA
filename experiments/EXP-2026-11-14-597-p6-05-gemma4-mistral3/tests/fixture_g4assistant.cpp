// Structure gate: gemma4-assistant builder draft (P6-05 architecture 19).
//
// Expected values are the native loader's declarations in
// src/models/gemma4-assistant.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "runtime-gen-g4a.h"
#include "gate_common.h"

extern "C" void fg200g_fail(int64_t code) {
    printf("FAIL shim: native abort %lld\n", (long long) code);
    ++g_fails;
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    return (int64_t) ((const uint32_t *) (uintptr_t) base)[index];
}

// Shared fixture contract (tests/cases.md): six draft layers (all nextn), SWA
// pattern 6 so layer 0 is SWA and layer 5 is full, 2048 draft hidden, 4096
// target hidden, GQA heads 8 with head 256 full / 128 SWA.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(25, 0);
    p[0]  = 2048;      // n_embd
    p[1]  = 262144;    // n_vocab
    p[2]  = 6;         // n_layer (effective)
    p[3]  = 6;         // n_layer_all
    p[4]  = 6;         // n_layer_nextn
    p[5]  = 8;         // n_head
    p[6]  = 8;         // n_head_kv (no KV projections in this architecture)
    p[7]  = 256;       // n_embd_head_k
    p[8]  = 256;       // n_embd_head_v
    p[9]  = 128;       // n_embd_head_k_swa
    p[10] = 128;       // n_embd_head_v_swa
    p[11] = 256;       // n_rot
    p[12] = 8192;      // n_ff
    p[13] = 0;         // n_expert (unused: the draft is dense)
    p[14] = 0;         // n_expert_used
    p[15] = 0;         // n_ff_exp (unused)
    p[16] = 0;         // n_ff_shexp (unused)
    p[17] = 0;         // n_embd_per_layer (unused: no PLE in the draft)
    p[18] = 0;         // n_kv_shared_layers (unused)
    p[19] = 4096;      // n_embd_inp = target hidden size
    p[20] = 1;         // output present (duplicated from token_embd in native)
    p[21] = 1065353216; // f_attention_scale = 1.0f bits
    p[22] = 0;         // attention temperature (unused)
    p[23] = 0;         // longrope (unused)
    p[24] = 0;         // final logit softcapping (unused)
    return p;
}

int main() {
    goose_init(0, nullptr);
    std::vector<int64_t> p = params();
    std::vector<uint32_t> swa((size_t) p[2], 0);
    for (int64_t i = 0; i < p[2]; ++i) {
        swa[(size_t) i] = (uint32_t) ((i % 6) < 5);
    }
    const int64_t swa_b = (int64_t) (uintptr_t) swa.data();

    // 1. plan: six nextn blocks (gemma4-assistant.cpp:50, :130).
    {
        std::vector<int64_t> kinds(6, -1);
        const int64_t n = g4a_plan(as_sli(p), as_sli(kinds));
        gate_plan("plan", n, 6, kinds, std::vector<int64_t>(6, 0));
    }

    // 2. globals (gemma4-assistant.cpp:37-46)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 0, swa_b, as_sli(out));
      gate_shape("token-embd", ok, out, 2048, 262144, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 1, swa_b, as_sli(out));
      gate_shape("output-duplicated", ok, out, 2048, 262144, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 2, swa_b, as_sli(out));
      gate_shape("output-norm", ok, out, 2048, 1, 1); }
    // optional masked-embedding metadata; native shape is {} (0-dim), the gate
    // contract reports it as {1,1,1}.
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 3, swa_b, as_sli(out));
      gate_shape("masked-embd-centroids", ok, out, 1, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 4, swa_b, as_sli(out));
      gate_shape("masked-embd-ordering", ok, out, 1, 1, 1); }

    // 3. the two cross-width projections (gemma4-assistant.cpp:46, :58)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 6, swa_b, as_sli(out));
      gate_shape("nextn-proj-post", ok, out, 2048, 4096, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 5, swa_b, as_sli(out));
      gate_shape("nextn-proj-pre", ok, out, 8192, 2048, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("nextn-proj-pre-layer1",
          g4a_shape(as_sli(p), 1, 5, swa_b, as_sli(out))); }

    // 4. per-layer attention follows the per-layer head width (llama-hparams.cpp:115)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 8, swa_b, as_sli(out));
      gate_shape("attn-q-swa", ok, out, 2048, 1024, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 5, 8, swa_b, as_sli(out));
      gate_shape("attn-q-full", ok, out, 2048, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 9, swa_b, as_sli(out));
      gate_shape("attn-wo-swa", ok, out, 1024, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 10, swa_b, as_sli(out));
      gate_shape("attn-q-norm-swa", ok, out, 128, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 5, 10, swa_b, as_sli(out));
      gate_shape("attn-q-norm-full", ok, out, 256, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 11, swa_b, as_sli(out));
      gate_shape("attn-post-norm", ok, out, 2048, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 12, swa_b, as_sli(out));
      gate_shape("layer-out-scale", ok, out, 1, 1, 1); }

    // 5. rope_freqs on full layers only (gemma4-assistant.cpp:70-73)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 5, 13, swa_b, as_sli(out));
      gate_shape("rope-freqs-full", ok, out, 128, 1, 1); }
    { std::vector<int64_t> out(3, -1); gate_absent("rope-freqs-swa",
          g4a_shape(as_sli(p), 0, 13, swa_b, as_sli(out))); }

    // 6. dense FFN (gemma4-assistant.cpp:75-79)
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 14, swa_b, as_sli(out));
      gate_shape("ffn-norm", ok, out, 2048, 1, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 15, swa_b, as_sli(out));
      gate_shape("ffn-gate", ok, out, 2048, 8192, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 16, swa_b, as_sli(out));
      gate_shape("ffn-up", ok, out, 2048, 8192, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 17, swa_b, as_sli(out));
      gate_shape("ffn-down", ok, out, 8192, 2048, 1); }
    { std::vector<int64_t> out(3, -1); int64_t ok = g4a_shape(as_sli(p), 0, 18, swa_b, as_sli(out));
      gate_shape("ffn-post-norm", ok, out, 2048, 1, 1); }

    // 7. no KV projection anywhere, no tensor past the draft
    { std::vector<int64_t> out(3, -1); gate_absent("no-kv-shape-past-draft",
          g4a_shape(as_sli(p), 6, 8, swa_b, as_sli(out))); }

    // 8. decisions
    gate_value("nextn-equals-all", g4a_nextn_matches_all(as_sli(p)), 1);
    gate_value("hidden-widths-differ", g4a_hidden_ok(as_sli(p)), 1);
    gate_value("no-kv", g4a_has_kv(), 0);
    gate_value("out-scale-required", g4a_out_scale_required(), 1);
    gate_value("kq-scale-fixed", g4a_kq_scale_auto(), 0);
    gate_value("head-dim-swa", g4a_head_dim(as_sli(p), 0, swa_b), 128);
    gate_value("head-dim-full", g4a_head_dim(as_sli(p), 5, swa_b), 256);
    gate_value("rope-need-swa", g4a_rope_freqs_need(as_sli(p), 0, swa_b), 0);
    gate_value("rope-need-first-full", g4a_rope_freqs_need(as_sli(p), 5, swa_b), 1);
    gate_value("kind-last", g4a_layer_kind(as_sli(p), 5), 0);
    gate_value("kind-past-draft", g4a_layer_kind(as_sli(p), 6), -1);

    gate_report("FG-ARCH19");
    return g_fails == 0 ? 0 : 1;
}
