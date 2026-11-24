# EXP-2026-11-24-1858 — Move the prefill DMA wait after Q8_1 packing

Status: **not adopted**. On the current mailbox and deferred-decode-fence
checkpoint, give the two existing prefill slabs separate readiness events.
A reused slab's wait moves to the first consuming MMQ launch, after the
independent activation pack. Other consumers retain their original fences.
No cache, context, arithmetic or slab capacity changes.

## Apex 32K/512 on RTX 4070

Context 262144, batch and ubatch 4096, Q4_0 KV, expert bank 6290 MiB.
One warm-up and one measurement per server, same binary for every arm.

| Arm | Prefill tok/s | Decode tok/s | Client seconds |
| --- | ---: | ---: | ---: |
| A1 | 3247.150 | 83.476 | 16.301410 |
| B1 | 3255.594 | 83.362 | 16.284384 |
| B2 | 3231.444 | 82.942 | 16.392536 |
| A2 | 3244.880 | 83.360 | 16.318195 |

Paired mean prefill changed 3246.015 to 3243.519, or -0.077%. Decode
changed 83.418 to 83.152, or -0.319%. All 512 IDs, text and cache counters
matched on all eight requests. VRAM was 8774 MiB in every arm, swap-out
was zero. Candidate logs reached 600 deferred MMQ consumer fences.

## Full-window trace evidence

The control profile marks all eight measured prefill ubatches. Causal
Flash totals 1711.6 ms across 80 launches. The eight consecutive
10-launch groups took 28.1, 82.3, 133.3, 186.9, 238.9, 292.6, 346.7
and 402.7 ms. An earlier 479 ms result used a shorter diagnostic window.

The combined MMQ packing kernels occupy 381.4 ms, intersecting observed
H2D copies for 28.2 ms. The candidate trace could not initialize the
6290 MiB bank under reduced free VRAM. Consequently the active control
trace provides a budget, but there is no GPU-side A/B overlap claim.

The code is preserved as an experimental patch only. Build it against
EXP-1856 with the same CUDA images and archive. Enable with
`GOOSE_EXPERIMENT_PREFILL_CONSUMER_FENCE=1`. The full-window NVTX marker is
in [source/mark-prefill.c](source/mark-prefill.c). Raw run IDs and exact
measurements are recorded in [results.json](results.json).
