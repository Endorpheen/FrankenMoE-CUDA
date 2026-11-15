# P4-13 — allocation and scheduler interface unification (decision note)

Status: **owner decision required**. The allocator criteria are closed and the
streaming/hook decisions are ported; the two lineages still expose different
entry points.

## What exists

- P4-05/06/07 closed the allocator and backend-interface criteria
  (`P4-ALLOC-BACKEND-CLOSURE.md`, EXP161/162/163): tallocr/dyn_tallocr, graph
  reservation and reuse, provider interface, error and bounds cases.
- P4-12 closed the meta backend, registry identity, threading and the ggml-opt
  optimizer (EXP617/EXP624).
- P9-03/P9-04 ported expert LRU, dense placement, row-stream planning, router
  hooks, overlap waves and row-source selection.

## What differs

The two lineages (the working Apex tree and the reconstructed/candidate tree)
expose different block-table ownership and eviction callback shapes. A single
interface must decide:

1. who owns the block tables (allocator or stream source);
2. whether eviction is a callback or a return value;
3. whether the scheduler sees the same block size on both paths.

## Options

A. **Allocator owns the block tables; stream source calls in.** Smallest change
   to the accepted allocator gates; the stream source loses its current
   bookkeeping.

B. **Stream source owns them; allocator receives plans.** Matches the ported
   `sp_`/`xl_` contracts; the allocator gates need an adapter.

C. **Split: tables in the allocator, eviction policy in the stream source.**
   Cleanest conceptually, largest amount of code to touch, and it needs a new
   differential gate for the boundary.

## Recommendation

Option B: it keeps the already-accepted streaming contracts (`xl_touch` takes a
committed access, `xl_set_budget` returns the budget, the plan is ordered) and
confines the change to the allocator adapter. It still needs a differential gate
on the adapter before any call site is switched.

No code change is proposed by this note.
