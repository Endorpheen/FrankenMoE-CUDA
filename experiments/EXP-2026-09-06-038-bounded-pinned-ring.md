# EXP-2026-09-06-038 — Bounded pinned host ring (R4)

Status: `ACCEPTED` (`kind=performance`; model-free checks and formal 5-pair model A/B PASS)

## Hypothesis (one)

Two pinned host slots of 16 MiB let the scheduler copy the next expert range from the
pageable mmap into a free slot while the H2D of the previous slot is still in flight.
This removes part of the confirmed 3118 ms host submission (EXP-037) and cuts median
prefill by at least 3%.

## Evidence gate (from EXP-037, not re-measured)

- Host-side union memcpyAsync+streamSynchronize = 3516.785 ms = 89.14% of the 3945.542 ms request window.
- memcpyAsync host union alone = 3118.249 ms over 18,445 calls (median 164.2 us per call).
- GPU H2D union 1661.183 ms; kernels union 438.915 ms; H2D/kernels intersection exactly 0 ms.
- Expert copies: median 900.5 KiB, p90 2700.5 KiB, p99 5400.5 KiB, max 15.27 MiB (all fit one 16 MiB slot).

## What is NOT repeated

- No EXP-034-037 recomputation, no new Nsight/H2D attribution.
- No packing, no device scratch, no scatter kernels (rejected EXP-033).
- No coalescing work: exact-adjacent expert IDs are already merged in `ggml_backend_sched_compute_splits` (ggml-backend.cpp:1705-1727).
- No R5/EXP-039 work (NOT_RUN/DEFERRED).
- `work/llama.cpp-integration` is never touched; all edits live in the isolated copy.

## Isolation

- Sources: `work/llama.cpp-exp038-pinned-ring` (copy of `work/llama.cpp-integration` at pin `4aaad5d` + local MTP changes)
- Build: `build/exp038-pinned-ring`

## Design (fixed before code change)

### Components

- Two context-owned pinned host slots, 16 MiB each, allocated with `cudaHostAlloc` in
  `ggml_backend_cuda_context` (backend-owned object, deleted in `ggml_backend_cuda_free`;
  NOT process-global).
- Host budget: 2 x 16 MiB = 32 MiB pinned; metadata (slot state, events, flags) < 1 MiB.
- One `cudaEvent_t` (DisableTiming) per slot, recorded on the existing compute stream after the slot's H2D.
- No device-side staging buffer, no scatter kernel. Destination stays the original
  weights buffer at the original offset.

### Slot state machine

`FREE -> CPU_FILL -> H2D_IN_FLIGHT -> FREE`

1. Before CPU_FILL the slot's outstanding event is checked; if it has not completed,
   `cudaEventSynchronize` waits. A slot must not be refilled before its CUDA event completes.
2. CPU_FILL: plain `memcpy` from the pageable mmap source into the pinned slot.
3. H2D_IN_FLIGHT: `cudaMemcpyAsync(slot -> tensor->data + offset, cudaMemcpyHostToDevice)`
   on the existing CUDA stream (`cuda_ctx->stream()`), then `cudaEventRecord` on the same stream.
4. FREE again once the event completes (observed by the next `cudaEventSynchronize` for that slot).

With two slots the CPU memcpy of slot N+1 overlaps the queued H2D of slot N.

### Copy ranges

- Each scheduler `copy_experts` range (already merged for adjacent IDs, padding included)
  is submitted through the ring in chunks of at most 16 MiB; a chunk occupies exactly one slot.
- The 512-byte trailing padding is part of the copied byte range and comes from the original
  source data (same bytes the current pageable path sends).
- Chunks of one range go to consecutive slots on the same stream, so destination ordering is preserved.

### Activation and opt-in

- Env `GGML_EXPERT_PINNED_RING`: unset or `0` disables the path completely; any other value enables it.
- Activation site is the existing scheduler weights branch only
  (`ggml_backend.cpp:1643-1727`): host weights buffer + `GGML_OP_MUL_MAT_ID` node + proven
  bulk-prefill shape (`ids_tensor->ne[1] >= 8` tokens; `ggml_mul_mat_id` requires this to equal
  `node->src[1]->ne[2]`; EXP-030/031b proved 391-token batches;
  single-token decode and small MTP verify batches keep the normal path).
- New optional backend interface member in `ggml-backend-impl.h` (`ggml_backend_i`):
  `bool (*set_tensor_async_pinned_ring)(...)`. Other backends leave it null (designated
  initializers). Scheduler calls it only when the branch conditions hold and the backend
  implements it; returning false falls back to the original `ggml_backend_tensor_set_async`.
- CUDA graph capture guard: if `cudaStreamIsCapturing` reports capture in progress, the ring
  returns false and the copy goes through the original path (replay must never embed staged
  host pointers that could become invalid; pinned slot lifetime is tied to the backend context
  which outlives any graph, but the ring's host-side event waits are illegal during capture anyway).

### Allocation failure and fallback

- On the first eligible call, both slots and both events are allocated before any staged
  submission. If any allocation fails: free what was allocated, clear CUDA errors, log once,
  mark the ring DISABLED for the context lifetime, and use the original pageable path for
  every subsequent copy. Compute never proceeds on a partially staged load.
- Flag off or allocation failure => byte-identical original path.

### Lifetime and cleanup

- Slots/events belong to `ggml_backend_cuda_context`; freed in its destructor (reached via
  `ggml_backend_cuda_free`): outstanding slot events are synchronized (drain), then
  `cudaEventDestroy` + `cudaFreeHost`, then the existing stream/handle teardown.
- No process-global ring state.

### Files to change (preferred area, minimal)

- `ggml/src/ggml-backend.cpp` — weights-branch gate + optional call.
- `ggml/src/ggml-backend-impl.h` — one optional interface member (last position).
- `ggml/src/ggml-cuda/ggml-cuda.cu` — ring implementation + interface hookup.
- `ggml/src/ggml-cuda/common.cuh` — ring fields inside `ggml_backend_cuda_context` + destructor drain.

## Model-free checks (before any server run)

1. Isolated candidate builds.
2. Flag OFF uses the original path (no ring log, ring never initialized).
3. Flag ON activates the ring (WARN-level init log, staged counters > 0).
4. Byte equality at destination for sparse and consecutive expert IDs.
5. First and last expert (edge IDs, padding_end rule at last expert).
6. Padding bytes and 16 MiB chunk boundary correctness.
7. At least three slot-reuse cycles.
8. Sentinel bytes around the destination range are unchanged.
9. Allocation failure returns to the old path (fault injection).
10. Shutdown/cleanup leaves no pinned memory/events (allocated == freed).
11. CUDA graph replay path stores no invalid host pointers (capture falls back).
12. The scheduler fixture uses 395 tokens and verifies that `ids->ne[1] == src1->ne[2]`;
    the valid `MUL_MAT_ID` graph therefore disproves the proposed gate replacement.

## Model A/B

- Control A: current server binary (exp023-mtp-sidecar), flag unset.
- Candidate B: exp038 build, `GGML_EXPERT_PINNED_RING=1`.
- 5 interleaved warm-cache pairs, fixed request. Cold and malformed preflight runs were retained
  separately and excluded before the formal block.
- Immediate abort on error/OOM/swap/output corruption/prefill degradation >10%.
- ACCEPT: median prefill gain >= 3%, win in >= 4 of 5 pairs, decode regression no worse than 2%.
- A working ring or fewer calls without measured speedup is NOT success.

## Verdict

`ACCEPTED` (`kind=performance`). The candidate reduced median prompt latency from 3990.943 ms
to 3465.613 ms (`13.163%`), won all 5 pairs, and passed the pre-registered correctness,
memory, activation, and decode gates. The next roadmap stage is R6 acceptance coverage; R5 remains
`NOT_RUN`/`DEFERRED`.

## Model-free check results (2026-09-06, no server, no model)

Harness: `results/archive/EXP-2026-09-06-038/tests/test-exp038-ring.cpp` (+ `run-model-free.sh`, logs `log-*.txt`).
Two modes: `ring` (direct backend-interface staging) and `sched` (real scheduler weights branch with
MUL_MAT_ID, host WEIGHTS buffer, 395-token batch, deterministic ids covering all experts).

| # | Check | Result |
| --- | --- | --- |
| 1 | Isolated candidate builds (`build/exp038-pinned-ring`, server built) | PASS |
| 2 | Flag OFF: ring never initialized, every staged call declined, pageable fallback | PASS |
| 3 | Flag ON: WARN-level ring init logged (`expert ring enabled`), all calls staged | PASS |
| 4 | Byte equality at destination for sparse and consecutive IDs (ring + sched output sha identical across OFF/ON/alloc-fail) | PASS |
| 5 | First expert (offset 0) and last expert (exact to buffer end, no trailing padding) | PASS |
| 6 | Padding bytes copied; 16 MiB chunk boundary split (16 MiB+512 -> 2 chunks, 20 MiB+512 -> 2 chunks) | PASS |
| 7 | >=3 slot reuse cycles (8 chunks over 2 slots) | PASS |
| 8 | Sentinel guards around destination and uncovered expert untouched | PASS |
| 9 | Allocation failure (`GGML_EXPERT_RING_ALLOC_FAIL=1`): fallback logged, ring marked disabled, old path used | PASS |
| 10 | Shutdown frees ring: no CUDA error on context free; `/proc/self/status` VmPin unchanged (note: CUDA pinned pages are not reflected in VmPin, weak signal) | PASS (weak) |
| 11 | Capture guard: ring declines to stage while a CUDA graph capture is in progress on the compute stream | PASS |
| 12 | 395-token scheduler fixture: token dimensions equal, activation logged, staged calls/chunks non-zero | PASS |

The aborted preflight is not performance evidence. The first valid A request took 21782.95 ms
with a cold file cache; the following B request took 3505.62 ms after the cache was warm. The
candidate INFO activation line was hidden by the server log level. The same code path activates
with the unchanged gate in the 395-token scheduler fixture, so changing the gate to the equivalent
`node->src[1]->ne[2]` expression is neither necessary nor testable as a behavioral fix.

Candidate binary: `build/exp038-pinned-ring/bin/llama-server`, sha256 `fbbdd956df1d99afae8700139a40639daea5b9d8bb40451f803310650d43d707`.
Candidate CUDA library: `build/exp038-pinned-ring/bin/libggml-cuda.so.0.22.0`, sha256
`b1b9ca799383167d4cbf547f3968d4e2d7fbecd057230b618132678aa02dbbef`.
Patch: `results/archive/EXP-2026-09-06-038/exp038-pinned-ring.patch`, sha256
`e1e6803cec1e26099ee452fcb3537b7e660132e6e30da69febffca4785dbd7de`
(4 files, 149 insertions).

## Formal model A/B results (2026-09-06)

The senior helper in Codex executed all measurements manually and sequentially. Each arm used a
fresh server process with the same model, `-c 196608`, `-np 1`, 12 threads, EHS disabled, MTP
`n_max=2`, the same 395-token prompt, 48 predicted tokens, temperature 0, and prompt cache disabled.
The candidate's only runtime difference was `GGML_EXPERT_PINNED_RING=1` and the isolated EXP-038
binary/library. Every server exited via SIGINT; port 8081 was free after the block.

| Pair | Order | A prompt ms | B prompt ms | Gain | A decode tok/s | B decode tok/s |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1 | A, B | 4361.766 | 3854.742 | 11.624% | 21.061 | 22.163 |
| 2 | B, A | 3930.674 | 3657.907 | 6.939% | 21.395 | 22.433 |
| 3 | A, B | 3990.943 | 3412.125 | 14.503% | 20.767 | 22.015 |
| 4 | B, A | 3904.822 | 3397.844 | 12.983% | 21.641 | 22.759 |
| 5 | A, B | 4007.060 | 3465.613 | 13.512% | 20.676 | 21.996 |

- Median A/B prompt latency: 3990.943 / 3465.613 ms; median-arm gain `13.163%`.
- Median paired gain: `12.983%`; wins: `5/5`.
- Exact paired bootstrap (all `5^5` resamples), 95% percentile interval for mean pair gain:
  `[9.357%, 13.803%]`, entirely above zero.
- Median decode: 21.061 -> 22.163 tok/s (`+5.234%`), so the `-2%` regression gate passes.
- All 10 formal requests returned HTTP 200 and the same completion content SHA-256:
  `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1`.
- VmSwap was 0 before and after every request; no CUDA error, OOM, hang, or output corruption.
- Every B log contains exactly one activation line and one shutdown summary with
  `17886 calls, 17886 chunks`. This count matches EXP-032's expert H2D attribution.
- Median post-request RSS delta B-A was `24732 KiB`, within the fixed 32 MiB pinned budget plus
  metadata allowance. Device staging remains zero.
- Machine-readable results: `results/archive/EXP-2026-09-06-038/ab/results.json`; compact report:
  `results/archive/EXP-2026-09-06-038/ab/results.txt`.
- Excluded provenance directories: malformed endpoint, two cold-cache controls, and the original
  B run whose INFO activation message was below the visible server log level. None enters the
  statistics.
