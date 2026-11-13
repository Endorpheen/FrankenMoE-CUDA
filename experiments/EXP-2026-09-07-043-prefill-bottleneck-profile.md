# EXP-2026-09-07-043 — what limits prefill after the pinned ring (PROTOCOL + STAGE2 RESULT)

Status: `STAGE2-DONE` (model profile completed 2026-09-08, rule 1 agreed, Igor stopped
his own server; offline tail attribution from the saved trace — 2026-09-08, no new runs).
**Diagnosis: case 1 — CPU gather-bound. There are NO grounds to move to limited-gap A
(EXP-042)** (A adds bytes to gather and H2D, the predicted saving is smaller than the
losses; arithmetic below, both values are estimates, no A/B was run).
Side finding: the 217.774 ms tail after the last gather (≈8.5% of the window) has been
attributed — it is NOT per-token draft: the checkpoint break cuts the prompt into 391+4,
the ~112.6 MiB checkpoint save sits on the critical path, the draft runs as a single
batched pass (391 rows), and the final 4 trunk tokens are computed with CPU experts
(batch < 32). Details — "Tail attribution" and the diagnosis table below.

STAGE1 (model-free, night of 2026-09-08): the diagnostic build was compiled and verified
model-free (harvest EXP-041): the `expert_gather/submit/slotwait` intervals are visible in
the shared CUDA/NVTX trace, harness correctness is preserved (ring 9/9, sched sha
identical), primitive overhead ~38 ns/op (~2.0 ms CPU-side upper bound on a 395-token
prefill, >100× below the 3-5% target). Artifacts:
`results/archive/EXP-2026-09-07-043/` (patch `exp043-diag.patch`, `STAGE1-RESULTS.txt`,
logs, probe).

## Question

After the pinned ring was accepted (EXP-038..041, default-on), part of prefill time still
goes to servicing many separate H2D transfers or to something else. Distinguish among
three limitations on the current EXP-041 for the saved 395-token request:

1. The CPU is busy gather-copying weights into pinned slots.
2. The GPU is genuinely starving because submission/events/slot lag behind (starvation on
   the GPU timeline).
3. The transfers hit the achievable PCIe bandwidth of this path.

Decision logic: test limited-gap A (EXP-042, ≤512 KiB) only if case 2 is confirmed — that
is, the GPU timeline shows starvation that is removed by reducing the number of calls.
Cases 1 and 3 mean only "no grounds to move to A yet" (A adds bytes and gather). A
different location of the delay yields a new optimization target.

## Tools (scheme B, refined)

A. nsys CUDA tracing (GPU side):
   `nsys profile -o exp043prefill -t cuda,nvtx --force-overwrite true bash scripts/run_qwen38_server.sh`
   then a single saved 395-token request (`.../exp032-h2d-ts/request.json`), SIGINT;
   `nsys export --type sqlite`. Extraction inside an explicit request window (see below):
   GPU `Memcpy HtoD` (count/bytes/union busy -> bandwidth), GPU idle intervals and their
   position, API `cudaMemcpyAsync`/`cudaEventRecord`/`cudaEventSynchronize`/
   `cudaStreamSynchronize`.

B. Diagnostic build `build/exp043-diag` (separate directory; the working one is not
   touched) with INTERVAL ANNOTATION, not only sums. In
   `ggml_backend_cuda_set_tensor_async_pinned_ring`:
   - NVTX range `expert_gather` around the `memcpy` into the slot;
   - NVTX range `expert_submit` around `cudaMemcpyAsync`;
   - NVTX range `expert_slotwait` around `cudaEventSynchronize` on slot reuse;
   - NVTX range `expert_prefill` (start/end) around the entire prefill under study
     (window markers).
   Additionally, cheaply accumulate `gather_ns/submit_ns/slot_wait_ns` and `calls/chunks`
   for the summary.
   Key point: the gather/submit/slotwait intervals land in the shared CUDA trace and are
   CORRELATED with the GPU timeline (GPU idle intervals vs ranges on the CPU thread).
   This is what is needed for cases 1/2.

   Annotation cost is accounted for: before the model run, measure the
   NVTX+`clock_gettime` overhead against gather time on the model-free EXP-041 harness;
   if the overhead is comparable to gather, narrow the annotation (for example, gather
   only) or drop it in favor of GPU window analysis.

   Feasibility confirmation (offline, done): `nsys profile -t nvtx` writes NVTX ranges
   into the `NVTX_EVENTS` table (the test probe produced 5/5 intervals). So the interval
   scheme works on this machine; the `-t cuda,nvtx` profile is mandatory.

C. CPU sampling (`nsys --sample=cpu`, `perf`) is NOT an equivalent replacement for the
   intervals from B: it gives only the CPU distribution over symbols, without tying
   pauses to the GPU timeline. It is also blocked in the current environment
   (`perf_event_paranoid=4`). Therefore C is not used; scheme B covers CPU gather with
   intervals.

## The window of the prefill under study (explicit)

- The window start/end is defined by the NVTX range `expert_prefill` (or by server
  request start/complete markers), NOT by the "first H2D". The window starts BEFORE the
  first gather, to capture the preparation and the first gather (cutting at the first H2D
  loses them).
- Prompt cache policy: `cache_prompt=false`, so that the repeated request actually
  computes the prompt (otherwise there will be fewer/no expert copies — the window is
  wrong). Check in the server log that the prompt was processed (prompt eval), not taken
  from cache.
- The number of expert H2D copies in the window ≈ 17 886 — a consistency check against
  the old workload, not a hard rule; a deviation is explained (parameters/cache) rather
  than automatically meaning "the window is wrong".

## Metrics and thresholds

- PCIe: record the ACTUALLY negotiated link gen/width (`nvidia-smi ...pcie.link.*`;
  currently observed gen4 x16) and the achievable path bandwidth — the peak bandwidth of
  large H2D transfers from the trace itself. Do NOT use a hard "80% of theoretical". A
  value below the theoretical ceiling by itself does NOT rule out a transfer limitation;
  saturation is asserted only if GPU H2D busy fills the window and adding bytes linearly
  lengthens the window (no idle headroom).
- GPU starvation: GPU idle intervals inside the window; correlate them with the
  `expert_gather`/`expert_slotwait` ranges on the CPU thread. A large
  `cudaEventSynchronize` by itself is NOT proof that call servicing is expensive: the CPU
  may be waiting for a slot while the GPU is routinely executing previous copies/kernels
  on the same stream. Case 2 only if the GPU idle is NOT explained by the normal pipeline
  and is removed by reducing the number of calls.

| Observation (in the explicit window) | Diagnosis | Action |
|---|---|---|
| GPU H2D busy ≈ window, bandwidth close to achievable, no idle headroom | 3 PCIe | no grounds to move to A yet |
| gather ranges on the CPU thread fill the window, GPU idle coincides with gather | 1 gather | no grounds to move to A yet |
| GPU idle not explained by the normal pipeline; removed by reducing the number of calls | 2 submission/events/slot | grounds to test limited-gap A |
| delay outside H2D (MMQ kernels, dense, router) | other | new target, A is beside the point |

## Run rules (future launch, per rule 1)

- First a model-free check: build `build/exp043-diag`, run it on the model-free EXP-041
  harness, make sure the gather/submit/slotwait/prefill NVTX ranges are visible in
  `NVTX_EVENTS` and that the annotation overhead is negligible. This is BEFORE the model
  run.
- Then ONE agreed model profile: one warmup request + one 395-token request under study
  (`cache_prompt=false`), strictly sequential.
- A second run — only given a specific uncertainty in the first (for example, the window
  did not isolate or the overhead ate the signal). "At most three for reproducibility" is
  not justified and is not planned.
- Profiled wall timings are not a performance claim. Artifacts in
  `results/archive/EXP-2026-09-07-043/`: window SQL (modeled on `EXP-037/analysis.sql`),
  `offline-results.txt`, counter/NVTX logs, `PROTOCOL.md`.

## Pre-flight artifacts (offline, already done)

- The model-free EXP-041 harness under `nsys -t cuda`: H2D
  (`CUPTI_ACTIVITY_KIND_MEMCPY`) and event API
  (`cudaMemcpyAsync`/`cudaEventRecord`/`cudaEventSynchronize`) are visible; there are no
  CPU samples (`perf_event_paranoid=4`) — hence the move to interval scheme B.
- NVTX probe under `nsys -t nvtx`: 5/5 intervals in `NVTX_EVENTS` — interval gather
  annotation is feasible.
- All of this lives in temporary directories and is not put into the repository.

## What the local agent has already done (STAGE1, model-free)

- The diag. build `build/exp043-diag` is compiled; patch
  `results/archive/EXP-2026-09-07-043/exp043-diag.patch` (3 files: `common.cuh` — 3
  counters after `curr_stream_no`; `ggml-cuda.cu` — NVTX ranges
  `expert_gather/submit/slotwait` + `*_ns` counters + a teardown log;
  `ggml-cuda/CMakeLists.txt` — `libnvToolsExt` via `-Wl,--no-as-needed`). Env gate:
  `GGML_EXPERT_DIAG=1` enables it.
- Model-free check passed: ring 9/9 PASS (with DIAG=1 and without), sched sha
  `39505119…e91510` (equal to the accepted EXP-041). NVTX ranges visible in
  `NVTX_EVENTS` (gather 8 / slotwait 6 / submit 8).
- Primitive overhead measured (`tests/ovh_probe.c`): ~38 ns/op, upper bound ~2.0 ms
  CPU-side on a 395-token prefill. No need to lighten the scheme.
- NOT done (deliberately): the `expert_prefill` window markers and the model profile.

## STAGE2 — assignment for another agent (model profile)

The order is strict; do not touch the model without a separate rule-1 block
(goal/duration/number of runs) and an explicit «да» ("yes") from Igor.

1. Take the same diag patch based on the accepted EXP-041 (`work/llama.cpp-exp041`),
   build your own `build/exp043-diag` (Release, `GGML_CUDA=ON`, arch 89). Check
   `ldd libggml-cuda.so | grep nvTools` — it must show `libnvToolsExt.so.1` (otherwise
   NVTX is not written; see --no-as-needed in the patch).
2. BEFORE the model — repeat the model-free check with the EXP-041 harness: ring PASS +
   `NVTX_EVENTS` contains `expert_gather/submit/slotwait`. This is the gate before the
   model.
3. Window markers `expert_prefill` (still NOT in the patch). Place an NVTX range that
   bounds the processed prompt IN ITS ENTIRETY, starting BEFORE the first gather:
   - preferably at the llama-server level: a wrapper around the prompt eval phase (prompt
     eval) — push on entering prompt evaluation, pop when it completes; the same
     `nvtxRangePushA/Pop` API, env gate on `GGML_EXPERT_DIAG`.
   - or at the `ggml-backend` sched level: `nvtxRangeStart` at the first enqueue of a
     split's expert copies, `nvtxRangeEnd` when the request's last slot completes (make
     sure the range covers ALL splits of one prompt, not each one separately).
   - Window correctness check: exactly one wide `expert_prefill` window in the trace for
     the request under study; exactly one burst of expert_gather/submit inside it; the
     H2D-copy count in the window ≈ 17 886.
4. Server: launch with the same 395-token workload, `cache_prompt=false`; make sure in
   the server log that there was a prompt eval (the prompt was computed), not a cache
   hit.
5. Profile: `nsys profile -t cuda,nvtx -o exp043-model --force-overwrite true
   <server...>`, then ONE warmup + ONE request under study, strictly sequential. A second
   run — only given a specific uncertainty in the first.
6. Export/analysis: `nsys export --type sqlite`, then window SQL modeled on
   `EXP-037/analysis.sql`: inside the `expert_prefill` window, correlate GPU H2D busy
   (from `CUPTI_ACTIVITY_KIND_MEMCPY`) with the `expert_gather`/`expert_slotwait` ranges
   on the CPU thread and find GPU idle. Fill in the diagnosis table (section "Metrics and
   thresholds").
7. Artifacts in `results/archive/EXP-2026-09-07-043/`: `analysis.sql`,
   `offline-results.txt`, server/NVTX/counter logs, update the card status. Do NOT put
   model/sqlite/nsys-rep binaries into git.

ACCEPT/REJECT criterion for limited-gap A (EXP-042): moving to A is justified ONLY if
case 2 is confirmed (GPU idle in the window is not explained by the normal pipeline and
is removed by reducing the number of calls). Otherwise — "no grounds to move to A yet"
(cases 1/3), and A stays parked. Do not count profiled wall timings as a performance
claim.

## STAGE2 — model profile results (2026-09-08)

Gate before the model (all PASS): the `build/exp043-diag` build is clean; `ldd` shows
`libnvToolsExt.so.1` on `libggml-cuda.so`, `libllama-server-impl.so`, `llama-server`; a
probe with the same linking scheme as server-impl (weak `_impl_init_v3` from the header +
strong definitions from the library) produced a range in `NVTX_EVENTS` under nsys; the
EXP-041 harness against the diag build — ring 9/9 PASS (default and DIAG=1), sched sha
`39505119b1…e91510` identical to the accepted one; the `expert_gather/submit/slotwait`
intervals are visible under live nsys. The `expert_prefill` window markers were placed in
`work/llama.cpp-exp043-diag` (`server-context.cpp`: push in the STARTED block BEFORE
prompt work begins, pop on the transition to GENERATING and in `release()` for abnormal
exits; id-based, survives chunked prefill) — patch
`results/archive/EXP-2026-09-07-043/exp043-window.patch` (sha256 `4f88bbf8…da5a`,
regenerated from the stage1 tree; the first variant's CMake hunks were not applied to
this tree). Gate logs: `tests/log-st2-*.txt`.

Run (rule 1: goal/duration/2 requests agreed, an explicit «да» ("yes"), Igor stopped his
own server himself; port 8081): 1 launch of `build/exp043-diag/bin/llama-server` (profile
of the accepted runtime: PINNED_RING default-on, `-np 1`, ctx 196608, KV q4_0, MTP
n_max=2, draft CPU-MoE) + `GGML_EXPERT_DIAG=1`, wrapped in `nsys profile -t cuda,nvtx`;
strictly sequentially 1 warmup + 1 measured 395-token request
(`results/archive/EXP-2026-09-06-034/raw/exp032-h2d-ts/request.json`, `n_predict=0`,
`cache_prompt=false`, temp 0); SIGINT to the exact server pid. Trace of 433 053 events,
sqlite export 22 MB (`profile-binaries-sha256.txt`: nsys-rep `52673442…`, sqlite
`f86cd32b…`; the binaries themselves are in /tmp and are not put into git). Controls
matched: teardown `expert ring staged 35772 calls, 35772 chunks` (exactly 2×17886); in
the measured window exactly 17886 `expert_gather/submit/slotwait`; H2D in the window
17979 copies / 22208.38 MiB; `cache_n=0` (the prompt was computed, not from cache); the
warmup and measured responses are identical (content sha `75a11da4…`, 1 EOG token each —
the known n_predict=0 behavior from EXP-040).

The `expert_prefill` windows: warmup 18621.762 ms, measured 2572.660 ms (server timings
18622.322 / 2573.154 ms — agreement to ~1 ms). Profiler wall time is NOT a performance
claim (without nsys the accepted runtime gives ~1742-3465 ms on this workload in
EXP-040/038); the warmup reflects cold first-touch, the object of study is the measured
window. Warm window summaries: gather 16643.360 / submit 125.147 / slotwait 82.075 ms
(in the cold warmup the GPU almost never waits for slots — gather is slow because of
first page touches).

### Decomposition of the measured 2572.660 ms window (395 tokens)

| Component | Time | Window share |
|---|---|---|
| CPU `expert_gather` (union, contiguous) | 1463.641 ms | 56.9% |
| CPU `expert_submit` (sum) | 66.701 ms | 2.6% |
| CPU `expert_slotwait` (sum) | 326.679 ms | 12.7% |
| GPU H2D busy (union) | 1346.071 ms | 52.3% (achieved 16498 MiB/s ≈ 17.3 GB/s) |
| GPU kernels (union) | 420.620 ms | 16.3% |
| H2D ∩ kernels intersection | 0.000 ms | one stream, strictly sequential |
| GPU busy total (memcpy+kernel+memset) | 1779.320 ms | 69.2% |
| **GPU idle** | **793.340 ms** | **30.8%** (1311 pauses >100 µs, 53 >1 ms, max 47.832 ms) |

Cross table (GPU busy × CPU in gather): both busy 967.188 ms (37.6%) — the ring works,
staging overlaps with H2D; **GPU idle while gather is running 496.453 ms (19.3%)**; GPU
busy without gather 812.132 ms (31.6%); both idle 259.416 ms (10.1%). Host API:
`cudaMemcpyAsync` 18450 calls totaling 97.539 ms (mean 5.29 µs — submission is cheap after
the ring); `cudaEventSynchronize` 17886 × mean 17.95 µs = 321.093 ms;
`cudaStreamSynchronize` 1126 × mean 350.7 µs = 394.856 ms (layer boundaries). Gather
histograms: mean 81.8 µs, p50 62.5 µs, p99 362.3 µs, max 1.369 ms at an average transfer
size of 1.24 MiB — p50 ≈ a pure 1.24 MiB memcpy (~20 GB/s RAM→pinned), the fixed per-call
part is small (~15-20 µs).

Window structure: the expert staging phase (first gather → end of the last gather) takes
~2354.9 ms; the tail after the last `expert_gather` is 217.774 ms (in the early summary
"~259 ms" — the boundary was drawn at the end of the last staging interval from
analysis.sql; below the exact boundary SE = end of the last gather is used). The tail is
NOT the expert path (after SE exactly 0 gathers; the early summary's claim of "382 gather
/ verify loop" is wrong and retracted) and NOT per-token draft processing: see "Tail
attribution" below — it is the prompt checkpoint break (391+4), the 112.6 MiB checkpoint
save on the critical path, the CPU-MoE draft as a single batched pass, and the final
4-token trunk batch with CPU experts.

### Diagnosis table (from the section "Metrics and thresholds")

| Observation in the window | Diagnosis | Verdict |
|---|---|---|
| gather takes 56.9% of the window; GPU idle coincides with gather (496.5 ms) + CPU labor without gather (259.4 ms); p50 gather ≈ pure memcpy | **1 — CPU gather-bound** | **confirmed** |
| GPU idle of 793.3 ms exists, but calls do not remove it: submission 66.7 ms total; the idle is generated by the byte rate of the CPU copy and CPU labor outside staging, not by the number of calls | 2 — starvation | NOT confirmed |
| GPU H2D busy 52.3% of the window at 17.3 GB/s; idle headroom 793 ms | 3 — PCIe | NOT confirmed |
| ~218 ms tail — checkpoint break (391+4), 112.6 MiB checkpoint save, CPU-MoE draft (batched), final 4-token batch with CPU experts; outside the H2D path | other (side finding) | new candidate target, not assigned |

Arithmetic against limited-gap A (EXP-042, threshold ≤512 KiB: −5570 calls, +2785 MiB
bytes): CPU saving ≈ 5570 × (15-20 µs fixed — THIS IS AN ESTIMATE from submit tails, not
a measured value) ≈ 84-111 ms; losses: gather +2785 MiB / ~20 GB/s ≈ +139 ms CPU and
+2785 MiB / 17.3 GB/s ≈ +161 ms GPU H2D. 2785 MiB = 2.920 GB (decimal); the early
summary wrongly said "2.785 GB". The prediction is negative on both axes, but this is a
calculation, not a measured A/B — EXP-042 A stays parked without a negative experiment.

### Attribution of the 217.774 ms tail (offline, 2026-09-08 — from the saved trace and sources, without runs)

Boundary: SE = end of the last `expert_gather` = 152762456149, WE = end of the window =
152980230160. All offsets below are ms after SE. A correction to the early summary: after
SE the trace has NOT A SINGLE `expert_gather` (0 intervals), so its "382 gather / first
verify loop" is wrong; "sequential per-token MTP-draft processing" is wrong too: the
draft pass is ONE batched pass (confirming Igor's correction: batching of MTP prompt
processing already exists in `common_speculative_impl_draft_mtp::process()`,
speculative.cpp:1520-1636).

| Phase | Offset, ms | Duration | What happens | Code |
|---|---|---|---|---|
| end of batch 1 | +0.0…+2.3 | 2.3 | the trunk's last layers for the 391-token batch (final expert MMQ + dense tail) | end of batch 1's `llama_decode` |
| host preparation | +2.3…+15.0 | 12.7 | decode return, bulk extraction of outputs, `common_speculative_process`: reading the trunk's h_nextn, assembling the draft batch | server-context.cpp:3717-3721 → speculative.cpp:1520-1570 |
| draft inputs upload | +15.0…+16.1 | 1.1 | pageable H2D ~20.2 MiB, sync-per-copy; the large chunk is exactly 16 015 360 B = 391 rows × 40 960 B (h_nextn width 10240 float) | graph inputs `llama_decode(ctx_dft)` |
| **draft pass (c02)** | +16.2…+24.0 | 7.8 | ONE batched pass of 391 rows: eh_proj (vec 6066 µs), dense/attention/shexp on GPU; router and experts are ABSENT on GPU | speculative.cpp:1600 |
| T1 | +24.0…+26.7 | 2.6 | 394 cheap `cudaStreamSynchronize` calls (391 on the compute stream, 0.72 µs each — no-op waits; the count matches the number of draft rows, the exact call site is not resolvable from the trace — see gaps) | — |
| CPU-MoE draft | +26.7…+71.8 | 45.1 | pure CPU, 0 CUDA API: router + `MUL_MAT_ID` of the draft experts (`--spec-draft-cpu-moe`) for 391 rows × top-10 | CPU backend |
| **checkpoint save** | +71.8…+88.1 | 16.3 | D2H ~112.6 MiB: 36×3 MiB + 36×120 KiB + small stuff. The size matches the EXP-040 S-run logs: `created context checkpoint … size = 112.592 MiB` (37 tokens) / `112.919 MiB` (608) — almost independent of length | server-context.cpp:3606-3608 `create_checkpoint` before batch 2 |
| **trunk batch 2** | +88.1…+217.6 | 129.5 | THE LAST 4 PROMPT TOKENS as a separate batch: 48 layers × [GPU cluster router+dense 0.5-3.4 ms (34×87 and 11×116 cores) + 2 small D2H per layer (48×40 KiB + 48×6 KiB — readback of ids/activations for the CPU split) + CPU `MUL_MAT_ID` of experts] | server-context.cpp:3537-3551 (checkpoint break), ggml-cuda.cu:5641 (`min_batch_size` = 32) |
| finale | +217.6…+217.8 | 0.2 | a small 4-row draft pass (clusters of 39+46 cores), logits extraction (1 row = 993 280 B = 248320 vocab × 4), sample → GENERATING (window close) | post_decode → SLOT_STATE_GENERATING |

Answers to the assignment's questions:

1. **The specific call.** The dominant part of the tail (129.5 ms) is the `llama_decode`
   of the second prompt batch of 4 tokens. The batch exists because of checkpoint logic:
   `n_ctx_checkpoints = 32` (default, common.h:639), and prompt-batch filling is cut off
   4 tokens before the end (server-context.cpp:3537-3551, `checkpoint_offsets = {4 +
   n_ubatch, 4}`), so that a checkpoint is created before the last batch. The
   second-largest piece is `create_checkpoint` itself (16.3 ms, 112.6 MiB D2H on the
   critical path). The third is the CPU-MoE draft (45.1 ms, 391 rows in one batch).
2. **Actual batch/ubatch sizes.** Prompt 395 = batch 1 of 391 tokens (n_batch 2048, one
   ubatch: 391 < 512) + batch 2 of 4 tokens; draft = 391 rows in one ubatch (upload
   16 015 360 B = 391×40 960 — exactly one batch) + 4 rows on the second call; there is
   no verify/generation in the window (n_predict=0 → after the sample, 1 EOG token — the
   known EXP-040 behavior).
3. **The reason for the sequence.** (a) the op-offload gate: `MUL_MAT_ID` goes to CUDA
   only at batch ≥ 32 (`get_op_batch_size`→`ne[2]`, `GGML_OP_OFFLOAD_MIN_BATCH` default
   32, ggml-cuda.cu:5641) — the 391-token prefill stages experts on the GPU (17886
   gathers), while the 4-token batch computes experts on the CPU with per-layer
   synchronizations/readbacks; (b) the checkpoint break artificially creates this
   4-token batch and inserts the 112.6 MiB save between the two decodes; (c) draft
   experts on the CPU are the structural price of `--spec-draft-cpu-moe` (VRAM,
   EXP-023/024).

Honest gaps (exactly the missing signals, if it becomes necessary to close them): (1) T1 —
the exact call site of the 391 syncs: from the trace the candidates are indistinguishable
(sched split-handling vs some other loop); resolvable by a run with `GGML_SCHED_DEBUG=1`
or nsys with host call stacks (`--sample=cpu`), both require a separate model run per
rule 1. The contribution is ≤2.6 ms — not a limiter. (2) The exact layer-by-layer
breakdown of the 36×3 MiB inside the checkpoint save (the size matches the known
checkpoints byte-for-byte — this is enough for phase attribution). The upper bound of the
whole tail is 217.8 ms (~8.5% of the window); the realistically removable part is smaller
and requires a separate experiment (for example, behavior at `-ctxcp 0` — but that changes
checkpoint/ctx-shift functionality and is not proposed without Igor's decision).

Candidate observations for FUTURE priorities (not commitments, Igor assigns them): (a)
the 391+4 checkpoint break: ~146 ms of tail (save 16.3 + 4-token batch 129.5) exists only
because the last prompt batch is cut off for the sake of a checkpoint; without the break
the whole prompt would fit into a single 395-token batch with GPU experts; (b) ring
depth: `slotwait` 326.7 ms is the union of slot waits, inside which mostly lie the
321.1 ms of `cudaEventSynchronize` (a nested call — NOT an additive quantity, a separate
648 ms reserve does not exist); (c) the CPU-MoE draft's 45.1 ms on a 395-token prefill;
(d) CPU serialization of router/dense/staging in one thread (812 ms GPU busy without
gather — a window for overlap).

Methodological notes: (1) SIGINT under nsys must go EXACTLY to the server pid —
`pgrep -f` on the launch string also matches the nsys process itself; the first SIGINT
hit the wrapper process, and the profile was NOT lost (the nsys tree survived, a repeated
SIGINT to the exact pid finalized the report correctly); (2) the nvtx3 header emits weak
init stubs — NVTX is written only with `DT_NEEDED libnvToolsExt.so.1` (verified with ldd
on all three binaries); (3) profiler wall timings are slowed by CUPTI/NVTX and are not
performance claims.

STAGE2 artifacts in `results/archive/EXP-2026-09-07-043/`: `analysis.sql` +
`analysis-results.txt` (window SQL modeled on EXP-037, boundaries from NVTX markers),
`profile-server-log.txt` (a copy of the log), `measured-response.json`,
`warmup-response.json`, `profile-binaries-sha256.txt`, `exp043-window.patch`,
`tests/log-st2-*.txt`. Binaries (.nsys-rep/.sqlite/.bin) — LOCAL_ONLY, not put into git.
