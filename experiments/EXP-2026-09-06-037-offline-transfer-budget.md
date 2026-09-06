# EXP-2026-09-06-037 — Offline transfer budget and dependency map

Status: `ACCEPTED` (`kind=infrastructure`, offline analysis, no performance claim)

## Hypothesis

A noticeable part of bulk-prefill latency can come from host-side submission,
synchronization, or the absence of a safe H2D/compute overlap — and this can be
checked against the already-saved EXP-031b/EXP-032 traces without any code
change. The experiment decides offline whether R4 (bounded pinned ring) and
R5 (intra-layer transfer/compute overlap) have a justified budget or must be
`NOT_RUN`.

## Inputs (saved data only)

- `results/archive/EXP-2026-09-06-034/raw/exp031b/exp031b-cuda-prefill-1788632387.sqlite` (Nsight Systems export, read-only SQL)
- neighboring `exp031b` artifacts: `.nsys-rep`, server log, response JSON, `exp031b-stats.txt`
- `results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/` (`h2d.log`, server log, request/response)
- cards and JSON of EXP-030–033; rejected EXP-033 patch as negative evidence only

## Fixed control numbers (from saved records, not re-measured)

- EXP-031: prompt 4016.342 ms, bulk ~3760 ms, 17,975 H2D calls, ~22,207.85 MiB; GPU H2D timeline 1661.18 ms; kernel sum 438.91 ms.
- EXP-032: prompt 4029.308 ms, 17,886 H2D calls, 22,160.56 MiB; H2D submission span 3634.692 ms; median copy 900 KiB, p90 2700 KiB, p99 5400 KiB.
- EXP-033: prefill 3979.34 -> 6502.22 ms, +63.4%, REJECTED (single-buffer event serialization).

## Method

1. SHA-256 of all analyzed artifacts; SQLite opened strictly read-only.
2. Establish the measured-request window; correlate server log, CUPTI CUDA API
   records and GPU activity records only via proven timestamp/correlation links.
   Different clock domains are never mixed without a demonstrated conversion.
3. Offline budget: H2D count/bytes, copy-size percentiles, host CUDA API sum vs
   union, GPU H2D sum vs union, kernel union, real H2D/kernel overlap,
   synchronization API durations, gaps without CUDA activity (called gaps, not
   CPU idle), per-stream distribution, share of adjacent/sequential expert
   ranges. Overlapping intervals are never summed.
4. Dependency map from current sources: `ggml_backend_sched_compute_splits`,
   `copy_experts` branch, `ggml_backend_cuda_set_tensor_async`, staging
   allocation/reuse, copy enqueue, synchronization/events, consuming compute
   launch, last destination consumer. Verify that exact-adjacent-range merging
   already exists; propose no new coalescing patch.
5. Gate decisions per `docs/QWEN-IMPLEMENTATION-TASK.md` section 7.

## Allowed / forbidden

- Allowed: read-only SQL over the saved trace, source reading, arithmetic.
- Forbidden: server/model start, benchmark, Nsight, profiler, new build,
  runtime code change, applying the rejected EXP-033 patch, re-measuring the
  known H2D count and 22.16 GiB, moving to R4/R5 before this card is decided.

## Accept / reject criteria (for the R4/R5 decision, not for EXP-037 itself)

- R4 allowed only if a host-side submission/sync bottleneck is proven, two
  staging slots are safely possible, and expected net gain is at least 5% with
  margin over the 3% threshold; otherwise `NOT_RUN`.
- R5 allowed only if a genuinely independent H2D/compute window is proven with
  a clear destination-buffer lifetime; otherwise `NOT_RUN`.
- If offline data cannot support the budget: EXP-037 ends `INCONCLUSIVE`
  without new measurements. Any divergence above 2% from the saved summaries
  must be explained or the analysis is `INCONCLUSIVE`.

## Artifacts

- `experiments/EXP-2026-09-06-037-offline-transfer-budget.md` (this card)
- `benchmarks/exp037-offline-transfer-budget.json`
- `results/archive/EXP-2026-09-06-037/analysis.sql`
- `results/archive/EXP-2026-09-06-037/offline-results.txt`

## Results (offline, single source: exp031b SQLite + exp032-h2d-ts logs)

Request window in the trace: WS=10417843203 ns to WE=14363385682 ns since profiler
session start; span 3945.542 ms. Server log order-anchored only; EXP-032 h2d.log
clock never joined to CUPTI ns.

| Item | Value |
| --- | ---: |
| H2D calls / volume (window) | 17,975 / 22,207.85 MiB (matches EXP-031 exactly) |
| Expert H2D on stream 16 | 17,886 / 22,160.56 MiB (equals EXP-032 to the byte) |
| Expert H2D submission span | 3610.294 ms (EXP-032 run: 3634.692 ms; 0.7% cross-run) |
| Copy bytes median / p90 / p99 / max | 922,112 / 2,765,312 / 5,530,112 / 16,015,360 B |
| Host cudaMemcpyAsync sum/union | 3118.249 / 3118.249 ms (18,445 calls, mean 164.2 us) |
| Host memcpyAsync + streamSynchronize union | 3516.785 ms = 89.14% of the window |
| cudaStreamSynchronize | 1,114 calls, 395.664 ms host, max 8.622 ms |
| GPU H2D sum/union | 1661.183 ms (saved 1661.18) |
| Kernel sum/union | 438.915 ms (saved 438.91) |
| H2D/kernel intersection | 0.000 ms |
| GPU busy union / gaps without CUDA activity | 2112.785 ms / 1832.758 ms (67 gaps >1 ms, max 58.1 ms) |
| DtoH / DtoD (window) | 222 / 168.43 MiB; 817 / 471.60 MiB (566 via 2D-async, not experts) |
| Streams | 16 = main (copy+kernel+sync), 13 = readback/aux 260 copies, 17 = minor |
| Adjacent consecutive expert ranges in h2d.log | 0 of 17,885 pairs (99.2% same tensor; min intra-tensor gap 524,288 B) |

Exact-adjacent merging already exists: `ggml-backend.cpp:1705-1727` merges
`id == last_id + 1` runs into one `copy_experts` submission. Zero adjacent pairs
in the log is consistent with that. No new coalescing patch.

Dependency map: `ggml_backend_sched_compute_splits` (ggml-backend.cpp:1596) copies
inputs per split then enqueues compute (1745). The weights branch (1643-1727)
reads router IDs host-side with a full backend synchronize per node, then submits
`ggml_backend_tensor_set_async` from the raw pageable mmap pointer
(ggml-cuda.cu:2439 `ggml_backend_cuda_set_tensor_async` -> `cudaMemcpyAsync` on
`cuda_ctx->stream()`, the same single compute stream used by kernels). No host
staging on the expert path; destination is a persistent device weights tensor,
last consumer = MUL_MAT_ID of the same split, next overwrite only next request.

## Decision

Hypothesis confirmed. The dominant serial component of the bulk window is host
pageable submission: host-side CUDA copy/sync API union covers 89.14% of the
3945.542 ms window while GPU busy union is 2112.785 ms and H2D/kernel
intersection is exactly 0. Copy and compute never overlap; 1832.758 ms are gaps
without CUDA activity (GPU waiting on submission, not proven CPU idle).

- R4/EXP-038: ALLOWED. Host submission bottleneck proven; two bounded pinned
  slots are safe per the lifetime map; predicted net gain range 10-35%
  (central ~20%) exceeds the 5% gate with margin. Unknowns named: single-thread
  memcpy bandwidth, overlap fraction at ring depth 2, page-cache state. Running
  EXP-038 still requires Igor's approval per AGENTS.md hard rule 1.
- R5/EXP-039: NOT_RUN. No proven independent intra-layer window: intersection
  0.000 ms, total kernel union only 438.915 ms caps compute-side hiding at ~11%,
  and later-node IDs cannot exist before prior-split compute. Destination
  lifetime is clear but the window requirement fails; direction DEFERRED.
- Consistency: every recomputed value matches saved EXP-031/032 summaries within
  0.01% (exact on counts and bytes). No INCONCLUSIVE trigger.

EXP-037 does not claim any speedup; it only gates the next experiments.
