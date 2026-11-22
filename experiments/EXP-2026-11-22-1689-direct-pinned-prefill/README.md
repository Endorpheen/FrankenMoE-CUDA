# Apex VRAM reserve and direct pinned prefill uploads

The target is 3,043 prefill and 67 decode tokens/s on Apex 32K/512.
The initial matched hybrid checkpoint is in the
[previous report](../EXP-2026-11-22-1687-matched-expert-pinning/README.md).
All Goose arms here use the same request, 4,096/4,096 batches, Q4_0
KV, 262,144 context and explicit 18,601 MiB host pin budget.
The runtime registered 120 ranges covering 19,504,005,120 bytes.
Each arm uses two requests on one process, with the second measured.

## Allocating the device-memory reserve

| Goose SLRU / arena, MiB | Prefill tok/s | Decode tok/s | Client s | Peak process VRAM, MiB |
|---|---:|---:|---:|---:|
| 5,632 / 384, instrumented environment without Nsight | 2,639.02 | 33.37 | 27.842 | 7,898 |
| 6,400 / 384, paired with 5,632 | 2,659.05 | 33.72 | 27.593 | 8,666 |
| 6,272 / 512, separate near-equal-VRAM pair | 2,650.00 | 33.91 | 27.551 | 8,668 |
| 6,400 / 384, paired with wider arena | 2,653.76 | 34.28 | 27.368 | 8,666 |

There was no process swap or system swap-out from launch in the four
rows above, and the Goose responses and all 512 probability rows match.
The 6,400 MiB bank with the 384 MiB arena wins the equal-footprint pair.
A standalone 6,272 MiB bank reached 2,729.26 tok/s prefill in an earlier
pair with system swap-out, so that raw observation is not a qualified
checkpoint. A 6,560 MiB request was rejected by the existing reservation
guard, which reported 6,776,315,904 available bytes after model allocations.
No CUDA allocation OOM was required to establish the limit.

The fresh near-VRAM Hybrid/Goose pairs produce 2,949.71/65.88 versus
2,651.25/34.47 and 2,933.28/66.13 versus 2,652.93/33.80. Client times
are 18.886/27.298 and 18.920/27.586 seconds. Peaks are 8,850/8,666 MiB
process VRAM. Both register 120 ranges and show zero process swap.
During Goose startup the system recorded 3,505 and 95 swap-out pages,
respectively, and zero pages after readiness. Both cross-runtime
observations remain separate from the clean same-runtime pairs and
from the existing 3,043/67 target.

## Why fewer bytes did not translate into a large prefill gain

The 5,632 versus 6,400 MiB full Nsight pair uses the same executable
and exact output. Scope starts at the first 4,096-row RMSNorm and
splits at the first scalar GDN decode event. Each trace has 15,330
decode GDN launches. GPU idle and busy times partition the measured
windows, so the following intervals are not additive with CPU API time.

| Trace measure | 5,632 MiB | 6,400 MiB | Change |
|---|---:|---:|---:|
| Prefill H2D bulk, decimal GB | 94.704 | 94.122 | -0.582 |
| Prefill exclusive H2D, ms | 3,758.281 | 3,744.525 | -13.756 |
| Prefill GPU idle, ms | 941.731 | 994.761 | +53.030 |
| Prefill window, ms | 12,573.927 | 12,636.027 | +62.100 |
| Decode H2D bulk, decimal GB | 44.314 | 34.759 | -9.556 |
| Decode exclusive H2D, ms | 1,827.551 | 1,464.162 | -363.390 |
| Decode GPU idle, ms | 6,418.328 | 6,617.436 | +199.108 |
| Decode window, ms | 16,057.020 | 15,882.120 | -174.900 |

The ordinary A/B improves both phases modestly. The short prefill
difference under Nsight reflects little prefill traffic removed and
more uncovered gaps. The larger cache mainly helps decode. Median
SM clocks during ordinary prefill were 2,835/2,820 MHz, memory clocks
10,251 MHz for both, and temperature 62 C for both. The earlier
2,724 tok/s result used a different run and did not capture clocks,
so a specific historical cause cannot be identified from its log.

The short matched 32K/32 trace against the hybrid shows 94.76 versus
61.27 GB prefill H2D. The hybrid uses a read-only cache lookup for large
batches and a temporary tensor for misses, while Goose populates its
expert bank. The hybrid also batches adjacent expert copies. Different
cross-runtime continuations and transfer layouts prevent assigning
the entire difference to cache replacement. Measure a temporary-plane
Goose variant directly before drawing that attribution.

## EXP-1689 — Direct asynchronous copies from registered mappings

The activation of current-plane staging depended on the pageable
host ring. With near-exact full pin, its remaining host budget is
only about 0.53 MiB, below the 12 MiB ring requirement. The candidate
permits direct asynchronous submission when the source span is already
registered. Old reads, the upload and the new DAG consumer remain
ordered on the legacy stream. Pageable sources with no ring retain
their blocking path.

This patch applies to the imported EXP-1682 driver
(`109e4e91edcadb26355a23d0a9841bfc0596c838`). Only
`cuda-driver.o` was rebuilt on top of the cumulative EXP-1687/1668
archive. The original model, context, bank, tensor arena, compiled
Goose and CUDA images are unchanged.

| Ordinary pair at 6,400 / 384 MiB | Control | Direct pinned |
|---|---:|---:|
| Forward prefill, tok/s | 2,659.78 | 2,721.75 |
| Forward decode, tok/s | 33.65 | 33.24 |
| Forward client, s | 27.618 | 27.523 |
| Reverse prefill, tok/s | 2,655.68 | 2,722.21 |
| Reverse decode, tok/s | 34.20 | 34.04 |
| Reverse client, s | 27.394 | 27.163 |
| Peak process VRAM, MiB | 8,666 | 8,666 |

All 512 IDs, probability rows, content and SLRU counters agree in both
pairs. Both use zero process swap. The forward candidate incurred 81
system swap-out pages during startup. Both arms of the reverse pair
have zero system swap-out from launch. Prefill improves by 280/302 ms,
while decode slows by 188/69 ms. Preserve the candidate as an
experimental prefill checkpoint and retain the control as the default
for follow-up requiring a regression-free decode.

The candidate CUDA trace at the same budget records 7,718 synchronous
prefill H2D calls versus 56,389 in the control and 148,180 asynchronous
calls versus 99,509. Total prefill H2D bytes match (94,122,125,416).
GPU idle drops 994.761 to 643.806 ms and prefill window drops
12,636.027 to 12,241.138 ms. The sum of synchronous CUDA API call
durations overlaps device activity and is not a wall-time saving.
These trace observations validate the changed execution path.

## Artifacts

The preserved binaries, link archives, profile scripts, raw ordinary
results, telemetry and Nsight databases are in the FrankenMoE worktree's
`build/exp1687-exp1668` and `build/exp1689-direct-pinned-prefill`
directories. Each checkpoint has a SHA-256 manifest. The alternate
budget result is `build/exp1687-exp1668/vram-reserve-comparison.json`.
Full trace phase data is in `pin-full-cache-pair/analysis.json` and
`build/exp1689-direct-pinned-prefill/analysis.json`. No binaries or
SQLite databases are tracked in Git.
