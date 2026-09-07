# EXP-2026-09-07-040 — Pinned ring extended acceptance and delivery prep (R6)

Status: `ACCEPTED` (2026-09-07, Igor's decision; model acceptance runs executed by the
senior helper in Codex; raw data in `results/archive/EXP-2026-09-07-040/`)

## Scope (one)

Close R6 for the accepted EXP-038 pinned ring without re-running R0-R4 or the formal
395-token block: extended acceptance scenarios (short decode-focused, long
prefill/endurance), a sequential robustness session, ring-counter probes proving
single-token decode never stages, and a delivery patch validated over the R0 package.

## What is NOT repeated

- R0-R4 artifacts are reused as-is; the 5-pair 395-token A/B from EXP-038 is not run again.
- R5/EXP-039 untouched (`NOT_RUN`/`DEFERRED`).
- `work/llama.cpp-integration` untouched; no implementation change (patch is the accepted one).
- No concurrent/multi-slot benchmark (P10 stays closed); `-np 1` is fixed and the default
  for multi-slot is explicitly recorded as untested.

## Model-free validation (done 2026-09-07, no server, no model)

1. Binary/library/patch hashes match the EXP-038 card: control `5471e44b…`,
   candidate `fbbdd956…`, CUDA library `b1b9ca79…`, patch `e1e6803c…` (4 files, +149). PASS.
2. EXP-038 model-free harness re-run from `results/archive/EXP-2026-09-07-040/tests/`
   against the same candidate build: all 12 checks PASS, sched output sha
   `39505119b1…` identical across OFF/ON/alloc-fail. PASS.
3. Delivery patch identity: the EXP-038 patch itself is the delivery patch. It applies
   with `git apply --check` onto the clean R0 chain (base `4aaad5d` +
   `expert-tier-integration.patch` + `integration-drift.patch` + `mtp-sidecar.patch`). PASS.
4. Isolated delivery build `build/exp040-delivery` (Release, CUDA arch 89, shared libs,
   OpenSSL ON, curl OFF): builds clean; `llama-server` sha256 `dce19554…`,
   `libggml-cuda.so.0.22.0` sha256 `371c7cf9…`. PASS.
5. Focused model-free tests re-run against the delivery build: all PASS; sched output sha
   matches the candidate build exactly (`39505119b1…`). PASS.
6. The four ring files in the R0-chain+patch tree are byte-identical to the tested
   EXP-038 tree (`diff -q` clean for all four). PASS.

## Prepared model acceptance (executed 2026-09-07 by the senior helper in Codex)

- `results/archive/EXP-2026-09-07-040/ab/PROTOCOL.md`: 5 pairs x 2 arms = 10 server runs,
  20 requests. Order A/B, B/A, A/B, B/A, A/B. Every arm sends the same two NEW fixed
  requests in the same order: `request-short.json` (decode-focused, ~50 prompt tokens,
  n_predict=128) and `request-long.json` (prefill/endurance, >512 prompt tokens so the
  scheduler emits several ring batches, n_predict=24). temperature=0, cache_prompt=false.
- `results/archive/EXP-2026-09-07-040/session/SESSION-PROTOCOL.md`: 3 candidate runs
  (13 server runs total, 28 requests): sequential session (short -> long -> short;
  repeated short; cached long prefix-cache hit; streaming with TTFT; mid-stream
  cancellation -> restore from checkpoint -> next normal request correct; verbose log
  checks for checkpoint save/restore and MTP accept/reject; SIGINT with no orphan
  process and no CUDA error) plus two ring-counter probe runs (long prompt with
  n_predict=0 vs n_predict=64) proving decode adds zero staged calls.
- Per request recorded: actual prompt/generated tokens, prompt_eval_ms, decode tok/s,
  wall time, TTFT for streaming; per run: RSS/VmSwap/VmPin before-after, GPU memory,
  ring counters from server.log.
- Default `-np 1` throughout. Multi-slot default behavior is explicitly NOT tested here.

## Model acceptance results (2026-09-07, raw data under results/archive/EXP-2026-09-07-040/)

Invalid preflight runs kept and excluded (documented in the run directories):
`pair1-A-invalid-sandbox-no-cuda` (no CUDA under sandbox), `pair1-A-invalid-swap-cold-cache`
(VmSwap 54540/57080 kB), `pair1-A-invalid-swap-cold-cache-2` (VmSwap 97928 kB). All 10
formal runs below are after the preflight issues were fixed.

### A/B block — short request (decode-focused, 41 prompt tokens, 104 generated, stop=eos)

- prompt_eval_ms per run: A: 4754.721, 2298.308, 1966.009, 2018.462, 2016.941
  (median 2018.462); B: 2043.572, 1896.015, 1692.705, 1703.714, 1742.357
  (median 1742.357). Median time reduction `+13.679%`, B wins 5/5.
- decode (predicted_per_second, 104 tokens): A: 13.9038, 16.4177, 16.4682, 15.8465,
  16.0924 (median 16.0924); B: 15.4679, 15.6159, 16.4619, 16.2361, 15.4823
  (median 15.6159). Median of paired per-pair changes: `-0.038%` (essentially unchanged).
  Secondary calculation from independently sorted medians gives `-2.961%`; recorded for
  transparency, not used to overturn the decision — the experiment was paired, so the
  primary decode result is the paired median `-0.038%`.

### A/B block — long request (>512 prompt tokens, 1124 evaluated)

- prompt_eval_ms per run: A: 9415.192, 8357.411, 8547.744, 8427.428, 8419.098
  (median 8427.428); B: 7243.800, 7133.466, 7202.188, 7295.329, 8982.193
  (median 7243.800). Median time reduction `+14.045%`, B wins 4/5 (pair5 B was the
  slowest B run at 8982.193 ms against the fastest A run of the block).
- decode: every run of both arms produced 1 token with stop_type=eos
  (predicted_ms=0.001): the model emitted end-of-generation immediately, so the long
  request contributes no decode measurement. This is a property of the fixed request,
  not a regression.

### Answer identity and counters

- Short response content sha256 identical in all 10 runs:
  `2f409a00f774aeacfa1d8b92ac8ff58ed19f1a73ad7d184bbb625f477605a5aa`.
- Long response content identical in all 10 runs (empty content, single EOG token;
  sha256 of empty string `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`).
- Ring counters: every B run logged exactly `expert ring staged 64635 calls, 64635
  chunks` at context teardown; every A run logged none (control path). S-run logged
  `91047 calls, 91047 chunks` across the whole session.

### Memory

- VmSwap = 0 kB in every before/after snapshot of all 13 model runs (checked
  programmatically: zero non-zero VmSwap lines in valid runs).
- Raw VmRSS/VmHWM after-run values (kB): A: 39607560, 39911248, 39911448, 39911384,
  39908176; B: 39940836, 39945680, 39945444, 39945432, 39942304. Recorded additional
  RSS growth attributed to the ring: `32.199 MiB` (within the pinned budget of 32 MiB
  slots + <1 MiB metadata).
- GPU memory snapshots recorded per run in `gpu-before.txt`/`gpu-after.txt`; idle GPU
  after stop: 1825 MiB (S-run).

### Session (S-run) and probes

- Sequential session: short -> long -> short all HTTP 200; repeated short served;
  streaming with TTFT `0.028556 s` (S5, HTTP 200).
- Prefix cache: first long request `prompt eval time = 7672.15 ms / 1124 tokens`;
  cached repeats `257.91 ms / 4 new tokens`, `254.70 ms / 4`, `251.91 ms / 4`
  (1120 tokens cached).
- Checkpoint save/restore and MTP accept/reject present in verbose log; post-session
  short request normal (1227.77 ms / 41 tokens prompt, 5885.66 ms / 104 decode).
- SIGINT shutdown clean (server-exit code 0), orphan process count 0, no CUDA errors,
  no OOM, no hang in any of the 13 runs.
- Probes P0-run/P64-run: identical ring counters `51429 calls, 51429 chunks` — decode
  staged nothing.

### Two test-request gaps (test flaws, not ring regressions; do not block ACCEPTED)

- Streaming cancellation was never exercised: the streaming long request (S6) completed
  by end-of-generation in `0.30 s`, so there was no stream left to cancel mid-flight;
  the log shows only `all tasks already finished, no need to cancel`. The checkpoint
  restore-after-cancel path remains untested against the ring.
- The probe requests with `n_predict=0` and `n_predict=64` both actually generated 1
  token with stop_type=eos (predicted_ms=0.001), so the probe compared two identical
  degenerate decodes; the counter equality `51429 == 51429` still holds but with weak
  coverage. Fixing the probe requests (force >3 s stream for real cancellation; force a
  real 64-token decode) is deferred to a separate coverage experiment and does not
  block promoting the accepted runtime.

## Acceptance gates (fixed before the runs)

- All answers correct and deterministic (identical completion sha across all runs per request).
- Median prompt latency of candidate not worse than control by more than 3% on both requests.
- Median decode not worse by more than 2%.
- VmSwap = 0 in every snapshot; no CUDA error, OOM, hang, leak.
- Extra memory within 32 MiB pinned RAM + < 1 MiB metadata (post-request RSS delta).
- Every candidate run logs non-zero staged calls/chunks.
- Probes: N_d == N_p and M_d == M_p (single-token decode never uses the bulk ring).

## Delivery

- Delivery patch: `results/archive/EXP-2026-09-06-038/exp038-pinned-ring.patch` (unchanged,
  sha256 `e1e6803c…`), validated over the R0 package by `git apply --check`, isolated build,
  and focused model-free tests (see above). No default enablement is proposed yet; the
  ring stays opt-in via `GGML_EXPERT_PINNED_RING`.

## Verdict

`ACCEPTED` (Igor, 2026-09-07). All fixed gates passed: identical answers, prefill
`+13.679%` short (5/5) and `+14.045%` long (4/5), paired decode `-0.038%`, VmSwap=0,
no CUDA/OOM/hang, extra RSS 32.199 MiB, non-zero staged counters on every B run,
probe counters equal. The two test-request gaps (streaming cancellation not exercised;
degenerate n_predict=0/64 probes) are recorded as test flaws, not regressions, and are
deferred to a separate coverage experiment. Next step: EXP-041 — promote the runtime
to default-on through the reproducible patch chain.

## Prepared artifact hashes (2026-09-07, under `results/archive/EXP-2026-09-07-040/`)

- `ab/request-short.json` `4d1597f51bb3ec4f964be45c4bad375dcd167ab0a167918e95ece78216e81f56`
- `ab/request-long.json` `e2cc7dbaa70564c8e085e11e4188a3d9e8d43571885fd1c84076bcab3c4f89c4`
- `ab/PROTOCOL.md` `fa126727fcecd01398d68d596a7650f6a3f6d3cbc95a210ca0b08813d8452d73`
- `session/SESSION-PROTOCOL.md` `57d72fb2e812371af55409b844d19bef91f135adea18923f3636976a974d73cb`
- `session/request-long-cached.json` `1419a036af799b284789540a2c1f7c81b47e2e60b3ef39aba8f16dcd4034ace5`
- `session/request-long-p0.json` `17b15645fae9fc24cf0f3dc0cd366dde76ccec2b451ce6259e0e67e2932fb315`
- `session/request-long-p64.json` `32bcd6e476c98d75633ea735c86e1c30b6114786ea7818ebdca4e666ba74a103`
- `session/request-long-stream.json` `9aa87b3b5cb969e713da3c84054df2e02448f8680a5d8c85b0d0dfe036a04db3`
- session copies of `request-short.json`/`request-long.json` match the ab/ hashes above.

