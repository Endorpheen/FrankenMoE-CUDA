# EXP-2026-11-25-1929: Group the Inkling decode Q8 attention projections

**Verdict: INCONCLUSIVE for request throughput.** The launch
reduction is measured. Keep EXP-1925 as the executable checkpoint.

## Mechanism

The single-token Inkling attention graph projects Q, K, V,
relative query, sinks and gates, plus key and value taps on
full-attention visits. Replace the six or eight kernel launches
with one segmented grid when all source weights are resident.
The grid preserves the exact original output row offsets and
calls the same Goose single-row Q8 dot product for each row.
The 2304-wide Inkling path is the only eligible shape. Prefill
and the output projection retain the original dispatch. The
control uses `I2_DECODE_Q8_PROJECTION_PACK=0`. The isolated
packed kernel compiled for SM89 with 40 registers and no spills.

## Timed comparison

RTX 4070, EXP-1917 2048/256 fixture, context 36864, batch 4096,
ubatch 2048, Q4_0 K/V, 576 MiB activation arena and the isolated
6720 MiB cache from EXP-1928. The same executable ran A–B–B–A.
Each fresh server handled one first and one repeated request with
prompt recomputation and state hashing.

| Paired mean | Original | Packed |
| --- | ---: | ---: |
| First prefill, tok/s | 201.96 | 202.51 |
| Repeat prefill, tok/s | 215.76 | 215.79 |
| First decode, tok/s | 14.435 | 14.538 |
| Repeat decode, tok/s | 14.499 | 14.520 |
| Repeat decode, ms | 17587.7 | 17562.3 |
| Repeat client request, s | 27.287 | 27.268 |

First decode saved 219 and 35 ms in the pairs. Repeat decode
saved 74 ms and lost 23 ms, for a mean speed change of 0.14%.
All 5118 fingerprints, output IDs, text and prefill expert
traffic matched across arms. The peak board VRAM values were
11370 and 11360 MiB in the forward pair and 11264 MiB in both
reverse arms. Peak RSS varied by less than 2 MiB. Process swap
and system swap-out were zero.

## Independent CUDA traces

Both traces used the same 2048 prompt IDs and 64 output tokens
with the timed profile's context and cache. The warm window
covers 63 intervals between vocabulary readbacks. The output
IDs and text match both arms.

| Per decode interval | Original | Packed |
| --- | ---: | ---: |
| Q8 kernel launches | 641 | 385 |
| Packed kernel launches included above | 0 | 48 |
| Summed Q8 kernel time, ms | 7.196 | 6.092 |
| Whole trace interval, ms | 73.806 | 74.674 |

The grouped grid removes 256 launches and about 1.10 ms of
summed kernel time per token. The measured end-to-end response
does not establish a matching decode gain. Summed kernel
duration and full elapsed time are different measurements.
Further work on repeated launch dependencies should target
longer resident regions and measure host waits directly.

The frozen Apex checksum manifest passed. The changed device
image and host glue remain in the isolated experiment. There
was no combined Apex performance test or Inkling 32K/512 test.

## Reproduction

Use the Q8 source and build helper from EXP-1913 with the
current EXP-1925 CUDA driver and numerical object. Apply the
three patches in `source` and use its build helper with KeelWing,
Clang 18, GCC 13 and ptxas for SM89. Keep the EXP-1921 host archive.
The original Q8 kernels remain in the same image. See
`inputs.sha256`, `results.json`, `control-trace.json` and
`pack-trace.json`. Analyze the raw reports with the EXP-1925
decode timeline analyzer.
