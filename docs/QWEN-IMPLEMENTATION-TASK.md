# Task specification for the local Qwen3.8 Flash Next

Prepared: 2026-09-06; status updated after EXP-038. The executor is the local Qwen. The user is Igor. Communication and reports in Russian; code comments and commit messages in English.

## 0. Current continuation point — do not repeat closed stages

- R0/EXP-034: `ACCEPTED`, committed in `1cf489a`. Closed; do not run again.
- R1/EXP-035: `ACCEPTED`, committed in `7e8b819`. Closed; do not run again, including synthetic/lifecycle/model smoke.
- R2/EXP-036: `NOT_RUN_DUPLICATE`. Do not create or run: the necessary baseline/transfer measurements are already in EXP-030–033.
- R3/EXP-037: `ACCEPTED`, committed in `2c76446`. Closed; do not repeat the offline trace analysis.
- R4/EXP-038: `ACCEPTED` (`kind=performance`), awaiting separate commit approval. The formal A/B has already been performed: median prefill `+13.16%`, 5/5 pairs, decode `+5.23%`, correctness/memory gates PASS. Do not rebuild or repeat the A/B.
- R5/EXP-039: `NOT_RUN`/`DEFERRED`. Do not start.
- R6: the only next stage, after a separate handoff and agreeing on the runs with Igor.

The R0–R2 sections below are kept only as a historical description of the already closed plan. They are not an instruction to execute. On a new handoff, always continue from the current status in this section and in `ROADMAP.md`, not from the first stage of the document.

## 1. Goal and reading order

Reduce the real prompt processing time in the patched expert-tier server while preserving quality, the current warm decode, and the RAM/VRAM limits. Reproducibility, safety profile, offline transfer analysis, and the main R4 A/B are already closed; the current work starts with R6 acceptance coverage. Do not speed up an arbitrary microbenchmark in place of the user scenario.

Read in this order:

1. `AGENTS.md` — the user's rules about runs/commits and communication, and the full handoff.
2. `ROADMAP.md` — the only current queue; the actual continuation point is R6.
3. `docs/AUDIT-2026-09-06.md` — findings and the history of 000–033.
4. This document in full; then only the entries related to the current stage.
5. `experiments/EXP-2026-09-05-030-bulk-prefill-cuda-path.md`, EXP-031/032/033, rejected EXP-033 patch — before R3/R4.

Do not follow old "Next experiment", "P1 closed", "P6 closed" entries in the historical log if they contradict the ROADMAP. Do not rewrite negative results to fit a new hypothesis.

## 2. Fixed constraints

- Only one experiment at a time, no parallel models, benchmarks, or CPU/CUDA builds. Do not launch child agents to execute experiments.
- `work/llama.cpp-integration` and `build/expert-tier-franken-cuda` are the user's working paths; do not edit/rebuild them. Any candidate gets separate `work/llama.cpp-expNNN-*` and `build/expNNN-*`.
- Do not run `git reset --hard`, `git clean`, force checkout, force push, bulk restore/revert, or stash other people's files. Do not delete failed attempts. `git add -A` is forbidden; stage only an explicit allowlist after reviewing the diff.
- Do not update upstream, CUDA, compiler, model quantization, tokenization, chat template, top-k, EHS, context, batch sizes, or thread counts as a side effect.
- EHS=0, OpenMP ON, 12 threads, MTP `--spec-draft-n-max 2`. Do not try a larger draft limit, CPU-only draft, ngram, 24/32 threads, PASSIVE/SPINCOUNT, the old CPU fused, madvise/warmer, or single-buffer packed upload.
- Do not perform live eviction/madvise/DONTNEED on a running CUDA server. Do not create memory pressure/cgroup swap/OOM to test the watchdog. Do not change sysctl on your own.
- Run measurements manually and sequentially; do not run the existing batch benchmark scripts. Offline processing of already completed results and agreed telemetry of a single server are allowed. This is not permission for a background load process.
- Before starting the model/server, including the tiny-model gate, tell Igor the goal, the expected duration, and the exact number of runs/requests; wait for an explicit «да» ("yes") as `AGENTS.md`, Hard rule 1, requires. A whole finite block of runs may be agreed in advance; do not ask again within an unchanged agreed block. When extending the block, request new approval.
- Before a commit, show the outcome, memory impact, correctness, and the list of files; commit only after approval per `AGENTS.md`, Hard rule 2. Prepare a reviewable diff before asking. Do not treat permission to audit as permission for the model or a commit.
- Apply the existing gitrules skills before code comments/commits. Do not change language rules retroactively in the user's files.

## 3. Experiment card and decision protocol

Before any code, create `experiments/EXP-<date>-NNN-<name>.md` with status `PLANNED`, using a free number no lower than 034. Roadmap numbers are a reserve of meanings: if they are taken by new user work, rename the queue in the documentation first.

The card must contain:

- exactly one testable hypothesis and the link to the previous measurement;
- baseline source/patch/binary/libs SHA-256, CMake flags, argv/env, request SHA-256;
- the file allowlist, inference phase, enable/disable condition;
- the single primary variable, metric, pre-selected memory budget;
- the number of warmups/measured runs, cache state control, stop/accept/reject criteria;
- which results lead to the next stage and which close the direction.

Statuses: `ACCEPTED` separately with `kind=performance|correctness|infrastructure`; `REJECTED` for a negative result; `INCONCLUSIVE` for noise/non-comparability; `BLOCKED` for an unavailable dependency; `NOT_RUN` when a design gate fails before the run. Working code by itself is not grounds for ACCEPTED.

For performance the main gate is **a median prompt_ms reduction of ≥3%** relative to a fresh A from the same protocol. Use five pairs, order A/B, B/A, A/B, B/A, A/B; compute `gain_i=100*(A_ms-B_ms)/A_ms`, the median gain, and the median of each arm. Do not mix gain latency with gain tok/s. At least 4 of 5 pairs must win, median paired gain ≥3%; the bootstrap 95% interval of paired gain must be above zero. With a small sample the interval is auxiliary, not proof of broad generalizability. If the natural spread is comparable to the effect or the intervals do not allow a conclusion — INCONCLUSIVE, do not change the baseline. Do not increase the sample until a random win: one pre-agreed additional block up to 10 total pairs is allowed, then stop.

Regression bounds for the new transfer-only path:

- Warm decode median no worse than 2%; degradation >2% means rejection or a separate noise check before the decision, not a waiver.
- For secondary prompt lengths, median latency no worse than 3%; zero tolerance for silent fallback instead of the declared coverage.
- VmSwap=0; no CUDA errors, OOM, watchdog trips, partial copies, hangs, invalid output.
- New buffers have a hard cap; after request/shutdown there is no growth in unreleased resources. For R4 the host budget is 2×16 MiB = 32 MiB, metadata ≤1 MiB, device staging 0. Investigate RSS growth beyond the budget +32 MiB measurement tolerance; for R5 a separate lower cap.
- The agreed baseline memory envelope stays the same for A/B. The historical 934 MiB headroom is an observation, not a guarantee. For new allocations, determine the available margin before performance; if there is a shortage, stop rather than quietly cut the context.
- Early stop: correctness failure/OOM immediately; >10% latency degradation on a comparable exploratory pair — REJECTED without the mandatory five pairs. The first request with a cold file cache is by itself not comparable to warm.

Infrastructure/safety stages are not required to speed up the model: they are accepted on precise functional gates. Do not claim throughput changes. A rejection keeps the table of numbers and the candidate patch in rejected if it is useful; working defaults remain at the baseline.

## 4. R0 / EXP-034 — CLOSED, do not run again

Status: `ACCEPTED`, committed in `1cf489a`. Provenance, the EXP-023–033 archive, the reproducible MTP patch chain, and the isolated build have already been verified. The full history is in `experiments/EXP-2026-09-06-034-provenance.md`. Do not repeat the inventory, hashing, patch reconstruction, or build.

## 5. R1 / EXP-035 — CLOSED, do not run again

Status: `ACCEPTED`, committed in `7e8b819`. Units/thresholds watchdog, launcher lifecycle, synthetic boundaries, and model smoke have already been verified. The full history is in `experiments/EXP-2026-09-06-035-watchdog-safety-profile.md`. Do not repeat the tests and do not launch the model.

## 6. R2 / EXP-036 — NOT_RUN_DUPLICATE, do not run

Cancelled before the run: EXP-030/031/032 already contain the baseline and transfer measurements, and EXP-033 a comparable control and a rejected candidate. Do not create an EXP-036 card, fixture, manifest, or results. A future performance candidate must have a fresh control A inside its own A/B, but a separate baseline experiment is not needed for that.

## 7. R3 / EXP-037 — existing traces and dependency budget first

**Hypothesis:** a substantial part of bulk-prefill latency is tied to host submission/preparation of pageable expert ranges, and bounded staging can reduce it without changing the graph. Here the hypothesis is verified analytically, without implementing the optimization.

**Reading areas:** `ggml/src/ggml-backend.cpp::ggml_backend_sched_compute_splits`, the `copy_experts` branch; `ggml/src/ggml-cuda/ggml-cuda.cu::ggml_backend_cuda_set_tensor_async`, `ggml-cuda/common.cuh` (context/pool/stream), scheduler events/allocator. Read the EXP-033 patch as rejected evidence, do not apply it.

**Output:** an offline report/JSON and, if needed, a small parser of saved traces. First SQL read-only on the EXP-031 SQLite, CUDA API↔activity correlationId. Cross-check the time origin of the server log and the profiler; their timestamps must not be automatically assumed identical.

A table is needed separately for in-request bulk, tail, and startup:

- number/bytes copies, src memory kind by enum, stream IDs;
- host cudaMemcpyAsync API durations (sum and union), device copy durations (sum and union), kernel union, copy/kernel intersection;
- sync API durations, intervals without CUDA work; do not call them CPU idle without CPU evidence;
- sizes p50/p90/max, consecutive range counts, actual selection coverage;
- IDs materialization locations, allocator input reuse wait, copy enqueue, compute enqueue, last destination consumer.

Verify that exact adjacent ranges are already coalesced. Do not make a new "coalescing adjacent IDs" patch. Check the previous assertions about graph inputs against the current source: trace the real branch, do not rely on the tensor name.

Preliminary estimate: total GPU kernel time 0.439 s in a 4.016 s request — even fully hiding this sum does not by itself promise a multiple speedup. The added CPU memcpy of all 21.64 GiB and memory bandwidth competition can eat the entire effect. Predict the saved host critical time and the added gather cost numerically as a range, and explicitly name the unknown parameters. Do not substitute cycle shares and timeline span for the wall critical path.

**Gate R4:** a pageable/host submission component is proven, there is a way to prepare chunk N+1 before completion N without removing allocator barriers; expected net gain of at least 5% with margin relative to the 3% threshold. If the saved trace lacks a parameter, only a new small diagnostic signal is allowed by separate agreement: do not repeat the full attribution of EXP-031/032. No positive budget — R4 NOT_RUN. **Gate R5:** a map of the real independent intra-layer window and destination ownership; without it R5 NOT_RUN. If both gates are negative — P1 DEFERRED, finish the handoff without a new kernel/cache experiment.

## 8. R4 / EXP-038 — bounded host pinned ring, direct upload

**Novelty:** EXP-033 did whole-projection pack→device scratch→scatter and expected a single host buffer. This effort uses only two bounded host chunks and direct async writes into the previous destination offsets. No device scratch or scatter; the number of logical selected ranges is not reduced as a goal of the experiment.

**Allowlist isolated source:** `ggml/src/ggml-backend.cpp`, `ggml/src/ggml-cuda/ggml-cuda.cu`, if needed the internal backend interface `ggml/src/ggml-backend-impl.h`, a targeted scheduler integration test. If extending an optional interface is needed, explicitly initialize it in all backends used; that is part of compatibility, not permission to change their algorithms.

**Baseline:** the currently accepted split-MTP runtime and a fresh control A inside EXP-038 itself; there is no separate R2 baseline. The A/B runs in one candidate binary with the flag OFF/ON for timing; before that, OFF must pass parity with a clean reference. Flag default OFF; the value `0` really means OFF, check the value, not just the presence of the env. Record the new flag's name in the card, for example `GGML_EXPERT_PINNED_RING`.

The algorithm in small sub-steps, each with a short handoff:

1. On the CPU, build a view of the list of already existing `copy_experts` ranges without new IDs/router passes. Guard only the verified bulk path (`MUL_MAT_ID`, host weights→CUDA, token dimension ≥32), not draft verify Ny≤3 and not the four-token tail. Verify the meaning of the dimension on source/coverage.
2. In a specific backend's context, without process-global mutable state, create at most two pinned host slots of 16 MiB each. Initialize lazily outside the repeatable timed steady-state part via warmup; do not grow capacity. Check for successful pinned allocation; a pageable fallback must not be counted as pinned success.
3. Split a chunk range >16 MiB byte-for-byte while preserving addresses; keep the original extra padding bytes `min(expert_size,512)` for a non-last expert. Do not fill padding with arbitrary zeros instead of the original bytes. Do not do quant/dequant operations or ID remap.
4. For a slot: `FREE → CPU_FILL → H2D_IN_FLIGHT → FREE`. Before overwriting a slot, wait only for its recorded completion event. After the async enqueue on an **existing stream**, record an event. The CPU can fill the second slot while DMA reads the first. The source mapping stays alive during the CPU copy; the pinned slot lives until the DMA completes.
5. Do not remove scheduler sync/wait and do not touch allocator reuse. All H2D chunks of the projection are placed before its MMQ consumer on the same stream. Do not call this H2D/MMQ overlap: this stage verifies host-fill/H2D overlap.
6. If slots cannot be allocated or events created before submission — disable the candidate path and take the regular path with a clear reason/counter. On an error after partial submission — correctly wait/abort per the backend contract; do not continue compute on partially loaded data. Unsupported backend/shape stays on the previous path.
7. On shutdown/reset/reallocation, drain all in-flight slot events before free. No host buffer reuse between independent contexts. Do not use a short-lived vector.data() for asynchronous metadata without a lifetime guarantee.

Correctness before performance:

- Test-backend-ops for the actual CUDA build and relevant quantized MUL_MAT_ID shapes; additionally a scheduler test host weights→CUDA with ≥32 tokens and an explicit hit counter for the new path.
- Sparse/adjacent IDs, repeat IDs, expert 0/last, non-contiguous IDs strides, ranges at the 16 MiB boundary and beyond, ≥3 slot cycles. Byte-compare destination selected ranges/padding with the reference after synchronize; sentinel guards around the destination, verify the source is unchanged.
- Flag OFF/no CUDA/alloc failure fallback; cancellation and shutdown with an in-flight upload; consecutive requests changing selected sets and graph shape. The tiny fixture must activate the ring, not only the direct CUDA resident kernel.
- The same fixed 395+16 request in A/B, nonempty hash and tokens; MTP flags identical. A discrepancy is REJECTED/BLOCKED correctness, not a "near tie" without investigation.

Then an agreed exploratory pair with warmups; if the gate does not fail, five formal pairs per section 3. For each leg a fresh server, one warmup primary, one measured primary, one warmup decode, one measured decode; that is 10 server starts and 40 requests for the five pairs. Run manually, not in parallel. If the overall protocol has changed by the agreement time — apply it identically to both legs in advance, with a new manifest.

Save prompt_ms, decode timings, raw outputs, actual bytes/calls, GPU/RSS peaks, faults/read_bytes, temperatures. Collect host slot waits/gather counters in aggregate in a separate diagnostic pass or with identical minimal overhead, not per-copy printf in a timed run. Verify the causal explanation separately from the unprofiled speed.

**ACCEPTED:** all gates and ≥3% end-to-end latency improvement. **REJECTED:** fewer calls or visible overlap without gain, memory overrun, regressions. **After acceptance:** R6 for this candidate. **After rejection:** save the patch/reason, return to the baseline; R5 only if its own R3 design gate is positive, not as an automatic "one more buffer" attempt.

## 9. R5 / EXP-039 — separate intra-layer transfer/compute overlap

This stage is conditional. Do not start it merely because R4 is done or rejected. If R3 did not prove the window, finish as `NOT_RUN` with a description of the missing dependency. Do not develop a cross-layer predictor.

**Hypothesis:** with one layer's IDs already known, the weights of an independent next projection can be transferred while the GPU computes the current one, and the gain exceeds the price of the events and the extra memory. Availability of IDs does not mean the scheduler destination is free yet.

**Allowlist:** the same scheduler/CUDA boundary; context-owned stream/events and tests. Do not change the MMQ kernel, the graph mathematically, the router, the CPU scheduler, or MTP. R4 can become the new baseline only after its R6 acceptance; otherwise the last accepted runtime is kept and a fresh control A is taken for each candidate without rejected code.

A mandatory design document before the patch:

1. Using concrete node names/source edges, show A compute, B copy, and why A does not read/write B's destination. Show the production order of gate/up/down separately; do not assume a fixed order by name.
2. Record the destination lifetime from allocator allocation to the last consumer. B must not be enqueued into A's reusable buffer. If that is impossible without newly allocated memory, limit the total additional VRAM to **128 MiB**; use chunks only if MMQ does not need to be changed to consume them. If an entire required projection does not fit, gate FAIL → NOT_RUN, do not increase the budget.
3. The producer pinned slot stays alive until `copy_done`. The compute stream waits for `copy_done` via a device event; the copy stream waits for `last_use_done` before destination overwrite. The host slot and the device destination have different lifetimes. Do not use pool allocations made with a single-stream assumption on a second stream without proven event-safe ownership.
4. Check CUDA graph capture/replay compatibility; the graph must not keep pointers to already freed slots or change IDs binding between replays. If correctness requires disabling CUDA graphs, that is a different primary variable — stop this candidate, do not swap the experiment.
5. Fallback/reset/cancel/error drains both streams before free; backpressure when the bounded pool is full; no global device synchronize on every range as a hidden "solution" to the race.

First the focused lifetime/race/byte tests from R4 plus repeated CUDA graph replay and cancellation. Then one separate diagnostic run proves the actual intersection of copy B and compute A on the timeline, not just the presence of a second stream. No overlap — REJECTED design. Then an unprofiled paired benchmark with the same protocol/gates. A win — R6; a failure — close this implementation and finish P1 with the baseline, do not build a third redesign on your own.

## 10. R6 — accept only a confirmed result and deliver the patch

This is the completion of the current performance experiment, not a license to combine other ideas. The primary five pairs are already done, do not repeat them without a reason.

1. Five consecutive pairs of secondary short/long fixtures and warm decode, if the corresponding five-pair decode has not been run yet. Approve the number of runs in advance. Measure actual evaluated tokens, TTFT/request wall, and memory; cache policy identical. Measure streaming TTFT separately if the primary `/completion` nonstream response does not provide it directly.
2. Correctness of a sequential session: short→long→short, prefix-cache path, MTP rejection/rollback and checkpoint restore, cancellation→next request, clean shutdown. Do not turn off checkpoints for the sake of a result. Do not run the concurrent aggregate benchmark P10. If the new state is not verified with multiple slots, the candidate stays scoped opt-in single-request until the corresponding gate; do not change the default multi-slot behavior.
3. If the secondary regression exceeds the thresholds — REJECTED for the general default. A narrow opt-in is allowed only with a pre-defined shape guard and a re-check of exactly that guard; do not cut an inconvenient prompt from the table post hoc.
4. Prepare a minimal delivery patch on top of the R0 package. From the new isolated source, verify apply/build and focused correctness; do not replace the user's integration tree. Defaults stay unchanged until the review.
5. Update the experiment report, compact JSON, manifest, roadmap status, and handoff. ACCEPTED performance requires numbers, memory envelope, and correctness, not just a test pass. For a rejected result, similarly keep before/after and the reason, attaching the patch as needed.
6. Show Igor the concrete diff/list/results for commit approval. While waiting for approval the implementation status is `VALIDATED_PENDING_COMMIT`, do not start the next experiment; do not announce an already established daily default.

If all performance candidates are rejected/NOT_RUN, the outcome is valid: "baseline preserved, no measurable improvement found". Do not automatically implement P2/P4/P5/P8/P9/P10 just to necessarily show a change.

## 11. Data format and handoff

Each JSON contains `experiment_id`, `kind`, `status`, `baseline_id`, source/patch/binary/libs identity, argv/env, model/head/request hashes, attempts and exclusions, all per-run values, medians/paired deltas/spread, memory/correctness, decision, and next_id. For durations the unit is ms, byte counters in bytes or explicitly MiB; convert cumulative counters into window deltas. Do not add up cumulative `graphs reused` from different request snapshots.

Raw names are unique, for example `results/archive/EXP-.../pair-01-A-{server.log,response.json,monitor.csv}`. Do not overwrite old results. Do not divide the generated budget by time if the actual count differs; for llama timings check the n vs n−1 convention. "Latency +63.4%" is not equal to "tok/s −63.4%".

After each small sub-step, briefly:

```text
Verified: <gate/coverage or the reason for NOT_RUN>
Changed: <exact files and the single variable>
Before → after: <metrics, units, n; if without a benchmark — "not measured">
Conclusion: <what the data proves and what it does not prove>
Decision: <ACCEPTED / REJECTED / INCONCLUSIVE / BLOCKED / NOT_RUN>
Next: <one concrete R# item and the nearest action>
```

After completing the experiment, be sure to follow up with the full «ПЕРЕДАЧА РАБОТЫ» (handoff report) block from `AGENTS.md`: this is a requirement of that file, not a replacement for the short report. Update the date, branch/full SHA, dirty state, mandatory rules, current runtime, baseline, latest verdict/artifacts/hashes, recent commits, next experiment, risks, the remaining P0–P11, and the nearest action. No new stage starts from conversation memory; only from the saved handoff and roadmap.
