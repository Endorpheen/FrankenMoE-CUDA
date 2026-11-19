# Maxwell C++ ledger and bridge-removal plan

Policy: AGENTS.md rule 31 (C++ must die). Every commit reports C++ lines added,
deleted, and what moved under Goose control.

## Current C++ (one new runtime file)

`vendor/flint/src/models/maxwell.cpp`: 442 lines, 103 `ggml_*` calls,
35 `create_tensor` calls.

| part | lines (approx.) | role |
|---:|---|---|
| hparams and tensor map | 120 | loader wiring (substrate) |
| mHC pre/comb/post/head graph | 150 | ggml wiring around fused DSV4 HC ops |
| attention / MoE / dense FFN / head | 170 | existing builders and ggml ops |

Plus additive edits: `llama-arch.{h,cpp}` (+10), `llama-model.cpp` (+3 here,
factory/rope/MSA branch), `llama-context.cpp` (+1), `models.h` (+15),
`llama-hparams.h` (0 after the ABI fix - settings are a static in maxwell.cpp).

No C++ was deleted in this migration yet: deletion waits for proven replacement
and consumer checks (rule 15).

## What is already Goose

`experiments/EXP-2026-11-17-926-maxwell-v1-flash-goose/source/runtime.goose`
owns the decisions: block-route geometry and eligibility, per-group block-max
selection, prefix/remote/local final route and route bias, the mHC Sinkhorn
projection, hyper-connection gates, the sigmoid router with expert bias, the
learned HC-head weights, MTP index rolling. Gate: 2768 checks, 14/14 mutations
killed. This is the part that must not drift back into C++.

## Bridge-removal plan (ordered)

1. Move the graph builder to Goose following `goose/model/qwen35moe.goose`
   (Goose owns the graph decisions; ggml calls stay host-side through the
   existing ABI). Target: `goose/model/maxwell.goose`, with the same numeric
   gate in F32 (max_abs 1.74e-4) and the same 45-layer load check.
2. Switch the runtime to call the Goose builder; keep `maxwell.cpp` only until
   the goose-built trunk produces identical logits on the synthetic model and
   on the real GGUF.
3. Delete `maxwell.cpp` and the corresponding factory branch in one pass; verify
   clean build, no dangling references, and re-run the dense numeric gate plus
   the 45-layer load and a short real-weight generation.
4. Never delete before steps 1-2 pass; never keep the C++ copy "just in case".

After step 3 the Maxwell architecture contributes zero C++ lines; the loading
and graph operations live in Goose, and the substrate (ggml kernels, CUDA
library) remains the universal runtime, not Maxwell-specific code.
