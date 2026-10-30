# EXP-2026-10-30-096: pinned checkpoint storage — REJECTED

The prepared DeepSeek candidate was built unchanged and screened directly on real Apex inference against EXP090. It replaces checkpoint host storage with a backend host allocation and retains vector fallback, serialization and cache APIs. No source review or cosmetic rewrite was performed. Only the three changed common/server translation units were compiled; the server-context archive was copied and its two changed members replaced. Only libllama-common and libllama-server-impl differ from EXP090; the executable and seven other runtime files are unchanged. Exact commands and all nine hashes are included.

## Real-model qualification

Apex-35B-V1.0 Q4_K_M, context262144, all40 CPU expert layers, batch/ubatch4096, threads8/16, existing masks/strict placement/poll0, FA, Q4_0 KV, mmap, one slot,32 checkpoints, cache-ram0, no speculation/EHS. One candidate process followed by one fresh control process, each receiving the identical32768-token cold request then repeat, up to128 outputs, seed123, normal sampling and EOS, top5 records. No synthetic or model-free tests preceded this requested real-model screen.

| Diagnostic metric | Fresh EXP090 | EXP096 |
|---|---:|---:|
| Repeat client TTFT, ms (primary) |186.377|185.485|
| Repeat single-response decode, tok/s |28.50864|26.78994|
| Cold client TTFT, s |14.369707|14.920128|
| Cold single-response decode, tok/s |29.41098|26.79199|
| Observed post-request process VRAM, MiB |4428|4428|
| Repeat sampled peak RSS, KiB |22209784|22209856|

Repeat TTFT reduction is only0.4786%, below the>=5% goal; observed repeat decode is6.0287% lower. These are a single fresh qualification comparison, not five-pair statistics or proof of a causal decode regression. The earlier saved control is retained in initial-qualification-result.json but is not selected to claim performance.

All256 selected IDs per arm, ordered top5 and probabilities match exactly (delta0), cache counts0/32764. Candidate cold/repeat and fresh-control repeat have process swap0/system swap-out0/major faults0. Fresh-control cold has152 system swap-out pages while process swap and major faults remain0: its cold timing is not a clean performance sample. System swap-in was nonzero and is recorded. VmLck/VmPin both0 do not by themselves prove whether CUDA host allocations are pinned; actual checkpoint pinning was not separately established in this early-rejected screen. No memory or mechanism acceptance is claimed.

Stop after two test starts/four requests. No formal performance series, slot serialization, capacity/Flint regression or model-free ownership tests follow a failed qualification; those remain required for any future promotion, not implicitly passed. No runtime patch/default is promoted. The rejected candidate.patch is retained as evidence only. Retry requires a changed transfer/restore mechanism or workload, not a repeat of the same pinning screen.

## Restoration and provenance

Reference runtime is EXP090; project commit before this screen f078a8f19430b9c60234d42a9b167c047bb02bda. Source base is the preserved prefix tree overlaid by EXP090, then the unchanged DeepSeek candidate. Build root /home/end0/AI/LlamaServer/exp096-checkpoint-pinned; original link working directory /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda. Two compile flags were initially placed before the executable; that orchestration error was corrected before the successful compile, without changing candidate code. Compile/link logs have no diagnostics.

The working EXP090 service was restored on8081 without an extra generation request. Health, all nine mapped runtime hashes and120 registered expert ranges are verified in restored-runtime.json. Local SSE/traces/resource samples and logs are hashed in local-evidence-manifest.json; prompts, model weights and process environment are excluded. The new>=5% goal remains active.
