# EXP-2026-11-26-2072 — E4M3 matrix products for Bonsai PQ2

**Rejected at the component performance gate.** Encode the PQ2
coefficients −1, 0, +1 and +2 exactly in E4M3. Encode activations in
E4M3 with one F32 scale per 128 values. Accumulate each block dot
product in FP32, apply the stored weight scale and activation scale,
and accumulate the blocks. This is a new numerical path versus
the cumulative EXP-2071 FP16 partials.

Both N64 and N128 candidates produce finite outputs, return a zero
output for the zero activation row, and agree with the independent
sampled CPU E4M3 oracle. Maximum sampled absolute CPU difference is
below 0.000107. Relative L2 versus cumulative MMQ is about 2.57%.
N64 uses 56 registers and 18,944 B shared memory. N128 uses 91 and
28,416 B. Neither spills.

## RTX 4070 diagnostic component trace

One CUDA sample per combination, including activation pack, in ms.
All rows use actual GGUF matrices and 1024 seeded activation rows.

| Matrix | EXP-2071 | E4M3 N64 | E4M3 N128 |
| --- | ---: | ---: | ---: |
| QKV | 1.635 | 1.531 | 1.600 |
| SSM output | 1.012 | 0.952 | 0.965 |
| FFN up | 3.174 | 2.563 | 2.807 |
| FFN down | 2.878 | 2.708 | 2.768 |
| FFN up, reverse order | 2.738 | 2.962 | 2.814 |

Only the first FFN-up diagnostic met the preregistered 15% component
saving. Its control was slower than prior observations on that
shape. The targeted reverse check restores the expected control
time and shows neither candidate faster. These component timings
do not establish an end-to-end gain. No HTTP dispatch, long A/B,
process VRAM/RSS or model quality was measured. E4M3 QK from
EXP-2070 is separate and stays in the cumulative line.

## Reproduce

Use the EXP-2067 device source and saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2072-bonsai-pq2-fp8-mmq
out=build/bonsai-pq2-fp8-mmq-2072
bash "$exp/source/build.sh" "$out" build/bonsai-mmq-width6144-2067/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin" \
  build/bonsai-combined-half2-down-2071/pipeline.cubin
```

Add `--up-only-reverse` for the targeted diagnostic.
[Component trace](data/component.json) and [reverse observation](data/reverse.json).
