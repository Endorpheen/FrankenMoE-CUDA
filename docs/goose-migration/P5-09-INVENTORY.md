# P5-09 inventory: defrag, shift, cleanup, reuse

Status: inventory. Stays TODO; one of its four clauses names a mechanism this tree does not have.

## Clause by clause

| clause | state here | evidence / proof |
|---|---|---|
| defrag | **absent from the substrate** | no defrag implementation in `src`: `defrag_thold` is assigned once in a default param struct and never read (`llama-context.cpp:4061`), and the cell-move helper the comment refers to is commented out (`llama-kv-cells.h:105-106`, `void mv(...)`). Upstream removed the defrag path; this tree is in that state. |
| shift | Goose modules, gated | `seq_add` (EXP-327), `seq_div` and the position extrema (EXP-328), `set_input_k_shift` + rotation copy (EXP-333), the `update` transaction with the `can_shift` guard (EXP-329). Live path exists (`get_has_shift`, `do_shift` in `llama-kv-cache.cpp:808`), no model witness yet |
| sequence cleanup | Goose modules, gated | `seq_rm` (EXP-291/295), `seq_keep`, `seq_cp` same and cross stream (EXP-291/292), recurrent variants (EXP-294/295), hybrid-idx delegates (EXP-341) |
| context reuse | Goose, live | slot search `fg199k`/FG392S live in the accepted runtime; SWA/MSA/DSA cache policies gated (EXP-298/299/300) |
| no dangling caches after cancellation | measured | EXP-379: drop a streaming client after 8 events, the server logs `cancel task`, the next request is served normally with `e68df05b` |
| no dangling caches after model unload | clean shutdown measured, unload under load not | the rewind run (EXP-266) ended with a SIGINT and balanced teardown in its own log: `FG195V alloc=16472 free=16472 native=0`, `FG196P reg_plan=120 unreg=240 native=0`. A shutdown while a request is in flight is not tested |

## What this means

"Defrag" cannot be ported: there is no defrag path to own. The live maintenance work this substrate does is shift,
sequence cleanup and reuse, and those have Goose modules already -- gated, not yet witnessed on a model run.

The two lifecycle clauses are asymmetric: cancellation is measured, unload is argued from the balanced teardown
counters but not exercised as a scenario of its own.

## Proposed restated criterion (owner decision)

> P5-09 (restated): every live KV maintenance path is Goose-owned -- shift (sequence offset, division, input shift and
> rotation), sequence cleanup (remove/keep/copy, same and cross stream, recurrent and hybrid delegates) and context
> reuse (slot search, windowed policies) -- witnessed on a model run, with no dangling cache after cancellation or
> unload. The defrag clause is retired: the substrate has no defrag implementation (`defrag_thold` unread,
> `llama_kv_cells::mv` commented out).

## What closing it needs

1. A model run that exercises shift and cleanup: add a sequence, shift positions, remove a range, then complete, and
   require the same tokens and probabilities as a from-scratch run. The rewind run (EXP-266) covers removal only.
2. A near-capacity run (shared with P5-07) that reaches the cache bound and then reuses.
3. An unload scenario: shut a server down while a request is in flight and after a completed one, then check the pool
   teardown counters and that no cache file or buffer survives.
