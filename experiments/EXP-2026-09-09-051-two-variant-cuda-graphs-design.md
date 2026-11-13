# EXP-2026-09-09-051 — design: two CUDA Graph variants per key (draft shapes 1/3)

Status: `DONE` (`kind=design`, 2026-09-09, no runs). Igor's assignment after EXP-050:
investigate storing separate graphs for shapes 1 and 3 (not splitting catch-up into three
single-token passes — that would increase the number of compute calls and could lose the
benefit of batching); before implementation, check four points; syncs and D2H are kept.
Igor's hypothesis: two variants will reduce rebuild/capture and the CPU cost of launching
the draft; potentially ≥2% decode, but the budget must be assessed — the draft is only
part of the step.

## Decision formulation

The value of the `cuda_graphs` map (the key is the `nodes[0]` pointer, common.cuh:1464)
becomes an array of variant slots rather than a single `ggml_cuda_graph` object.
Slot = {graph, instance, warmup_complete, node_props snapshot, uid, last_used}. Decision
per call: iterate over the slots, find a ready instance with `update_required == false`
→ replay; otherwise a victim (empty slot or LRU) → warmup + capture into it. The key
difference from today: a neighboring variant's snapshot is no longer overwritten by a
foreign shape — it is precisely the single-snapshot-per-key overwrite that kills draft
replay (EXP-050: reset→unstable→capture loop, 0 launches per 353 calls).

Compatibility and correctness do not change: the only criterion is byte-for-byte
identity of the `node_properties` of all nodes (the same memcmp as today). No size hash
is introduced into the design. Control: env `GGML_CUDA_GRAPH_VARIANTS` (1 = today's
semantics, default for baseline compatibility; 2 = variants; if accepted — default 2).

## The four verification points (Igor's assignment)

1. Whether L1-rebuild changes addresses and the usability of stored graphs — no,
   addresses are stable. Code: `llm_graph_result::reset()` re-initializes the ggml
   context on the same `buf_compute_meta` (llama-graph.cpp:1347–1360) → tensor metadata
   addresses and `nodes[0]` are preserved. Empirical evidence from EXP-050: both draft
   keys kept the same addresses across all 351 calls with 173 L1-rebuilds; target keys
   continued launching after the single verify-rebuild. Residual risk: the EXP-050 diff
   prints only the first mismatched field, hidden discrepancies may follow it — this
   does not affect correctness (a full memcmp will reject a mismatch and send the slot
   to recapture), efficiency is checked by the gate and A/B.

2. Properties beyond shape: today and in the design, compatibility = node_count + the
   full `ggml_tensor` of each node (type, buffer, ne[4], nb[4], op, op_params, flags,
   src[] with their data/ne/nb, view_src, view_offs, data, name, extra, padding) —
   ggml-cuda.cu:2927, common.cuh `node_properties`. A size hash is insufficient and
   unnecessary: the slot is chosen by iterating over ≤2 with a full comparison, a
   "variant" is a slot index, not a hash. A legitimate periodic shape change within a
   variant: `kq_mask ne[0]` grows with KV with a padding of 256
   (llama-kv-cache.cpp:1250–1264) → one recapture per ~request is the norm.

3. The two-variant limit and memory: slots ≤ V (default 2). Draft backend: 2 keys × 2
   = ≤4 instances (graphs of 116/63 nodes; on the order of tens–hundreds of KB VRAM,
   the exact figure at implementation) + ≤4 snapshots × ~179 nodes ≈ 0.2–0.4 MB host
   (~1 KB/node). Target backend: 49 keys; in steady-state decode there is one shape —
   the second slot is not lazily taken; at the prefill(36)/verify(3) boundary, at most
   +49 instances (~1–2 MB VRAM, estimate) with the bonus of prefill replay. Per Igor's
   clarification the estimate is insufficient: the actual limit of additional
   host/device allocations is measured in EXP-052 (exact host arithmetic
   sizeof(node_properties)×n_nodes×slots + the cudaMemGetInfo delta around captures,
   with a caveat about page granularity); a second slot may also appear for target —
   counted by a slot counter in the implementation. LRU within a key; the previous
   10-s key sweep (common.cuh:1468–1489) also applies to slots. Invalidation: an
   ExecUpdate failure — destroy the instance of only the owning slot, the neighbor
   survives.

4. What share of costs disappears (the budget is an extrapolation from EXP-043/048/050,
   not a measurement): a step is 124.8 ms (14600.79 ms eval / 117 steps). Draft backend
   per step: 3 compute calls × (116+63) = 537 nodes — today direct launches + 2
   captures; it will become 6 graph launches (2 splits × 3 calls). CPU saving ≈ 531 ×
   3.08 μs (mean `cudaLaunchKernel` from EXP-043) ≈ 1.64 ms/step; plus the 2
   capture+instantiate per step disappear (estimate 0.1–0.5 ms, no direct measurement).
   What remains: 2 L1-rebuilds/step (build cost not measured, the host-serialization
   bucket of EXP-048) and the useful computation. Total L2-only ≈ 1.7–2.1 ms ≈
   1.4–1.7% of the step — below the ≥2% threshold; extending to L1 (two result chains)
   would add ~0.4–1 ms → 1.7–2.5%. Caveat (the lesson of EXP-046): the server-side
   effect may exceed the local estimate. The decision on implementation and on the
   L1 extension is made from the A/B result.

## Lifetime (summary)

Creation: the first call of a shape takes an empty slot (direct), the second identical
one in a row — capture + instantiate. Use: replay on a full match of the slot's
properties. Eviction: within a key — LRU by last_used (≤V slots); between keys — the
previous 10-s sweep. Invalidation: property-mismatch — a warm reset of its own slot
(the current reset of the branch, ggml-cuda.cu:4655–4658); an ExecUpdate failure —
destroy the slot's instance. Off switch: `GGML_CUDA_GRAPH_VARIANTS=1` restores today's
behavior one to one.

## Igor's clarifications (accepted 2026-09-09, implementation conditions for EXP-052)

Decision: L2-only as the first step; L2+L1 at once would complicate verification and
hide the source of the result. The expected 1.4–1.7% below the threshold — this is a
research candidate, not a promise of ≥2%. Do not add L1 automatically, even if the
gain turns out to be below 2%.

1. Key stability does not prove the lifetime of all addresses of a captured graph: the
   full memcmp is useful, but device buffers, kernel arguments, and temporary
   allocations after an L1-rebuild are checked separately. How it is checked: (a) code
   analysis — GGML's kernel arguments come from tensor fields (data pointers, ne/nb),
   which are part of node_properties, and from the backend's cublas-workspace/pool
   addresses; (b) a model-free gate with the scenario "rebuild of the graph on the same
   metadata buffer (like reset() in llama) → replay without recapture → the result
   equals the control"; (c) empirical evidence from EXP-050: 173 captures of d2 after
   rebuild — the properties, including data pointers, matched byte-for-byte. The
   remaining risk (temporary pool addresses) is closed by the full memcmp: any
   discrepancy leads to recapture, an incorrect replay is impossible.
2. Memory is only an estimate (see the fix in item 3): a measured limit instead of
   "negligible", a slot counter per backend (target included).
3. An eviction/restoration gate — an extended list of scenarios (each is a comparison
   of the result against the control path without graphs): alternation 1→3→1→3 (both
   shapes on launch); a third shape (LRU eviction, correctness after recapture); a
   pointer change with the same shape (a new key, first_call/capture); update-failure
   (a forced ExecUpdate failure through a test env seam, destroy of only the owning
   slot, the neighbor survives); sweep/teardown (10-s eviction of keys, a clean
   destructor without CUDA errors).

## What does not change

Syncs, D2H, algorithms, capture-legality (`ggml_cuda_graph_check_compability` at the
key level), data paths. Localization of the fix: common.cuh (slot + map) and
ggml-cuda.cu (slot choice in the decision automaton), ~60–100 lines; the EXP-050
diagnostic fields — per slot.

## Verification plan (next steps, both require Igor's approval)

1. Model-free gate: an extension of `exp050_synth.c` — alternation of two property sets
   on one key (renaming node[0] back and forth, the same trick that produced
   direct_warmup_reset in EXP-050): with VARIANTS=2 both shapes reach launch, the
   checksums of all rounds are equal; with VARIANTS=1 the control reproduces 0 launches
   (the EXP-050 behavior).
2. One A/B on the server per the EXP-046 protocol (after the gate): measure the effect,
   decide on acceptance and on whether the L1 extension is needed.

## Artifacts

Log basis: `results/archive/EXP-2026-09-09-050/` (server.log, analysis-offline.txt).
Code basis: the file:line refs in the text; the EXP-050 diagnostic patch. No new runs
or code changes in this EXP.
