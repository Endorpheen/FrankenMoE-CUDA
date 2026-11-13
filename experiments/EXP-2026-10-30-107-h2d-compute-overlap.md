# EXP-2026-10-30-107: overlap pinned H2D expert uploads with GPU compute

Status: ACCEPTED for measured Apex prefill on 2026-10-31. Five manual pairs:
+10.609% median paired prefill throughput, cold decode -0.421%, repeat +1.876%;
peak VRAM3188->3348MiB (+5.019%), retained3188->2928MiB (-8.156%).
1280 selected IDs per arm/top5/probabilities exact; swap/out/major faults0.
Igor explicitly accepted this speed/memory tradeoff. Flint three cases exact,
peak2536MiB unchanged; final caller leaves Flint/Qwen3Next on the old path.
Separate CUDA trace proves1.415233s H2D/kernel overlap versus0, same bytes/count.
Final default/off, slot release/reallocate,21 CUDA graph launches and Q6_K
memcheck0 errors qualify the mechanism. Existing accepted bundles remain intact.

Artifacts: [report](../results/exp107-expert-overlap/README.md), paired data,
source/runtime manifests, incremental build commands and model-free harness in
`results/exp107-expert-overlap/`; replayable feature patch in
`patches/expert-h2d-compute-overlap.patch`.

The earlier unimplemented structural-dependency verdict below is historical;
it was reopened by Igor's explicit implementation/test request.

## Reopened hypothesis and acceptance gates

Reference: EXP101 at project commit 87209f121765d0fa4f4af752ea56a548864f5493,
with all runtime hashes from results/exp101-kv-lazy/runtime-manifest.json.
Implement a separate candidate under /home/end0/AI/LlamaServer/exp107-expert-overlap.
Preserve the EXP101 KV, mmap registration, lazy reservation and kernels.

Use two externally bound expert-copy slots with a separate upload stream and
per-slot read-complete/upload-ready events. Bind only actual execution graphs,
never metadata/no-alloc reservations; remove these copies from ordinary graph
arena planning. Restrict eligibility to one GPU plus CPU, one sequence, no MoE
cache or pipeline parallelism, prefill, registered contiguous Q4_K/Q6_K experts.
All other paths keep the original copies. Two slots have a fixed total cap of
512 MiB and are released when returning to an ineligible graph. The candidate
defaults to this path only inside the supported mode; GGML_EXPERT_OVERLAP=0
disables it. The accepted runtime is unchanged until acceptance.

First run a finite correctness screen (cold32K, repeat, branch) against the
preserved fixtures, then a balanced manual performance block if correctness
passes. Exact selected IDs and ordered top5, probability tolerance 0.001,
unchanged cache boundaries and zero swap-out/process swap are required.
Require an observed copy/kernel overlap mechanism and at least 5% paired gain
in the final real workload, with VRAM reported explicitly. Profiling times are
diagnostic only. Stop on correctness failure or memory-budget violation;
investigate and reject unhelpful candidates without changing EXP101.

The old pageable H2D timing is not a post-EXP089 speedup bound. Re-profile the
pinned reference; do not add overlapping intervals or claim a new gain from
the historical trace.

## Finding

- The expert uploads are issued by `llama_context::sched_copy_experts()`, which
  the scheduler calls at the start of a split whose first node consumes the
  destination (`node->op == GGML_OP_MUL_MAT_ID && node->src[0] == dst`).
- The copies are already `ggml_backend_tensor_set_async()` on the backend
  stream, immediately followed by the dependent kernels on the same stream.
  There is no independent GPU work inside that split to overlap with; the only
  earlier work is the previous split on the same stream.
- Overlapping across splits would require issuing the next split's expert copies
  before the previous split completes and before the next destination buffer is
  reused. That needs double-buffered destinations, explicit CUDA events, a
  changed scheduler copy order, and capture/replay validation. The next expert
  set must not be predicted (the existing ids are only read when the split
  starts), so the overlap window is limited to the already-selected set.
- The extra destination buffer has a real VRAM cost and needs its own budget and
  fallback; none exists today.

The pre-EXP089 trace shows why the direction is attractive (copy union 11.97 s
of a 21.87 s cold prefill, pageable source), but the current post-EXP089 pinned
path share was not re-traced, and the structural ordering above prevents a
minimal one-stream change.

## Historical bounded-cycle verdict

REJECTED for the earlier unimplemented cycle: a correct overlap needs a scheduler pipeline change
(double buffer + events + capture/replay + VRAM budget), which is a separate
architecture experiment, not a bounded patch. No pageable staging ring was
reintroduced and no future-layer expert set is predicted.
