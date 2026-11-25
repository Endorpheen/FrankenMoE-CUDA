# EXP-2026-11-25-2052 — Two Q8 activation scales per Bonsai PQ2 block

**Rejected for all-layer FFN gate/up on the quality gate.** Use two
independent signed Q8 scales per 128-value activation block. A single
integer matrix pass uses separate 64-value integer subtotals before
scale application. This retains the PQ2 weight format and requires no
residual matrix product. The standalone fixture replaces gate/up
projections for all 64 layers with the 128-token variant. The
EXP-2049 control retains the already accepted late-layer Q8 path.

## RTX 4070 component probe

Both 64- and 128-token tile variants pass the independent CPU sample
check with joint tails and an all-zero activation row. Relative L2
against FP16 falls from 0.645–0.646% for the one-scale baseline to
0.592–0.594%. One CUDA trace measures the pack and projection
duration in milliseconds. These are diagnostic samples.

| Projection | Tokens | FP16 | Q8 one scale | Two scales, N64 | Two scales, N128 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Layer 0 QKV, row tail | 129 | 0.600 | 0.289 | 0.333 | 0.433 |
| Layer 0 QKV | 1024 | 2.455 | 1.444 | 1.691 | 1.532 |
| Layer 0 FFN up | 1024 | 4.149 | 2.391 | 2.834 | 2.635 |
| Layer 0 FFN down | 1024 | 4.295 | 2.562 | 3.050 | 2.750 |

The wide candidate uses 121 registers, 28,928 shared bytes and
no spills. Its diagnostic full-tile times satisfy the preregistered
15% minimum saving versus FP16, and accuracy improves over one scale.
These timings do not establish end-to-end speedup.

## Independent model gate

The EXP-1994 code-trace prompt uses 440 raw input IDs, context 36,864,
ubatch 1024, Q4_0 K/V and greedy generation with normal EOG. Both
arms use the same locally built executable and CUDA image. Quality
requires no decrease in correct cells, maximum absolute full-logit
delta of 0.5 and top-10 overlap of at least 8 on every saved
same-history position.

- Independent score falls from 18/63 to 17/63.
- Control and candidate produce 258 and 264 IDs, respectively.
  The first 59 match.
- Across 11 saved full-vocabulary comparisons, all sampled top-1
  choices agree, the worst top-10 overlap is 9 and the largest
  absolute logit delta is 0.500011.
- At the actual divergence, replay on identical 59-token history
  picks ID 15 in the control and ID 11 in the candidate. The
  maximum absolute logit delta on that forced history is 0.389395.

The hard quality gates stop the all-layer candidate here. No
scored 32K/512 performance, process memory or cross-architecture
tests were performed. The fixture is opt-in and does not change
the HTTP baseline, model binaries or other model paths.

## Reproduce

Use the EXP-2049 build and the saved GGUF inventory. Set
`FRANKENMOE_CHECKPOINTS` to the checkpoint directory.

```bash
exp=experiments/EXP-2026-11-25-2052-bonsai-mmq-q8-two-scales
out=build/bonsai-q8-two-scales-2052
base=build/bonsai-flash-fused-padding-2049
fixtures="$FRANKENMOE_CHECKPOINTS/bonsai-integration"
bash "$exp/source/build.sh" "$out" "$base/pipeline.cl"
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$fixtures" "$fixtures/inventory.json" "$out/pipeline.cubin"
bash "$exp/source/build-quality.sh" "$out" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/quality.py" \
  "$out/code-trace" "$out/quality-fixture" /path/to/bonsai.gguf \
  "$fixtures/inventory.json" "$fixtures/vocabulary.gguf" /path/to/llama-tokenize code-trace
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/replay.py" \
  "$out/quality-fixture" /path/to/bonsai.gguf "$out/code-trace/code-trace"
```

[Component trace](data/component.json), [quality](data/quality.json),
and [first divergence](data/history-59.json).
