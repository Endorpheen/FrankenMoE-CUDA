# EXP-2026-11-25-2008 — Wider token tiles for exact Bonsai PQ2 MMQ

**Rejected at the component gate.** Keep the EXP-2006 runtime and its
64x128 K64 prefill dispatch. This experiment shares one expanded
weight stage among 192 or 256 tokens per CTA instead of 128. It keeps
the on-disk PQ2 format, FP16 operands and F32 accumulator order. It
adds no global workspace. Compare four variants with the accepted
Q4_0 runtime's CUDA image on actual model weights and common packed
FP16 input. Prior expansion and K32 studies did not test these token
tile widths.

## RTX 4070 results

CUDA event medians in milliseconds, four samples per variant.
All output bytes match the accepted control in every test. The
1023-token QKV case also removes one real output row to exercise
both output tails.

| Real GGUF projection / tokens | 64x128 w8 | 64x192 w8 | 64x256 w8 | 32x256 w8 | 64x256 w16 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Layer 0 QKV / 512 | 1.159 | 1.409 | 1.443 | 1.535 | 1.237 |
| Layer 0 QKV / 1024 | 2.267 | 2.680 | 2.587 | 3.028 | 2.413 |
| Layer 0 QKV row tail / 1023 | 2.272 | 2.687 | 2.576 | 3.023 | 2.432 |
| Layer 0 SSM output / 1024 | 1.383 | 1.676 | 1.799 | 1.860 | 1.520 |
| Layer 3 Attention key / 1024 | 0.282 | 0.428 | 0.368 | 0.366 | 0.365 |
| Layer 0 FFN gate / 1024 | 3.888 | 4.722 | 4.683 | 4.895 | 4.249 |
| Layer 0 FFN up / 1024 | 3.889 | 4.720 | 4.685 | 4.890 | 4.247 |
| Layer 0 FFN down / 1024 | 3.546 | 4.420 | 4.661 | 4.327 | 3.775 |
| Layer 42 FFN down / 1024 | 3.535 | 4.444 | 4.680 | 4.367 | 3.813 |

The 64x256 w16 candidate uses 46,080 bytes shared memory and 64
registers per thread at 512 threads per CTA, versus 27,648 bytes,
56 registers and 256 threads for the control. The wide w8 candidate
has 96 registers. No candidate spills. Larger tiles increase live
output fragments or the per-CTA thread footprint. They also reduce
the number of CTAs in the token dimension. Either can reduce available
parallelism relative to the benefit of reusing unpacked weights.
Hardware occupancy and cache counters were not measured.

The objective at 170% of EXP-1977 prefill allows about 17.632 s per
32768 inputs. EXP-1995's GGUF census counts about 1594.243 TF
operations in dense batched projections alone. At its observed
2775 MHz and peak 65.36 TFLOP/s for FP16-to-F32, the projection
arithmetic floor is 24.39 s. This bound describes the current dense
operation count and instruction class. Reaching the target requires
reducing work or using a faster numeric representation with the
agreed quality gate. It is not a bound on different algorithms.
The separately validated 32K/512 decode result is unchanged.

No model launch or shared runtime changes were made. Preserve this
negative result alongside the EXP-1993 expansion, EXP-2000 K64
staging and EXP-2001 narrow K32 studies. For an arithmetic-changing
candidate, include activation preparation and require fixed-history
logits, token selection, long generation and independently scored
complex tasks. EXP-1994 and EXP-1996 describe failures that a single
matching long benchmark did not rule out.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2008-bonsai-mmq-token-reuse
base=build/bonsai-row-reuse-2006-http
out=build/bonsai-mmq-token-reuse-repro
bash "$ex/source/build.sh" "$out" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/rows.cubin" "$out/reuse.cubin"
```

[Raw samples](data/component.json) and [ptxas output](data/ptxas.txt).
