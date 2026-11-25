# EXP-2026-11-25-1940: Parallelize Inkling batched relative attention scores

**Verdict: ACCEPTED for the isolated Inkling prefill profile.** Both
short pairs and one full run improve prefill. The full run meets the
percentage tradeoff rule even with a slightly lower measured decode.

## Mechanism

The original Flash Q4_0 kernel computes the relative correction for
32 keys in series for each query row. The new batched entry distributes
the row/key pairs across all 128 CTA threads. It retains the same
ordered FP64 sum of FP32 products for each relative correction.
A CTA barrier makes those corrected scores available to the existing
ordered maximum, exponentials and probability sum for each row.
Masks, MMA, reduction order, KV encoding and key partitions are kept.

The new entry uses 62 registers without spills on SM89. Its dispatch
requires Q4_0 and more than one query. The existing single-query
specialization remains the decode path. Disable the new entry with
`I2_PREFILL_PARALLEL_RELATIVE=0` for the same-binary control.

## Device screen

Seeded Q4_0 KV on RTX 4070. All six pairs of partial outputs and
metadata match byte-for-byte. The four samples per case use CUDA
events and reverse the order on alternate rounds.

| Keys | Queries | Original, ms | Parallel, ms |
| ---: | ---: | ---: | ---: |
| 1024 | 16 | 1.029 | 0.303 |
| 1024 | 128 | 1.455 | 0.636 |
| 4096 | 16 | 1.339 | 0.431 |
| 4096 | 128 | 2.745 | 1.561 |
| 32768 | 16 | 2.496 | 1.433 |
| 32768 | 128 | 10.378 | 7.103 |

These are Flash partial-kernel timings. They exclude prefill graph
execution, the merge and transfers.

## Paired 4096/128 screen

EXP-1927 fixture prefix, RTX 4070, context 36864, batch and ubatch
4096, Q4_0 K/V, the 736 MiB arena, wide vectors and 6720 MiB expert
cache. B–A–A–B, first and repeat request per launch, matching internal
state checks and full prompt recomputation.

| Paired mean | Original | Parallel |
| --- | ---: | ---: |
| First prefill, tok/s | 213.18 | 225.15 |
| Repeat prefill, tok/s | 220.42 | 233.12 |
| First decode, tok/s | 18.005 | 18.068 |
| Repeat decode, tok/s | 18.082 | 17.997 |
| Repeat prefill, ms | 18582.5 | 17570.7 |
| Repeat client request, s | 25.828 | 24.842 |

Repeat prefill gained 5.76%, saving 949 and 1074 ms. Decode changes
were small relative to the prefill gain. All 8702 fingerprints per
launch and both sets of 128 output IDs and text match. Expert traffic
matches. The board peak in each pair was equal at 11711 and 11735 MiB.
Peak RSS varied by less than 2 MiB. Process swap and swap-out were zero.

## Full workload

One fresh 32K/512 candidate compared with the saved EXP-1939 run at
the same settings. Both runs used the 6720 MiB cache.

| Full request | EXP-1939 | Parallel prefill |
| --- | ---: | ---: |
| Prefill, tok/s | 202.715 | 219.015 |
| Decode, tok/s | 17.784 | 17.657 |
| Prefill, s | 161.645 | 149.615 |
| Decode, s | 28.733 | 28.940 |
| Client request, s | 190.835 | 179.034 |
| Peak board VRAM, MiB | 11725 | 11731 |
| Peak RSS, KiB | 14264240 | 14265536 |

Prefill improved 8.04% and saved 12.030 s. Decode fell 0.71% in the
single full comparison. Total time fell 11.802 s. All 512 IDs and
text match. Prefill expert traffic remains 20760 fills and 89.54 GB.
Swap and swap-out were zero.

The frozen Apex checkpoint checksum manifest passed. Production
launchers and Apex binary were preserved. No Apex throughput benchmark
ran. The 500 prefill and 25 decode milestones remain open.

## Reproduction

Apply the Flash and Goose patches to EXP-1932 and the driver header
patch to EXP-1937 in the isolated sibling layout. Build with the
supplied KeelWing, Clang 18, ptxas and GCC 13 script against the
EXP-1937 archive. Use `bench-prefill.py` in the build directory
for the seeded device screen. The binary hashes, compiler record
and exact model-run settings are in `inputs.sha256`,
`compiler.ptxas`, `prefill-bench.json` and `results.json`.
