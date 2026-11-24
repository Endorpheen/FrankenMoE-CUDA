# EXP-2026-11-24-1862 — Clean paired validation of causal Flash interior tiles

**PROVEN for Apex prefill.** Baseline promotion remains pending memory
parity. This supersedes the throughput comparison under system swap-out
in [EXP-1861](../EXP-2026-11-24-1861-flash-interior-ab/README.md).

Apex 32K/512 ran on RTX 4070 with context 262144, batch and ubatch 4096,
Q4_0 KV, 6290 MiB expert cache and decode split 4/count 1. One warm-up
and one scored request ran on each fresh server. The same embedded
executable was used for all arms. A set `GOOSE_FLASH_INTERIOR=0`;
B left it unset. Server logs confirmed the dispatch difference.

| Arm | Prefill tok/s | Decode tok/s | Client s | Peak sampled VRAM MiB | Peak RSS KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 3208.98 | 83.27 | 16.437 | 8774 | 21490568 |
| B1 | 3344.53 | 83.50 | 16.006 | 8776 | 21492072 |
| B2 | 3336.62 | 83.54 | 16.026 | 8776 | 21490696 |
| A2 | 3239.05 | 80.84 | 16.524 | 8774 | 21491876 |

Mean prefill moved from 3224.01 to 3340.57 tok/s (+3.62%, 355 ms).
Both timed pairs favored B. Mean measured decode was 82.06 vs 83.52
tok/s. The controls varied by 2.43 tok/s and this experiment does
not establish a direct decode effect. All eight replies matched all
512 IDs and the text, and every arm matched the accepted control's
SLRU counters and other post-request properties byte-for-byte.
Runtime swap and system swap-out both stayed at zero. Peak RSS
ranges overlap. GPU samples at 250 ms show a repeatable 2 MiB
increase with the extra module. The mechanism of that increase has
not been isolated beyond toggling the new dispatch.

Reproduce with the binary and manifest from EXP-1861 and the flags
above. Keep the official profile fixed and run A-B-B-A with a warm-up
per arm. Check the original IDs, rendered text, SLRU data, swap
and memory. Raw run IDs, timings and sample counts are in
[results.json](results.json).

Next isolate whether the two MiB come from resolving the original
kernel before immediately replacing it. When using the embedded
module, delay the original function lookup until the embedded
lookup fails or the feature is disabled. Verify memory parity and
repeat the measured comparison before promoting a new checkpoint.
