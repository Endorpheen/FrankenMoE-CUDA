# EXP-2026-11-25-2063 — Shallow K staging on the cumulative Bonsai MMQ checkpoint

**Rejected below the component performance threshold.** Start from
the EXP-2059 CUDA image. Load one K32 segment of activations and PQ2 weights
into shared memory at a time instead of loading K64 and consuming
two segments. Keep the existing FP16 K32 partials, final accumulation
in FP32 and per-output operation order. A shallower stage trades
weight-scale reuse and fewer barriers for smaller allocations.

## RTX 4070 diagnostic component trace

The test compares all outputs bitwise on four real matrix shapes.
All match, including a zero row and the joint QKV tail. The table
adds the common activation pack to a single trace sample per matrix,
in milliseconds.

| Matrix / tokens | EXP-2059 K64 stage | K32 stage N64 | K32 stage N128 |
| --- | ---: | ---: | ---: |
| QKV tail / 129 | 0.503 | 0.457 | 0.570 |
| QKV / 1024 | 1.759 | 2.061 | 1.982 |
| FFN up / 1024 | 3.004 | 4.053 | 3.412 |
| FFN down / 1024 | 3.239 | 3.676 | 4.145 |

The N128 path uses 76 registers and 15,360 B shared memory,
versus 94 and 27,648 B for the cumulative control. N64 uses
48 registers and 10,240 B. Neither spills. Both are slower on
all full 1024-token shapes and miss the preregistered 5% saving
on FFN down. The N64 tail observation does not justify changing
the full-prefill path. The trace does not isolate barrier or
weight reload costs from realized occupancy.

No new model throughput, process VRAM or model quality was
measured. There was no change to cumulative or stable artifacts.
The paired 32K/512 checkpoints remain EXP-2049 at 748.10 / 32.70
tok/s and experimental EXP-2059 at 815.16 / 32.71 tok/s.

## Reproduce

Use the EXP-2057 device source and the saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2063-bonsai-mmq-shallow-stage
out=build/bonsai-mmq-shallow-stage-2063
bash "$exp/source/build.sh" "$out" build/bonsai-fp16-acc-2057/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Times and bitwise component results](data/component.json).
