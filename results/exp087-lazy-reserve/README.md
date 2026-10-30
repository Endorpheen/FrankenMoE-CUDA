# EXP-087: lazy prompt compute reservation with bounded host growth

Status: ACCEPTED. New Flint/Apex baseline; all pre-stated gates passed.

The previous runtime physically reserved a worst-case prompt graph for the entire configured context during initialization. A 32K request therefore retained compute buffers planned for 262K. This change keeps graph planning and the configured context, reserves the small token-generation graph physically, and lets the existing scheduler allocate prompt buffers for actual graph shapes.

Exact-size growth alone incurred repeated pinned-host allocation costs. Eligible contexts now measure the original worst-case requirements on a fresh empty scheduler and use bounded physical headroom for host compute buffers. A single-chunk host buffer can allocate up to four times its required size, capped by the original plan and backend maximum. A failed enlarged allocation retries the exact required size. Required allocations are never clamped. GPU buffer allocation remains exact on the tested CUDA path. Multiple chunks and zero limits retain exact allocation; aliased allocators use the smallest limit, including zero.

The context policy is default on for ordinary causal Flash Attention inference with one sequence, one GPU backend followed by CPU, no training or pipeline parallelism, and the default context type. `GGML_LAZY_PP_RESERVE=0` restores eager reservation. No model-name checks were added. Metadata/no-alloc size prediction remains eager and unchanged, and the explicit graph-reserve API still allocates the requested full graph physically.

## Five matched pairs

Apex Q4_K_M, configured context262144, batch/ubatch4096, CPU MoE, Q4_0 KV, one slot, threads8/16, existing masks/strict placement, mmap, Flash Attention, speculation off and EHS off. Each arm starts a fresh process and performs one identical32768-token request with seed123, up to128 output tokens, EOS respected and top5 probability records. Order is A-B, B-A, A-B, B-A, A-B.

| Metric | Original EXP-083 | Candidate |
|---|---:|---:|
| Sampled model-process request-window VRAM peak, MiB | 6404 in every run | 4428 in every run |
| Median post-request RSS, KiB | 23782432 | 22208912 |
| Median prefill elapsed time, ms | 21975.031 | 22083.785 |
| Median single-response decode, tok/s | 29.3030 | 30.7468 |

Median paired changes: VRAM-30.8557%, RSS-6.6163%, prefill elapsed time+1.1647%, decode throughput+3.4048%. Paired percentage medians are calculated before aggregation and need not equal a percentage calculated from the two arm medians. These are single-response results, not aggregate throughput. The VRAM monitor uses a100ms NVML-backed sampling cadence; this is a sampled request-window peak, not a whole-process-lifetime startup peak.

All640 selected tokens per arm, ordered top5 IDs and probabilities match exactly; maximum probability delta0. Every request has process swap0, no request-window system swap-out growth and no major faults. Qualification and profiled runs are excluded from the five pairs.

Savings depend on actual graph sizes. Larger requests grow compute buffers; the configured context is not reduced. Buffers can retain a larger allocation after such a request. The32K result is not a promise of30% less memory at every context length.

## Mechanism and checks

One diagnostic profile of the initial exact-growth candidate recorded8 cudaMallocHost calls totaling1172.009ms and8 cudaFreeHost calls totaling348.596ms inside prefill. The prior baseline capture had no corresponding calls in that window. This justified bounded host growth. Profiled API durations are not added into an end-to-end speedup estimate or used as acceptance timings; Nsight warned about its driver-version support.

Completed checks:

- 13 fake-backend allocator cases cover default/exact behavior, NULL reset, headroom bounds, reuse, alias minima, multiple chunks, overflow, enlarged-allocation failure with exact retry, and recovery after both requests fail.
- Metadata/no-alloc estimates match the reference exactly. The probe uses LOAD_MODE_NONE, matching the existing memory-fit implementation; an initial mmap probe was discarded after the original runtime rejected that combination.
- Explicit full graph reservation produces identical physical compute allocations across runtimes.
- Full and partial state roundtrips at a24576-token prefix match bitwise, including identical state hashes across runtimes.
- Apex native cache and capacity: cold32K, 24K-shared branch, repeat, early16K branch and cold261632 all match selected IDs, ordered top5 IDs and probabilities exactly (maximum delta0). Cache counts match0/24576/32764/4096/0. The early branch ends naturally at EOS after4 tokens in both arms; other requests produce128. Process swap0, swap-out growth0 and request-window major faults0.
- Flint native compatibility: cold32K, branch, repeat and early16K all produce128 identical tokens and identical top5 probabilities. Cache counts match0/24576/32767/4096. Process swap0 and swap-out growth0. Major faults229/138 make this a functional regression check, not a performance comparison. The initial cold-load process with swap and a premature HTTP503 request was discarded and repeated with the same flags after cache warmup.
- Disable behavior: GGML_LAZY_PP_RESERVE=0 restores the exact eager physical compute sizes, selected output IDs/top5/probabilities and state roundtrip of the reference. The functional request had5 major faults after cold model loading; it is not a performance sample.

## Source and runtime identity

Apply `patches/lazy-pp-reservation.patch` after `patches/flint-apex-prefix-baseline.patch` on pinned source commit `1b44e30775dcb2f93952116b05f89e5bd797fb00`. The feature patch changes five files. Replaying it against preserved reference copies reproduces all compiled source hashes exactly.

Only `libllama.so.0.7.0` and `libggml-base.so.0.27.0` differ from EXP-083. The server executable, server implementation, CUDA kernels, CPU backend, common library and remaining runtime components retain their reference hashes. The executable version string alone therefore does not identify this variant; use `runtime-manifest.json`.

The measured candidate lives under `/home/end0/AI/LlamaServer/exp087-lazy-reserve/build/bin`. Keep its shared libraries together and set `LD_LIBRARY_PATH` to that same directory. Original builds remain untouched. `launch-profiles.json` records commands for the new baseline; installed Desktop/runtime profile files were not edited. The accepted runtime was activated on8081 with its default policy; the temporary external wire recorder was removed. `baseline-activation.json` records mapped-library verification and process identity. `source-provenance.json` records the feature patch, source hashes, reference/control relinking and build directory. Raw prompts, helper conversations, packet captures, private instructions, binaries and model weights are not part of the delivery.
