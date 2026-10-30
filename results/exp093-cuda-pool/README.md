# EXP-2026-10-30-093: CUDA temporary-pool retention — REJECTED

The diagnostic falsifies a pool-only >=5% VRAM saving for the fixed 32768-token Apex workload above EXP090. It does not establish that pool trimming is safe, beneficial at larger workloads, or useful for latency. No allocation policy or default changes are shipped.

## Measured bound

One instrumented startup and two serial requests (cold 32768 and identical prefix repeat, up to 128 outputs) produce 266 reports of one CUDA VMM pool. Its maximum mapped size is 74 MiB; the separately recorded cuBLAS workspace is 4 MiB. Against the observed 4428 MiB process VRAM, 5% would require 221.4 MiB. Even the impossible removal of the entire pool saves at most 1.6712%; pool plus all cuBLAS workspace at most 1.7615%. The post-request point NVML reading remains 4428 MiB. These figures are not sampled request peaks.

Both requests match preserved EXP092 fresh 8-thread control IDs, ordered top5 and probabilities exactly (delta 0). Process swap and system swap-out are zero. Cold/repeat major faults are 242/1; diagnostic logging and faults mean recorded request times are NOT performance evidence. No formal A/B series follows a failed upper-bound screen.

Host allocation live bytes return to zero, while CUDA graphs retain captured addresses. Graph replay can use temporary storage without new host allocations. Neither zero live bytes nor a zero replay high-water report proves that mappings can be freed. No unmapping candidate is implemented.

## Instrumentation and provenance

Reference project commit: 852da2162a67090c458083cb50a673db7a2e2f67. The isolated source/build/binaries live under /home/end0/AI/LlamaServer/exp093-pool-diagnostic. The diagnostic patch is incremental against the composed EXP090 source. It adds a VMM host-allocation high-water counter and a private backend report callback, invoked after graph compute only when EXP093_POOL_REPORT=1. It changes no arithmetic, allocator policy, placement, context, tensor layout, checkpoint or kernel selection.

build-commands.json records the two compile commands and two links; link commands run from /home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda. Only llama-context.cpp and the CUDA host master are rebuilt. The EXP090 model object and other preserved objects remain in use. runtime-manifest.json records all nine runtime hashes; only libllama and libggml-cuda differ from EXP090. This instrumentation is a diagnostic artifact, not an accepted runtime feature.

The diagnostic server was stopped, then unchanged EXP090 was restored on port 8081 without an extra completion request. Health is OK, all nine mapped hashes match the accepted manifest, and 120 mmap expert ranges are registered. restored-runtime.json records this check. Raw SSE, traces, logs and resource samples are local and hashed in local-evidence-manifest.json; model weights, prompts and process environment are excluded from publication.

Revisit only if a meaningfully different workload or backend produces a larger measured retained pool. Do not repeat the same fixed-32K experiment or trim captured mappings merely because host allocations are logically free. The current >=5% optimization goal remains unmet.
