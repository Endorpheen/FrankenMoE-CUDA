# Prefill pipeline and VRAM allocation experiments

The starting point is the
[EXP-1691 cold-aware checkpoint](../EXP-2026-11-22-1691-cold-aware-prefill/README.md).
Apex is evaluated with 32,768 prompt tokens, 512 generated tokens,
4,096/4,096 batches, Q4_0 KV, a 384 MiB activation arena and fixed
full expert host registration (120 ranges, 19,504,005,120 bytes).
Each arm warms the server with one full request and measures the
second request with no reused tokens. Raw responses include 512 IDs
and all probability rows. The arms match these and the response
bytes within each differential.

## EXP-1692: two complete temporary planes

A dedicated release event fences earlier legacy-stream readers.
The copy stream stages the next plane for the current routed layer.
The consumer waits for readiness and validates its tensor, route
membership and SLRU fill generation. Gate and up can be visited
in either order. This remains within one MoE layer.

Replacing 210 MiB of the SLRU bank with a second 210 MiB plane
kept both process peaks at 8,668 MiB. The pipeline consumed 640
prefetches with none discarded. Prefill was 3,009.13 / 2,999.42
tokens/s (single / double), decode 34.25 / 33.95, and client time
25.922 / 26.090 seconds. Both launches had zero system and process
swap. SLRU uploads increased by 6.147 GB over the two requests.
This full-size double-buffer arrangement loses against the larger bank.

## EXP-1693: packed misses in two 105 MiB slices

Only misses take space in a packed temporary slice. Projections
that exceed a slice use the whole 210 MiB workspace, without
prefetch. The persistent bank is 6,190 MiB in both arms. Both
peaks are 8,668 MiB. The packed path consumes 604 prefetches,
with 36 capacity skips and no discarded uploads. All SLRU and
temporary transfer byte totals match. Prefill is 2,996.03 / 2,989.99
tokens/s, decode 34.27 / 33.68, and client 25.960 / 26.244 seconds.
Startup system swap-out was 21 / 52 pages. There is no measured
speed gain in that pair.

## EXP-1694: equal incremental budget for SLRU or workspace

The unchanged runtime admission guard admits 6,500 MiB. It rejects
6,580 MiB and reports 6,849,323,008 bytes available after the same
model allocations and reservation. No allocation OOM is needed.
A 310 MiB workspace with the 6,190 MiB bank is tested. The 390 MiB
workspace is outside the admitted combined budget on this profile.

| SLRU + workspace, MiB | Prefill tok/s | Decode tok/s | Client s | Peak process VRAM MiB | Prefetches used |
|---|---:|---:|---:|---:|---:|
| 6,190 + 210, forward | 3,009.71 | 33.67 | 26.172 | 8,668 | 604 |
| 6,190 + 310, forward | 2,993.08 | 33.85 | 26.155 | 8,768 | 640 |
| 6,290 + 210, forward | 3,013.08 | 33.87 | 26.078 | 8,766 | 607 |
| 6,290 + 210, reverse | 3,015.00 | 34.19 | 25.925 | 8,766 | 607 |
| 6,190 + 210, reverse | 2,937.72 | 33.64 | 26.457 | 8,668 | 604 |

All five arms show zero process swap and zero system swap-out
from launch. Peak RSS spans 21,489,708–21,490,616 KiB. The 310 MiB
workspace removes all 36 capacity skips without improving prefill.
The extra 100 MiB in SLRU lowers SLRU upload counters by 1.938 GB
and temporary uploads by 0.407 GB across the two requests versus
the 6,190 MiB bank. Total client time improves in both orders,
but the forward and reverse gains differ in magnitude.

Use 6,290 + 210 MiB, cold-aware mode and full host pin for this
verified experimental checkpoint. Keep the 6,190 + 210 MiB
checkpoint as rollback. The clean absolute sample is 3,015.00
prefill / 34.19 decode tok/s. The target 3,043 / 67 remains open.

The fixed-budget comparison does not support adding a third full
plane or enlarging the number of reserved copy buffers. The
prefill/decode scratch handoff remains an unmeasured design:
reusing these bytes as cache slots would require invalidating
those slots before the next prefill and retaining stable device
offsets and CUDA Graph dependencies.

Apply the three included patches on top of the EXP-1691 artifact's
driver to reproduce the final executable. Only the CUDA driver
object was rebuilt. Runtime options:
`GOOSE_CUDA_PREFILL_TEMP_WEIGHTS=1`,
`GOOSE_CUDA_PREFILL_TEMP_WARM_ONLY=1`,
`GOOSE_CUDA_PREFILL_TEMP_DOUBLE=1`,
`GOOSE_CUDA_PREFILL_TEMP_COMPACT=1`,
`GOOSE_CUDA_PREFILL_TEMP_MIB=210`, `GOOSE_EXPERT_PIN_MIB=18601`,
and `--expert-cache-mib 6290`.
Keep the other options and original request from EXP-1687.

The binary SHA-256 is
`9b70ef5c535373139df9586229c15f796bda04a4cf3bc1b25ead75eda924514b`.
Raw artifacts, binary and archive are in the worktree under
`build/exp1694-prefill-workspace-budget`, with full A/B results
in `comparison.json`. Earlier stages are at
`build/exp1692-double-prefill-plane` and
`build/exp1693-compact-prefill-pipeline`, with the admission probe
in the latter's `admission` subdirectory.
