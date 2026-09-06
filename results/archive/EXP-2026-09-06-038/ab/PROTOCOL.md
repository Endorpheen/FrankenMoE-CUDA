# EXP-038 exploratory A/B protocol (for the senior helper in Codex)

Preconditions:
- The OpenCode server is stopped by Igor; port 8081 is free.
- Nothing is rebuilt; both binaries are ready.
- Control A: /home/end0/AI/FrankenMoE-CUDA/build/exp023-mtp-sidecar/bin/llama-server
  sha256 5471e44b9d394d2e45c7adfb65c46a4ef1deeb32cee58b815eb2e0d02f4b9b78
- Candidate B: /home/end0/AI/FrankenMoE-CUDA/build/exp038-pinned-ring/bin/llama-server
  sha256 fbbdd956df1d99afae8700139a40639daea5b9d8bb40451f803310650d43d707
  (the same tree plus EXP-038, +149 lines, 4 files)

Run command A (identical argv to the live server):

    /home/end0/AI/FrankenMoE-CUDA/build/exp023-mtp-sidecar/bin/llama-server \
      -m /home/end0/AI/FrankenMoE-CUDA/models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf \
      --host 127.0.0.1 --port 8081 -c 196608 -np 1 -fa on -t 12 -ctk q4_0 -ctv q4_0 \
      --reasoning-effort low -ehs 0 --cpu-moe \
      -ot per_layer_token_embd.weight=CPU,token_embd.weight=CPU -ngl 99 \
      -md /home/end0/AI/FrankenMoE-CUDA/models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf \
      --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe \
      -ctkd q4_0 -ctvd q4_0 \
      > <run-dir>/server.log 2>&1 &

Run command B: same argv with the exp038 binary, plus the env:

    GGML_EXPERT_PINNED_RING=1

Fixed request (identical file for every run):
`results/archive/EXP-2026-09-06-038/ab/request.json`
(2169-char EXP-028/030/031 prompt, n_predict=48, cache_prompt=false, stream=false)

    curl -s http://127.0.0.1:8081/completion \
      -H "Content-Type: application/json" \
      -d @/home/end0/AI/FrankenMoE-CUDA/results/archive/EXP-2026-09-06-038/ab/request.json \
      -o <run-dir>/response.json

Order per pair (5 pairs, alternating first arm):
1. Start arm (log to file), wait for /health 200 (~10-12 s load).
2. Send one fixed request, save response.
3. Graceful stop: kill -INT <pid>, wait for exit (no pkill, no kill -9).
4. Start the other arm, same run.

Run naming and paths (one dir per run):

    results/archive/EXP-2026-09-06-038/ab/pairN-{A|B}/server.log
    results/archive/EXP-2026-09-06-038/ab/pairN-{A|B}/response.json
    order: pair1 A,B; pair2 B,A; pair3 A,B; pair4 B,A; pair5 A,B

Candidate validity gate (per B run): server.log must contain
`expert ring enabled: 2 pinned host slots x 16 MiB` and, after graceful shutdown,
`expert ring staged <N> calls, <M> chunks` with `N > 0` and `M > 0`.
If either line is absent, the run is invalid; stop and report.

Discarded preflight data:
- `pair1-A-invalid-http400`: wrong `/v1/chat/completions` endpoint, HTTP 400.
- `pair1-A-invalid-cold-cache`: first cold-cache request; not comparable.
- `pair1-A-invalid-cold-cache-2`: second cold-cache control used only to warm the formal block.
- `pair1-B-invalid-info-log`: candidate followed the warm run, while the activation message was
  emitted below the server's visible log level. It is not A/B evidence.
- These directories are retained for provenance and never enter the formal statistics.

Immediate abort (any run): CUDA error, OOM, new swap growth in server.log/monitor, output corruption
(different completion text between arms beyond deterministic expectation), or prefill degradation >10%.

Metrics per run (from response timings):
- prefill_ms = prompt_eval_ms
- decode_tps = completion_tokens / (eval_ms/1000) minus prompt time if needed (use timings as in prior EXPs)

Expected duration: ~25 s per run (load ~10-12 s, request ~7-9 s, stop ~2-3 s),
10 runs total, wall clock ~6-8 minutes including gaps.

Decision rules (from Igor, fixed before the run):
- ACCEPT only if median prefill gain >= 3%, win in >= 4 of 5 pairs, decode regression no worse than 2%.
- A working ring (log present, fewer stalls) without measured speedup is NOT success.
- No commit until the final report and Igor's separate approval.
