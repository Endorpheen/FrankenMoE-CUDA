# EXP200: Apex model-graph decisions in Goose (APX: model runtime)

First step into the model runtime, kept separate from EXP199's KV work. Goose answers the
per-layer decisions the qwen35moe builder makes, while the graph construction itself stays native.

## Ported

| Goose | native | behaviour |
|---|---|---|
| `is_recr` | `llama-hparams.cpp:260` | is layer `il` a recurrent (gated delta net) layer, from `is_recr_impl` |
| `crop_before_nextn` | `llama-graph.h:1086` | `out_ids && (!emb_nextn \|\| emb_masked)` |
| `crop_after_nextn` | `llama-graph.h:1091` | `out_ids && emb_nextn && !emb_masked` |
| `capture_layer_input` | `models/qwen35moe.cpp:182` | record the layer input during the main pass |
| `shift_skip` | KV shift gate | a zero shift does not build the shift graph |
| `gating_kind` | `build_moe_ffn` 2227-2249 | which of the four gating functions applies |
| `group_select` | line 2283 | `n_expert_groups > 1 && n_tokens > 0` |
| `weight_before_ffn` | line 2214 | only llama4 weights before the FFN |
| `weights_need_scale` | line 2345 | `w_scale != 0 && w_scale != 1`, decided on the float bit pattern |
| `kq_scale_auto` | `qwen35moe.cpp:344` | `f_attention_scale == 0` means compute `1/sqrt(n_embd_head)` |
| `q_full_width` | line 288 | the QG projection width is twice the head width |
| `gate_elem_offset` | line 311 | the gate is the second half of the QG row |
| `head_v_dim` | `qwen35moe.cpp:365` | `d_inner / num_v_heads` |
| `conv_channels` | line 400 | `d_inner + 2*n_group*d_state` |
| `qkv_dim` | line 425 | `head_k_dim*num_k_heads*2 + head_v_dim*num_v_heads` |
| `k_view_offset` / `v_view_offset` | lines 447/453 | where the K and V views start in the row |

`is_recr` is the one integrated into the library: it is called once per layer per pass and decides
which attention mechanism the graph builds.

## A defect the gate could not catch and the model could

The first integrated version read `is_recr_impl` as a packed bitset. It is
`std::array<uint32_t, LLAMA_MAX_LAYERS>` -- one 4-byte element per layer. The gate passed because
it fed the same packed layout, so both sides agreed on the wrong contract.

The real Apex run refused to start:

    llama_model_load: error loading model: check_tensor_dims:
      tensor 'blk.1.attn_qkv.weight' has wrong shape; expected 2048, 9216, got 2048, 8192

Layer 1 was classified as non-recurrent when it is recurrent, so the loader expected a different
attention shape. Reading the element as `uint32_t` fixed it, and the gate was corrected to feed an
array rather than a bitset -- the gate had been mirroring my mistake.

That is worth stating plainly: a differential gate only proves both sides agree. When the same
author writes both, a shared misunderstanding passes it, and only a real model load catches it.

## Evidence

    FG200G_MODEL_GRAPH is_recr=880 recurrent=660 native=0
    FG200G_MOE group_select=760 scale_check=760
    FG200G_RECR geometry=1710
    tokens and all 128 top-5 probabilities equal, 2570.97 / 26.24

880 calls in one pass, 660 of them recurrent layers, and `native=0` -- the switch never fell back.
Gate: 5506 checks / 0 failures.

## Two corrections the live run forced

1. `group_select` asserted `n_expert_groups >= 1`; native has no such bound, and the model reports
   0 for the grouped-selection count. The abort took the server down at startup. Relaxed to `>= 0`.
2. `weights_need_scale` compares floats, so the bit pattern is passed in and compared as integers;
   a first attempt wrote the conversion inline as a lambda inside the condition, which was
   unreadable and unsafe. It is now a plain `memcpy` before the test.

## Integration reach

`is_recr` needed `llama-hparams.cpp`, the MoE decisions needed `llama-graph.cpp`, and the
attention decisions live inside `src/models/qwen35moe.cpp`, which the build compiles inside
`unity_8_cxx.cxx` together with eleven other model files. The integration therefore copies the
unity source and redirects one include; nothing in the reference tree is touched. The copy needs
`-I src/models` because its quoted `#include "models.h"` resolves relative to the original
directory.

## Why the recurrent geometry matters

`head_v_dim` is an integer division and `qkv_dim`/`conv_channels` are products that drive view
shapes. An error there does not fail loudly: it produces a view of the wrong extent and the state
corrupts over time. 1710 calls in one pass, `native=0`.

## Switch

`GGML_GOOSE_MODEL_GRAPH=0` restores the native `is_recr` body. Opt-out, so a new build has it on.

## Next

The same treatment for the rest of the builder: `build_layer_attn` vs `build_layer_attn_linear`
selection, the residual/crop ordering, and the MoE branch. Those need the header-level builder,
which means copying `models/qwen35moe.cpp` as EXP199 copied `llama-kv-cache.cpp`.
