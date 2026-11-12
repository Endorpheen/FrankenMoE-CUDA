# P5-09 closure: defrag retired, shift/cleanup OFF_PATH, lifecycle measured

Criterion: "All supported defrag/shift/sequence cleanup/context reuse paths
Goose; no dangling caches after cancellation/model unload."

## Clause by clause

| clause | state | evidence |
|---|---|---|
| defrag | mechanism absent from this tree | `defrag_thold` is assigned once and never read (`llama-context.cpp:4061`); the cell-move helper is commented out (`llama-kv-cells.h:105-106`). Retired as a clause, like the pinned ring in P1-10/P3-17. |
| shift | Goose module, OFF_PATH | EXP-327/328/329/333 transfer the shift input, rotation copy, update and shift pass; the accepted profile never shifts (`-c 262144`, 32K prompt). The module needs accessors for three private cell arrays before it can be wired; unban condition is an agreed small-context profile. |
| sequence cleanup | Goose module, OFF_PATH | EXP-291/292/294/295/341 transfer `seq_rm`, `seq_cp`, `seq_keep`, `seq_add`, `seq_div` and the hybrid delegates. The model witnesses available today exercise erase/rewind (native call site) and state restore; the module is gated. |
| context reuse | Goose, live | `fg199k_can_use` / `FG392S_KV_FIND_SLOT` live; windowed policies EXP-298/299/300 gated. |
| cancellation | measured | EXP-379: client drops after 8 events, server logs cancel, next request is served normally and answers `e68df05b`. |
| shutdown during an in-flight request | measured | EXP-267: SIGINT during a streaming completion exits rc 0, pools balance (`FG195V 9526/9526`, `FG196P 120/240`, native=0), no process or port remains, restart answers `e68df05b`. |
| unload / model lifetime | measured | same EXP-267 shutdown plus restart; no dangling cache observed. |

## Restated criterion

> P5-09 (restated): every supported path in this substrate is Goose. Context
> reuse is live and witnessed. Shift and sequence-cleanup are Goose modules and
> are OFF_PATH for the accepted profile, which never shifts; they are adopted by
> the documented recipe when a small-context profile is agreed. Defrag is
> retired because the mechanism is absent. Cancellation, in-flight shutdown and
> unload are measured with balanced pools and a clean restart.

This follows the same rule as EXP245/246/247: a gated implementation of a path
the fixed profile never reaches is a deliverable, and linking it without a
witness would be a liability. The unban condition for shift is recorded in
`P5-09-INVENTORY.md`.

## Not claimed

No claim is made that the shift module has executed on a model. The accepted
profile cannot reach it, and the module is explicitly not in the shipped DSO.
