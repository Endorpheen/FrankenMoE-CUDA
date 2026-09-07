# EXP-040 extended A/B protocol (for the senior helper in Codex)

Purpose: acceptance coverage of the accepted EXP-038 pinned ring on two NEW fixed
scenarios (short = decode-heavy, long = prefill-heavy, >512 tokens => several ring
batches). The old 395-token block from EXP-038 is NOT repeated; its numbers are
reused from `results/archive/EXP-2026-09-06-038/ab/`.

Preconditions:
- The OpenCode server is stopped by Igor; port 8081 is free.
- Nothing is rebuilt; both binaries are ready and already hashed.
- Control A: /home/end0/AI/FrankenMoE-CUDA/build/exp023-mtp-sidecar/bin/llama-server
  sha256 5471e44b9d394d2e45c7adfb65c46a4ef1deeb32cee58b815eb2e0d02f4b9b78
- Candidate B: /home/end0/AI/FrankenMoE-CUDA/build/exp038-pinned-ring/bin/llama-server
  sha256 fbbdd956df1d99afae8700139a40639daea5b9d8bb40451f803310650d43d707
  (candidate CUDA library libggml-cuda.so.0.22.0 sha256
  b1b9ca799383167d4cbf547f3968d4e2d7fbecd057230b618132678aa02dbbef)

Run command A (same argv as the EXP-038 formal block):

    /home/end0/AI/FrankenMoE-CUDA/build/exp023-mtp-sidecar/bin/llama-server \
      -m /home/end0/AI/FrankenMoE-CUDA/models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf \
      --host 127.0.0.1 --port 8081 -c 196608 -np 1 -fa on -t 12 -ctk q4_0 -ctv q4_0 \
      --reasoning-effort low -ehs 0 --cpu-moe \
      -ot per_layer_token_embd.weight=CPU,token_embd.weight=CPU -ngl 99 \
      -md /home/end0/AI/FrankenMoE-CUDA/models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf \
      --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe \
      -ctkd q4_0 -ctvd q4_0 \
      > <run-dir>/server.log 2>&1 &

Run command B: same argv with the exp038 binary, plus env `GGML_EXPERT_PINNED_RING=1`.

Fixed requests (identical files for every run, sent in this order in every arm):
1. `request-short.json`  - ~50-token prompt, n_predict=128, temperature 0,
   cache_prompt=false, stream=false. Decode-focused.
2. `request-long.json`   - ~1100-token prompt (>512, many scheduler batches),
   n_predict=24, temperature 0, cache_prompt=false, stream=false. Prefill + ring
   endurance-focused.

    curl -s http://127.0.0.1:8081/completion -H "Content-Type: application/json" \
      -d @.../request-short.json -o <run-dir>/response-short.json
    curl -s ... -d @.../request-long.json -o <run-dir>/response-long.json

Block structure: 5 pairs x 2 arms = 10 server runs, 20 model requests.
Pair order (first arm): pair1 A,B; pair2 B,A; pair3 A,B; pair4 B,A; pair5 A,B.

Procedure per run:
1. Start arm (fresh process), wait for /health 200 (~10-12 s load).
2. Snapshot `grep -E "VmRSS|VmSwap|VmPin|VmHWM" /proc/<pid>/status` to
   `<run-dir>/memory-before.txt`; `nvidia-smi --query-gpu=memory.used --format=csv`
   to `<run-dir>/gpu-before.txt`.
3. Send short request, save response; send long request, save response.
4. Same memory/GPU snapshots to `memory-after.txt` / `gpu-after.txt` (after long,
   before stop).
5. Graceful stop: kill -INT <pid>, wait for exit (no pkill, no kill -9).

Run naming: `results/archive/EXP-2026-09-07-040/ab/pairN-{A|B}/` with
`server.log`, `server.pid`, `response-short.json`, `response-long.json`,
memory/gpu before-after files.

Candidate validity gate (per B run): server.log must contain
`expert ring enabled: 2 pinned host slots x 16 MiB` and, after graceful shutdown,
`expert ring staged <N> calls, <M> chunks` with N > 0 and M > 0. Missing line =>
run invalid; stop and report.

Per-request metrics (from response `timings`): actual prompt/generated tokens
(`prompt_breakdown`/`completion_breakdown` where present), `prompt_ms`
(prompt_eval_ms), decode tok/s = completion_tokens/((eval_ms-prompt_eval_ms)/1000),
wall time from `time` around curl, `time_to_first_token_ms` only for streaming runs
(n/a here). Plus per-run: RSS delta, VmSwap (must be 0), VmPin (B only, ~32 MiB),
GPU memory used before/after, ring counters from server.log.

Determinism gates (all must hold before any perf claim):
- `response-short.json` completion content sha256 identical across all 10 runs.
- `response-long.json` completion content sha256 identical across all 10 runs.

Acceptance gates (Igor, fixed before the run):
- All responses correct and deterministic.
- Per-request median prompt latency of B not worse than A by more than 3%.
- Median decode tok/s of B not worse than A by more than 2%.
- VmSwap = 0 in every snapshot; no CUDA error, OOM, hang, leak.
- Post-request RSS delta B-A within 32 MiB pinned + < 1 MiB metadata.
- B logs show non-zero staged calls/chunks on every run (bulk requests stage).
- Single-token decode must not use the ring; proven separately by the probe runs
  in `../session/SESSION-PROTOCOL.md`.

Immediate abort (any run): CUDA error, OOM, swap growth, output divergence between
arms, hang, or prompt degradation > 10% on either request.

Expected duration: ~45-60 s per run (load ~10-12 s, short ~6-8 s, long ~12-20 s,
stop ~3 s). 10 runs + gaps: ~12-14 minutes for this block.

Results: fill `results.json` (same schema as EXP-038 `ab/results.json`) plus a
compact `results.txt` summary in this directory. No commit by the runner.
