# EXP-2026-10-30-084: one critical-path diagnosis on the unified Apex baseline

Status: ACCEPTED (`kind=diagnostic`) for identifying and bounding the next optimization direction. No runtime optimization or performance acceptance is claimed.

## Frozen reference and hypothesis

Reference delivery: EXP-083, project commit `8c1f02e9a824ef4ed573140cc7c2dd92fd80008e`. Executable and every runtime library must match `results/exp083-flint-apex-baseline/runtime-manifest.json`. Use the existing unified runtime and the saved Apex profile, without rebuilding it.

Hypothesis: a single Nsight Systems capture of cold prefill, a late branch and an identical branch repeat can separate the critical-path costs of recurrent-state serialization, host/device transfers and CPU/GPU coordination. A useful next direction must have a material end-to-end upper bound and an observable mechanism. This diagnostic does not claim a candidate is faster.

## M0 and scope

Ryzen 9 5950X, RTX 4070 12 GB, Apex-35B-V1.0 Q4_K_M. Context 262144; GPU dense layers; all MoE experts CPU (the actual working command); fit off; Flash Attention; Q4_0 KV; batch/ubatch 4096; threads 8/16; the actual working CPU placement and polling settings; one slot; mmap; no speculation; RAM prompt cache 0; 32 recurrent checkpoints with the existing default 2048 MiB payload cap. Ordinary attention KV stays native. No storage or numerical algorithm is changed. The 2026-10-29 best-profile measurements used five GPU expert layers; they are not the profile of the live server inspected on 2026-10-30. This diagnostic freezes the actual all-CPU-expert working command instead of changing placement while tracing. GPU background applications leave about 1.3 GiB free beside that server, so the larger historical GPU placement is not assumed feasible.

## Pre-stated execution contract

- One optimization-diagnostic server start, at most ten minutes; dedicated port 18484.
- Exactly three manually issued serial generation requests, each 32768 input tokens and 128 generated tokens: original cold fixture, branch sharing 24576 tokens, identical branch repeat. Use the saved 2026-10-29 token fixtures, native prompt cache enabled, seed 123, temperature 1 and saved sampling parameters.
- Trace CUDA API/kernel activity, graph nodes and OS runtime calls. No performance acceptance based on profiled timings. No background build, optimizer, helper inference or parallel measurement.
- Inspect process VmSwap and system pswpout before/after requests, host headroom and GPU availability. Stop on process swap, new global swap-out, OOM or insufficient headroom. Existing occupied system swap is not by itself new swap activity.
- Freeze source/runtime hashes. Require successful non-truncated requests, expected prefix reuse and trace completeness before drawing workload-specific conclusions.
- Report GPU busy time as an interval union and intersect it with the workload window. API waits, copies and kernels may overlap; do not add them as independent time savings.
- Identify whether checkpoint/transfer costs could clear the project's materiality criterion even if removed entirely. Mark measured data, estimates and unknowns separately.

## Helper boundary

Outside measurement, the user-authorized local Apex received a bounded coding task through the active Desktop/mobile runtime. It implemented `results/exp084-apex-profile/apex_interval_union.py`. Parent source review and independent boundary checks passed, and the code was accepted unchanged. The module computes clipped interval unions, complements and overlap without side effects. Helper inference finished before measurement. No other agents were launched. Private application receipts and session identifiers are not part of this delivery.

## Execution and observed latency

The existing 8081 server was idle before its temporary stop. Its complete launch command, working directory and relevant runtime environment were retained locally and restored after capture. One diagnostic server ran on 18484; three requests were issued manually, one at a time. The capture lasted about 423 seconds, including startup and idle preparation time; analysis clips each client request window and does not charge that idle interval to inference. After capture the original 8081 command was restarted and `/health` returned `ok`. All reference executable/library hashes still match EXP-083.

The cold case means no prompt-prefix cache hit, not a cold OS page cache. Client TTFT is request-header send to first non-empty SSE content, including a newline token. It is not time to a completed answer or to the end of reasoning.

| Request | Cached | Evaluated | Client TTFT s | Client total s | Server prefill s | Prefill tok/s | Single-response decode tok/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Cold 32K | 0 | 32768 | 21.873608 | 26.292531 | 21.864453 | 1498.69 | 28.75 |
| 32K branch, 24K shared | 24576 | 8192 | 5.753714 | 10.225509 | 5.745872 | 1425.72 | 28.41 |
| Identical branch repeat | 32764 | 4 | 0.171310 | 4.627433 | 0.163460 | 24.47 | 28.51 |

All requests returned HTTP 200, generated 128 tokens and reported no input truncation. Prefix reuse was 24576/32764 tokens. The branch/repeat restore calls were 7.338/7.931 ms for 62.813 MiB. These are single observations under a profiler; no speedup against another runtime/profile or quality-equivalence claim follows.

## Hardware-time account

The accepted Apex module computes actual unions rather than adding overlapping durations. Nsight's `utcEpochNs` anchors curl's local timestamps to the trace clock. The client prefill window includes transport, parsing and delivery of the first sampled token; it is not an instrumented internal operator boundary.

| Client prefill window | Window s | GPU kernel union s | GPU copy union s | Kernel/copy overlap s | Window outside recorded GPU activity s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Cold | 21.873608 | 7.207931 | 11.966826 | 0.000000 | 2.698851 |
| Branch | 5.753714 | 2.046337 | 3.028226 | 0.000000 | 0.679151 |
| Repeat | 0.171310 | 0.010325 | 0.008766 | 0.000000 | 0.152219 |

Cold H2D: **133167587368 bytes (124.022 GiB), 28268 copies, 11.831852 seconds of GPU copy time**, or **54.09%** of client TTFT. Of those bytes, 131140214784 (98.48%) are recorded as pageable host memory. The enum mapping in this capture is `srcKind=0` pageable, `1` pinned. Branch H2D: 32.161 GiB, 6086 copies, 2.995486 seconds, or 52.06% of TTFT.

Cold host `cudaMemcpyAsync` calls occupy 13.678899 seconds summed; GPU transfers occupy part of those intervals. They must not be added to GPU copy time as separate savings. Likewise, the approximately 1.104-1.128 seconds of summed decode `cudaStreamSynchronize` calls include GPU waits. Decode contains approximately 3.407-3.458 seconds outside the recorded GPU activity, but no CPU-sampling evidence identifies all of that remainder as CPU arithmetic.

## Source mechanism and selected direction

Measured evidence is consistent with the exact source path:

- `src/llama-context.cpp`, `llama_context::sched_copy_experts`: fetch routing IDs, mark selected experts, group consecutive experts, and submit spans directly from `src->data` with `ggml_backend_tensor_set_async`. Preserve its up-to-512-byte MMQ padding when testing an alternative.
- `ggml/src/ggml-backend.cpp`, `ggml_backend_sched_copy_input` / `ggml_backend_sched_compute_splits`: submit host weights through that callback before computing the consuming split.
- `ggml/src/ggml-cuda/ggml-cuda.cu`, `ggml_backend_cuda_set_tensor_async`: perform H2D with `cudaMemcpyAsync(..., cuda_ctx->stream())`.

In the captured cold prefill all kernels run on stream 14; 28441 copies also run on stream 14 and 700 copies on stream 13. The observed kernel/copy overlap is zero. The direct callback does not use the earlier Franken bounded pinned ring. The trace does not label every H2D span by tensor name, so attributing the bulk pageable traffic to expert uploads is a source-backed inference, not per-tensor instrumentation.

**Next candidate: a bounded pinned staging path for this callback**, initially considering the previously used 32 MiB budget rather than pinning the model. Keep routing, tensors, offsets, transferred bytes including padding, quantization, checkpoint policy and placement unchanged. The first question is whether the total gather + upload + final drain improves on direct pageable submission for this workload. Merely observing pinned memory or shorter host enqueue time is insufficient.

ESTIMATE: if H2D time falls by 10% with all other costs unchanged, cold TTFT can fall by about **1.183 seconds (5.41%)**; the branch estimate is about 0.300 seconds (5.21%). This is conditional Amdahl arithmetic, not a prediction that staging will deliver that reduction. Added CPU gather, RAM-bandwidth contention, more chunks, unchanged compute-stream serialization and buffer lifetime can erase it. A new copy stream is a separate architectural decision with dependency and allocator-lifetime obligations; do not assume pinned staging alone overlaps GPU kernels.

The proposed next minimal gate is a model-free comparison using representative real expert ranges and order, byte-for-byte parity including padding, the same transfer stream and a final drain, a bounded 32 MiB staging allocation, and no process/global swap growth. Count CPU gather in the main time. Move to model A/B only if that complete path wins materially; use the project's repeated paired acceptance criteria. No such probe, runtime patch or second optimization experiment was started here.

## Memory, correctness and profiler limits

Process VmSwap was zero at all four request-window resource snapshots. Peak sampled RSS/HWM reached 24441504 KiB (23.309 GiB). System `pswpout` stayed **7714264** before/after all three requests. Earlier preparation had recorded 7439122: a cumulative increase of 275142 pages (about 1.05 GiB on this system) happened before the request-window baseline. Attribution is unavailable; do not claim the whole preparation/startup was swap-free. No OOM was observed.

No model/kernel/server source or runtime library was modified. This diagnostic does not repeat state-roundtrip or finished-task quality validation; EXP-083 retains the prior evidence and its limits. The interval module passed parent review and hand-checked overlap, clipping, adjacency, duplicates, empty inputs/window, input preservation and invalid-bound cases.

Nsight Systems reported three warnings: installed CUDA driver 13.0 is unsupported by this profiler build and CUDA tracing uses 12.6 libraries; thread scheduling information is absent; no NVTX events were collected. Thus this is a scoped CUDA timeline diagnosis, not a validated CPU attribution or a replacement for unprofiled acceptance. These warnings are preserved in `analysis.json`.

## Verdict and artifacts

ACCEPTED for the diagnostic question: the capture identifies the large serialized H2D path, documents its source mechanism, bounds a specific candidate and separates estimates from measured data. No new accelerated runtime is delivered. The working server has been restored; this pass is finished.

`results/exp084-apex-profile/` contains the accepted helper module, offline analyzer, `analysis.json`, `metrics.json`, `resources.json`, `runtime-profile.json`, `runtime-integrity.json`, `candidate.json`, helper review and raw-data hashes. The analyzer never launches a model or submits requests. Raw Nsight, SSE, curl traces and server logs stay ignored under `results/archive/EXP-2026-10-30-084/raw/`. Private launch restoration and application receipts stay outside Git.
