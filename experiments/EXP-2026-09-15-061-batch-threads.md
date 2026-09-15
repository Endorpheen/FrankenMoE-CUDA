# EXP-2026-09-15-061: batch-pool thread count (`-tb 16` vs `-tb 12`) for the prefill gather (PRE-5)

Status: `REJECTED` (2026-09-15).
Kind: `performance`.

This card records a completed env/CLI-only A/B screen. The raw data is under
`results/archive/EXP-2026-09-15-061/`. The card itself starts no server, loads no model,
builds nothing and runs no curl. It is the written verdict for the candidate tracked as
PRE-5 in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`, design note
`experiments/designs/exp057/D3-thread-affinity.md` (section P1).

The candidate is a CLI-only knob, so no source, patch, binary or launcher change was made.
The single variable was the batch-pool thread count: `-tb 12` (arm A) versus `-tb 16`
(arm B).

## Hypothesis (one)

On the accepted runtime, raising the batch-pool thread count from 12 to 16 speeds up the CPU
gather that dominates bulk prefill: the prefill path uses the batch thread pool, the gather is
memory-bandwidth-bound and currently runs on 12 workers, so a 16-worker batch pool shortens
the fixed 395-token prompt-eval latency.

## Link to previous measurements

- EXP-2026-09-07-043 (prefill bottleneck profile): in a 2572.660 ms measured window for this
  same 395-token prompt, the CPU `expert_gather` union was 1463.641 ms (56.9%), confirming a
  gather-bound prefill on 12 threads.
- EXP-2026-09-08-045 (two-thread CPU gather prototype, model-free): a positive signal (median
  total -5.04%) but `INCONCLUSIVE`, and it flagged contention risk. It is a different mechanism
  (helper thread), not this variable, and is not ported here.
- EXP-013 (thread count): `-t 16` was rejected for warm DECODE only (mean occupied cores 15.93
  vs 11.95 at 12 threads; decode 17.656 vs 17.615 tok/s, -0.23%, inside noise). That rejection
  is about the generation pool during decode and does NOT cover the batch pool during the
  prefill gather. This card reopens only the prefill-gather question, not decode.

## Baseline identity (fixed before the runs)

- Source tree: `work/llama.cpp-exp046-default`, base
  `4aaad5d318a790a42c2197975ec8fadbad42602b` plus
  `expert-tier-integration` + `integration-drift` + `mtp-sidecar` + `pinned-ring`
  (sha256 `e1e6803cec1e26099ee452fcb3537b7e660132e6e30da69febffca4785dbd7de`) +
  `pinned-ring-default-on` + `two-thread-gather`
  (sha256 `563a66036497e654b203909c95446ba8d111593acb3e831f2c790f3a433a1c21`) +
  `iq2s-fused-decode` + `iq2s-fused-decode-default-on`. Read only; not modified.
- Server binary: `build/exp046-default-runtime/bin/llama-server`,
  sha256 `3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`.
- Launcher: `scripts/run_qwen38_server.sh`, committed blob
  sha256 `17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc`.
  The working tree carries an uncommitted comment-only edit. The edit adds comment lines
  noting the ring-depth-3 patch and `build/exp063-default-runtime` and states that the
  launcher default still points at the EXP-046 runtime. No executable line changed, and the
  executable default (`BUILD_DIR`, `THREADS`, port, KV, MTP, EHS) is unchanged from the
  committed blob.
- Model: Qwen3.8-Flash-Next UD-IQ3_XXS, three shards; reference SHA-256 in
  `docs/RUN_QWEN38.md` (not re-hashed here; 81.96 GB).
- MTP head: `models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf`,
  sha256 `4563303ab6e7d40f409750fd48af76bf33dd76b0773951cbf484109107de8140`.
- Measured request: `results/archive/EXP-2026-09-06-038/ab/request.json`,
  sha256 `792ad70b50bbee09fed7c48ef75336d7be9fb20e34155592813d4a039ed56341`.
  395 evaluated tokens, `n_predict` 48, `temperature` 0, `cache_prompt` false, greedy,
  `stream` false.
- Warmup request: `results/archive/EXP-2026-09-07-040/ab/request-short.json`,
  sha256 `4d1597f51bb3ec4f964be45c4bad375dcd167ab0a167918e95ece78216e81f56`
  (`n_predict` 128; the warmup stopped at EOS after 104 tokens, which is fine because it is
  discarded).
- Reference completion content SHA-256 from the ten EXP-038 runs:
  `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1`.

The runtime identity is verified by sha256 because `build/` and `work/` are gitignored; it is
never verified by `git diff`.

## Single variable

The batch-pool thread count: `-tb 12` (A) versus `-tb 16` (B). Setting `-tb 12` explicitly in
arm A (instead of leaving it at the inherited value of 12) keeps both arms symmetric, so the
only difference is the value.

Held constant across both arms: binary, libraries, model, MTP profile, generation thread
count (`-t 12`), context (`-c 196608`), KV quantization (`q4_0`), `-np 1`, `-fa on`, batch
shape (`-b 2048 -ub 512`), `cache_prompt=false`, temperature, port and
`GGML_EXPERT_PINNED_RING=1`.

## Second-threadpool caveat (inherent confound)

`common_threadpools::init` (`common/common.cpp:1818-1860`) creates ONE thread pool and reuses
it for both batch and generation when the two parameter sets match, and otherwise creates a
second, initially paused batch pool.

- Arm A (`-t 12 -tb 12`): the sets match, so arm A reuses a single 12-thread pool.
- Arm B (`-t 12 -tb 16`): the sets differ, so arm B creates a second 16-thread batch pool.

The A/B difference is therefore not only the count 12 versus 16. Arm B also adds a second
OpenMP team. This is an inherent confound of the lever and is stated here in the verdict: a
win could not be attributed to the thread count alone, and a loss may come from the extra team
rather than from the four extra workers. 16 threads on 16 physical cores is safe; SMT
configurations (24/32) were rejected for decode and are out of scope. A larger batch pool
competes for RAM bandwidth with the CUDA submission thread and the desktop; that contention is
exactly what this card measures.

## Scope note: EXP-013 rejection covers decode only

EXP-013 rejected `-t 16` for warm DECODE (17.656 vs 17.615 tok/s at 12 threads, -0.23%, inside
noise, with mean occupied cores 15.93 versus 11.95). That rejection concerned the generation
pool during decode and does not cover the batch pool during the prefill gather. This card
reopens only the prefill-gather question. It does not reopen or contradict the decode result,
and a null here leaves the decode rejection standing.

## Protocol (manual, one server at a time)

- Accepted profile, port 8087 (kept separate from the daily 8081 server).
- One fresh server process per run, one warmup request (`request-short.json`, discarded),
  then one measured 395-token request (`request.json`). No measured request was repeated
  inside a server session.
- Balanced order A, B, B, A: four fresh-server runs, tagged T0a (A), T1a (B), T1b (B),
  T0b (A). Pair 1 is A then B; pair 2 is B then A.
- Every measured run is 395/395 prompt tokens and 48/48 generated tokens with `stop_type`
  `limit`.
- The page cache is warm; the first request after a fresh server is the discarded warmup,
  not a measured sample.
- No profiler runs alongside a timed run, and no two servers run at the same time.

## Paired results

`prompt_ms` is `timings.prompt_ms` for the fixed 395-token request. Pair gain is
`100 * (A - B) / A`, so a positive number means the `-tb 16` arm is faster than the control.

| pair | order | A run | B run | A prompt_ms | B prompt_ms | B gain vs A |
| --- | --- | --- | --- | ---: | ---: | ---: |
| 1 | A/B | T0a | T1a | 2979.751 | 2956.459 | +0.78% |
| 2 | B/A | T0b | T1b | 2845.844 | 2997.230 | -5.32% |

Summary:

- Median paired gain: -2.27%. With two pairs this is the mean of +0.78% and -5.32%.
- Wins: B wins 1 of 2 pairs, but the sign is inconsistent between pairs, which is the
  signature of noise rather than of a repeatable effect.
- Independent arm medians: A 2912.797 ms, B 2976.844 ms, so B is +2.20% slower than A on the
  arm-level medians, in the opposite direction to pair 1 alone.
- The candidate does not reach the +3% accept bar and there is no reproducible gain.

The two directions do not agree, and the arm medians lean slower, so the only defensible
reading is that any effect is smaller than the natural spread on this two-pair screen.

## Byte-identity control

`sha256(content)` computed with `jq -j '.content'` (no trailing newline) is
`7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1` for every one of the
four measured documents (T0a, T1a, T1b, T0b). It is identical in all four runs and matches the
EXP-038 reference for the same request, so changing the batch-pool thread count changes timing
only, not results. Every measured document reports `stop_type` `limit`, `tokens_predicted` 48
and `timings.predicted_n` 48, with `prompt_n` 395.

## Decode context (not the primary metric)

`timings.predicted_per_second` from the same 48-token responses: A T0a 22.3571, A T0b 22.2260,
B T1a 21.9514, B T1b 21.0759. Both B samples are at or below both A samples, but the spread is
wide at one sample per arm. Decode is not the verdict and is never presented as a prefill
gain; the counter is reported only so the table is complete.

## Verdict: REJECTED

There is no reproducible gain. The paired gains are +0.78% and -5.32%, the signs disagree, the
median paired gain is -2.27%, and the independent arm medians put B +2.20% behind A. Every
value is below the +3% accept bar, and the effect is within the natural spread of a two-pair
screen. The candidate is either neutral or slightly worse.

The second-threadpool confound makes the result harder to attribute: arm B does not just add
four workers, it also adds a whole second OpenMP team, so neither a win nor a loss could be
read as a clean thread-count effect. The batch-thread direction is closed for the 395-token
prefill workload. No other count (14, 18, and so on) is swept automatically, the launcher
default stays at `-t 12`, and nothing is enabled by default.

This null does not reopen or weaken the EXP-013 decode rejection; it is a separate lever on a
separate pool, and it too came back null.

## Run protocol (literal commands, already executed; do not re-run from this card)

```
cd /home/end0/AI/FrankenMoE-CUDA

# Precondition: the owner's daily server on port 8081 is stopped; port 8087 is free.

# Arm A: batch pool 12, one shared pool with the generation pool.
# Run four times in the order A, B, B, A, using the matching tag suffix.
TAG=T0a \
PORT=8087 \
EXTRA_ARGS="--threads-batch 12" \
LOG_PATH=results/exp061-T0a.log \
MONITOR_PATH=results/exp061-T0a-monitor.csv \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

# After "server is listening" in the log: warmup, discarded.
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-07-040/ab/request-short.json \
  > results/archive/EXP-2026-09-15-061/T0a-warmup.json

# One measured request per fresh server. Record prompt_ms and the content hash.
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-06-038/ab/request.json \
  | tee results/archive/EXP-2026-09-15-061/T0a-prefill.json \
  | jq -r '"prompt_ms \(.timings.prompt_ms)  prompt_n \(.timings.prompt_n)  pred_n \(.timings.predicted_n)  stop \(.stop_type)"'
jq -j '.content' results/archive/EXP-2026-09-15-061/T0a-prefill.json | sha256sum

# Stop cleanly with SIGINT, confirm no orphan, then start the next run.

# Arm B: batch pool 16 (a second paused batch pool is created).
TAG=T1a \
PORT=8087 \
EXTRA_ARGS="--threads-batch 16" \
LOG_PATH=results/exp061-T1a.log \
MONITOR_PATH=results/exp061-T1a-monitor.csv \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
# then the same warmup and measured curl calls with the T1a output names,
# kill -INT. Repeat for T1b (B) and T0b (A) to complete A, B, B, A.
```

The launcher expands `EXTRA_ARGS` unquoted on its own command line (line 90), so the value
must be passed as one quoted assignment (`EXTRA_ARGS="--threads-batch 16"`); splitting it
across lines leaves fragments outside the assignment and the server never starts. The launcher
defaults stay as accepted (`THREADS=12`, `-c 196608`, `-np 1`, `-fa on`, KV q4_0, `--cpu-moe`,
MTP `n_max=2`, EHS=0). Port 8087 keeps this block separate from the daily 8081 server. The
environment is read once at start-up, so each arm needs its own server start.

## Raw data

All raw request responses are under `results/archive/EXP-2026-09-15-061/`:

- `T0a-prefill.json`, `T1a-prefill.json`, `T1b-prefill.json`, `T0b-prefill.json`: the four
  measured responses;
- `T0a-warmup.json`, `T1a-warmup.json`, `T1b-warmup.json`, `T0b-warmup.json`: the discarded
  warmups.

## What this closes

- The batch-thread (`-tb`) direction is closed for the 395-token prefill workload. No other
  count is swept automatically.
- The launcher default stays at `-t 12`; nothing is enabled by default and no file other than
  this card and the PRE-5 row in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md` was
  changed.
- The second-threadpool confound is recorded as a property of this lever: on this runtime a
  differing `-tb` value does not only change the worker count, it also forces a second OpenMP
  team, so future thread-pool work must either accept that confound or design it out.
- The EXP-013 decode rejection stands unchanged.
