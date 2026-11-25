# EXP-2026-11-25-1936: Decode Inkling IQ4_XS by 256-weight block

**Verdict: ACCEPTED for the isolated Inkling checkpoint.** Both
paired short comparisons and a full workload test show a decode
gain. The baseline retains the EXP-1934 host plans and EXP-1932
Q4_0 Flash specialization.

## Mechanism

EXP-1933 attributed about 9.55 ms per token to the small IQ4_XS
matrices in its full decode trace. The prior single-row path
decoded the block base scale and scale fields for each group of
32 weights. The new Goose device function loads the header once
per 256 weights. It processes the eight groups in the same
order with the same products, FMA recurrence and warp reduction.
A separate device entry handles small IQ4_XS dispatches up to
eight jobs. Larger batches retain the existing dispatch. The
current control uses `I2_DECODE_IQ4_XS_BLOCKS=0`.

The kernel uses 38 registers per thread on SM89, zero stack bytes
and no spills. The existing Q8, IQ4_NL, grouped prefill and Apex
images and dispatch paths are preserved.

## Device-only screen

The seeded probe compares the original and candidate entry on
2304-by-2304 IQ4_XS planes and FP32 input. Output and reduction
order match byte-for-byte at all four route counts. The timed
sequence cycles through three job sets and alternates arm order.
Four samples of 120 launches use CUDA events.

| Selected routes | Original, us | Block header, us |
| ---: | ---: | ---: |
| 1 | 33.52 | 16.71 |
| 2 | 58.42 | 32.11 |
| 4 | 115.46 | 58.29 |
| 8 | 220.55 | 118.15 |

These values describe the isolated kernel. They exclude transfers,
host preparation, gating and other graph operations.

## Paired 2048/256 screen

RTX 4070, context 36864, batch 4096, ubatch 2048, Q4_0 K/V, a
576 MiB activation arena, wide vectors, 6720 MiB expert cache.
One first and repeated request per launch in A–B–B–A order.
The system file cache was warmed with a read-only sequential
read of the model after the failed EXP-1935 probe. Both arms
recomputed the prompt and used identical state hashing.

| Paired mean | Control | Block header |
| --- | ---: | ---: |
| First prefill, tok/s | 203.04 | 202.96 |
| Repeat prefill, tok/s | 216.31 | 216.78 |
| First decode, tok/s | 16.803 | 17.830 |
| Repeat decode, tok/s | 16.880 | 17.799 |
| Repeat decode, ms | 15107.0 | 14327.3 |
| Repeat client request, s | 24.793 | 23.991 |

Repeat decode gained 5.44%. The two pairs saved 886 and 673
ms. All 5118 fingerprints per launch, both sets of 256 IDs
and text, and prefill expert fills and upload bytes matched.
Board peaks were equal within each pair at 11279 and 11257 MiB.
Peak RSS varied by less than 2 MiB. Process swap and system
swap-out were zero.

## Full workload

One fresh 32K/512 candidate request at the same hardware and
settings compared to the corresponding EXP-1934 saved run.
The full comparison has one observation per binary.

| Full request | EXP-1934 | Block header |
| --- | ---: | ---: |
| Prefill, tok/s | 191.925 | 193.225 |
| Decode, tok/s | 16.608 | 17.582 |
| Decode, s | 30.769 | 29.064 |
| Client request, s | 201.976 | 199.113 |
| Board peak, MiB | 11277 | 11283 |

Decode saved 1.705 s. All 512 IDs and text matched, along with
40619 prefill fills and 175.19 GB of prefill expert payload.
The candidate peak RSS was 13840592 KiB, within 1 MiB of the
saved baseline. Process swap and system swap-out were zero.
The fresh EXP-1927 llama.cpp decode target still leads by
5.15 s over the 511 full-request decode intervals.

The frozen Apex checkpoint manifest passed. No Apex performance
test or production change was made.

## Reproduction

Apply the matrix Goose and OpenCL patches to the EXP-1913
matrix sources and the driver header patch to EXP-1932 in the
isolated sibling layout. Build with the supplied KeelWing,
Clang 18, ptxas and GCC 13 script and the EXP-1934 host archive.
Run `bench-iq4.py` from the build directory for the seeded device
probe. See `inputs.sha256`, `compiler.ptxas`, `iq4-bench.json`
and `results.json` for artifacts, settings and raw timings.
