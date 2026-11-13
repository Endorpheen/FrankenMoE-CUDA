# EXP-2026-10-30-099: sm89 wide-GQA Flash Attention dispatch — REJECTED

The unchanged DeepSeek patch was built and screened directly on real Apex against preserved EXP090. Only fattn.cu is recompiled; the accepted EXP089 CUDA master object (including resident mmap expert registration) and all other existing CUDA objects are retained. Only libggml-cuda differs from the nine-file EXP090 bundle. Compile/link commands and hashes are included. No source review or cosmetic rewrite was performed.

## Hypothesis and fixed gate

Use the existing <256,256,16,4> instance instead of <256,256,8,8> only for the prepared sm89 quantized wide-GQA prefill predicate, retaining context, KV precision and other dispatches. Different reduction order was expected to be a risk, not assumed equivalent. Historical FA occupancy was6.11% of the older cold TTFT, an upper bound rather than an expected>=5% gain. The original numerical gate remains unchanged: selected/ordered top5 IDs equal, probability tolerance0.001. Do not relax it to the synthetic test's0.01 after seeing a result.

## Real-model result

One candidate startup/one cold32768-token native Apex request, up to128 outputs, seed123, normal sampling and EOS; accepted EXP090 profile unchanged (context262144, all40 CPU expert layers,4096/4096,8/16, existing masks/poll0, mmap, FA, Q4_0 KV, one slot, no speculation/EHS). Reuse fresh EXP090 control from EXP096 rather than duplicate its request.

First ordered-top5 divergence is position3; first selected-ID divergence position64 (zero-based). Maximum common-top5 probability delta on the shared-history positions0..64 is0.1910838462. The legacy comparator's position3 message says token-id mismatch while the selected IDs there are equal: it also checks top5; divergence-details.json records the correct distinction. Probabilities after selected histories diverge are not shared-input numerical evidence. Semantic task quality was not evaluated and no quality loss is claimed.

Candidate process swap0, major faults0; system swap-out1504 pages fails the resource gate. Observed post-request VRAM4428MiB. Diagnostic TTFT14.822050s, prefill2212.1795tok/s and single-response decode29.9267tok/s are NOT accepted performance evidence after the numerical/resource failure. No full-context speed, aggregate throughput or kernel-name profiling claim.

Stop after the first request: repeat and formal pairs skipped. No extra synthetic/sanitizer/capacity/Flint/disable/serialization tests after this failed qualification, and none are implicitly passed. The rejected patch is retained only as evidence; working defaults stay EXP090. Retrying would require a separately justified numerical approach/gate, not ignoring this result.

Reference project4cd19cb5db04d77ab3978114431bf51faff5b42f, isolated build /home/end0/AI/LlamaServer/exp099-sm89-wide-gqa. EXP090 restored on8081 with health/all nine mapped hashes/120 registered expert ranges verified. Raw SSE/traces/resources/logs are local and hashed; weights, prompt contents and process environment are excluded. Goal of new>=5% above EXP090 remains active.
