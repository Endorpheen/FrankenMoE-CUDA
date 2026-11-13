# P1-10 restatement for the substrate that actually runs

Status: decision layer complete and live; one owner decision needed before the item can close.

## Clause by clause

| clause | owner in this substrate | evidence |
|---|---|---|
| expert scratch policy (grow / reuse / failure) | Goose | EXP-145, accepted |
| expert residency planning (bank budget, slot capacity, tensor counts, zero-free fallback) | Goose | EXP-150, gated against the native policy on 30296 scenarios |
| cache admission and grouping (which layers, which banks, host bytes, enable/disable) | Goose, live | EXP-262 module, EXP-265 integration; A/B `FG262A_MOE_CACHE_ADMIT plans=1` vs `0`, identical output |
| upload-copy run coalescing and stats classification | Goose, live | EXP-263 module, EXP-264 integration; A/B `runs=15855 pairs=46221 stats=5200` vs zeros, identical output |
| "staging" ring (pinned slots, upload grouping, ring depth) | **absent from the substrate** | recorded by the audit: no grouping path in flint `ggml-backend.cpp`, `GGML_EXPERT_PINNED_RING` not in the build |
| "scatter" | live code is the tiled expert path (`ggml-cpu/tiled/tiled.cpp`) plus `build_moe_ffn` in `llama-graph.cpp`; the tiled control/pool/executor half is Goose (FG143/147/149), the MoE graph decisions are Goose (APEX.MODEL_GRAPH) | P1-09 inventory; APEX.MODEL_GRAPH acceptance |
| allocation failure / resize / empty / overlapping / cancellation checks | covered where they exist: pool plans refuse (EXP-260 fixture, 51/0), cache admission refuses (EXP-265 throws instead of a partial cache), splitter and cancellation paths are covered by their own items | EXP-260, EXP-265 |

Also stale: the requirement's source list names `src/llama-expert-*.cpp`; no such file exists in this substrate
(`find` over the flint tree), so that part of the requirement cannot be ported as written.

## What remains native, and why that is not a porting gap

- tensor, bank and view construction in `llama-moe-cache.cpp` -- ggml calls plus a loop over the tensors the plan
  already selected;
- the copy call sequence -- one `ggml_backend_tensor_set_async` per run the module computed;
- the allocations themselves.

These are the same class as `cuMemMap`, `cudaMallocHost` and `memcpy` elsewhere: primitives, not decisions. The
decisions around them have modules and gates.

## Proposed restated criterion (owner decision)

> P1-10 (restated): Goose owns the decisions of expert scratch, residency and cache lifecycles -- scratch growth and
> reuse policy, bank budget and slot capacity, admission and grouping, upload-copy run coalescing and its statistics,
> with the refusal paths (allocation failure, empty admission, capacity exceeded) implemented and gated. The
> allocation, buffer and copy calls themselves remain primitives. The pinned-ring staging clause is retired because the
> substrate has no ring; the `llama-expert-*.cpp` reference is retired because no such file exists here.

With this restatement the item is closed by the evidence already collected. Without it, the item can never close,
because two of its clauses describe code and mechanisms that are not in this tree.

## If the owner prefers the other option

Reintroducing a pinned staging ring is a deliberate mechanism change: it needs its own design, its own failure cases and
a new Apex regression. It is not implied by the current runtime and the accepted profile does not use it.
