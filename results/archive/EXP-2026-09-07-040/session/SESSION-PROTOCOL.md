# EXP-040 session robustness protocol (for the senior helper in Codex)

Runs on candidate B only (the ring is the object under acceptance; control behavior
on these features is the live production baseline). No checkpoints are disabled and
no flags are weakened: server argv is identical to `../ab/PROTOCOL.md` command B
plus env `GGML_EXPERT_PINNED_RING=1` and the log flags stated per run.

Three server runs, in this order:

## S-run: sequential session (verbose logs)

Start server B with `--verbose` added to argv (session is not timed; verbose is for
checkpoint/spec/MTP observability). Requests, in order, all to /completion:

1. S1: `request-short.json` (normal). Save response; sha256 of completion content = H_short.
2. S2: `request-long.json` (normal, cache_prompt=false). sha256 = H_long.
3. S3: `request-short.json` again. Must reproduce H_short exactly (repetition determinism).
4. S4: `request-long-cached.json` (same prompt, cache_prompt=true). Expect prefix-cache
   hit: prompt_eval_ms collapses to a small fraction of S2 and token_count details show
   cached tokens; completion content must equal H_long.
5. S5 (TTFT): `request-long-stream.json` (stream=true, cache warm). Stream to completion
   with `curl -N -w '%{time_starttransfer}\n' -o <run-dir>/sse-long.txt`; record TTFT ms.
6. S6 (cancellation): `request-long-stream.json` again, kill curl mid-stream
   (`timeout 3 curl -N ...`): client disconnect cancels the task. Server log must show
   the cancellation and a prompt restore from a context checkpoint (`create_checkpoint`
   / restore lines with --verbose). Then:
7. S7: `request-short.json` (normal). Must reproduce H_short exactly (post-cancel
   correctness).
8. Check in server.log: context checkpoint creations present; MTP speculative decode
   activity with accepted and rejected drafts (draft-mtp n_max=2; look for
   speculative accept/rollback or decode-decision lines); no CUDA error, no warning
   about ring.
9. Graceful stop: kill -INT, wait for exit. server.log tail must show
   `expert ring staged <N> calls, <M> chunks` (N > 0).
10. Post-stop checks: `pgrep -a llama-server` empty (no orphans); `nvidia-smi` shows
    the GPU memory back at idle; no CUDA error lines anywhere in server.log.

## P0-run: ring counter probe, prefill-only

Start server B (normal log level, no --verbose). Send one `request-long-p0.json`
(same long prompt, n_predict=0). Graceful stop. Record shutdown counters as N_p / M_p.

## P64-run: ring counter probe, prefill + 64 decode tokens

Identical P0-run but with `request-long-p64.json` (n_predict=64). Record counters
N_d / M_d. Gate: N_d == N_p and M_d == M_p exactly. Any increase means decode entered
the bulk ring path => FAIL (single-token decode must never use the ring).

Note: with `--spec-type draft-mtp --spec-draft-n-max 2`, draft evaluation uses small
multi-token batches; they are not bulk-prefill MUL_MAT_ID weight copies through the
host-weights branch (ids ne[1] < 8 gate). If N_d still grows above N_p, stop and
report exact numbers; do not adjust the gate.

## Recording per run

Per request: actual prompt/generated tokens, prompt_eval_ms, decode tok/s, wall time
(time around curl), TTFT only for S5. Per run: memory-before/after
(VmRSS/VmSwap/VmPin/VmHWM), gpu-before/after, server.log, ring counters, exit status
of server (must be clean SIGINT), orphan check output. Store under
`results/archive/EXP-2026-09-07-040/session/{S-run,P0-run,P64-run}/`.

Abort conditions: any CUDA error, OOM, VmSwap > 0, hang > 120 s on one request,
hash mismatch (H_short/H_long divergence), prefix-cache miss in S4, missing
checkpoint lines in S6, counters mismatch in P64-run.

Expected duration: S-run ~4-5 min, each probe ~50 s. Whole session block ~7 min.
