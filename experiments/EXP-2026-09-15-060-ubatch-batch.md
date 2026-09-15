# EXP-2026-09-15-060: prefill batch shape (`-b`/`-ub`) on the fixed 395-token prompt (PRE-4)

Status: `REJECTED` (2026-09-15).
Kind: `performance`.

This card records a completed env/CLI-only A/B screen. The raw data is under
`results/archive/EXP-2026-09-15-060/`. The card itself starts no server, loads no model,
builds nothing and runs no curl. It is the written verdict for the candidate tracked as
PRE-4 in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`, design note
`experiments/designs/exp057/D3-thread-affinity.md` (section P2).

The candidate is a CLI-only knob, so no source, patch, binary or launcher change was made.
The single variable was the batch shape, expressed as the `(-b, -ub)` pair: the accepted
defaults `-b 2048 -ub 512` (arm A) versus `-b 512 -ub 256` (arm B).

## Hypothesis (one)

On the accepted runtime, a smaller prefill batch shape reduces bulk-prefill latency on the
fixed 395-token prompt. Changing the `(-b, -ub)` pair from the runtime defaults
`-b 2048 -ub 512` (arm A) to `-b 512 -ub 256` (arm B) changes the number of physical prefill
chunks and, through the prompt checkpoint break, the size of the CPU 4-token tail, and that
shifts the measured prompt-eval latency in a repeatable direction.

## Link to previous measurements

- EXP-2026-09-07-043 (prefill bottleneck profile): in a 2572.660 ms measured window for this
  same 395-token prompt, the CPU `expert_gather` union was 1463.641 ms (56.9%), so prefill is
  gather-bound; a 217.774 ms tail (about 8.5% of the window) was attributed to the checkpoint
  break `391+4`, a 112.6 MiB checkpoint save on the critical path, and a 4-token CPU batch
  (`server-context.cpp:3537-3551`, `checkpoint_offsets = {4 + n_ubatch, 4}`).
- EXP-2026-09-06-038 (accepted pinned ring, same request): baseline A median prompt latency
  3990.943 ms, candidate 3465.613 ms, median paired gain 13.163%.
- Prior art for the variable itself: D3 section P2 states that `-ub` sets the physical prefill
  chunk and that `-b` only caps the logical batch, so for a 395-token prompt `-b` is inert
  while it stays above the prompt. No `-b`/`-ub` A/B had been run on the accepted runtime.

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

The prefill batch shape, expressed as the `(-b, -ub)` pair:

- A: runtime defaults `-b 2048 -ub 512`, no batch-shape flags passed.
- B: `-b 512 -ub 256`, passed through `EXTRA_ARGS`.

The pair moved as one variable. Held constant across both arms: binary, libraries, model, MTP
profile, thread count (`-t 12`), context (`-c 196608`), KV quantization (`q4_0`), `-np 1`,
`-fa on`, `cache_prompt=false`, temperature, port and `GGML_EXPERT_PINNED_RING=1`.

## Protocol (manual, one server at a time)

- Accepted profile, port 8087 (kept separate from the daily 8081 server).
- One fresh server process per run, one warmup request (`request-short.json`, discarded),
  then one measured 395-token request (`request.json`). No measured request was repeated
  inside a server session.
- Balanced order A, B, B, A: four fresh-server runs, tagged B0c (A), B1c (B), B1d (B),
  B0d (A). Pair 1 is A then B; pair 2 is B then A.
- Every measured run is 395/395 prompt tokens and 48/48 generated tokens with `stop_type`
  `limit`.
- The page cache is warm; the first request after a fresh server is the discarded warmup,
  not a measured sample.
- No profiler runs alongside a timed run, and no two servers run at the same time.

### Superseded first attempt (excluded)

An earlier attempt in the same session did not produce a usable balanced block. Its default
arm samples are on disk as `B0a-prefill.json` and `B0b-prefill.json`, and the B-side launch
failed to start because `EXTRA_ARGS` was word-split instead of being passed as a single
quoted assignment, so no B-side server came up. Per the pre-registered protocol only the
successful balanced runs B0c, B1c, B1d and B0d count; B0a and B0b are excluded and are never
averaged into the statistics. The fix for the failed launch is the quoted assignment shown in
the run protocol below.

## Paired results

`prompt_ms` is `timings.prompt_ms` for the fixed 395-token request. Pair gain is
`100 * (A - B) / A`, so a positive number means the smaller batch shape is faster than the
control.

| pair | order | A run | B run | A prompt_ms | B prompt_ms | B gain vs A |
| --- | --- | --- | --- | ---: | ---: | ---: |
| 1 | A/B | B0c | B1c | 2843.254 | 4887.083 | -71.88% |
| 2 | B/A | B0d | B1d | 3272.902 | 4746.358 | -45.02% |

Summary:

- Median paired gain: -58.45%. With two pairs this is the mean of -71.88% and -45.02%.
- Wins: B wins 0 of 2 pairs. The candidate is far slower on both.
- Independent arm medians: A 3058.078 ms, B 4816.721 ms, so B is +57.51% slower than A.
- The candidate is nowhere near the +3% accept bar; the direction is not escalated.

Control spread: the two A samples are 2843.254 ms and 3272.902 ms (a 429.648 ms spread, about
14% of the smaller sample), which is wide. The B samples are 4887.083 ms and 4746.358 ms.
Even at the optimistic edge the effect is about +45%, so the spread cannot explain a result of
this size.

## Correctness divergence (the decisive finding)

`sha256(content)` computed with `jq -j '.content'` (no trailing newline):

| run | arm | content sha256 |
| --- | --- | --- |
| B0c | A | `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1` |
| B0d | A | `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1` |
| B1c | B | `09609ca7829525d35d30cd4d400e25297efcd40cbbfc62e3d141737e07b9808f` |
| B1d | B | `09609ca7829525d35d30cd4d400e25297efcd40cbbfc62e3d141737e07b9808f` |

The default arm reproduces the EXP-038 reference hash exactly. The `-b 512 -ub 256` arm
diverges from the reference and is stable at its own hash across both B runs. A different
batch shape changes the physical prefill chunking and the checkpoint break, so the resulting
token stream is not the same. This is a correctness divergence, not a timing-only difference.

The MTP counters also move with the shape: the default runs report `draft_n` 35 and
`draft_n_accepted` 29, while the B runs report `draft_n` 34 and `draft_n_accepted` 30. The
divergence is therefore consistent with a different verify/accept path, not with noise on a
single token.

Per the card's hash policy, a divergent hash is never accepted as a performance pass, and the
candidate does not pass silently.

## Decode context (not the primary metric)

`timings.predicted_per_second` from the same 48-token responses: A B0c 21.5996, A B0d 21.5628,
B B1c 21.5011, B B1d 23.8367. Decode is not the verdict here and the two B samples straddle
the control, so no decode conclusion is drawn; the counter is reported only so the table is
complete.

## Verdict: REJECTED

The smaller batch shape fails on both counts. It is far slower: the arm medians put B at
+57.51% above the control, B wins 0 of 2 pairs, and the median paired gain is -58.45%, which
is below the +3% accept bar by a wide margin. It is also not correctness-preserving: the
`-b 512 -ub 256` arm changes the 395-token completion content from the reference
`7dfa7d1e...` to `09609ca7...` in both runs, while the control reproduces `7dfa7d1e...`
exactly.

Either finding alone would reject the candidate. The control spread is wide enough that the
exact slowdown magnitude is uncertain, but it is not wide enough to hide a positive effect,
and no safe acceptance could be built on a shape that changes the model output. The `-b`/`-ub`
batch-shape direction is closed for this workload. No sweep of other shapes is run
automatically, the launcher default stays untouched, and nothing is enabled by default.

## Run protocol (literal commands, already executed; do not re-run from this card)

```
cd /home/end0/AI/FrankenMoE-CUDA

# Precondition: the owner's daily server on port 8081 is stopped; port 8087 is free.
# EXTRA_ARGS must be a single quoted assignment. Word-splitting it caused the failed
# first launch described above.

# Arm A: default batch shape, no EXTRA_ARGS, one fresh server.
# Run four times in the order A, B, B, A, using the matching tag suffix.
TAG=B0c \
PORT=8087 \
LOG_PATH=results/exp060-B0c.log \
MONITOR_PATH=results/exp060-B0c-monitor.csv \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

# After "server is listening" in the log: warmup, discarded.
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-07-040/ab/request-short.json \
  > results/archive/EXP-2026-09-15-060/B0c-warmup.json

# One measured request per fresh server. Record prompt_ms and the content hash.
curl -s http://127.0.0.1:8087/completion -H 'Content-Type: application/json' \
  -d @results/archive/EXP-2026-09-06-038/ab/request.json \
  | tee results/archive/EXP-2026-09-15-060/B0c-prefill.json \
  | jq -r '"prompt_ms \(.timings.prompt_ms)  prompt_n \(.timings.prompt_n)  pred_n \(.timings.predicted_n)  stop \(.stop_type)"'
jq -j '.content' results/archive/EXP-2026-09-15-060/B0c-prefill.json | sha256sum

# Stop cleanly with SIGINT, confirm no orphan, then start the next run.

# Arm B: batch shape -b 512 -ub 256, identical block with EXTRA_ARGS added as one string.
TAG=B1c \
PORT=8087 \
EXTRA_ARGS="-b 512 -ub 256" \
LOG_PATH=results/exp060-B1c.log \
MONITOR_PATH=results/exp060-B1c-monitor.csv \
GGML_EXPERT_PINNED_RING=1 GGML_CPU_IQ2S_FUSED=1 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
# then the same warmup and measured curl calls with the B1c output names,
# kill -INT. Repeat for B1d (B) and B0d (A) to complete A, B, B, A.
```

The launcher expands `EXTRA_ARGS` unquoted on its own command line (line 90), so the value is
split into argv by the shell only once, at assignment time. Writing `EXTRA_ARGS=-b 512 -ub
256` without quotes leaves `512`, `-ub` and `256` outside the assignment and the server never
receives a usable command line. The launcher defaults stay as accepted (`THREADS=12`,
`-c 196608`, `-np 1`, `-fa on`, KV q4_0, `--cpu-moe`, MTP `n_max=2`, EHS=0). Port 8087 keeps
this block separate from the daily 8081 server.

## Raw data

All raw request responses are under `results/archive/EXP-2026-09-15-060/`:

- `B0c-prefill.json`, `B1c-prefill.json`, `B1d-prefill.json`, `B0d-prefill.json`: the four
  counted measured responses;
- `B0c-warmup.json`, `B1c-warmup.json`, `B1d-warmup.json`, `B0d-warmup.json`: the discarded
  warmups;
- `B0a-prefill.json`, `B0a-warmup.json`, `B0b-prefill.json`, `B0b-warmup.json`: the excluded
  superseded first-attempt default-arm samples, together with the matching logs
  `results/exp060-B0a.log`, `results/exp060-B0b.log` and monitors
  `results/exp060-B0a-monitor.csv`, `results/exp060-B0b-monitor.csv`.

## What this closes

- The `-b`/`-ub` batch-shape direction is closed for the 395-token prefill workload. No other
  shape is swept automatically.
- The launcher default stays at `-b 2048 -ub 512`; nothing is enabled by default and no file
  other than this card and the PRE-4 row in
  `experiments/HYPOTHESES-2026-09-15-runtime-speed.md` was changed.
- The hash divergence is recorded as a property of the lever on this runtime: a smaller
  physical prefill chunk is not correctness-preserving here, so any future batch-shape work
  must treat the completion hash as a hard gate, not an auxiliary check.
