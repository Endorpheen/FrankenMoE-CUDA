# EXP-2026-11-25-2022 — PQ2 packed half2 expansion

**Experimental, no runtime promotion.** Compare two ways of replacing
the F32 conversion and multiply sequence in the EXP-2019 cached
PQ2-to-FP16 staging. The first reuses the known four-value half LUT.
The second constructs packed factors -1, 0, 1, and 2, then scales them
with two FP16x2 multiplies. Both reuse the same packed codes, stored
half scale, K64 MMA order and shared layout as the accepted control.

## Verification and component timing

An RTX 4070 device probe compares half2 results byte for byte with the
original F32 unpack on all 63,488 finite FP16 scales and all 256
packed code bytes (16,252,928 cases). This includes signed zero,
subnormals and products that overflow to infinity. Nonfinite scale
encodings are excluded. There are zero mismatches. Each candidate
also reproduces every output byte on nine real GGUF matrix shapes,
including a row tail and a 1023-token tail.

CUDA event medians in ms for the matching tile width at each batch size:

| Projection / tokens | Cached K64 | LUT | FP16x2 |
| --- | ---: | ---: | ---: |
| Layer 0 QKV / 128 | 0.309 | 0.302 | 0.306 |
| Layer 0 QKV / 257 | 0.775 | 0.742 | 0.764 |
| Layer 0 QKV / 1024 | 2.098 | 2.105 | 2.247 |
| Layer 0 FFN gate / 1024 | 3.572 | 3.585 | 4.130 |
| Layer 0 FFN up / 1024 | 3.936 | 3.498 | 3.492 |
| Layer 0 FFN down / 1023 | 3.414 | 3.528 | 3.355 |
| Layer 42 FFN gate / 1024 | 3.214 | 3.524 | 3.289 |
| Layer 42 FFN down / 1024 | 3.435 | 3.466 | 3.333 |
| Layer 3 Attention output / 1024 | 1.132 | 1.131 | 1.128 |

The candidate resource counts match the accepted cached tiles:
56/72 registers and 18,432/27,648 B shared memory for 64/128
tokens, no spills. FP16x2 does not consistently improve the full
batch. The narrow gains do not justify adding a model dispatch
branch without a whole-request hypothesis. No prefill, decode,
peak VRAM or peak RAM model measurement was taken. Keep the
EXP-2019 baseline unchanged. Exact packed expansion is reusable
for finite PQ2 scales, subject to a new occupancy and runtime
check. No cross-model or cross-GPU performance result is claimed.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2022-bonsai-pq2-half2
base=build/bonsai-mmq-pipeline-2019-packed
out=build/bonsai-pq2-half2-repro
bash "$ex/source/build.sh" "$out" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/pipeline.cubin" "$out/half2.cubin"
```

[Measured samples](data/component.json), [exhaustive check](data/check.txt)
and [hashes](data/SHA256SUMS).
