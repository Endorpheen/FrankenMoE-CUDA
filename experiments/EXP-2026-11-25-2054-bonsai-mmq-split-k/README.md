# EXP-2026-11-25-2054 — Split-K for long Bonsai FP16 PQ2 projections

**Rejected at the component performance gate.** Divide the quantized K
dimension into two or four contiguous groups of whole PQ2 blocks. Run
the EXP-2049 K64 tile over each group and combine their partial FP32
outputs in ascending part order. The MMA operands and packed GGUF
weights are unchanged. Reassociation changes FP32 rounding.

The RTX 4070 probe checks all outputs for four real shapes, including
an input containing an all-zero row and a joint QKV tail. Maximum
relative L2 vs the unsplit projection is `2.22e-5`, within the
component threshold of `3e-5`. This is a component comparison only.

## Diagnostic CUDA trace

The launch durations below include the shared activation pack plus
projection and, for the split cases, the separate reduction. One
diagnostic sample per path, in milliseconds.

| Matrix / tokens | EXP-2049 | Two parts | Four parts | Four-part scratch, MiB |
| --- | ---: | ---: | ---: | ---: |
| QKV tail / 129 | 0.598 | 0.609 | 0.620 | 20.15 |
| QKV / 1024 | 2.141 | 2.562 | 2.718 | 160 |
| FFN up / 1024 | 4.066 | 4.422 | 4.552 | 272 |
| FFN down / 1024 | 3.869 | 4.060 | 4.164 | 80 |

The preregistered component target was at least 10% time saved
on full-width FFN down. Both partition counts lose on all inputs.
The projection uses 72 registers and 27,648 B shared memory,
matching the control. The combine kernel uses 32 registers.
The trace includes extra global stores and reads, but does not
isolate memory versus dispatch or scheduling costs.

No model quality, long-run rate, process VRAM or RSS tests were
performed. The partial buffer sizes are specific component data,
not production process peaks. No model dispatch or shared Apex
and Inkling paths were changed.

## Reproduce

Use the accepted EXP-2049 source and the saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2054-bonsai-mmq-split-k
out=build/bonsai-mmq-split-k-2054
bash "$exp/source/build.sh" "$out" build/bonsai-flash-fused-padding-2049/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Output differences, kernel durations and scratch](data/component.json).
