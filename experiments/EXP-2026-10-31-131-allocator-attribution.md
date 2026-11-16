# EXP-2026-10-31-131: actual graph-arena attribution

Status: IN_PROGRESS (diagnostic prerequisite for EXP123/124 and the requested EXP107 implementation).

Reference: EXP101, project87209f121765d0fa4f4af752ea56a548864f5493, runtime manifest results/exp101-kv-lazy/runtime-manifest.json. Apex Q4_K_M, context262144, b/ub4096, CPU MoE, threads8/16, existing masks/poll0, FA, Q4_0 KV, one slot, no speculation/EHS. Existing model/runtime files remain immutable.

## Hypothesis and M0

The ~1.4GiB residual after cold32K is not an attribution or reclaimable-memory measurement. Actual graph allocations can distinguish expert staging, mask, activation and retained arena capacity. The current scheduler splits on incompatible weights specifically to reuse their allocations; EXP107 must not overlap writes with earlier readers of aliased storage. Metadata/no-alloc planning must be separated from physical execution.

Add read-only graph-arena snapshots after ggml_gallocr_alloc_graph assigns actual tensor buffers, with physical arena/chunk sizes, tensor offsets/layouts and graph dependencies. Skip snapshots whose owned layout is unchanged. No allocator policy, placements, arithmetic, kernels, precision or lifetime changes. Only the allocator object/libggml-base changes in a separate diagnostic bundle under /home/end0/AI/LlamaServer/exp131-allocator-attribution. Diagnostic output is enabled only by GGML_ALLOC_DIAG=1 and is not performance evidence.

## Finite probe and gates

One diagnostic startup, two serial existing native requests: cold32768 then identical repeat, up to128 outputs/seed123/top5/EOS unchanged. Compare exact selected IDs/ordered top5/probabilities (tolerance0.001) with preserved EXP101 results; record process swap/system swap-out/faults and sampled process VRAM. Restore the saved working EXP101 service afterward. No formal speed claim or change to the accepted runtime. Target is a complete graph-arena inventory and a measured bound for a next candidate; do not sum overlapping allocations or independently peaked categories.
