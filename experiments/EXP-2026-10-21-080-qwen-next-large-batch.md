# EXP-2026-10-21-080: Qwen Next prefill on the ported upstream runtime

Status: IN_PROGRESS. Authorized by Igor on 2026-10-21 to clean safe caches and run the necessary Qwen Next tests after stopping his server.

## Hypothesis and protocol

The updated runtime and larger physical batches may independently reduce long-prompt processing time. Compare the existing EXP-063 and separately built upstream-839a port on identical Qwen3.8-Flash-Next UD-IQ3_XXS weights, 64000 context, CPU experts, EHS off, Flash Attention and Q4_0 KV. Use the same 8192-token prompt and 256-token greedy output after a separate warmup; measure each profile twice. Start without MTP to separate runtime/batch effects, then test the existing MTP sidecar and thread counts on the best feasible configuration. Include matched 512/4096 batches on both runtimes when VRAM permits; record failed allocations separately from measured throughput. Preserve output/correctness evidence, RAM/VRAM, disk reads, major faults and swap counters. Model output identity is checked, not assumed across runtime/batch changes.

Measurements are manual and sequential. No benchmark queue, subagents, simultaneous server, build, or cleanup work. Disable core dumps and test-process swap; stop on low host-memory headroom or sustained new swap activity. Do not change the existing production launchers or integration tree. Data and exact commands: `results/qwen-next-20261021/`.

## Results

Pending.
