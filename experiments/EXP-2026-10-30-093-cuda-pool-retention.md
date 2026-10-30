# EXP-2026-10-30-093: CUDA temporary-pool retention

Status: REJECTED for pool-only >=5% savings at the fixed32K Apex workload. Reference EXP090, project852da2162a67090c458083cb50a673db7a2e2f67. The new>=5% goal remains active.

## Source audit and M0

The current CUDA VMM pool maps physical memory on demand, aligns requests to128 bytes, rounds mapping growth to driver-recommended granularity and decrements logical pool_used on LIFO frees. It retains mappings until destruction. The legacy pool similarly caches freed buffers and already invalidates captured CUDA graphs when freeing underlying buffers on OOM. These exact paths have no prior experiment card in the searched history; older state-cache/CUDA-graph variants are not this pool-lifetime experiment.

Captured graphs can retain temporary pointers after pool_used reaches0. Never infer that a zero host allocation counter permits unmapping. Model, KV and scheduler buffers are separate from this temporary pool and remain unchanged.

MEASURED historical control: post-request process VRAM4428MiB on the fixed32K Apex workload. A5% reduction requires at least221.4MiB. The entire physically mapped temporary pool is an upper bound on possible savings; if its sum is below this threshold, reject this direction for this workload without implementing trimming. If above it, further graph-lifetime/required-working-set proof is mandatory. Distinguish request peak, active-decode memory and idle memory; they are not interchangeable.

## First diagnostic

Separate instrumentation-only source/build/bin under /home/end0/AI/LlamaServer/exp093-pool-diagnostic. Add host-only mapped/live/high-water counters and a private backend registry report hook, called after the existing whole graph compute when EXP093_POOL_REPORT=1. Report pp/tg, device/stream, mapped/live/host-alloc-peak/granularity and captured-graph count; record cuBLAS workspace separately. Reset only the diagnostic high-water counter between reports. A graph replay can use captured memory without new host allocations, so a zero peak on replay is not proof of zero physical requirement.

No allocator policy, numerical kernel, layout, buffer size, context, checkpoint or CPU/GPU placement changes. Build only CUDA host master/context objects and relink, retaining EXP090's model object and all other kernel/CPU/base/server objects. Existing accepted builds remain immutable.

One serial diagnostic server start/two requests: cold32768 then identical repeat, up to128 outputs, seed123/top5/EOS unchanged. Timing with diagnostic logging is not performance acceptance. Require identical selected/ordered-top5 IDs and probabilities within0.001 against preserved current control to establish instrumentation consistency. Capture pool reports, process swap/system swap-out/faults and point NVML memory. Restore EXP090 afterward. If the upper bound cannot yield5%, stop without a candidate or formal A/B. Otherwise record a separate supported trimming policy and its correctness/resource gates BEFORE implementation or performance claims.

## Result and stop decision

Measured mapped VMM pool maximum74MiB, cuBLAS workspace4MiB. Against observed process VRAM4428MiB, the required5% is221.4MiB. Even removing the entire pool has an impossible upper bound1.6712%, or1.7615% with the whole workspace. Stop without implementing trimming or running formal performance pairs. This rejection applies to the fixed32K pool-only direction, not every context/backend.

Both cold/repeat outputs match selected IDs, ordered top5 and probabilities exactly, delta0. Process swap/system swap-out0; major faults242/1. Logging/faulted request times are not performance claims. Instrumentation does not change allocation policy. One diagnostic startup/two requests, then service-only EXP090 restoration; health/120 registered ranges/all nine runtime hashes verified. Artifacts, diagnostic-only patch, commands and raw-data hashes: results/exp093-cuda-pool/.
