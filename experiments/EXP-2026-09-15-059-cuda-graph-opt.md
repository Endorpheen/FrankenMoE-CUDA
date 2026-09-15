# EXP-2026-09-15-059: GGML_CUDA_GRAPH_OPT prefill screen (PRE-3)

Status: `REJECTED` (2026-09-15).
Kind: `performance`.

This card records a completed env-only A/B screen. The raw data is under
`results/archive/EXP-2026-09-15-059/`. The card itself starts no server, loads no model,
builds nothing and runs no curl. It is the written verdict for the candidate tracked as
PRE-3 in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.

The candidate is an env-only knob, so no source, patch, binary or launcher change was
made. The single variable was `GGML_CUDA_GRAPH_OPT`, unset in arm A and `1` in arm B.

## Hypothesis (one)

`GGML_CUDA_GRAPH_OPT=1` enables an extra graph-rewrite pass on the CUDA backend. The
pass finds `attn_norm` fork nodes whose fan-out is exactly 3 and interleaves the Q/K/V
branches across streams to overlap them. It requires `USE_CUDA_GRAPH` (present, because
`GGML_CUDA_GRAPHS=ON`), CUDA graphs enabled and a single GPU. It touches only the GPU
dense subgraph and does not change the graph key, capture or reuse logic.

The testable consequence: if the QKV overlap shortens the prefill critical path without
changing the output, the 395-token `prompt_ms` drops relative to the unset control while
the completion content stays byte-identical.

## Distinguishing this pass from the closed two-variant capture direction

This experiment is NOT a reopening of EXP-049, EXP-050, EXP-051 or EXP-052. Those
addressed how a CUDA graph is captured, keyed, reused or extended (the two-variant
capture direction, with EXP-052 rejecting the L2-only variant at -0.66%).
`GGML_CUDA_GRAPH_OPT` is a different thing on a different code path:

- it is a graph-rewrite pass on the GPU dense subgraph, not a capture or keying change;
- it does not add a second captured graph variant and does not change graph reuse, the
  graph key or graph buffer lifetimes;
- it only reorders the Q/K/V branches of `attn_norm` forks across streams.

A result here says nothing about the closed direction, and a null here does not reopen
it. This screen is distinct because it changes the graph before capture or at build
time; the closed work changed capture and reuse.

## Baseline identity (fixed before the runs)

- Source tree: `work/llama.cpp-exp046-default` (base `4aaad5d318a790a42c2197975ec8fadbad42602b` +
  expert-tier-integration + integration-drift + mtp-sidecar + pinned-ring +
  pinned-ring-default-on + two-thread-gather + iq2s-fused-decode +
  iq2s-fused-decode-default-on). Read only; not modified by this experiment.
- Server binary: `build/exp046-default-runtime/bin/llama-server`,
  sha256 `3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`.
- Launcher: `scripts/run_qwen38_server.sh`, committed blob
  sha256 `17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc`.
  The working tree carries an uncommitted comment-only edit (working-tree sha
  `0cfb3e8b2670d630328147998be52a7f80e69581616864ae475d244243c21dd6`). The edit adds
  four comment lines noting the ring-depth-3 patch and `build/exp063-default-runtime`,
  and it states that the launcher default still points at the EXP-046 runtime. No
  executable line changed, and the executable default (`BUILD_DIR`, `THREADS`, port,
  KV, MTP, EHS) is unchanged from the committed blob.
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
  (`n_predict` 128; the warmup stopped at EOS after 104 tokens, which is fine because
  it is discarded).

The runtime identity is verified by sha256 because `build/` and `work/` are gitignored;
it is never verified by `git diff`.

## Single variable

`GGML_CUDA_GRAPH_OPT`: `1` (the pass on) versus unset (the pass off, the accepted
runtime). The code enables the pass only when the value is exactly `1`
(`env != nullptr && atoi(env) == 1`); `0` and any other value are not used as a third
arm. Nothing else changes between arms. `GGML_EXPERT_PINNED_RING=1` and
`GGML_CPU_IQ2S_FUSED=1` were explicit and identical in both arms.

## Protocol (manual, one server at a time)

- Accepted profile, port 8087 (kept separate from the daily 8081 server).
- One fresh server process per run, one warmup request, then one measured prefill
  request. No request was repeated inside a server session.
- Balanced order A, B, B, A: four fresh-server runs, tagged G0a (A), G1a (B),
  G1b (B), G0b (A). Pair 1 is A then B; pair 2 is B then A.
- Every measured run is 395/395 prompt tokens and 48/48 generated tokens with
  `stop_type` `limit`.
- No profiler runs alongside a timed run, and no two servers run at the same time.
- The page cache is warm; the first request after a fresh server is the discarded
  warmup, not a measured sample.

This screen is Stage 1 only. The screen pre-registered an escalate-only-if-signal rule
(median paired gain >= 1% opens the formal stage). The measured gain was negative on
both pairs, so no formal stage was opened and no decode256 guard arm was run. Decode
from the measured prefill responses is reported below for context only; it is not a
clean regression guard at 48 generated tokens and one sample per arm.

## Paired results

`prompt_ms` is `timings.prompt_ms` for the fixed 395-token request. Pair gain is
`100 * (A - B) / A`, so a positive number means the `GGML_CUDA_GRAPH_OPT=1` arm is
faster than the unset control.

| pair | order | A run | B run | A prompt_ms | B prompt_ms | B gain vs A |
| --- | --- | --- | --- | ---: | ---: | ---: |
| 1 | A/B | G0a | G1a | 3185.494 | 3281.893 | -3.03% |
| 2 | B/A | G0b | G1b | 3290.943 | 3550.965 | -7.90% |

Summary:

- Median paired gain: -5.46%. With two pairs this is the mean of -3.026% and -7.901%.
- Wins: B wins 0 of 2 pairs. The candidate is slower on both.
- Independent arm medians: A 3238.2185 ms, B 3416.429 ms, so B is +5.50% slower than A.
- The candidate did not reach the 1% screen signal; the direction is not escalated.

## Byte-identity control

`sha256(content)` computed with `jq -j '.content'` (no trailing newline) is
`7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1` for every one of
the four measured documents (G0a, G1a, G1b, G0b). It is identical in all four runs and
matches the EXP-062 reference for the same request, so the two arms produce
byte-identical output and the pass changes timing only, not results. Every measured
document reports `stop_type` `limit`, `tokens_predicted` 48 and `timings.predicted_n`
48, with `prompt_n` 395.

## Graph-reuse structure observation

This was recorded to separate a timing regression from a structural change. The
`graphs reused` counter is cumulative, so only in-session window deltas are comparable.
In all four runs the warmup ends at 56 and the measured request ends at 72, so the
per-request window delta is 16 in both arms. The pass did not change the graph capture
or reuse structure on this workload. The regression is therefore not explained by a
different capture pattern: the branch is rewritten but no graph is added, re-keyed or
reused differently, and the wall time still rises.

Each log also shows `expert ring enabled: 2 pinned host slots x 16 MiB` and a teardown
of `expert ring staged 31092 calls, 31092 chunks`, identical in all four runs, so the
CPU-side staging workload was constant.

## Decode context (not the primary metric)

`timings.predicted_per_second` from the same 48-token responses: A G0a 22.0189, B G1a
20.5158, B G1b 22.4544, A G0b 21.1424. The arm medians are close and the spread
overlaps, so decode is neither a win nor a clean loss here. This is not used as the
verdict and is never presented as a prefill gain.

## Safety controls

- Swap: `process_swap_kib` is 0 in every telemetry sample of all four monitor CSVs
  (`results/exp059-*-monitor.csv`), so `VmSwap` stays 0 throughout.
- Memory: peak RSS stayed between 34.5 and 34.9 GiB and peak GPU used between 10.4 and
  10.5 GiB across the four runs, inside the accepted profile's noise. The env knob
  allocates nothing new.
- Logs: each log ends with `cleaning up before exit...` and the ring teardown line; no
  CUDA error, OOM or watchdog line appears in any log, and no orphan `llama-server`
  remained.
- Baseline identity: the server binary, launcher and model match the fixed EXP-046
  identity above and were not rebuilt or replaced for this screen.

## Verdict: REJECTED

The graph-rewrite pass is not inert: it is slower on both paired comparisons and buys
nothing. The prefill median paired gain is -5.46%, B wins 0 of 2 pairs, and the arm
medians put the candidate +5.50% behind the control. None of the accept bars is met:
the prefill gain is far below +3%, the win count is 0 of 2, and the byte-identity and
safety controls pass but do not offset a regression. The 1% screen signal was not
reached, so the direction is closed without escalation.

The result does not reopen the closed two-variant capture direction (EXP-049, EXP-050,
EXP-051, EXP-052). The accepted EXP-046 runtime stays the baseline and the fallback;
nothing is enabled by default, no file other than this card and the PRE-3 row in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md` was changed, and no launcher
default is touched. The pass is either not reached on this graph, or its reordering
adds overhead without shortening the critical path; the unchanged `graphs reused`
window favors the latter reading, since the same graphs run and the wall time grows.

## Run protocol (literal commands, already executed; do not re-run from this card)

```
cd /home/end0/AI/FrankenMoE-CUDA

# Arm A: GGML_CUDA_GRAPH_OPT unset (accepted runtime), one fresh server.
# Run four times in the order A, B, B, A, using the matching tag suffix.
TAG=G0a \
PORT=8087 \
LOG_PATH=results/exp059-G0a.log \
MONITOR_PATH=results/exp059-G0a-monitor.csv \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

# After "server is listening" in the log: warmup, discarded.
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-07-040/ab/request-short.json \
  > results/archive/EXP-2026-09-15-059/G0a-warmup.json

# One measured request per fresh server. Record prompt_ms and the content hash.
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-06-038/ab/request.json \
  | tee results/archive/EXP-2026-09-15-059/G0a-prefill.json \
  | jq -r '"prompt_ms \(.timings.prompt_ms)  prompt_n \(.timings.prompt_n)  pred_n \(.timings.predicted_n)  stop \(.stop_type)"'
jq -j '.content' results/archive/EXP-2026-09-15-059/G0a-prefill.json | sha256sum

# Graph-reuse window and staging evidence for the arm.
grep -n 'graphs reused' results/exp059-G0a.log

# Stop cleanly with SIGINT, confirm no orphan, then start the next run.

# Arm B: GGML_CUDA_GRAPH_OPT=1, identical block with the single variable exported.
TAG=G1a \
PORT=8087 \
LOG_PATH=results/exp059-G1a.log \
MONITOR_PATH=results/exp059-G1a-monitor.csv \
GGML_CUDA_GRAPH_OPT=1 GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
# then the same warmup and measured curl calls with the G1a output names,
# grep the log, kill -INT. Repeat for G1b (B) and G0b (A) to complete A, B, B, A.
```

`GGML_CUDA_GRAPH_OPT=1` is the only variable between arms; the launcher defaults stay
as accepted (`THREADS=12`, `-c 196608`, `-np 1`, `-fa on`, KV q4_0, `--cpu-moe`, MTP
`n_max=2`, EHS=0). Port 8087 keeps this block separate from the daily 8081 server. The
environment is read once at start-up, so each arm needs its own server start.

## Raw data

All raw request responses are under `results/archive/EXP-2026-09-15-059/`:

- `G0a-prefill.json`, `G1a-prefill.json`, `G1b-prefill.json`, `G0b-prefill.json`: the
  four measured responses;
- `G0a-warmup.json`, `G1a-warmup.json`, `G1b-warmup.json`, `G0b-warmup.json`: the
  discarded warmups.

Server logs are `results/exp059-G0a.log`, `results/exp059-G1a.log`,
`results/exp059-G1b.log`, `results/exp059-G0b.log`; telemetry is
`results/exp059-G0a-monitor.csv`, `results/exp059-G1a-monitor.csv`,
`results/exp059-G1b-monitor.csv`, `results/exp059-G0b-monitor.csv`.

## What this closes

- The `GGML_CUDA_GRAPH_OPT` direction is closed for this runtime. It is not carried into
  the baseline or the launcher, and nothing is enabled by default.
- The formal stage and the decode256 guard are not run; the screen signal was negative.
- The closed two-variant capture direction stays closed; this screen neither supports
  nor reopens it.
