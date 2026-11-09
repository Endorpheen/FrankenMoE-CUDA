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
    tokens and all 128 top-5 probabilities equal, 2585.03 / 26.16

880 calls in one pass, 660 of them recurrent layers, and `native=0` -- the switch never fell back.
Gate: 5506 checks / 0 failures.

## Switch

`GGML_GOOSE_MODEL_GRAPH=0` restores the native `is_recr` body. Opt-out, so a new build has it on.

## Next

The same treatment for the rest of the builder: `build_layer_attn` vs `build_layer_attn_linear`
selection, the residual/crop ordering, and the MoE branch. Those need the header-level builder,
which means copying `models/qwen35moe.cpp` as EXP199 copied `llama-kv-cache.cpp`.
