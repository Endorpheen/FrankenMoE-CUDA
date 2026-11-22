# EXP-1690 — Temporary weight plane for large Apex prefill projections

The hybrid uses the expert bank read-only on large prefill batches.
This Goose candidate keeps the cached weights across prompt chunks
and stages only misses in a single reusable GPU plane. The layout adds
220,200,960 bytes (210 MiB) beside the expert bank. MMQ consumes both
through the existing offset descriptors. Consecutive misses can be
uploaded together from registered mmap ranges. Uploads and their
readers retain legacy-stream ordering. The experiment activates only
on prefill projections selecting at least 64 distinct experts. The
earlier SLRU path still owns small projections and decode.

The artifact is an incremental EXP-1689 build. Only `cuda-driver.o`
changed. Apply the included patch on top of the driver patch chain in
[EXP-1687](../EXP-2026-11-22-1687-matched-expert-pinning/README.md) and
[EXP-1689](../EXP-2026-11-22-1689-direct-pinned-prefill/README.md).
The host, CUDA images, compiler and request are identical.

## Apex 32K/512

Both sides use the same candidate executable. The control uses a
6,400 MiB expert cache with temporary weights disabled. The trial
enables `GOOSE_CUDA_PREFILL_TEMP_WEIGHTS=1` with a 6,190 MiB expert
cache and 210 MiB temporary plane. Full host registration totals
19,504,005,120 bytes in 120 ranges. Both use a 384 MiB activation
arena, 4,096/4,096 batches, 262,144 context, Q4_0 K/V and the existing
request digest. There are two requests per process, with r2 measured.

| Metric | Forward control | Forward temp | Reverse control | Reverse temp |
|---|---:|---:|---:|---:|
| Prefill r2, tokens/s | 2,714.59 | 3,003.86 | 2,711.09 | 3,012.58 |
| Decode r2, tokens/s | 32.30 | 33.58 | 34.06 | 34.11 |
| Client r2, seconds | 28.004 | 26.236 | 27.205 | 25.969 |
| Cold prefill r1, tokens/s | 2,676.10 | 2,610.44 | 2,674.1 | 2,615.6 |
| Peak process VRAM, MiB | 8,666 | 8,668 | 8,666 | 8,668 |
| Peak process RSS, KiB | 21,490,048 | 21,489,924 | 21,490,448 | 21,490,664 |
| Peak process swap, KiB | 0 | 0 | 0 | 0 |
| System swap-out from launch, pages | 8,548 | 0 | 0 | 67 |

All 512 IDs, all probability rows and the response content match
between arms on both full measured requests. Cache prompt reuse
remains zero. Logged across both requests, the temporary path ran
1,920 projections, read 58,872 resident planes and uploaded
149,946,089,472 bytes. Adding those bytes to the existing SLRU upload
counter gives 226,131,935,232 bytes, down from 256,863,535,104 bytes
for the control. The reduction is 30.73 GB across two requests.
The temporary path does not count its uploads as SLRU fills.

Keep 3,003.86 / 33.58 as the clean warmed prefill checkpoint and
3,012.58 / 34.11 as the separate peak observation with 67 startup
swap-out pages. The warmed benefit repeats in reverse order.
Cold prefill is 58–66 tok/s slower, so leave the feature experimental
until cold operation is addressed. The 3,043 / 67 target remains open.

Local preserved binaries, build source, link archive, and raw logs
are in `build/exp1690-prefill-temp-weights`. The manifest is
`SHA256SUMS`, phase and memory evidence are in `comparison.json`.
The executable SHA-256 is
`64556877754def76516c05443a8d3cf7f8e1d46c0767294eb97822ca27a06bee`.
