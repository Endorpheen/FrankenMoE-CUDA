# EXP-2026-09-15-058: GGML_OP_OFFLOAD_MIN_BATCH sweep (PRE-2)

Status: `REJECTED` (2026-09-15). The sweep ran and closed. No tested value improves
the 395-token prefill over the default `32`; the two large probes `4096` and `8192`
are both roughly twice as slow and change the completion. No source, patch, build,
model or default was changed. The only variable was the `GGML_OP_OFFLOAD_MIN_BATCH`
environment value, one value per server start.

Raw data: `results/archive/EXP-2026-09-15-058/`. Logs: `results/exp058-*.log`.
Source: design note `experiments/designs/exp057/D2-knob-survey.md` (row for
`GGML_OP_OFFLOAD_MIN_BATCH`) and row `PRE-2` in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.

## Kind

`performance`, environment-only tuning screen. No source, patch, build, model,
quantization, KV, context, batch or thread-count change. The accepted EXP-046
default runtime stays byte-identical and remains the baseline and the fallback.

## Hypothesis (one)

`GGML_OP_OFFLOAD_MIN_BATCH` is the only surveyed environment knob that shifts MoE
work between the CPU `MUL_MAT_ID` path and the CUDA path. With `--cpu-moe` the expert
weights live in a host buffer, so the scheduler assigns `MUL_MAT_ID` to CUDA and
copies the selected expert tensor over PCIe only when the batch metric (`MUL_MAT_ID`
uses `ne[2]`) reaches the threshold (`ggml/src/ggml-backend.cpp:961-967`, default 32).

The testable consequence for this workload:

- decode verify is a 1-3 token batch, so at every tested value (4 and above) decode
  MoE stays on the CPU and decode throughput should be unchanged;
- prefill is a large batch, so bulk prefill MoE is already on the GPU at the default
  32;
- the small checkpoint tail (below 32) is forced onto the CPU today, so a threshold
  of 4 can move that tail to the GPU;
- a threshold of 4096/8192 forces all prefill MoE onto the CPU and probes how much of
  the prefill time is expert H2D transfer versus CPU compute.

Expected direction: decode flat at 4, 8 and 16; prefill possibly a little better at
4/8/16 if the tail upload overlaps or is cheap; prefill clearly worse at 4096/8192,
which is the point of those two probes rather than a defect.

Main risk stated before the run: a threshold low enough to cover the decode batch
(1-3) would pull decode MoE onto the GPU and add a per-token expert H2D copy on every
verify step, which is expected to lose badly. Values 1, 2 and 3 are therefore
excluded from the sweep by construction, and they were never tested by design
(`D2-knob-survey.md`; series plan line 78). At 4 the verify batch (max 3) stays below
the threshold, so decode was expected to stay CPU-side.

## Baseline identity (fixed before the runs)

- Source tree: `work/llama.cpp-exp046-default` (pinned base
  `4aaad5d318a790a42c2197975ec8fadbad42602b` + `expert-tier-integration` +
  `integration-drift` + `mtp-sidecar` + `pinned-ring` + `pinned-ring-default-on` +
  `two-thread-gather` + `iq2s-fused-decode` + `iq2s-fused-decode-default-on`). Read
  only; not modified by this experiment.
- Server binary: `build/exp046-default-runtime/bin/llama-server`,
  sha256 `3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`.
- Launcher: `scripts/run_qwen38_server.sh`. The committed version is
  sha256 `17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc`. At the
  time of this card the working copy carries a comment-only edit (four added comment
  lines about the EXP-062 depth-3 ring and the future `build/exp063-default-runtime`),
  which changes the file hash but not a single executable line; the launched binary,
  flags and defaults are unchanged. Identity is therefore anchored on the committed
  hash above plus the server-binary hash.
- Model: Qwen3.8-Flash-Next UD-IQ3_XXS, three shards; reference SHA-256 in
  `docs/RUN_QWEN38.md` (81.96 GB, not re-hashed here).
- MTP head: `models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf`,
  sha256 `4563303ab6e7d40f409750fd48af76bf33dd76b0773951cbf484109107de8140`.

Requests (carried over unchanged):

- Warmup: `results/archive/EXP-2026-09-07-040/ab/request-short.json`,
  sha256 `4d1597f51bb3ec4f964be45c4bad375dcd167ab0a167918e95ece78216e81f56`
  (n_predict 128, temperature 0, cache_prompt false, stream false; byte-identical to
  `results/archive/EXP-2026-09-15-056/requests/request-short.json`).
- Decode: `results/archive/EXP-2026-09-09-046/request-decode256.json`,
  sha256 `5975261ba9441955f53227d8c7784b176c7fbcd96d5661040d50374f4655d2ac`
  (n_predict 256, temperature 0, cache_prompt false, stream false).
- 395-token prefill: `results/archive/EXP-2026-09-06-038/ab/request.json`,
  sha256 `792ad70b50bbee09fed7c48ef75336d7be9fb20e34155592813d4a039ed56341`
  (n_predict 48, temperature 0, cache_prompt false, stream false). Measured
  `prompt_n` = 395 in every run.

The runtime identity is verified by sha256 because `build/` and `work/` are
gitignored; it is never verified by `git diff`.

## Single variable

`GGML_OP_OFFLOAD_MIN_BATCH`: unset (default 32, the shared control) or exactly one of
`4`, `8`, `16`, `4096`, `8192`. One value per server start. Values 1, 2 and 3 are
never used by design (see Hypothesis). Nothing else changes between starts.

## Server argv and env (verbatim)

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

Fixed env, identical in every run: `GGML_EXPERT_PINNED_RING=1` (explicit) and
`GGML_CPU_IQ2S_FUSED=1` (explicit). Every run reports `expert ring enabled: 2 pinned
host slots x 16 MiB`. Environment knobs are read once at start-up, so every value
needs its own server start.

## Protocol

Two sweeps in one block.

First sweep: 6 server starts, strictly one at a time, one value per start, in the
order control (default 32), 4, 8, 16, 4096, 8192. The single control start is shared
by all five values. Per start: warmup (`request-short.json`, discarded), then the
decode256 request, then the 395-token request. 3 requests per start, 18 requests
total. The large-value probes ran last so their expected slowdown cannot bias the
candidate values.

Warm re-run: 5 additional server starts with the same per-start protocol, to break
the cold-first confound of the first sweep. Control 32 was run twice (v32b, v32c) so
the control itself is bracketed; then 16 (v16b), 8 (v8b) and 4 (v4b).

## Results

Raw data: `results/archive/EXP-2026-09-15-058/<tag>-warmup.json`,
`<tag>-decode256.json`, `<tag>-prefill.json`. Logs: `results/exp058-<tag>.log`.

`prefill_ms` is `timings.prompt_ms` of the 395-token request (`prompt_n` = 395 in
every run). `decode` is `timings.predicted_per_second` of decode256
(`tokens_predicted` 256/256, `stop_type` `limit` in every run). The two `sha` columns
are the first 8 hex digits of `sha256(content)`.

### Cold first sweep

The first run (v32) was cold: the page cache was not yet warm, visible in its log as
a 3764.89 ms / 41-token warmup prompt eval at 10.89 tok/s against 1630-2255 ms in the
later runs. It is the first-run confound, not a threshold effect.

| tag | value | prefill_ms | decode tok/s | decode ms/tok | 395-tok sha | decode256 sha |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| v32 | 32 (control, cold) | 4503.439 | 14.8061 | 67.5396 | 7dfa7d1e | 9270353f |
| v4 | 4 | 3336.449 | 17.8132 | 56.1382 | 7dfa7d1e | eabe28ea |
| v8 | 8 | 3123.907 | 18.6782 | 53.5382 | 7dfa7d1e | 9270353f |
| v16 | 16 | 2682.857 | 18.6805 | 53.5317 | 7dfa7d1e | 9270353f |
| v4096 | 4096 | 6770.820 | 18.6339 | 53.6657 | 585438c9 | f52507e8 |
| v8192 | 8192 | 6674.625 | 19.4972 | 51.2895 | 585438c9 | f52507e8 |

The cold sweep is descriptive only. Taken at face value it would show prefill
improving as the threshold drops (v16 2682.857 ms, the best of the six), but the
control is cold and must not be compared directly.

### Warm re-run

| tag | value | prefill_ms | decode tok/s | decode ms/tok | 395-tok sha | decode256 sha |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| v32b | 32 (control) | 3027.795 | 18.4332 | 54.2499 | 7dfa7d1e | 9270353f |
| v16b | 16 | 3084.243 | 18.8644 | 53.0098 | 7dfa7d1e | 9270353f |
| v8b | 8 | 3124.316 | 18.5651 | 53.8646 | 7dfa7d1e | 9270353f |
| v4b | 4 | 3672.170 | 19.3369 | 51.7147 | 7dfa7d1e | eabe28ea |
| v32c | 32 (control) | 3102.162 | 18.5091 | 54.0276 | 7dfa7d1e | 9270353f |

Warm control: prefill median 3064.979 ms (v32b 3027.795, v32c 3102.162) and decode
median 18.4711 tok/s. Prefill deltas against that median (positive is worse):

| value | run | prefill delta | decode delta |
| ---: | --- | ---: | ---: |
| 16 | v16b | +0.629% | +2.129% |
| 8 | v8b | +1.936% | +0.508% |
| 4 | v4b | +19.811% | +4.687% |

### Cold-first confound

The first sweep cannot separate a threshold effect from the cold first start. v16
looks 40% faster than v32 in the cold table (2682.857 against 4503.439) but is +0.6%
worse in the warm re-run once the control is bracketed by v32b/v32c. The two warm
control samples themselves span 3027.795 to 3102.162 ms (about 2.4% spread), so the
observed non-control deltas are at or below the run-to-run spread. The warm re-run is
the basis for the verdict; the cold sweep is context.

### Decode

Decode shows no consistent gain. In the warm re-run the largest decode number is 4
at 19.3369 tok/s, but in the cold sweep 4 was the slowest of the non-probe runs
(17.8132 tok/s), and 8/16 move by under 0.6% in the warm re-run. The direction does
not repeat across the two sweeps.

### Correctness

- 395-token completion (48 greedy tokens): `sha256(content)` is
  `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1` for 32, 4, 8 and
  16 in both sweeps (231 chars), and
  `585438c93074f38557a0b60437b34acdcd3ceb07fbe9ae8ef20c9636e7835d22` for 4096 and
  8192 (243 chars). The 4096/8192 completion is both longer and different, so those
  two values are not correctness-preserving.
- decode256 completion (256 greedy tokens): `sha256(content)` is
  `9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757` for 32, 8 and 16
  and for every warm control run (1187 chars);
  `eabe28ea0e9ea6455f7a144d6b31fe3d4abe30a595db598975b5ca9aa00395b4` for 4 and v4b
  (1155 chars); and
  `f52507e841868b27516765d97de413f97a6273b2dc3e3a26e3603da0cc38fdcd` for 4096 and 8192
  (1203 chars). The value-4 divergence repeats in both sweeps, so 4 also changes the
  greedy decode output even though its verify batch stays below the threshold.
- The warmup short request follows the same pattern (its output diverges at 4 and at
  4096/8192). Greedy output is otherwise deterministic: seven runs at 8/16/32 share
  the exact decode256 hash.

### Safety and resource controls

- No `CUDA error`, `out of memory`, watchdog or swap line in any of the 11 logs; no
  VmSwap growth and no fallback message.
- All 11 logs end with `cleaning up before exit...`. No orphan `llama-server`
  remained. No profiling ran alongside a timed run.
- Expert ring staged counter at shutdown is `43212 / 43212` for 4, 8, 16 and 32 (both
  sweeps). At 4096 and 8192 the ring never stages, because all prefill MoE runs on
  the CPU, which is exactly the mechanism those two probes exercise.

## Verdict: REJECTED

- Prefill bar (>= 3% better at one of 4, 8, 16 against the control) is not met. Best
  non-control warm value is 16 at +0.629%, inside the 2.4% warm-control spread; 8 is
  +1.936% and 4 is +19.811%, both worse. The apparent cold-sweep advantage of 16 is
  the cold-first confound and does not survive the warm re-run.
- Decode bar (regression <= 2%) is met, but there is no consistent gain: the warm
  winner (4) is the cold-sweep loser, so the sign does not repeat.
- Correctness: 4096 and 8192 change the 395-token completion and are far slower
  (about 2x the warm control), and 4 changes the greedy decode256 completion. No
  value is both faster and byte-identical.
- Conclusion: no value in `4, 8, 16` clears the screen bar, and `4096/8192` are
  rejected on both latency and correctness. The `GGML_OP_OFFLOAD_MIN_BATCH` direction
  is closed. Working defaults stay at 32; the accepted runtime and every default are
  unchanged.

## Run protocol (literal commands; DO NOT execute in this task)

```
cd /home/end0/AI/FrankenMoE-CUDA

# Identity preflight (all four must match before the first start)
sha256sum build/exp046-default-runtime/bin/llama-server
#   3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec
git show HEAD:scripts/run_qwen38_server.sh | sha256sum
#   17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc
sha256sum results/archive/EXP-2026-09-06-038/ab/request.json
#   792ad70b50bbee09fed7c48ef75336d7be9fb20e34155592813d4a039ed56341
sha256sum results/archive/EXP-2026-09-09-046/request-decode256.json
#   5975261ba9441955f53227d8c7784b176c7fbcd96d5661040d50374f4655d2ac

# Control: GGML_OP_OFFLOAD_MIN_BATCH unset => default 32
PORT=8087 LOG_PATH=results/exp058-v32.log \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

# After "listening on http://127.0.0.1:8087" appears in the log:
# 1. warmup (discarded)
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-07-040/ab/request-short.json >/dev/null

# 2. decode256
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-09-046/request-decode256.json \
  > results/archive/EXP-2026-09-15-058/v32-decode256.json

# 3. 395-token request
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-06-038/ab/request.json \
  > results/archive/EXP-2026-09-15-058/v32-prefill.json

kill -INT <server_pid>

# Variants: same block, the single variable exported before the launcher.
# TAG/VALUE pairs, in order: v4/4, v8/8, v16/16, v4096/4096, v8192/8192;
# warm re-run: v32b/(unset), v16b/16, v8b/8, v4b/4, v32c/(unset).
TAG=v4
GGML_OP_OFFLOAD_MIN_BATCH=4 PORT=8087 LOG_PATH="results/exp058-${TAG}.log" \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
# then the same three curl calls with the ${TAG} output names, then kill -INT
```

## What this leads to

- The `GGML_OP_OFFLOAD_MIN_BATCH` direction is closed. The accepted runtime and all
  defaults, including the default 32, stay unchanged.
- No promoted value remains for a formal paired A/B.
- Tracker row `PRE-2` is set to `REJECTED`, and the series continues with the next
  queued candidate.
