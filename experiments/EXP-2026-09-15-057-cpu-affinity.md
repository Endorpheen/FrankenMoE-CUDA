# EXP-2026-09-15-057 - CPU affinity placement screen (DEC-3)

Status: `REJECTED` (2026-09-15). Stage 1 screen run and closed; the numbers are
in `## Results (2026-09-15)` at the end of this card. No source, patch, model or
launcher change was made: the only variable was the launcher `EXTRA_ARGS`
affinity flags. Design source: `experiments/designs/exp057/D3-thread-affinity.md`
(DEC-3 in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`).

## Kind

`performance`, tuning screen. Runtime flags only; no source change, no patch, no
new buffer. The accepted EXP-046 default runtime stays byte-identical and remains
the baseline and the fallback.

## Hypothesis (one)

On the accepted runtime, explicit CPU placement of the 12 OpenMP workers changes
warm decode speed or its run-to-run spread. Specifically, pinning the workers to
12 distinct physical cores (`--cpu-range 0-11 --cpu-strict 1`) is expected to
leave warm decode neutral or slightly positive and to reduce per-request tok/s
spread by removing scheduler migrations and transient SMT co-tenancy.

This is a placement screen, not a kernel or memory win. A neutral decode result is
an acceptable and expected outcome; the direction closes on REJECTED if the screen
shows neither a decode gain nor a jitter improvement.

Link to the previous measurement: the accepted baseline for this screen is EXP-046
(`experiments/EXP-2026-09-09-046-fused-iq2s-ab-pair.md`), whose warm decode and
correctness controls define what an A arm must reproduce here. EXP-013 reported the
12-thread profile at mean 11.95 occupied cores versus 15.93 at 16 threads, so the
operating system already spreads the 12 workers over roughly 12 physical cores;
explicit affinity can only remove the residual migrations and co-tenancy.

## Hardware premise (fixed)

AMD Ryzen 9 5950X, 16 physical cores / 32 logical CPUs, one socket. CPUs 0-15 are
the first SMT sibling of each physical core; CPUs 16-31 are the second sibling
(`thread_siblings_list` pairs are (0,16) (1,17) ... (15,31)). A 12-thread worker set
fits on 12 distinct physical cores with 4 physical cores (8 logical CPUs) left
entirely free.

## Baseline identity (fixed before any run)

- Source tree: `work/llama.cpp-exp046-default` (chain: pinned base `4aaad5d3` plus
  `expert-tier-integration` + `integration-drift` + `mtp-sidecar` + `pinned-ring` +
  `pinned-ring-default-on` + `two-thread-gather` + `iq2s-fused-decode` +
  `iq2s-fused-decode-default-on`).
- Server binary: `build/exp046-default-runtime/bin/llama-server`,
  sha256 `3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`.
- Launcher: `scripts/run_qwen38_server.sh`,
  sha256 `17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc`.
- Linked libraries as recorded for the EXP-046 build (from
  `results/archive/EXP-2026-09-09-046/hashes-preflight.txt`, not re-hashed here;
  the server-binary hash above is the identity anchor for this screen):
  `libggml-cuda.so.0.22.0` sha256 `bb67604bb72977bc4a6a694fb46ec3ab27dc8f0ede8450fc2950ef6af40013d8`,
  `libggml-cpu.so.0.22.0` sha256 `8c8268201a45f119bc98fe600c7a09f0ab2a23f5471161938647e92d8802dc4c`.
- CMake flags (as recorded for this build, not re-derived): `Release`,
  `BUILD_SHARED_LIBS=ON`, `GGML_CUDA=ON`, `GGML_OPENMP=ON`, `CMAKE_CUDA_ARCHITECTURES=89`,
  `LLAMA_CURL=OFF`, `LLAMA_OPENSSL=ON`, `LLAMA_BUILD_SERVER=ON`. The OpenMP path is
  what makes `--cpu-mask`/`--cpu-strict` take effect.
- Model: Qwen3.8-Flash-Next UD-IQ3_XXS, three shards; reference hashes in
  `docs/RUN_QWEN38.md` (81.96 GB, not re-hashed here).
- MTP head: `models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf`,
  sha256 `4563303ab6e7d40f409750fd48af76bf33dd76b0773951cbf484109107de8140`.
- Warmup request: `results/archive/EXP-2026-09-07-040/ab/request-short.json`,
  sha256 `4d1597f51bb3ec4f964be45c4bad375dcd167ab0a167918e95ece78216e81f56`
  (n_predict 128, temperature 0, cache_prompt false, stream false).
- Measured request: `results/archive/EXP-2026-09-09-046/request-decode256.json`,
  sha256 `5975261ba9441955f53227d8c7784b176c7fbcd96d5661040d50374f4655d2ac`
  (n_predict 256, temperature 0, cache_prompt false, stream false).

## Accepted server argv and env (verbatim)

```
build/exp046-default-runtime/bin/llama-server \
  -m models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf \
  -ngl 99 --cpu-moe --reasoning-effort low -ehs 0 --ehs-reserve-mb 2048 \
  -ot per_layer_token_embd.weight=CPU,token_embd.weight=CPU \
  -c 196608 -np 1 -fa on --jinja \
  -ctk q4_0 -ctv q4_0 -ctkd q4_0 -ctvd q4_0 -t 12 \
  --host 127.0.0.1 --port 8087 \
  -md models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf \
  --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe
```

Env: `GGML_EXPERT_PINNED_RING=1` (default on), `GGML_CPU_IQ2S_FUSED=1` (default
on), `GGML_EXPERT_RING_GATHER2T=1` (default on). All three are the accepted EXP-046
defaults and stay unchanged in both arms. The launcher expands to this argv with
`PORT=8087`, `THREADS=12`, `EHS=0`, `PINNED_RING=1`, `MTP=1`, `CTX_SIZE=196608`.

## File allowlist and controls

- Runtime allowlist: none. No source, launcher, patch, model, quantization, KV,
  context, batch or thread-count change. The only per-stage change is the
  `EXTRA_ARGS` affinity flags, which the launcher passes through unquoted.
- Artifacts written by the run (not before it): this card, `results/archive/EXP-2026-09-15-057/`
  raw data, and `results/exp057-*.log` / `results/exp057-*-monitor.csv`.
- Inference phase: warm decode, measured with the accepted decode256 request after
  prefix-filled warmup. Prefill shape is untouched and is not a metric.
- Enable/disable condition: the candidate flags are enabled only by adding
  `EXTRA_ARGS` to a fresh launch; the default launch (arm A) has no affinity flags
  and is the enable/disable control. Nothing is enabled in the accepted default.

## Single variable per stage

Stage 1 (screen, C1 versus default), the only variable is:
- C1: `--cpu-range 0-11 --cpu-strict 1` added via `EXTRA_ARGS`. All 12 workers get
  exactly one distinct CPU from bits 0..11, so they land on the first sibling of
  physical cores 0..11 and cannot migrate or co-tenant a core.

Stage 2 (opened only if stage 1 shows signal), each candidate is its own single
variable against the same default A:
- C2: `--cpu-range 0-11` without `--cpu-strict` (confinement only, kernel still
  free to migrate inside the set).
- C3: `--cpu-range 16-27 --cpu-strict 1` (second SMT half; topology control).
- C4: `--cpu-mask 0xfff0 --cpu-strict 1` (cores 4..15; avoids the low cores where
  the kernel, IRQ/GPU and desktop tend to run).
- C5: `taskset -c 0-15` prefix around the launcher, optionally combined with C1
  (process-wide confinement; external tool, affects CUDA helper threads).

No candidate is combined with another unless a stage explicitly names the pair, and
`OMP_PROC_BIND`/`OMP_PLACES`/`GOMP_CPU_AFFINITY` are never set: ggml re-binds each
worker per region and the two mechanisms would fight.

## Metric and memory budget

- Primary metric: warm decode throughput, `timings.predicted_per_second` from the
  accepted decode256 request.
- Secondary (jitter) metric: the spread of per-request tok/s within one server
  session. Three measured decode256 samples per run; jitter is the sample standard
  deviation of `predicted_per_second` across those three samples, reported together
  with the min-max spread. Do not mix latency gain with tok/s gain.
- Paired gain per pair: `gain_i = 100 * (B_tok_s - A_tok_s) / A_tok_s`.
- Memory budget: 0 MiB new allocations. These are runtime flags only; there is no
  new buffer, ring slot, or staging area. VmSwap must stay 0 and RSS/VRAM must stay
  within the observed run-to-run noise of the accepted profile.

## Warmup, measured runs and cache state

Per run (one fresh server):
1. Launch one server and wait for `server is listening` in the log.
2. Warmup: one `request-short.json` with `cache_prompt false`.
3. Measured: three repeats of `request-decode256.json` in the same server session.
   Each is 256-token greedy, temperature 0, cache_prompt false, stream false.
4. Record `predicted_per_second` and `predicted_per_token_ms` for each of the three
   samples; record the content sha256 of the first sample.
5. Stop the server cleanly with SIGINT; confirm no orphan `llama-server`.

Cache state control: every run is a fresh server with the page cache warm (the
model is not re-read from a cold file cache). A first request with a cold file cache
is not comparable and is not used. Do not change requests between samples. Never run
two servers at the same time. Runs are manual and sequential.

## Run count and order

- Stage 1 screen: 4 runs = 2 balanced pairs, order A, B, B, A. Each run is one fresh
  server with one warmup plus three measured samples.
- Stage 2 (only on signal): the C2-C5 candidates screened with the same balanced
  A/B protocol (minimum 2 pairs each), and any candidate that clears the bars is
  escalated to 5 pairs = 10 runs, balanced 3 A->B and 2 B->A (or the reverse).
- Every pair uses a fresh A from the same protocol; no historical A is reused.

## Stop, accept and reject criteria

Bars, fixed before the runs:
- Decode bar: paired decode gain >= +2.0% AND >= 4 of 5 pairs win (escalated stage).
- Jitter bar: jitter (standard deviation of per-request tok/s) reduced by >= 20%
  with a decode regression <= 0.5%. The jitter bar is an alternative accept path,
  not an addition to the decode bar.
- Screen stop: record REJECTED if the paired decode gain is < +0.5% AND the jitter
  standard deviation improves by < 20%.
- Screen escalation: open stage 2 if the stage 1 screen meets either the decode
  >= +0.5% signal or the jitter >= 20% signal.

Verdicts:
- ACCEPTED: median paired decode gain >= +2.0% with >= 4/5 pairs winning, OR jitter
  reduced >= 20% with decode regression <= 0.5%, AND every correctness control below
  passes byte-for-byte.
- REJECTED: neither bar met, with the numbers recorded. If C1 is null, the whole
  affinity direction is recorded as REJECTED and closed; the OpenMP wait direction
  stays closed (EXP-015/017/018/020).
- INCONCLUSIVE: the natural spread is comparable to the effect or the intervals do
  not allow a conclusion. The baseline is not changed.
- Correctness controls for every run: 256/256 tokens `stop=limit`; content sha256
  identical between arms, expected `9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757`;
  draft acceptance identical to the digit (EXP-046 reference 0.58974, 138/234);
  expert ring teardown `25326/25326`; VmSwap 0; clean SIGINT exit 0; no orphan.

Do not increase the sample size to chase a random win. One pre-agreed additional
block up to 10 total pairs is the maximum, then the direction stops.

## Abort conditions

Any of these stops the block immediately and is reported as-is, without a
performance claim:
- any CUDA error, OOM, watchdog trip, partial copy, or invalid output;
- VmSwap > 0 in any telemetry snapshot;
- a request hang longer than 120 s;
- a content-hash mismatch: the decode256 content sha256 differs between arms or from
  the expected `9270353f...`, or the two arms disagree on draft acceptance or ring
  counters.
- any deviation of the baseline identity: the server binary, launcher, or model does
  not match the hashes above. The run is `BLOCKED`, not a result.

## Stage transitions

- Stage 1 signal met -> open stage 2 on C2-C5; the C1 result carries forward as the
  current best candidate.
- Stage 1 no signal -> the affinity direction is REJECTED and closed; no stage 2.
- A stage 2 candidate clears the bars -> escalate that candidate alone to 5 pairs
  and decide ACCEPTED or REJECTED on the full set.
- Any abort -> stop, report, and do not continue the branch. Independent approved
  work that cannot affect this diagnosis may continue.

## Run protocol

Nothing in this section is executed by this card. It is the literal protocol for a
future approved block. Owner approval is required before step 1.

```
cd /home/end0/AI/FrankenMoE-CUDA

# Arm A (default accepted profile, 12 threads, no affinity flags), port 8087:
PORT=8087 \
LOG_PATH=results/exp057-A.log \
MONITOR_PATH=results/exp057-A-monitor.csv \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

# Arm B (stage 1, C1: pin the 12 workers to 12 distinct physical cores), port 8087:
PORT=8087 \
EXTRA_ARGS="--cpu-range 0-11 --cpu-strict 1" \
LOG_PATH=results/exp057-B.log \
MONITOR_PATH=results/exp057-B-monitor.csv \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

# After "server is listening" in the chosen arm's log: warmup request.
curl -s http://127.0.0.1:8087/completion \
  -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-07-040/ab/request-short.json \
  | jq -r '.usage.completion_tokens'

# Measured decode256 request. Run three times against the SAME server session;
# capture tok/s, per-token ms and the content hash of the first sample.
curl -s http://127.0.0.1:8087/completion \
  -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-09-046/request-decode256.json \
  | tee results/archive/EXP-2026-09-15-057/A-decode256-1.json \
  | jq -r '"\(.timings.predicted_per_second)  \(.timings.predicted_per_token_ms)"'
jq -r '.content' results/archive/EXP-2026-09-15-057/A-decode256-1.json | sha256sum

# Stop the server cleanly.
kill -INT <server_pid>
```

Port 8087 keeps this screen separate from the daily 8081 server; the owner's own
8081 server must be stopped first. `EXTRA_ARGS` is expanded unquoted by the launcher
(line 86), so the two flags are passed verbatim. The launcher defaults are
unchanged: `THREADS=12`, `-c 196608`, `-np 1`, `-fa on`, KV q4_0, `--cpu-moe`,
MTP `n_max=2`, EHS=0. The only A/B variable is `EXTRA_ARGS`.

## What to record for the verdict

- Per run: arm, order, `predicted_per_second` (three samples), `predicted_per_token_ms`,
  content sha256, draft acceptance, ring counters, VmSwap, exit status.
- Per pair: `gain_i` on decode tok/s and the jitter ratio.
- Summary: median paired gain, wins/total, independent arm medians, jitter standard
  deviation per arm, and an explicit limit: this is a CPU-placement screen, not
  evidence of a kernel or memory win. No measurement exists for this card yet.

## Risk statement

Low. All candidates are runtime flags or an external `taskset` prefix; no source,
model, quantization, KV, context or launcher change. Correctness must still be
re-proven byte-for-byte per run. The realistic expectation is a neutral decode
result with a possible jitter improvement; if C1 is null, the affinity direction is
recorded REJECTED and closed.

## Results (2026-09-15)

Stage 1 screen, run and closed. One block of four fresh-server runs in the
balanced order A1, B1, B2, A2, executed on 2026-09-15 between 22:39 and 23:06.
Arm A is the accepted default launch with no affinity flags. Arm B is C1,
`EXTRA_ARGS="--cpu-range 0-11 --cpu-strict 1"`. Binary, launcher, model, MTP
head, context, KV and thread count are the fixed baseline identity above and
were unchanged in both arms.

Raw data: `results/archive/EXP-2026-09-15-057/`. Logs: `results/exp057-A1.log`,
`results/exp057-B1.log`, `results/exp057-B2.log`, `results/exp057-A2.log`.
Telemetry: `results/exp057-*-monitor.csv`.

### Per-run decode samples

`timings.predicted_per_second` from the accepted decode256 request. Run means
and standard deviations use the steady samples only; one cold first-request
sample is excluded where noted.

| run | arm | order | samples (tok/s) | excluded | mean (tok/s) | stdev | steady per-token ms |
| --- | --- | --- | --- | --- | ---: | ---: | --- |
| A1 | A default | 1 | 16.4508, 20.1299, 20.2649, 19.6565 | 16.4508 (cold) | 20.0171 | 0.3195 | 49.677, 49.346, 50.874 |
| B1 | B C1 | 2 | 20.1997, 19.6805, 19.3455 | none | 19.7419 | 0.4304 | 49.506, 50.812, 51.692 |
| B2 | B C1 | 3 | 19.5029, 19.1285, 19.6474 | none | 19.4263 | 0.2678 | 51.274, 52.278, 50.897 |
| A2 | A default | 4 | 19.5349, 19.1357, 19.0459 | none | 19.2388 | 0.2603 | 51.191, 52.258, 52.505 |

A1's first decode256 sample came back at 16.4508 tok/s while the next two sat
near 20.1 and 20.3 and the last at 19.66; it is the cold first-request outlier
of that session and is dropped. The B1, B2 and A2 sessions each ran an explicit
discard decode256 before the three measured samples, so all four runs
contribute exactly three steady samples. Every sample is 256/256 tokens with
`stop_type` `limit`.

### Paired comparison (order A, B, B, A)

Gain for each pair is `100 * (B - A) / A`, using the run means above. Pair 2
compares B2 against A2, the same arm convention as pair 1.

| pair | chronological runs | A run | B run | A mean | B mean | paired gain |
| --- | --- | --- | --- | ---: | ---: | ---: |
| 1 | A1, B1 | A1 | B1 | 20.0171 | 19.7419 | -1.374% |
| 2 | B2, A2 | A2 | B2 | 19.2388 | 19.4263 | +0.972% |

- Median paired gain: -0.201% (the two pair gains are -1.374% and +0.972%).
- Independent arm medians: A = 19.6282, B = 19.5841, so B is -0.224% relative
  to A.
- The two pair signs contradict: the flag loses in the early pair and wins in
  the late pair.

### Jitter (secondary metric)

Jitter is the sample standard deviation of the three steady tok/s values within
one session.

- A arm: A1 0.3195, A2 0.2603, mean 0.2899 (about 0.29).
- B arm: B1 0.4304, B2 0.2678, mean 0.3491 (about 0.35).

The pinned arm is about 20% noisier than the default, not quieter. The jitter
bar (a 20% or greater reduction) is not met, and the observed direction is the
opposite of the hypothesis.

### Correctness controls

- Stop and length: every decode256 sample reports `stop_type` `limit`,
  `tokens_predicted` 256 and `timings.predicted_n` 256, across all 16 documents
  in the archive including the discard samples.
- Content hash: `sha256(content)` is
  `9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757` for every
  decode256 document in all four runs. Output is byte-identical between arms
  and matches the expected value in the accept criteria.
- Draft acceptance: the first decode256 request of each session reports
  0.58974 (138 / 234), the EXP-046 reference; every later decode256 sample
  reports 0.60870 (140 / 230). The sequence is the same in A and B, so the arms
  agree to the digit at the same position in the session.
- Swap and shutdown: `process_swap_kib` is 0 in every telemetry sample of all
  four runs (VmSwap 0). Each log ends with `cleaning up before exit...` and the
  ring teardown line; no orphan `llama-server` remained, and no CUDA error, OOM
  or watchdog line appears in any log.
- Memory: peak RSS stayed between 37.3 and 37.4 GiB and peak GPU used between
  10.8 and 11.5 GiB across the four runs, inside the accepted profile's noise.
  The flags allocate nothing: 0 MiB new host or device memory.
- Baseline identity: the server binary, launcher and model are the fixed
  EXP-046 identity above and were not rebuilt or replaced for this screen.

### Affinity-binding verification

Binding was verified during a B arm request: each of the 12 OpenMP worker
threads had an allowed-CPU set of exactly one CPU drawn from 0..11, one
distinct CPU per worker, while background threads (server, CUDA helper, MTP)
stayed allowed on the full 0..31 set. The flags reached the worker pool and the
workers neither migrated nor shared a core. The placement was correct; the
numbers above show it did not help.

### Ring-counter observation

Expert ring teardown counters at server shutdown (`expert ring staged N calls,
N chunks`):

- A1 = 61686 / 61686, B1 = 61686 / 61686, A2 = 61686 / 61686.
- B2 = 73806 / 73806.

The counter tracks the number of decode requests served in the session, not the
affinity flag. The three 61686 runs served one warmup plus four 256-token
decode requests; B2 served one warmup plus five 256-token decode requests, and
its staged count is proportionally higher. The extra request is visible in
`results/exp057-B2.log` (six `draft acceptance` entries against five in the
other logs). The ring staged and consumed the same call and chunk count in
every run, and no partial copy or mismatch was reported. This is a
session-workload artifact, not a correctness failure and not an affinity
effect.

### Verdict: REJECTED

Neither accept bar is met.

- Decode bar: median paired gain -0.201%, far below the +2.0% accept bar and
  also below the +0.5% stage-1 signal threshold.
- Jitter bar: arm stdev rose from about 0.29 to about 0.35, so the 20% jitter
  reduction bar is not met and the direction is reversed.
- Pair signs contradict: -1.374% in the early pair against +0.972% in the late
  pair. A real placement win would not flip sign between two adjacent pairs of
  the same balanced design.
- Session drift confound: the per-run means fall monotonically in run order,
  A1 20.0171, B1 19.7419, B2 19.4263, A2 19.2388, independent of arm. The first
  run is the fastest and the last is the slowest. With the A, B, B, A order
  this drift pushes pair 1 toward A and pair 2 toward B, which is exactly the
  observed sign pattern. The screen cannot separate the flag from the drift,
  and the stage-1 signal threshold was not reached, so no stage 2 candidate is
  opened.

Stage 1 no-signal closes the affinity direction. The OpenMP wait direction
stays closed (EXP-015/017/018/020). The accepted EXP-046 runtime stays the
baseline and the fallback; nothing is enabled by default, and no file other
than this card and the DEC-3 row in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md` was changed.
