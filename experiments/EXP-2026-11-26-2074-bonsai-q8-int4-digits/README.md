# EXP-2026-11-26-2074 — Exact Q8 dot products from two INT4 digits

**Rejected below the component performance threshold.** Keep
the EXP-2073 gate/up activation quantizer and per-block scale.
Represent each signed Q8 code as 16 × its signed high nibble
plus its unsigned low nibble. Stage the ternary weight codes
in INT4 once and use two integer MMA streams. Reconstruct the
same int32 dot before the existing FP32 scale and accumulation.
Unlike EXP-2051, this is a lossless digit split of the current
Q8 arithmetic, not an additional precision correction.

All four actual gate/up fixtures match the cumulative INT8
outputs bitwise. The N128 entry uses 126 registers and
26,368 B of shared memory without spills. The EXP-2073
reference uses 93 registers and 28,416 B shared memory.

| RTX 4070 diagnostic, 1024 tokens including pack | EXP-2073 INT8, ms | INT4 digits, ms |
| --- | ---: | ---: |
| Layer 48 gate | 2.315 | 2.407 |
| Layer 48 up | 2.279 | 2.398 |
| Layer 63 gate | 2.835 | 2.405 |
| Layer 63 up | 2.279 | 2.411 |

These are single CUDA trace samples. Both up projections are
slower. The one fast gate observation uses a slower control
than the other shapes and does not satisfy the preregistered
gate/up criterion of 5% time saved. The trace does not isolate
integer throughput from staging or register effects. No HTTP
dispatch, model A/B, process VRAM or RSS tests were performed.
EXP-2049 and EXP-2073 remain the stable and cumulative lines.

## Reproduce

Use the EXP-2073 component CUDA source and saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2074-bonsai-q8-int4-digits
out=build/bonsai-q8-int4-digits-2074
bash "$exp/source/build.sh" "$out" build/bonsai-q8-full-tiles-2073/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Measured samples and bitwise component results](data/component.json).
