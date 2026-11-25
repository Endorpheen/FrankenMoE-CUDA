# EXP-2026-11-25-2055 — Resident PQ2 code and scale planes for Bonsai K64

**Rejected at the component performance gate.** Separate the 16-bit
scale from the codes and store aligned pairs of low/high K64 code
bytes in block-major order. Each quantization block retains exactly
34 bytes. A hypothetical resident layout would repack once during
model load. This probe measures that cost separately. FP16 staging,
PQ2 decoding and FP32 MMA update order are unchanged.

All four real matrix fixtures match the EXP-2049 K64 outputs bitwise,
including a joint input/output tail and a zero activation row. The
modified projection uses 64 instead of 72 registers, the same
27,648 B shared memory, and no spills. It does not change arithmetic.

## RTX 4070 diagnostic component trace

One sample per launch. Projection duration includes the common
activation pack. The candidate column excludes the one-time weight
repack, which is listed separately. All values are milliseconds.

| Matrix / tokens | Packed PQ2 | Resident planes | Repack once |
| --- | ---: | ---: | ---: |
| QKV tail / 129 | 0.6012 | 0.5997 | 0.0512 |
| QKV / 1024 | 2.1751 | 2.1374 | 0.0585 |
| FFN up / 1024 | 3.6057 | 3.5700 | 0.1052 |
| FFN down / 1024 | 3.8379 | 3.8153 | 0.0999 |

The maximum observed gain is 1.73%. Full FFN down improves by
0.59%, below the preregistered 5% threshold. This does not justify
changing the loader and every consumer of the resident weight
format for this scope. The component retains both representations
for correctness checks. A coexistence memory cost in a shipping
runtime was not measured.

No end-to-end prefill, decode, process VRAM or RSS tests were
performed. The existing EXP-2049 host and all model paths are
unchanged.

## Reproduce

Use the accepted EXP-2049 source and the saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2055-bonsai-mmq-resident-planes
out=build/bonsai-resident-planes-2055
bash "$exp/source/build.sh" "$out" build/bonsai-flash-fused-padding-2049/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Component correctness and kernel durations](data/component.json).
