# EXP-2026-11-15-727 — the copy path is graph-node based; the async API exists but the copies are serialised

Base: `6acd57f4`. Follow-up: EXP-726 established that 0% of the 4.24 s of expert copying overlaps
compute. This traces why, by disassembly, before changing anything.

## 1. The call chain, established from the binary

```
llama_context::sched_copy_experts(backend, src, dst, graph, ctx)
    -> llama_moe_cache::copy(backend, src, dst, graph)
        -> llama_moe_cache::impl::copy(...)
            -> ggml_graph_n_nodes, ggml_graph_node, ggml_is_contiguous,
               ggml_nelements, ggml_nbytes, ggml_backend_buffer_get_usage
```

`copy_experts` has exactly **one** caller: `sched_copy_experts`. So the expert cache is live in this
build - unlike the Goose version (`fg262a`), which is defined and called by nothing.

## 2. Why there is no overlap

`impl::copy` does not issue device copies itself. It walks the graph with `ggml_graph_node`, reads
`ggml_nbytes` and `ggml_is_contiguous`, and builds **copy operations as graph nodes**. Those nodes
execute in the same scheduled graph as the compute - the scheduler runs them in order, and the device
has nothing else in flight while they run.

That matches the measurement exactly: copies and kernels alternate, overlap 0.00 s.

## 3. The async infrastructure is present and already used

The runtime does **not** call the blocking entry point:

| symbol | callers |
|---|---|
| `ggml_backend_sched_graph_compute` (sync) | **0** |
| `ggml_backend_sched_graph_compute_async` | 1, from `llama_context::graph_compute` |

So the graph itself is dispatched asynchronously. The copies are inside that graph, which means the
GPU is not idling at the API level - it is idling because the graph's own node order is
compute, copy, compute, copy.

## 4. What this means for the optimisation

The lever is not "make the API async" - it already is. The lever is **graph construction**: expert
copy nodes for a later part of the graph would have to be issued before the compute nodes that do not
depend on them, so the scheduler can overlap them.

That is a change to how the graph is built, which is exactly the kind of thing the Goose graph
builder (`gb_common`, 28 modes) exists to express - and where the Goose expert-cache module
(`goose-moe-cache-copy-decisions.goose`) is meant to make the decision.

So the two findings connect:
- the Goose modules that decide expert copying exist and are not called (EXP-725);
- the native path that does the copying is a graph-node builder with no reordering (this card).

Wiring the Goose decision in, and having it place copy nodes earlier in the graph, is one change that
addresses both.

## 5. What is not attempted yet

No change was made. Reordering graph nodes is a correctness-sensitive operation: a copy issued too
early could be overwritten by a later token's selection, and a copy issued too late is simply the
current behaviour. Before touching it, the dependency structure needs to be read - which expert copy
belongs to which compute node - and that is the next measurement, not this card.

## 6. Balance

No performance change, by construction: this card is disassembly and two call-graph counts. The
pinned configuration and its measured numbers stand.
