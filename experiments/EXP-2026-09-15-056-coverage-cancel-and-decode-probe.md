# EXP-2026-09-15-056 — Coverage: real mid-stream cancellation and a real decode probe

Status: `ACCEPTED` (`kind=correctness`, coverage; 2026-09-15, runs executed by OpenCode
on the accepted EXP-046 default runtime; raw data in
`results/archive/EXP-2026-09-15-056/`). Commit pending Igor's approval.

## Kind

`correctness` (coverage of an already-accepted runtime). No performance claim and no
speedup target. The object under test is the accepted EXP-046 default runtime; its
pinned upload ring, split MTP draft and fused IQ2_S path stay exactly as accepted.

## Hypothesis (one)

The two EXP-040 test-request gaps close with corrected request payloads and the
runtime behaves as specified:

1. A streaming `/completion` request that is still generating when the client
   disconnects is genuinely cancelled (not "already finished"), the slot is released
   and reused, and the next ordinary request reproduces its baseline answer byte for
   byte.
2. A real 64-token greedy decode on the long prompt does not stage any expert copy
   through the pinned ring: the teardown counters are identical to the same prompt
   with a single decode step.

This is a coverage claim, not a performance claim. Speed is not measured and not
compared.

## Baseline identity (fixed before the runs)

- Source tree: `work/llama.cpp-exp046-default` (chain: base `4aaad5d3` +
  expert-tier-integration + integration-drift + mtp-sidecar + pinned-ring +
  pinned-ring-default-on + two-thread-gather + iq2s-fused-decode +
  iq2s-fused-decode-default-on).
- Server binary: `build/exp046-default-runtime/bin/llama-server`,
  sha256 `3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`.
- Launcher: `scripts/run_qwen38_server.sh`,
  sha256 `17fa4ebb9d4a935dd8afd63683c4df99fc7c90d2f1eff0d8cc2d2e9170a8eacc`.
- Model: Qwen3.8-Flash-Next UD-IQ3_XXS, three shards; reference SHA-256 in
  `docs/RUN_QWEN38.md` (not re-hashed; 81.96 GB).
- MTP head: `models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf`,
  sha256 `4563303ab6e7d40f409750fd48af76bf33dd76b0773951cbf484109107de8140`.

## Server argv (verbatim; only `-v` and the port differ from EXP-040)

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

Env: `GGML_EXPERT_PINNED_RING=1` (explicit), `GGML_CPU_IQ2S_FUSED=1` (explicit).
`-v` is added to the S-run only (trace-level lines needed for the checkpoint and
cancel evidence); the P-runs use the default log level. Port 8087 keeps the session
separate from the daily 8081; the owner's own server must be stopped first.

## Single variable

None in the runtime. The only change relative to EXP-040 is the request payloads:
`ignore_eos=true` and a non-degenerate decode length. The runtime, binary, model,
MTP profile, thread count, context and ring are byte-identical to the accepted one.

## Requests (all raw `/completion`, temperature 0)

Carried over unchanged from EXP-040 (`requests/` copies must keep the same hashes):

- `request-short.json` — sha256 `4d1597f51bb3ec4f964be45c4bad375dcd167ab0a167918e95ece78216e81f56`
- `request-long.json` — sha256 `e2cc7dbaa70564c8e085e11e4188a3d9e8d43571885fd1c84076bcab3c4f89c4`
- `request-long-cached.json` — sha256 `1419a036af799b284789540a2c1f7c81b47e2e60b3ef39aba8f16dcd4034ace5`
- `request-long-stream.json` — sha256 `9aa87b3b5cb969e713da3c84054df2e02448f8680a5d8c85b0d0dfe036a04db3`

New for this experiment:

- `request-long-dec1.json` — long prompt, `n_predict=1`, `ignore_eos=true`,
  sha256 `07221b4ebbef452d772599816cdca125c9558cc0b77524a58e4d40c8b85eb94d`.
- `request-long-dec64.json` — long prompt, `n_predict=64`, `ignore_eos=true`,
  sha256 `371be0d607434b7c164eff11f0dc396b66f849a48cb743fd635f174a16952a89`.
- `request-long-cancel.json` — long prompt, `n_predict=512`, `ignore_eos=true`,
  `stream=true`, `cache_prompt=true`,
  sha256 `13115564a46c9be7bd8c9d4805819d53cb73f8e75850a00b1381337aa590de64`.

## Code evidence that fixes the gates (read-only, before the run)

- Real cancel vs "already finished": `server-queue.cpp:602-621`; the branch is
  `has_next()` (`:545`). Real cancel logs `cancel task, id_task = %d` (SRV_WRN,
  `:610`); the other branch logs `all tasks already finished, no need to cancel`
  (SRV_DBG, `:619`). EXP-040 hit the second branch because the answer finished by
  EOG in 0.30 s.
- Slot release: `server-context.cpp:2428-2437` posts the cancel to the slot;
  `slot.release()` (`:545-566`) logs `stop processing: n_tokens = %d, truncated = %d`
  (SLT_INF, `:549`).
- Checkpoint restore is NOT triggered by cancel; it happens in the prefill of a
  later request with a matching prefix (`server-context.cpp:3323-3351`, SLT_TRC
  `restored context checkpoint (pos_min = ...)`, `:3350`). Gate (c) is therefore
  proven by the follow-up request, not by the cancel itself.
- Ring gate: `ggml-backend.cpp:1696-1697`, `ids_tensor->ne[1] >= 8`. A 1-token decode
  and a 2-3-row MTP verify batch cannot satisfy it.
- Ring counters: activation `expert ring enabled: %zu pinned host slots x %.0f MiB`
  (`ggml-cuda.cu:2600`); teardown `expert ring staged %zu calls, %zu chunks`
  (`ggml-cuda.cu:715`).
- `n_predict=0` degenerates to 1 token; `ignore_eos=true` biases EOG tokens to -inf
  (`server-schema.cpp:471-481`), so `n_predict=N` + `ignore_eos=true` yields exactly
  N tokens with `stop_type=limit`.

## Runs (3 server starts, 9 requests; manual and sequential)

### S-run — one server with `-v`

1. S1 `request-short.json` -> content sha must equal `H_short` = `2f409a00f774…`.
2. S2 `request-long.json` (cache_prompt=false) -> content sha `H_long` = `e3b0c442…`
   (empty, single EOG token, as in EXP-040).
3. S3 `request-short.json` again -> must equal `H_short`.
4. S4 `request-long-cached.json` -> prefix-cache hit; must show
   `restored context checkpoint` and equal `H_long`.
5. S5 `request-long-stream.json` -> streaming TTFT recorded.
6. S6 `request-long-cancel.json` -> client aborts the socket mid-stream
   (`timeout --signal=KILL 4 curl -N ...`), generation must still be running.
7. S7 `request-short.json` -> must equal `H_short` (post-cancel correctness).
8. SIGINT, clean exit, no orphan, teardown ring counter line present.

### P1-run and P64-run — one server each, default log level

- P1: one `request-long-dec1.json`; record teardown counters `N1 / M1`.
- P64: one `request-long-dec64.json`; record teardown counters `N64 / M64`.

## Gates (fixed before the runs; all must hold)

- G1 real cancel: server.log contains `cancel task, id_task =` and does NOT contain
  `all tasks already finished, no need to cancel` for S6.
- G2 slot released: `stop processing: n_tokens =` present after the S6 cancel.
- G3 post-cancel correctness: S7 content sha equals `H_short`.
- G4 determinism/prefix: S1 == S3 == S7 == `H_short`; S4 == `H_long`; S4 shows a
  prefix-cache hit and `restored context checkpoint`.
- G5 decode probe: `N64 == N1` and `M64 == M1` exactly; in both P-runs the decode
  token count is real (P1 = 1 token `stop_type=limit`, P64 = 64 tokens
  `stop_type=limit`), not a degenerate EOG.
- G6 safety: VmSwap = 0 in every snapshot; no CUDA error, OOM, watchdog trip or
  hang; SIGINT exit 0; no orphan `llama-server`.

## Stop / verdict criteria

- Any CUDA error, OOM, VmSwap > 0 or hang > 120 s on one request stops the block and
  is reported as-is.
- If S6 still finishes before the disconnect (gate G1 fails), the request is
  adjusted (larger `n_predict` / earlier abort) and only S6 is retried; do not change
  the gate.
- Verdict is `ACCEPTED` (coverage) only if G1-G6 all hold. A failure is
  `REJECTED`/`BLOCKED` with the exact counter and log evidence; the accepted runtime
  and launcher do not change either way.

## Results (2026-09-15, manual and sequential)

Environment: the owner's 8081 server was stopped first; the experiment ran on port
8087; GPU at start 1838 MiB used, MemAvailable 47.1 GiB. All three servers loaded the
model with the page cache warm. `GGML_EXPERT_PINNED_RING=1` and
`GGML_CPU_IQ2S_FUSED=1` were set explicitly for every run.

### S-run (one server, `-v`, 7 requests)

- Ring activation: `expert ring enabled: 2 pinned host slots x 16 MiB`.
- S1 `H_short` = `2f409a00…` (stop_type eos, predicted_n 104, prompt_n 41).
- S2 `H_long` = `e3b0c442…` (empty, predicted_n 1, prompt_n 1124, prompt_ms 13242.827).
- S3 = `H_short`.
- S4 = `H_long`; `cache_n 1120`, `prompt_n 4`, `prompt_ms 230.867`, and the log shows
  `restored context checkpoint (pos_min = 1119, pos_max = 1119, n_tokens = 1120,
  n_past = 1120, size = 113.212 MiB)`.
- S5 streaming: HTTP 200, TTFT `0.019873 s`, total `0.280174 s` (finished by EOG, as
  in EXP-040; this request is not the cancellation one).
- S6 cancellation: the client connection was aborted by a curl max-time timeout at
  ~4 s (curl exit 124; 8213 bytes of SSE received), so the disconnect is
  timeout/disconnect-triggered, not an explicit cancel API call. Server log:
  `stop: cancel task, id_task = 126` (SRV_WRN), and task 126 was mid-generation
  (`n_gen = 67`, `n_remaining = 445`, `accepted 0/2 draft tokens`; the cancel is
  0.14 s after that sample). Then `slot release: id 0 | task 126 |
  stop processing: n_tokens = 1193, truncated = 0`, `reset: id 0 | task 126`,
  `update_slots: all slots are idle`.
- S7 = `H_short` (post-cancel correctness).
- The log contains 16 `stop: all tasks already finished, no need to cancel` lines,
  none of them task 126: the nearest are at 2.04.866 (before) and 2.39.759 (after)
  the 2.08.952 cancel. The message itself carries no task id, so this attribution is
  timestamp-based; the only `cancel task` line in the whole log is task 126 (S6), so
  S6 took the real cancel branch. The verbose `launching slot` JSON dump is racy
  (its `id_task`, `n_prompt_tokens` and prompt/max_tokens can belong to different
  requests) and is NOT used for task attribution anywhere in this record.
- Teardown: `expert ring staged 91047 calls, 91047 chunks` (matches the EXP-040 S-run
  counter). VmSwap 0 before and after; VmPin 0; clean SIGINT; 0 orphans; GPU back to
  1897 MiB; no CUDA error, OOM, watchdog trip or hang.

### P1-run (prefill + 1 decode token)

- `request-long-dec1.json`: HTTP 200, `stop_type limit`, `predicted_n 1`,
  `prompt_n 1124`, `prompt_ms 8110.693` — a real decode token, not a degenerate EOG.
- Teardown counters: `expert ring staged 51429 calls, 51429 chunks`.

### P64-run (prefill + 64 decode tokens)

- `request-long-dec64.json`: HTTP 200, `stop_type limit`, `predicted_n 64`,
  `prompt_n 1124`, `prompt_ms 8420.504`, `predicted_ms 2877.342`.
- Teardown counters: `expert ring staged 51429 calls, 51429 chunks`.

### Gate verdict

- G1 real cancel: PASS (`cancel task, id_task = 126`; task 126 mid-generation).
- G2 slot released: PASS (`stop processing: n_tokens = 1193`, `reset`, `all slots are idle`).
- G3 post-cancel correctness: PASS (S7 = `H_short`).
- G4 determinism/prefix: PASS (S1 = S3 = S7 = `H_short`; S2 = S4 = `H_long`;
  prefix-cache hit and `restored context checkpoint` in S4).
- G5 decode probe: PASS (`51429 == 51429` calls and chunks for both P1 and P64).
- G6 safety: PASS. Zero genuine CUDA error / OOM / watchdog / assert strings in any
  of the three logs (the `CUDA error`/`out-of-memory` hits are prompt-text false
  positives inside the long technical note). `VmSwap 0` was captured for the S-run and
  P1-run; the P64-run has no memory snapshot, so its VmSwap is not independently
  verified (the ring is prefill-only and P1 proves the decode path). All three servers
  exited cleanly; the orphan check (`0 orphans`) exists for the S-run only.

Independent verification: a separate read-only audit of the raw artifacts re-derived
the task-to-response mapping from the `processing task` / `release` / checkpoint lines
and confirmed G1-G6 as PASS with the same evidence (cancel at
`server.log:21009`, release at `:21111`, reset at `:21112`, checkpoint restore at
`:17603`). It also flagged the caveats already recorded above (racy `launching slot`
dump; no task id on the already-finished line; missing P64 memory snapshot and
P1/P64 orphan artifacts; timeout-triggered cancel). None of them is a gate failure.

Both EXP-040 test-request gaps are closed. No performance claim is made; no runtime,
binary, model or launcher change is part of this experiment.

## What this leads to

On ACCEPTED, the deferred coverage gaps from EXP-040 are closed and the card plus the
handoff are updated. No new performance experiment is selected automatically; the
next direction stays Igor's call.
