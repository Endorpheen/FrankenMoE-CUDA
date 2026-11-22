# EXP-1712 — Reuse packed K/V across pairs of query heads

The packed Q4_0 decode path assigns two query heads that share one KV
head to a CTA. The heads share key loads and value dequantization.
They retain separate dot products, softmax states and output partials.
The partition geometry, FP32 reduction order per head, and merge
kernel are preserved. The driver uses the existing single-head path
when the query-to-KV head ratio is odd or the packed path is unavailable.

The compiled paired entry uses 94 registers per thread, 4,608 bytes of
shared memory and no spills. The candidate's server log confirms it
ran the paired path on Apex.

## Full warmed Apex pair

Apex-35B-V1.0 Q4_K_M on RTX 4070, 32,768 input and 512 output tokens,
batch/ubatch 4,096/4,096, context 262,144, Q4_0 KV, the 6,290 MiB expert
bank, 210 MiB temporary prefill workspace, and no probability reporting.
Both arms used the EXP-1710 profile and one warm-up request in the same
process before measurement. Both measured requests had zero prompt
reuse. The benchmark was unprofiled.

| Metric | EXP-1710 fresh control | EXP-1712 |
|---|---:|---:|
| Prefill, tok/s | 3008.50 | 3015.70 |
| Decode, tok/s | 41.92 | 43.58 |
| Decode, ms | 12189.86 | 11725.89 |
| Client, s | 23.170175 | 22.680520 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21491112 | 21584244 |
| Process swap, KiB | 0 | 0 |
| System swap-out, pages | 0 | 0 |

The candidate saves 463.969 ms decode and 489.655 ms client time.
All 512 token IDs and the full text match in both requests. The SLRU
cache counters, including fills, evictions and upload bytes, match.
Peak RSS rises by 93,132 KiB (90.95 MiB, 0.43%). The candidate also
had higher RSS before the first request. These measurements do not
attribute the extra resident memory to the new kernel.

Keep the measured speedup as an experimental cumulative checkpoint
with the RSS delta recorded. Preserve EXP-1710 as the rollback point
and EXP-1701 as the historical prefill best. The experimental decode
checkpoint remains below the 71.17 tok/s target.

## Reproduction

The `build/exp1712-flash-gqa-pair` directory saves the executable,
effective profile, raw pair, preprocessed fragment and device images.
The candidate binary SHA-256 is
`8f4de2873c6308b23310a88df59b2063a5c82f52d0dafba9799b12ffa7cbf32b`.
Apply the driver patch to the EXP-1710 driver and the device patches
to the EXP-1665 extracted `flash-vec.goose` and the tracked OpenCL
glue. Preserve the EXP-1710 MMVQ fragment and glue. Rebuild the CUDA
images and replace only the driver object in the EXP-1710 archive.
