# EXP-2026-10-30-087: lazy prompt compute reservation

Status: ACCEPTED (`kind=model-memory-ab`). New Flint/Apex baseline; all pre-stated gates PASS. The sections below preserve the hypothesis, pre-stated protocol and intermediate diagnostic evidence; the final result is at the end.

## Hypothesis before changes

EXP-086 failed a 4370.05 MiB compute-buffer reservation for batch6144/context262144. The preserved source constructs a full-memory worst-case graph and reserves prompt-processing buffers at context creation. Its actual cost drivers and the existing scheduler growth/failure contract must be verified before claiming that delayed reservation is safe or useful.

The potential target is at least 5% lower measured model-process peak VRAM on a matched workload with unchanged configured context, tensor math, sampling and existing optimizations. Lowering context, removing masks, moving checkpoints, changing weights/quantization, suppressing EOS, or merely moving an OOM to a later request is not an accepted result.

## M0 and scope

Reference project commit: `a7c411cd327feb7c2ec375314daa6a3af5a7ae0c`. Performance engine remains the immutable EXP-083 runtime/source snapshot. The comparison does not substitute current official upstream for the preserved performance baseline. Old builds, launchers and `work/llama.cpp-integration` stay untouched. Builds only under `/home/end0/AI/LlamaServer/`.

## First gate

1. Parent reads reservation, decode allocation and backend allocator contracts. Apex independently inventories the supplied source excerpts in the already-new official-server work session; no production code changes in that task.
2. Identify the dominant buffer/tensor shapes or honestly record that source inspection alone is insufficient. Preserve accurate no-alloc/autofit size prediction, explicit public reserve semantics and unsupported-mode fallbacks.
3. Establish whether scheduler growth synchronizes, frees/reallocates and invalidates reusable graph bindings safely. An unproved CUDA Graph address/lifetime assumption blocks integration.
4. Only after this gate, record a concrete candidate and pre-stated model A/B protocol. Require model-level token/probability and cache-branch correctness before memory/performance acceptance; at least five serial matched pairs, process swap 0, no request-window swap-out, full capacity/failure checks and legacy-mode coverage. An implementation requires at least 5% matched workload peak-VRAM reduction and no material prefill/decode regression.

## Boundaries recorded before implementation

The initial hypothesis did not prove that a large causal mask was the dominant allocation. No patch or production flag existed at that stage. The decision against moving/removing prompt checkpoints remains in force. Exact-prompt snapshot caching is a separate parked idea, not concurrent implementation.

## Source gate and candidate protocol (before implementation)

MEASURED: relinking the untouched original libllama objects reproduces the immutable EXP-083 libllama SHA-256 exactly (`ae8c25117528bc7500e05a03746440fa3fa5ae003d9a40a7f9c4e0b4177e12ce`). All four ggml libraries, common, llama and mtmd in that object build match the preserved runtime. The server implementation and executable are copied from EXP-083, not reused from the original object build.

SOURCE-VERIFIED: scheduler graph splitting assigns fresh split graph UIDs; CUDA checks node/source addresses when the UID changes; allocation growth synchronizes backends before freeing buffers. Decode allocation failure returns -2 and removes the failed ubatch positions; it does not promise transactional allocator rollback. The explicit public graph-reserve API must remain physical. Full-memory KV contexts expose configured KV capacity, whereas actual ubatches use occupied/padded KV length. This changes required mask shapes without changing their mathematical definition.

ESTIMATE: an F16 mask at 262144 x 4096 requires 2048 MiB, versus 256 MiB at 32768 x 4096; actual scheduler placement/liveness and other compute tensors must be measured. These sizes are not yet a demonstrated peak-VRAM saving.

Candidate: defer physical worst-case prompt-graph reservation in ordinary single-sequence, single-GPU causal inference; retain dry graph splitting and physically reserve the TG graph, letting the existing scheduler grow for actual prompt graphs. Keep no-alloc size estimation, training, MTP, pipeline parallelism, CPU-only, multi-device and multi-sequence paths eager. Keep explicit llama_graph_reserve physical. The new behavior is default on within its supported mode, with only a disabling environment flag. Preserve tensor computation, checkpoint scheduling, weights, precision and all EXP-083 runtime components except the isolated rebuilt libllama.

Qualification: two serial starts/two requests, up to six minutes, unchanged batch/ubatch4096 and EXP-086 cold fixture (32768 IDs, seed123, up to128 generated tokens, EOS respected, top5). No performance promotion before token IDs, ordered top5 and probability tolerance (absolute0.001, matching the accepted EXP-086 comparator) pass. Capture compute-buffer allocation, process peak VRAM, client/server timing, RSS, process swap and request-window system swap-out.

If qualified: five matched pairs in balanced A-B/B-A order, ten serial starts and cold requests, up to fifteen minutes. Accept at least5% lower paired model-process peak VRAM, every pair correctness passing, process swap0/no swap-out and no material median prefill/decode regression (more than3% requires investigation, not automatic acceptance). Retain raw responses and resource data. No parallel helper, build or model work during measurement.

Additional gates before acceptance: warm identical prompt; 24K-shared-prefix branch; 5120-shared-prefix early branch; growth to near configured capacity; explicit full-size reserve and injected allocation-failure recovery contract; disable flag/reference behavior and supported legacy model coverage. Do not treat only successful startup, a smaller initial allocation or a short prompt as proof of full-capacity compatibility. If a gate fails, keep the immutable reference and record the limitation.

## Measurement scope

Primary memory measurement is model-process resident VRAM peak during the matched request window, including its pre-request resident allocation. Use the same native NVML-backed nvidia-smi loop for both arms, record its cadence and raw samples, and distinguish this metric from whole-machine GPU usage and whole-process-lifetime startup peak. All requests remain manually started, serial and unprofiled by the wire recorder.

## First qualification pair (not acceptance)

The Apex one-file draft required a type correction (`llama_hparams`) and correction of the allocation mechanism comments. The corrected draft compiled and linked without warnings; it was used unchanged. Isolated candidate libllama SHA-256: `63a1f5262b6809e3f78cebec3c4627476e83514f8e4cad6e4b70e6f51566d917`. All other runtime components remain the EXP-083 versions. Original headers (59 files) match the preserved snapshot, and unchanged control relinking reproduced libllama exactly.

| Qualification metric | Control | Candidate |
|---|---:|---:|
| Sampled request-window process VRAM peak, MiB (100ms NVML cadence) | 6404 | 4428 |
| Server prefill, ms | 21741.425 | 23034.521 |
| Single-response decode, tok/s | 30.574900 | 30.029590 |
| Post-request RSS, KiB | 23782416 | 21945000 |
| Process swap, KiB | 0 | 0 |
| Request-window system swap-out growth, pages | 0 | 0 |
| Request-window major faults | 0 | 0 |

All128 selected IDs, ordered top5 IDs and probabilities match exactly (maximum probability delta0). Sampled VRAM reduction is30.8557%; prefill elapsed time increases5.9476%; decode throughput changes-1.7835%. This is one diagnostic pair, not five-pair confirmation or an accepted tradeoff. A slowdown above the pre-stated3% threshold needs investigation. Both logs report7 graph-reallocation callbacks, so that counter alone does not identify physical reallocation costs. The smaller initial TG size remains in the candidate memory-accounting field after later graph growth; a small helper follow-up addresses that reporting issue without changing allocation.

## Allocation-cost diagnosis and bounded-headroom refinement (before changes)

One candidate-only Nsight run, same32768/128 request, preserves all selected IDs/top5 probabilities exactly (delta0) after the reporting-only accounting fix. During the prefill client window, CUDA runtime API attribution records8 cudaMallocHost calls totaling1172.009ms and8 cudaFreeHost calls totaling348.596ms; cudaMalloc totals76.851ms. The prior EXP-084 baseline trace has no malloc/free-host calls in the cold-prefill window. Summed API durations are not additive end-to-end speedups; the profiler also warns about driver13.0 versus its supported12.6 tracing libraries. No profiled performance number is used for acceptance.

Refinement hypothesis: repeated exact-size pinned compute-buffer growth, rather than GPU allocation alone, accounts for much of the first-request penalty. Keep GPU allocation exact. On the existing supported lazy path, plan worst-case sizes once on a fresh empty scheduler, then enable host-only physical single-chunk allocation headroom of up to4x required size, capped by the original worst-case per-backend planned size and backend maximum allocation. This should cover the observed short-request host-buffer growth in fewer pin/unpin operations, while never imposing a smaller required capacity or a larger host buffer than the previous eager reserve for that shape. Full no_alloc estimation remains unchanged.

New allocator/scheduler APIs default to exact allocation (zero headroom limits); only the eligible lazy llama context enables host limits. Extra headroom is optional: a failed enlarged allocation retries the original exact required size. Multichunk graphs remain exact, zero limits disable growth, limits below requirements never clamp necessary allocations, and integer multiplication must be overflow-safe. Explicit reserve remains physical. After successful graph allocation, keep physical compute memory accounting current; do not suppress destructor validation.

This is one dependent refinement within EXP-087, not a new accepted baseline. Preserve the prior candidate library/source and retain the original qualification result. Before model tests require a fake-backend allocation gate for default/disabled behavior, capped headroom, reuse, overflow/bounds, multichunk fallback and enlarged-allocation failure with exact-size retry. Then repeat qualification against immutable EXP-083, followed by the previously required paired/capacity/cache/legacy gates. Primary5% threshold and the pre-stated performance-regression investigation threshold remain unchanged.


## Final result and regression gates

Five balanced manual pairs (A-B, B-A, A-B, B-A, A-B), ten fresh processes and identical32768/128 requests: model-process sampled request-window VRAM6404 ->4428 MiB in every pair (-1976 MiB, -30.8557%). Median paired RSS reduction6.6163%, prefill elapsed change+1.1647%, single-response decode change+3.4048%. Arm median prefill21975.031/22083.785ms; decode29.3030/30.7468tok/s. Percentage medians are computed per pair before aggregation. All640 selected tokens per arm, ordered top5 IDs and probabilities match exactly, maximum delta0. Every primary request has process swap0, swap-out delta0 and major-fault delta0. No aggregate-throughput claim.

All13 allocator cases PASS, including bounded headroom, zero/default/NULL behavior, alias minima, multichunk fallback, overflow and injected OOM exact-size retry/recovery. No-alloc estimates match exactly; the explicit reserve API allocates the same full graph physically. Full and partial24576-prefix state roundtrips have identical hashes across runtimes.

Apex cache/capacity chain PASS: cold32K, shared24576 branch, repeat, early16K and cold261632; identical IDs, ordered top5 and probabilities, delta0. Cache counts0/24576/32764/4096/0 match. The early branch reaches EOS after4 tokens in both arms; others produce128. No truncation/OOM/process swap/swap-out growth or request-window major faults. Configured context262144 is preserved.

Flint131072-context compatibility PASS on its existing vocabulary-valid cold/branch/repeat/early fixtures:128 identical selected tokens per case, delta0, cache counts0/24576/32767/4096. Process swap0 and swap-out growth0. Major faults229/138 make this functional compatibility evidence, not a performance comparison. The first cold-load process swapped pages and a premature request returned HTTP503; it was discarded and the identical profile repeated after cache warmup.

Apex supplied the candidate draft, allocator behavioral probe, arithmetic review and generalized SSE geometry verifier. Substantive omissions were returned only after completed tasks. The final verifier passes independent positive/negative cases; comparison functions and tolerance are unchanged. Correct portions were accepted without rewriting.

The final five-file feature patch is patches/lazy-pp-reservation.patch, applied after the preserved EXP-083 patch. Only libllama and libggml-base change; all other runtime components retain EXP-083 hashes. Patch replay matches compiled sources. No model-name branches: the default-on policy is restricted to ordinary single-sequence causal Flash Attention with one GPU+CPU, default context type and no training/pipeline/no-alloc mode. Metadata/no-alloc and explicit reserve semantics remain eager. GGML_LAZY_PP_RESERVE=0 disables the optimization; the functional request and API probe match the reference exactly.

The30.9% figure compares our previous EXP-083 build, not official upstream or a third-party fork. It applies to32K actual requests at262K configured context. Longer graphs grow buffers, which can remain resident; no30% saving at every context length is claimed. Previous Qwen builds remain preserved and are not replaced for unqualified models. EXP-086 batch6144 remains REJECTED. No model-quality or all-architecture claim follows from these deterministic fixtures.

Detailed metrics, ranges, source/runtime manifests, build commands and reproducible probes are in results/exp087-lazy-reserve/. Raw prompts, helper conversations, packet captures, private instructions, binaries and weights are excluded from Git. Installed Desktop/runtime profiles are unchanged. Disable and activation PASS; the new default-on runtime is active on8081, the external recorder is removed, and the installed profile hash is unchanged. No new experiment is started automatically after delivery.
