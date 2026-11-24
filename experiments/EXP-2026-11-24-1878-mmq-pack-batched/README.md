# EXP-2026-11-24-1878 — Group independent Q8_1 pack warps

**INCONCLUSIVE for full-model adoption.** EXP-1877 remains the baseline.
The kernel probe speeds up the dominant MMQ packing shapes. The
full-model pairs disagree and mean client time is unchanged.

The EXP-1870 prefill trace records 226.88 ms in 2200 launches of
`goose_mmq_pack_q8`. The dominant shapes are 16 groups by 4096 rows
(1190 calls) and 32 groups by 3584 rows (320 calls), with one
32-thread warp per CTA. The candidate groups two or four independent
warps for one row while keeping each warp's numerical code and
destination bytes. It introduces no CTA barriers and uses
24 registers versus 26. The runtime only selects the four-warp
path for prefill, reciprocal layouts 3/4 and at least 128 routes.
The native layout, reuse policy and scratch buffers are unchanged.

The isolated probe matched every packed byte on seven valid
fixtures, including nonzero route offsets, indexed jobs and an
incomplete four-warp group. For extent eight and 4096 rows,
the control and warp4 medians were 106.88 and 93.66 µs.
For extent sixteen and 3584 rows they were 182.47 and 166.68 µs.
An initial synthetic fixture using layout 2 failed with CUDA 716
already on the control. It was replaced with an Apex layout for
the same partial-group shape. Layout 2 is outside the dispatch.

## Full Apex 32K/512, RTX 4070

Context 262144, cache limit 6290 MiB, batch and ubatch 4096,
Q4_0 KV, one warm-up and one scored request per server. A-B-B-A
on the same executable. A sets `GOOSE_MMQ_PACK_BATCHED=0`.
Both B arms logged at least 4000 batched packs across two requests.

| Arm | Prefill tok/s | Decode tok/s | Client s | Sampled peak VRAM MiB |
| --- | ---: | ---: | ---: | ---: |
| A1 | 3474.70 | 83.26 | 15.655 | 8774 |
| B1 | 3455.28 | 83.19 | 15.714 | 8774 |
| B2 | 3456.69 | 83.42 | 15.696 | 8774 |
| A2 | 3454.21 | 82.69 | 15.753 | 8774 |

Mean prefill changed from 3464.46 to 3455.99 tok/s (-0.24%).
Mean decode changed from 82.98 to 83.30 tok/s (+0.39%). The
change applies to prefill only and does not establish a causal
decode gain. The client means differ by 0.96 ms. The first pair
lost on both throughput metrics and the reverse pair gained on
both. All eight replies matched the 512 token IDs and full text.
All SLRU counters and post-request properties matched byte-for-byte.
Peak RSS was 21491248–21493692 KiB. Runtime swap and system
swap-out were zero.

## Reproduction

Apply [cuda-mmq.cl.patch](source/cuda-mmq.cl.patch) to the frozen
EXP-1786 device tree. Build with the Clang NVPTX flags from EXP-1859,
and use CUDA 12.6 ptxas with `-arch=sm_89` and
`--entry goose_mmq_pack_q8_warp2,goose_mmq_pack_q8_warp4`.
Extract `goose_mmq_pack_q8` directly from archived PTX for control.
Name the images `grouped.cubin` and `control.cubin`.
Use [SHA256SUMS](SHA256SUMS), then run [probe.py](probe.py).

For Apex, embed the grouped cubin as `mmq_pack_batched_image`,
apply the [driver patch](source/cuda-driver.c.patch) to EXP-1877
and rebuild only the driver with its GCC 13 flags and frozen
archive. Use its exact argv and env, setting the toggle in A.
[results.json](results.json) and [probe.log](probe.log) contain
the samples and exact run identifiers.
