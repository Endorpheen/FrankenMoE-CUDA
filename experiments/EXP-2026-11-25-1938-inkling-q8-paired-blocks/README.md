# EXP-2026-11-25-1938: Screen paired Inkling Q8_0 block scheduling

**Verdict: REJECTED for the interleaved batched path; INCONCLUSIVE
for request throughput with the dedicated single-input entry.**
Keep EXP-1937 as the accepted checkpoint.

## Mechanism and device screen

The candidate processes two adjacent Q8_0 groups per loop
iteration. Within each dot product it preserves the scale,
FP32 FMA recurrence and warp reduction order. The first
eight-input version issued both FMAs per output before
advancing to the next output. The second version issues the
first group for all outputs before the second group. A final
separate entry strips the eight-input path from single-input
dispatch. It uses 39 registers and no spills on SM89.

The seeded CUDA-event probe compares the original and changed
kernels at widths 2304 and 3072, with one, 16, 128 and 512 jobs.
Every output digest matches, including the batched cases.
The initial interleaved variant regressed at all batched
sizes. For example, width 3072 with 512 jobs increased from
3624 to 5257 microseconds. The separated schedule removed
most of the loss but did not show a consistent additional
gain at 512 jobs. The final binary leaves the old batched
prefill dispatch in place. Its single-input path improved
from 15.53 to 14.68 microseconds at width 2304, and from
17.56 to 16.61 microseconds at width 3072.

Kernel timings are useful for screening. These measurements
exclude the wait, transfer and scheduling costs of a request.

## Same-binary model screen

RTX 4070, EXP-1917 2048/256 fixture, context 36864, batch 4096,
ubatch 2048, Q4_0 K/V, wide vectors, 576 MiB activation arena
and 6720 MiB expert limit. One control and candidate launch,
each with a first and repeat request and identical state
hashing. The control uses `I2_Q8_PAIRED_BLOCKS=0`.

| Request metric | Control | Dedicated |
| --- | ---: | ---: |
| First prefill, tok/s | 207.32 | 207.31 |
| Repeat prefill, tok/s | 221.06 | 220.83 |
| First decode, tok/s | 18.161 | 18.180 |
| Repeat decode, tok/s | 18.052 | 18.086 |
| Repeat decode, ms | 14125.8 | 14099.1 |
| Repeat request, s | 23.605 | 23.588 |

The 27 ms repeat decode difference does not justify another
qualification round. All 5118 state fingerprints, both sets
of 256 output IDs and text, and prefill expert traffic match.
Both arms peaked at 11277 MiB board VRAM and had zero swap
and system swap-out. The frozen Apex manifest passed. No full
Inkling target test or Apex performance run was performed.

## Reproduction

Apply the three patches to the isolated EXP-1936 matrix inputs
and link with the EXP-1937 CUDA and host inputs. Use the supplied
build script and `source/bench-q8.py`. The first batched variant
places its second FMA directly after the first within each output
update. The final source groups first and second updates
separately and provides both entry points for the microbenchmark.
The exact timed launch and per-variant results are preserved in
`results.json` and the three `q8-bench-*.json` files.
