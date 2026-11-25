# EXP-2026-11-25-2068 — Warp row reuse on cumulative Bonsai full tiles

**Rejected at the component performance gate.** Port EXP-2061's
warp row layout to the EXP-2067 full-tile FP16 partial path.
Each warp covers two 16-row groups and 32 tokens, reusing an
activation fragment across the groups. The control covers one
16-row group and 64 tokens. K32 partial boundaries and the
per-output accumulation order are unchanged.

All four real full-matrix fixtures match bitwise. Every width
uses 27,648 B shared memory. The control has 80 registers,
while the candidate has 95, with no spills in either arm.
CUDA's calculated maximum active blocks per SM drops from
three to two. The trace does not measure realized occupancy.

| RTX 4070 diagnostic, including pack | EXP-2067, ms | Warp reuse, ms |
| --- | ---: | ---: |
| QKV, width 5120 / 1024 | 1.622 | 1.798 |
| SSM output, width 6144 / 1024 | 1.012 | 1.121 |
| FFN up, width 5120 / 1024 | 2.739 | 3.083 |
| FFN down, width 17408 / 1024 | 2.908 | 3.259 |

Single CUDA trace samples. All full matrices miss the
preregistered 5% saving and instead take about 11–13% longer.
No end-to-end rate, process memory or model quality was measured.
This experiment adds no dispatch or change to the stable line.
The cumulative checkpoint is still EXP-2067, with measured
849.18 tok/s prefill and 32.71 tok/s decode at 32K/512.

## Reproduce

Use the EXP-2067 device source and saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2068-bonsai-cumulative-warp-row-reuse
out=build/bonsai-cumulative-row-reuse-2068
bash "$exp/source/build.sh" "$out" build/bonsai-mmq-width6144-2067/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Measured samples and bitwise component results](data/component.json).
