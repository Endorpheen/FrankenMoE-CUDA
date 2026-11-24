# EXP-2026-11-24-1881 — Submit predictive expert copies after the early decode split

**INCONCLUSIVE and not adopted for decode.** EXP-1880 remains the
baseline. Keep the existing early FFN submission. This candidate
queues the same predictive expert wave just after the early
four-kernel graph launch of the next layer. The intent is to overlap
host prediction polling and DMA submission with those kernels.
The fallback drains a pending wave before the next routed access
or on successful graph completion. This preserves route values,
expert-cache access ordering and the existing wait events. It
differs from the earlier experiment that delayed the entire wave
until the next blocking route read.

## Apex 32K/512 on RTX 4070

Context 262144, expert cache 6290 MiB, Q4_0 KV, batch and ubatch
4096, decode split 4/count 1. One warm-up and one scored request
per fresh process. A-B-B-A on the same executable. A sets only
`GOOSE_PREFETCH_AFTER_SPLIT=0`, B leaves it unset.

| Arm | Decode tok/s | Prefill tok/s | Client s | Predictor wait at 10000 waves, ms |
| --- | ---: | ---: | ---: | ---: |
| A1 | 81.36 | 3513.10 | 15.693 | 39.870 |
| B1 | 82.19 | 3504.74 | 15.653 | 12.726 |
| B2 | 82.54 | 3524.13 | 15.579 | 53.561 |
| A2 | 83.76 | 3477.06 | 15.612 | 84.708 |

Decode changed by +1.02% and −1.46% in the two pairs.
Mean decode was 82.56 versus 82.37 tok/s (−0.24%).
Prefill changed from 3495.08 to 3514.43 in the paired mean and
client time shortened by 36.5 ms. The patch is decode-only.
The prefill difference is not attributed to this change.

The cumulative prefetch counters agree across arms at 19958
predictions, 10000 waves, 48873 jobs and zero skips. Both
candidate logs reached 30000 after-split invocations without a
recorded fallback. Mean cumulative wait was 62.3 versus 33.1 ms
at the logging point. Mean decode did not improve, so the reduced
host polling is not counted as critical-path savings. Quantized
weights, outputs, all 512 IDs, exact text and post-request
properties matched. VRAM peaked at 8774 MiB in each arm.
Peak RSS ranged from 21491976 to 21493688 KiB, process swap
and system swap-out were zero.

## Reproduce

Apply [source/cuda-driver.patch](source/cuda-driver.patch) to
the exact EXP-1880 driver. Relink only its driver object against
the frozen archive, following the GCC 13 flags and include paths
from EXP-1863. Keep its original image headers, argv and env.
Verify [SHA256SUMS](SHA256SUMS). Use the toggle only in A.
See [results.json](results.json) for raw run identifiers,
reply hashes, counters and timings.
