# EXP-2026-11-25-2013 — Parallel exact Bonsai RMSNorm output

**Accepted in the experimental Bonsai HTTP line.** The new kernel
handles contiguous weighted RMSNorm with one 5120-wide row per token.
One warp still sums its original FP32 lanes in the same serial and
butterfly order. It broadcasts the same square-root divisor and
uses more lanes for the independent output divides and weights.
For up to 512 rows, 512 threads cooperate on shared input staging.
Larger batches use 128 threads and reread the input directly.
There is no new global allocation. Other widths, grouped heads,
gated normalization and L2 modes retain their existing kernels.

## Measured bottleneck

EXP-2012's CUDA-only profile on the EXP-1977 32768/16 diagnostic
spends 557.81 ms in 15 decode intervals. All 16 IDs match PrismML.
The one-CTA RMSNorm calls take 98.739 ms across 1935 calls, out of
103.160 ms for all norm shapes. This is the largest measured
non-matvec kernel group other than Q4 Attention and its merge.
[Trace summary](data/decode-profile.json).

## Component checks

All five variants reproduce every output byte on seven input
shapes with actual GGUF norm weights and the model epsilon. The
fixtures include signed zero and a very small norm. Median CUDA
event time per dispatch in microseconds:

| Rows | Control, 32 threads | 128 threads, direct | 512 threads, staged |
| ---: | ---: | ---: | ---: |
| 1 | 33.74 | 12.58 | 4.76 |
| 17 | 46.69 | 15.87 | 5.74 |
| 64 | 35.02 | 13.16 | 5.65 |
| 128 | 41.29 | 14.98 | 7.43 |
| 256 | 51.05 | 17.97 | 12.79 |
| 512 | 51.94 | 20.70 | 20.02 |
| 1024 | 114.53 | 58.72 | 86.67 |

Both dispatched variants use 23 registers per thread and no spills.
The 512-thread shape uses 20,484 bytes of local shared memory and
the 128-thread shape uses 4 bytes. The crossover between 512 and
1024 rows was not exhaustively tuned. [Raw timings](data/component-expanded.json).

## HTTP gate on RTX 4070

Same binary and CUDA images within both arms. Control disables
`BONSAI_WIDE_NORM`. Both use EXP-2012 DP4A and EXP-2011 Flash grouping,
context 36864, batch/ubatch 4096/1024, Q4_0 K/V, affinity 0f0f,
greedy with EOG masked, prefix reuse and MTP disabled.

| Workload | Prefill A → B, tok/s | Decode A → B, tok/s | Decode change |
| --- | ---: | ---: | ---: |
| 4097/64, two balanced pairs | 820.04 → 821.60 | 30.68 → 37.52 | +22.29% |
| 32768/512, one A–B pair | 619.99 → 620.89 | 27.082 → 32.255 | +19.10% |

Long decode saves 3.026 s, prefill saves 76.60 ms, and client wall
saves 3.103 s. The full 512 IDs and reference text SHA-256 match
PrismML in both arms. Short A–B–B–A outputs also match exactly.
Long peak process VRAM is 8932 MiB in both arms and VmHWM is
7,457,228 → 7,457,300 KiB. Process swap and system swap-out are zero.
No shared model graph or Apex/Inkling CUDA image changes are made.
Their full shared-binary regression gates remain open.

The method may apply to other low-row-count RMSNorm shapes that
use an ordered reduction, provided the reduction, math, weighting
and output layout are verified. There is no measured gain for any
other architecture or GPU.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2013-bonsai-wide-norm
dev=build/bonsai-wide-norm-repro
out=build/bonsai-wide-norm-http-repro
bash "$ex/source/build.sh" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json build/bonsai-dp4a-2012/mvq.cubin "$dev/norm.cubin"
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Use the EXP-2012 launch settings with `"$out/frankenmoe-infer"` and
`BONSAI_CUDA_IMAGE="$out/norm.cubin"`. Set `BONSAI_WIDE_NORM=0` for
the control and leave it unset for B. Post the exact EXP-1977
`/completion` request, or its 4097/64 prefix for the short profile.
See [timing and resource data](data/results.json) and [hashes](data/SHA256SUMS).
