# EXP-053: independent tester handoff

Date: 2026-09-09. Source prepared; final compilation status is in the experiment card. **The implementation agent has not run tests, a server, a model, a sanitizer or a profiler.**

Igor authorized an isolated implementation/build, not execution of these commands in this session. Before model/server runs, give Igor the purpose, duration and exact run count, then obtain an explicit yes. All measurements are manual, sequential, with no background build or competing workload. Do not kill Igor's server. Use port 8081, never the occupied 8080.

## Candidate and control

- Source: `work/llama.cpp-exp053-radix-topk`.
- Build: `build/exp053-radix-topk`.
- Runtime delta: only `ggml/src/ggml-cuda/top-k.cu` relative to the accepted EXP-046 snapshot.
- Test delta: extra cases in existing `tests/test-backend-ops.cpp`.
- Same binary: A = `GGML_CUDA_TOPK_ARGSORT=1`; B = `GGML_CUDA_TOPK_ARGSORT=0`, also the candidate build's unset default.
- `GGML_CUDA_TOPK_DIAG=1` reports `[EXP053-TOPK] path=radix|argsort ncols=... nrows=... k=...`. Off by default; keep it off for timing.
- Flags are cached on first use. Restart the process between arms. No EXP-052 variants, thread changes, MTP-depth changes or quantization changes.
- Accepted EXP-046 binaries and launcher stay unchanged; do not promote this build.

Eligible calls are contiguous F32 CUDA TOP_K, ncols>=8192, valid k and bounded dimensions, CUB enabled, no native DeviceTopK. Other calls use the original implementation. Both target and draft QSA and eligible prefill TOP_K use this branch: it is not a phase-specific guard. The performance objective is decode only.

## Rebuild if necessary: compilation only

The source snapshot includes the accepted uncommitted patch chain; its upstream Git HEAD alone is not sufficient provenance. `cuda-radix-topk.patch` and `topk-test-cases.patch` contain only EXP-053 changes against that snapshot.

```sh
cmake -S work/llama.cpp-exp053-radix-topk -B build/exp053-radix-topk \
  -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89 \
  -DGGML_NATIVE=ON -DGGML_CUDA_GRAPHS=ON -DLLAMA_BUILD_TESTS=ON \
  -DLLAMA_BUILD_MTMD=OFF -DLLAMA_BUILD_UI=OFF
cmake --build build/exp053-radix-topk --parallel 4 --target llama-server test-backend-ops
```

Wait for successful completion and no remaining compilers before testing. Do not use baseline objects or change common-structure layouts. Verify the supplied `SHA256SUMS` before the first arm; never rebuild between arms.

## Model-free correctness

Run these manually in separate processes and preserve complete output and exit status in unique files. They are not performance measurements.

```sh
GGML_CUDA_TOPK_ARGSORT=1 GGML_CUDA_TOPK_DIAG=1 \
  build/exp053-radix-topk/bin/test-backend-ops test -b CUDA0 -o TOP_K
```

```sh
GGML_CUDA_TOPK_ARGSORT=0 GGML_CUDA_TOPK_DIAG=1 \
  build/exp053-radix-topk/bin/test-backend-ops test -b CUDA0 -o TOP_K
```

Both must pass against the CPU reference. Confirm wide-row candidate `path=radix` and control `path=argsort`. Preserve total case counts and failures, not just a final PASS. The diagnostic describes host dispatch selection, not a count of CUDA graph replays.

Added coverage: ncols=8191/8192/8193/131072/196608; rows=1/2/3; k=1/2051/ncols; signed values, masked ties, signed zeros and all negative infinity. Two 128-row cases cover prompt-like scratch sizes. Existing higher-dimensional cases remain. `-p 'pattern='` selects the additional cases for a narrower diagnostic.

For ties the comparator checks the selected value multiset, unique indices and bounds. Ordering and tied index identity are not required by TOP_K. NaN equivalence is not claimed. Model parity still needs the following gates.

If within the tester's approved scope, run memory checking separately:

```sh
GGML_CUDA_TOPK_ARGSORT=0 GGML_CUDA_TOPK_DIAG=1 \
  compute-sanitizer --tool memcheck --error-exitcode 1 \
  build/exp053-radix-topk/bin/test-backend-ops test -b CUDA0 -o TOP_K -p 'pattern='
```

A sanitizer error blocks model A/B. Sanitizer times are not throughput. Repeated model graph execution and scratch reuse must be validated during diagnostic decode; a single operator check is not a substitute.

## Freeze requests before performance runs

1. Keep `results/archive/EXP-2026-09-09-046/request-decode256.json` as the short-context regression control. Its old content hash is not a reference for a new long prompt.
2. Prepare one meaningful long document/task targeting approximately 119k actual prompt tokens, with room below ctx=196608 for 256 output tokens. Do not silently replace it with repetitive padding. The tester and Igor must select this input; no fabricated 119k-token request is included here.
3. Confirm actual length with the model tokenizer in an approved diagnostic session. Character counts or context capacity are insufficient. Confirm wide QSA shapes, freeze request JSON and SHA256 before the series. A 32k workload is a separately labelled secondary case if Igor chooses it.
4. Use identical bytes, n_predict=256, temperature=0, fixed seed, cache_prompt=false, stream=false and identical template/sampling settings. Save full requests and responses. Do not enable ignore-eos to hide early EOG.
5. A short output is retained but invalid for sustained-decode performance. If the prompt must be revised, freeze a replacement and rerun both arms; never compare different prompts.

## Server command: only after approval

For control A, run manually from the project root:

```sh
export GGML_CUDA_TOPK_ARGSORT=1
export GGML_CUDA_TOPK_DIAG=0
export GGML_CPU_IQ2S_FUSED=1
export GGML_EXPERT_PINNED_RING=1
export GGML_EXPERT_RING_GATHER2T=1

build/exp053-radix-topk/bin/llama-server \
  -m models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf \
  -ngl 99 --cpu-moe --reasoning-effort low -ehs 0 --ehs-reserve-mb 2048 \
  -ot 'per_layer_token_embd.weight=CPU,token_embd.weight=CPU' \
  -c 196608 -np 1 -fa on --jinja \
  -ctk q4_0 -ctv q4_0 -ctkd q4_0 -ctvd q4_0 -t 12 \
  --host 127.0.0.1 --port 8081 \
  -md models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf \
  --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe
```

After cleanly stopping A, change only `GGML_CUDA_TOPK_ARGSORT=0` for B. Do not alter inherited OMP/GGML settings between arms; record them. Use `GGML_CUDA_TOPK_DIAG=1` only in a separately labelled diagnostic session, never a measured one.

Once the agreed request is actually saved at the following path, submit it manually:

```sh
curl --fail-with-body --silent --show-error http://127.0.0.1:8081/completion \
  -H 'Content-Type: application/json' \
  --data-binary @results/archive/EXP-2026-09-09-053/request-long-decode256.json
```

That request file is not supplied yet. This command prints the response; preserve it under a unique arm directory, with server logs and telemetry. Do not overwrite raw evidence. Signal the exact server PID with SIGINT and wait for exit; do not signal unrelated processes or a profiler wrapper.

## Staged run plan: proposal, not automatic authorization

- Complete correctness, then request one diagnostic session to validate the long request, shapes, output, repeated decode and clean shutdown. Use it to estimate the actual long-prefill duration; the old 1-2 minute short-prompt estimate does not apply to 119k.
- Freeze identical sufficient warmup in both arms. Inspect loading/warmup timing. Retain any cold anomaly and report both all-pair and predeclared clean-pair summaries. Exclude a whole invalid pair, never only its slower arm.
- Request one preliminary A/B pair: two fresh server processes, identical warmup, one measured long decode per process. Stop and report before more runs.
- If promising, ask Igor for a balanced confirmation series with equal A->B/B->A orders. Four pairs is a possible starting proposal, not a fixed authorization or a guarantee of statistical precision. Do not repeat eight pairs by habit.
- Keep prefill time separate; score decode only. The same binary, document, actual token count and output length are required.

## Reporting and stop conditions

Primary: single-response predicted_per_second and underlying predicted_n/predicted_ms, 256 actual generated tokens, paired changes, median/range, order and exclusions. A/B directly measures whether the implementation improves decode on the declared workload.

Correctness: full text and token hashes where available, stop reason, target/draft acceptance and rollback behavior, long-document answer checks, deterministic output differences, no CUDA errors, clean SIGINT and no orphans. If tied index choices change the answer, locate and explain the divergence; do not dismiss it as harmless or score a changed workload as a clean speedup.

Resources: RSS, process VmSwap, system swap activity, peak VRAM, CPU load, GPU clocks/temperature, prompt/cache counts and competing processes. Nonzero process VmSwap invalidates a clean timing arm; preserve the evidence. Diagnostic/profiler/sanitizer times never enter performance aggregates.

ACCEPT recommendation requires >=2% reproducible sustained long-context decode gain, correctness and acceptable memory/regression behavior. Short-context results are separate and cannot replace the primary long-context test. A null/below-threshold result is retained, not promoted.

After each executed test, update the card, EXPERIMENTS.md and ROADMAP.md immediately in English. Before a commit show Igor outcome, memory, correctness and exact files, then wait for approval. Baseline/launcher promotion is a separate decision. No commit or promotion is authorized by this handoff.
