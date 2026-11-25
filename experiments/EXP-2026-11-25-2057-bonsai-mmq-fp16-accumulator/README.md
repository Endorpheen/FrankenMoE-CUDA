# EXP-2026-11-25-2057 — Bonsai FP16 MMA partial accumulation

**Rejected for broad cached-MMQ dispatch on the independent quality gate.**
Keep the existing FP16 inputs, decoded PQ2 weights and tile geometry.
Accumulate 32 or 64 consecutive K values using FP16 Tensor Core
accumulators. Widen each partial to FP32 and add it to the persistent
FP32 accumulator. The K32 variant has two partial sums per K64 stage.
The operands and model layout are unchanged. Rounding is different.

## Diagnostic RTX 4070 component trace

The four fixtures use actual weight matrices, seeded activations and
a zero row. Values are finite. The table includes common activation
packing plus the projection, in milliseconds. One diagnostic
sample per combination is shown.

| Projection | Tokens | FP32 accumulation | K32 partials | K64 partials |
| --- | ---: | ---: | ---: | ---: |
| QKV, row and token tail | 129 | 0.596 | 0.494 | 0.508 |
| QKV | 1024 | 2.161 | 1.782 | 1.812 |
| FFN up | 1024 | 4.152 | 3.004 | 3.185 |
| FFN down | 1024 | 3.843 | 3.159 | 3.200 |

Relative L2 versus FP32 accumulation is about 0.033% for K32
and 0.044% for K64. The K32 kernel satisfies the preregistered
component threshold of 10% time saved on full FFN down.
K32/K64 use 94/92 registers per thread, the same 27,648 B shared
memory and no spills. These are isolated projection results.

## Targeted model gate

The standalone test uses the cumulative EXP-2049 Bonsai paths,
the EXP-1994 code-trace fixture (440 raw input IDs), context 36,864,
ubatch 1024, Q4_0 K/V, greedy and normal EOG. The experimental
switch replaces the cached PQ2 FP16 operand/FP32 accumulator path
with the K32 partial variant. The already accepted selective
INT8 dispatch is unchanged. For short wide batches, the candidate
uses its 128-token tile in place of the control's 64-token tile.
Only K32 was tested at model level.

- Independent score falls from 18/63 to 13/63 correct cells.
- The two output streams contain 258 and 264 IDs, with a matching
  59-token prefix.
- All 11 sampled full-vocabulary rows on identical forced history
  agree on top-1, with at least 9 of 10 common top-10 IDs.
  Maximum sampled absolute logit delta is 0.4660.
- At the first free-generation difference, replay with the same
  59 previous IDs changes the winner from 15 to 22. Maximum
  absolute logit delta is 0.3186 at that step.

The reused harness names the experimental arm `int8` in its
outputs. Here the changed path uses FP16 accumulation.
The five-cell quality loss stops broad dispatch before 32K/512.
The component gain justifies retaining the prototype for a new
hypothesis about numerical sensitivity, not repeating this broad
configuration. No model throughput, process VRAM or RSS claim was
made. EXP-2049 and other model artifacts are unchanged.

## Reproduce

Use the EXP-2049 snapshot and set `FRANKENMOE_CHECKPOINTS` to the saved
build tree.

```bash
exp=experiments/EXP-2026-11-25-2057-bonsai-mmq-fp16-accumulator
out=build/bonsai-fp16-acc-2057
base=build/bonsai-flash-fused-padding-2049
fixtures="$FRANKENMOE_CHECKPOINTS/bonsai-integration"
bash "$exp/source/build.sh" "$out" "$base/pipeline.cl"
bash "$exp/source/build-quality.sh" "$out" "$base"
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$fixtures" "$fixtures/inventory.json" "$out/pipeline.cubin"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/quality.py" \
  "$out/code-trace" "$out/quality-fixture" /path/to/bonsai.gguf \
  "$fixtures/inventory.json" "$fixtures/vocabulary.gguf" /path/to/llama-tokenize code-trace
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/replay.py" \
  "$out/quality-fixture" /path/to/bonsai.gguf "$out/code-trace/code-trace"
```

[Component trace](data/component.json), [quality](data/quality.json),
and [forced first divergence](data/history-59.json).
