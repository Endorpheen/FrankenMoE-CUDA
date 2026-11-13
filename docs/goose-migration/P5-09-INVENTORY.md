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

## Scope of wiring the shift modules into the live cache

Checked before starting, so the cost is known:

1. **The cell storage is private.** `llama_kv_cells` keeps `pos`, `ext`, `shift` and `seq` in a private section
   (`llama-kv-cells.h:493-520`) behind public per-cell accessors. A Goose module that receives flat arrays therefore
   needs either a small non-invasive copy-in/copy-out bridge or new accessors on the class. The copy is O(n_cells)
   `memcpy` per shift, and shifts are rare, so the copy is the cheaper and safer option: it leaves the class API and its
   invariants untouched.
2. **The accepted profile never shifts.** Its argv is `-c 262144` with a 32 768-token prompt, so `seq_add` is not
   reached. A live test needs a smaller context on the test server (for example `-c 4096` with a ~5 000-token prompt),
   which is a test-server change, not a profile change.
3. **The comparison for that test cannot be `e68df05b`.** A shifted context legitimately changes the answer. The gate has
   to be an A/B on one DSO -- `GGML_GOOSE_KV_SHIFT=1` versus `0` -- with identical content, tokens and probabilities,
   plus the shift witness non-zero only in the Goose arm.

So the remaining work is: copy-in/copy-out bridge, call sites behind a default-on switch, relink, and an A/B at a small
context. That is the same shape as EXP-264/265, with the extra first step of marshalling the cell arrays.

## Decision 2026-11-12: shift wiring deferred, with the cost written down

The integration is not a copy of the EXP-264/265 pattern. The module EXP-327 publishes a *journal* (cells shifted, head
written, per-sequence index unlinked/relinked), and replaying it needs access to three private structures: `pos`,
`shift`, and the `seq_pos` index (`llama-kv-cells.h:493-532`, `std::set<std::pair<llama_pos, uint32_t>>` per sequence).
A faithful integration therefore needs new accessors on `llama_kv_cells` plus either journal replay or an index rebuild
after the call -- a change to a core class, not a call-site swap.

Against that cost: the path is dormant on the accepted profile (`-c 262144`, 32K prompt, no shift), so the work would
not change any measured Apex result. The decision is to defer it until the profile that exercises shift (a small-context
configuration) is part of an accepted setup, rather than change a core class for a path the product does not run.

What would unban it: an agreed small-context profile, or a decision that shift ownership is required for a future
configuration. Both are owner decisions; the module itself is gated (EXP-327, 8/8 mutations) and stays ready.
