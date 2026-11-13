# EXP-2026-10-30-100: host state-transfer batching — REJECTED

The independent unchanged DeepSeek patch was built and screened directly on real Apex. It batches host state tensor transfers while retaining synchronous public state calls and the device-state path. Only llama-context.cpp is recompiled, using its prepared internal-header changes; linking retains EXP090's model object and all other existing objects. Only libllama differs from the nine-file EXP090 runtime. No EXP096 checkpoint pinning is included. Commands and hashes are provided; no candidate review or cosmetic rewrite preceded the requested real-model-first screen.

## Hypothesis and qualification

Reduce per-tensor synchronization on checkpoint save/restore without changing state format, flags, tensor order or public API completion. Historical restore7.3–7.9ms was about4.8% of an older163ms repeat TTFT even if entirely removed: an upper bound, not a promised gain. Primary repeat client TTFT>=5% improvement, unchanged numerical gate and clean resources. Build separately, screen real Apex cold32768 then identical repeat, up to128 outputs, seed123, unchanged sampling/EOS/top5 and accepted profile (context262144, CPU experts,4096/4096,8/16, masks/poll0, mmap, FA/Q4_0 KV, one slot,32 checkpoints, no speculation/EHS). Reuse fresh EXP090 control from EXP096 rather than launch duplicate control requests.

One candidate startup/two requests. Repeat client TTFT186.377 ->195.786ms (5.0484% longer), below the required gain. Cold candidate TTFT14.287778s versus old control14.369707s; that control cold has152 swap-out pages and is not clean speed evidence. Candidate cold/repeat single-response decode29.9720/28.8326tok/s; these are qualification observations, not five-pair statistics or causal speed/regression claims. Observed post-request VRAM4428MiB; repeat peak RSS22210240KiB.

All256 selected IDs per arm/ordered top5/probabilities match exactly, delta0, cache counts0/32764. Both candidate windows have process swap0, system swap-out0 and major faults0. System swap-in40/1 pages is recorded. Actual transfer/sync count reduction was not instrumented, so no mechanism success is claimed from unchanged outputs.

Stop qualification without formal same-binary disable pairs or additional state-file roundtrip/capacity/Flint/exception tests; these are unrun, not implicitly passed. No rejected code is installed as a new default. The result does not establish that state-transfer batching is universally slower; a changed transfer/restore workload or mechanism would be required to revisit it. Old working builds are preserved.

Reference projectd3c15de465bf2b310a00363de9982ae8b72f3b71. Isolated source/build /home/end0/AI/LlamaServer/exp100-state-transfer-batch. EXP090 restored on8081 with health/nine mapped hashes/120 registered expert ranges verified. Local raw SSE/traces/resources/logs are hashed, excluding prompts, weights and process environment. The new>=5% goal remains unmet.
