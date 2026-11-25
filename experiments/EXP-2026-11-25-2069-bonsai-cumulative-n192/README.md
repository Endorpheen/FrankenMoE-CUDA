# EXP-2026-11-25-2069 — Twelve-warp N192 cumulative Bonsai tiles

**Rejected at the component performance gate.** Keep EXP-2067
cached PQ2 weights, FP16 K32 partials and FP32 combination order.
Use twelve warps with eight output fragments per thread to
share a staged weight tile across 192 tokens instead of 128.
Warps beyond the valid token extent skip MMA and still participate
in barriers. This avoids increasing accumulator fragments per
warp as in EXP-2008's eight-warp N192 variant.

All full outputs and a 1023-token tail match their EXP-2067
references bitwise. The N192 entry uses 76 registers and
36,864 B of shared memory without spills. The CUDA residency
bound is two blocks of twelve warps, versus three blocks of
eight warps for N128. Both imply up to 24 active warps on an SM.

| RTX 4070 diagnostic, including pack | EXP-2067, ms | N192, ms |
| --- | ---: | ---: |
| QKV / 1024 | 1.638 | 1.811 |
| FFN up / 1024 | 2.730 | 3.049 |
| FFN down / 1024 | 2.919 | 3.308 |
| FFN down / 1023 | 3.228 | 3.301 |

One trace sample per measured kernel. All full matrices miss
the preregistered 5% saving and instead take 10.5–13.3% longer.
This trace does not isolate weight reuse, shared memory traffic,
addressing or scheduling as separate effects. No runtime dispatch,
end-to-end rates, model quality or process memory were measured.
Stable EXP-2049 and cumulative EXP-2067 artifacts are unchanged.

## Reproduce

Use the EXP-2067 device source and saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2069-bonsai-cumulative-n192
out=build/bonsai-cumulative-n192-2069
bash "$exp/source/build.sh" "$out" build/bonsai-mmq-width6144-2067/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Measured samples and bitwise component results](data/component.json).
