# EXP-1696 — Broadcast packed Vector Attention probabilities from registers

The baseline stores each key probability in shared memory, synchronizes
the warp, then reads every probability back by source lane. This
candidate broadcasts the same FP32 bit pattern directly with
`shfl.sync.idx.b32`, preserving the Q4 dot products, exponential and
FMA order, key partitioning, and the final reduction. It replaces
only the generated vector fragment, device glue and embedded CUDA
images in `cuda-driver.o`. Both PTX images passed sm_89 ptxas.

## Apex 32K/512 on the working default API profile

| Measure | Control | Warp shuffle |
|---|---:|---:|
| Prefill, tok/s | 3,010.12 | 3,014.06 |
| Decode, tok/s | 39.80 | 39.86 |
| Client, seconds | 23.817 | 23.785 |
| Peak process VRAM, MiB | 8,766 | 8,766 |
| Peak RSS, KiB | 21,489,056 | 21,582,940 |
| System swap-out from launch, pages | 0 | 0 |
| Process swap, KiB | 0 | 0 |

Warm-up and measured answers match all 512 IDs and text. SLRU
counters match. This ordinary A/B does not justify replacing
the frozen working configuration.

## Full warmed Nsight follow-up

| Packed Vector Attention | Control | Warp shuffle |
|---|---:|---:|
| Active launches | 5,110 | 5,110 |
| Grid | 16 × 64 × 1 | 16 × 64 × 1 |
| Threads per block | 128 | 128 |
| Registers per thread | 76 | 72 |
| Total GPU kernel time, ms | 2,442.516 | 2,411.867 |
| Decode window reported by server, ms | 13,641.797 | 14,062.248 |

No stack or spills are reported on the modified packed entry.
Both traces return the same 512 IDs. The 30.649 ms GPU reduction
is insufficient to explain an end-to-end change on its own.
Profiled decode is slower, and the ordinary client gain is only
32 ms with a higher RSS high-water mark. Preserve the trial
as a reproducible alternative and keep the control active.

The frozen hybrid's prior full capture shows 5,110 vector
launches at grid 1 × 11 × 16, 32 threads per block, 255 registers,
and about 1.218 seconds of exclusive device time. The NVIDIA
hybrid branch accumulates F32; the vendor half dot path is
conditional on HIP. The full-workload EXP-1562 test already
rejected changing just the partition count. A useful comparison
must additionally account for warp count, K/V work per block,
reuse and reducer geometry.

The local binary, source and comparisons are preserved in
`build/exp1696-flash-warp-probability`. Binary SHA-256:
`673a2b62e0d3e29dc72fdcee0c6ad6d0fb5ac515da17e3bb1e57ee80c2e2a76b`.
Use the extracted KeelWing `flash-vec.goose` from EXP-1665 for
`probability-shuffle.patch` and the standard OpenCL glue for
`device-shuffle.patch`. All raw responses, both ptxas reports,
and the trial SQLite trace stay in the local checkpoint.
