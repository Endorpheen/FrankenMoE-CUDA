# EXP-2026-11-25-2060 — Activation lookahead in cached Bonsai PQ2 MMQ

**Rejected at the component performance gate.** EXP-2019 measured
alternating activation buffers and packed weight reuse separately.
This variant combines those mechanisms in the accepted cached K64
path. It caches the existing codes and scale, prefetches the next
activation stage into a second shared buffer, and retains the
original FP16 operands and ordered FP32 MMA updates.

The bitwise component gate passes on four real shapes, including
an activation zero row and a joint QKV tail. The 64-token variant
uses 56 registers and 27,648 B of shared memory. The 128-token
variant uses 80 registers and 46,080 B. The accepted 128-token
control has 72 registers and 27,648 B. None has spills.

## RTX 4070 diagnostic component trace

The projected kernel time below includes common activation
packing. One diagnostic sample per path, in milliseconds.

| Matrix / tokens | Cached K64 N128 | Pipelined N64 | Pipelined N128 |
| --- | ---: | ---: | ---: |
| QKV tail / 129 | 0.603 | 0.577 | 0.683 |
| QKV / 1024 | 2.155 | 2.861 | 2.465 |
| FFN up / 1024 | 3.602 | 4.236 | 4.163 |
| FFN down / 1024 | 3.834 | 4.527 | 4.331 |

Both lookahead variants lose on every full prefill fixture.
The 129-token N64 observation compares against a wide fixed
component control, not the existing HTTP narrow dispatch.
It is not a measured improvement for production short workloads.
The preregistered 5% saving on full FFN down is not reached.
The trace does not isolate shared-memory residency from other
sources of the regression.

No runtime dispatch, end-to-end speed, process memory or other
model path was changed or tested. EXP-2049 remains the working
Bonsai HTTP reference at 748.10 tok/s prefill and approximately
32.70 tok/s decode for 32K/512.

## Reproduce

Use the accepted EXP-2049 source and the saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2060-bonsai-mmq-cached-pipeline
out=build/bonsai-cached-pipeline-2060
bash "$exp/source/build.sh" "$out" build/bonsai-flash-fused-padding-2049/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Component correctness and kernel durations](data/component.json).
