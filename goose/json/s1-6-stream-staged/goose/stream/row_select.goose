// P9-04 S3: which tables become a row-streamed source, and what the hook does
// with one at run time. From runtime/core/src/moe/router_hook.cpp (capture and
// dispatch), runtime/core/src/moe/row_stream.cpp (takeover acceptance) and
// runtime/core/src/engine/session.cpp (the dense/row split).
//
// The row policy is not a list of names and not a pattern. It is a verdict the
// hook reaches by watching the graph, a filter the session applies to that
// verdict, and an acceptance test the source applies to each candidate:
//
//   verdict   1405-1437  a table qualifies only when EVERY reference to it is a
//                        row gather at the table position, and only when the
//                        index is already materialized when the node runs
//             173-193    index_materialized: a graph input, or a pure view of
//                        one, up to eight hops; anything else is a node that may
//                        not have run yet
//             195-200    the answer is the set difference: gathered MINUS
//                        disqualified, so one bad reference vetoes the table
//   split     1051-1082  the session filters the captured weight leaves: expert
//                        tensors are the stream's, graph inputs and KV are not
//                        gguf tensors, CUDA-resident weights cannot be rebound,
//                        and a table is never both resident and streamed
//   takeover  45-80      a tensor with no size, no rows or no row width is left
//                        alone; a bad shard index or a failed reservation is
//                        fatal for the whole policy
//   dispatch  1446-1464  at run time the gather path needs a readable I32 index;
//                        anything else — an unserved table, a non-gather node —
//                        either does nothing or pulls the whole table in
//
// Not here: the slab plan, the eviction guard and the materialize size limit
// (EXP621's sp_ slice), the dense placement policy (dw_), and the mmap/rebind
// syscalls. The fixture supplies node kinds, references and tensor shapes.

// Node kinds for the index chain, mirroring the ggml ops the native walk admits.
const RS_OP_NONE: i64 = 0;
const RS_OP_VIEW: i64 = 1;
const RS_OP_RESHAPE: i64 = 2;
const RS_OP_PERMUTE: i64 = 3;
const RS_OP_TRANSPOSE: i64 = 4;
const RS_OP_OTHER: i64 = 5;

// Dispatch outcomes.
const RS_NONE: i64 = 0;
const RS_GATHER: i64 = 1;
const RS_MATERIALIZE: i64 = 2;
const RS_SCAN: i64 = 3; // the non-gather arm: every served source is materialized

// Takeover outcomes.
const RS_SKIP_EMPTY: i64 = 0;
const RS_SKIP_SHAPE: i64 = 1;
const RS_TAKE: i64 = 2;
const RS_FATAL_SHARD: i64 = 3;
const RS_FATAL_RESERVE: i64 = 4;

// router_hook.cpp:173-193 — index_materialized. Is the index readable the moment
// the node that consumes it is offered to the callback? A graph input is, and so
// is a pure view of one, because a view shares its source's memory and no
// computation stands between the two. The walk is bounded at eight hops, as the
// native guard is: a deeper chain is not a shape this decides.
//
// `kinds[i]` is node i's op and `types[i]` is 1 when node i is I32; index 0 is
// the index tensor itself, and kind RS_OP_NONE with a missing entry means the
// chain ended (a null source).
export fn rs_index_materialized(kinds: const i64[:], types: const i64[:]) -> i64 {
    var i: i64 = 0;
    while i < 8 {
        if i >= kinds.len {
            return 0; // nullptr src: nothing to read
        }
        let op: i64 = kinds[i];
        if op == RS_OP_NONE {
            if i < types.len && types[i] != 0 {
                return 1; // an index we can actually read as row numbers
            }
            return 0;
        }
        if op == RS_OP_VIEW || op == RS_OP_RESHAPE || op == RS_OP_PERMUTE || op == RS_OP_TRANSPOSE {
            i += 1; // pure view of the same memory: keep walking
        } else {
            return 0; // produced by a node that may not have run yet
        }
    }
    return 0; // the native guard ran out
}

// router_hook.cpp:1425-1426 — a captured source that is a weight LEAF (op NONE,
// named) and not an expert is a dense-rebind candidate. Graph inputs and KV
// tensors share that shape and are filtered out later by the gguf tensor set,
// so recording them here is harmless — which is why the admission test is only
// "named leaf".
export fn rs_leaf_admit(op_none: i64, named: i64) -> i64 {
    if op_none != 0 && named != 0 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1427-1433 — one reference to a weight. Only the TABLE position
// of a row gather counts, and only when the index is already in memory. Every
// other reference is a veto: recorded as gathered nowhere, disqualified here.
export fn rs_ref_verdict(get_rows: i64, slot: i64, idx_ok: i64) -> i64 {
    if get_rows != 0 && slot == 0 && idx_ok != 0 {
        return 1; // gathered
    }
    return 2; // disqualified
}

// router_hook.cpp:195-200 — the table's verdict over the whole capture: the
// difference of the two sets. A table seen once as a gather and once as
// anything else is not row-streamed; the veto is the point of the exercise,
// because a multiply needs the whole table every token.
export fn rs_table_verdict(gathered_refs: i64, disq_refs: i64) -> i64 {
    if gathered_refs > 0 && disq_refs == 0 {
        return 1; // row-gathered
    }
    return 0; // keeps the dense policy it had
}

// session.cpp:1060-1078 — the session's filter over the captured leaves, in the
// native order. 0 skip (expert: the stream's), 1 skip (no tensor or no buffer),
// 2 skip (not a host-rebindable weight), 3 skip (not a file tensor), 4 dense
// only, 5 dense AND row-streamed.
export fn rs_session_take(is_expert: i64, has_tensor: i64, has_buffer: i64, host_ok: i64,
                          in_gguf: i64, row_wanted: i64) -> i64 {
    if is_expert != 0 {
        return 0;
    }
    if has_tensor == 0 || has_buffer == 0 {
        return 1;
    }
    if host_ok == 0 {
        return 2;
    }
    if in_gguf == 0 {
        return 3;
    }
    if row_wanted != 0 {
        return 5;
    }
    return 4;
}

// session.cpp:1080 — the window the row policy may hold. A negative request is
// not a ceiling: it is clamped to zero and then to the module's minimum.
export fn rs_row_budget(mb: i64) -> i64 {
    var m: i64 = mb;
    if m < 0 {
        m = 0;
    }
    return m * 1048576;
}

// row_stream.cpp:45-61 — takeover acceptance for one candidate, in the native
// order: an unnamed or sizeless tensor is skipped silently, a shard index
// outside the readers is fatal for the whole policy, a tensor with no row width
// or no rows is not row-shaped and is left alone, and a reservation that fails
// is fatal. The policy never leaves a tensor half-owned: a failure anywhere
// releases everything.
export fn rs_init_accept(present: i64, size: i64, file_ok: i64, row_bytes: i64,
                         n_rows: i64, reserve_ok: i64) -> i64 {
    if present == 0 || size <= 0 {
        return RS_SKIP_EMPTY;
    }
    if file_ok == 0 {
        return RS_FATAL_SHARD;
    }
    if row_bytes == 0 || n_rows == 0 {
        return RS_SKIP_SHAPE;
    }
    if reserve_ok == 0 {
        return RS_FATAL_RESERVE;
    }
    return RS_TAKE;
}

// row_stream.cpp:74-78 — nothing qualified after the shape check: the readers
// are dropped and init still succeeds. The hook then costs exactly nothing per
// node, which is what makes "the graph had no row gather" a supported outcome
// rather than a failure.
export fn rs_policy_on(taken: i64) -> i64 {
    if taken > 0 {
        return 1;
    }
    return 0;
}

// router_hook.cpp:1446-1462 — the run-time dispatch of one node's source.
// `served` is what the row source answered for that pointer, and the two arms
// are the gather arm and the safety net for a graph shape the capture never saw.
// An index that is present but not a readable I32 is not a reason to fail: the
// whole table is pulled in instead, which is what materialize() is for.
export fn rs_dispatch(get_rows: i64, table_served: i64, ids_readable: i64) -> i64 {
    if get_rows != 0 {
        if table_served == 0 {
            return RS_NONE; // not one of ours
        }
        if ids_readable != 0 {
            return RS_GATHER;
        }
        return RS_MATERIALIZE;
    }
    if table_served != 0 {
        return RS_SCAN;
    }
    return RS_NONE;
}

// router_hook.cpp:1463 — `ok` is accumulated across the scan arm and checked
// once: any refused table aborts the graph, because the alternative is a node
// reading pages nobody committed.
export fn rs_prep_fatal(any_refused: i64) -> i64 {
    if any_refused != 0 {
        return 1;
    }
    return 0;
}

// row_stream.cpp:213-217 — how many slabs a materialize() has to pull: none for
// a table already whole, all of them otherwise. The size limit that can refuse
// the call is EXP621's sp_materialize_ok and is not repeated here.
export fn rs_materialize_need(whole: i64, n_slabs: i64) -> i64 {
    if whole != 0 {
        return 0;
    }
    if n_slabs < 0 {
        return 0;
    }
    return n_slabs;
}

// row_stream.cpp:233-241 — a materialized table leaves the LRU for the rest of
// the run: it is resident, and an eviction that took one of its slabs would put
// the run right back at the fault this call exists to avoid. Returns how many
// slabs leave the list.
export fn rs_whole_unlink(n_slabs: i64, in_lru: const u8[:]) -> i64 {
    var n: i64 = 0;
    var s: i64 = 0;
    while s < n_slabs && s < in_lru.len {
        if in_lru[s] != 0 {
            n += 1;
        }
        s += 1;
    }
    return n;
}
