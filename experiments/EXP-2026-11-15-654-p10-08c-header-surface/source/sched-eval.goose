// P10-08 S9: the scheduler eval callback sequence and the callback contracts
// around it.
//
// Native sources at the base commit (upstream/llama.cpp 0b5be7e4a):
//   ggml/include/ggml-backend.h:309-316   ggml_backend_sched_eval_callback
//   ggml/src/ggml-backend.cpp:1801-1838  the ask/observe/cancel loop
//   ggml/include/ggml-backend.h:422      ggml_backend_eval_callback
//   include/llama.h:244                  llama_progress_callback
//   src/llama-model-loader.cpp:1570-1574 the data-progress call and its abort
//   src/llama-model-loader.cpp:1722-1726 the final 1.0 call after the load
//   ggml/include/ggml.h:640-647          enum ggml_log_level
//   ggml/include/ggml.h:2845-2850        ggml_log_callback and ggml_log_set
//
// What is frozen is the sequence, not the typedef: the typedef shape was
// already frozen by the bkh_ slice, and it is repeated here only so the
// sequence and its callback stand together.
//
// The loop over one split graph is a small exact machine:
//   - the scheduler asks about nodes in index order, starting at j0, and keeps
//     asking while the answer is "do not observe";
//   - it computes the contiguous range it has reached, then synchronizes once;
//   - only when a node was requested does it call back with ask=false, and only
//     then can the callback stop the rest of the split;
//   - asking a node and saying "no" is not a cancellation: a graph where the
//     callback rejects every node is computed in one chunk with no observations;
//   - a cancelled split keeps the chunks already computed and asks nothing more;
//   - an empty split calls back zero times.
//
// sched_run is that machine. The fixture transcribes the C loop verbatim from
// ggml-backend.cpp and compares event for event over every want-mask up to six
// nodes and every cancellation point, so the port cannot pass by being
// plausible.
//
// The log and progress arms freeze the observable decisions around it: a null
// log callback routes to stderr, a null progress callback is never called, a
// progress callback that returns false aborts the load at that point, and a
// successful load ends with one final call at 1.0.
//
// The prefix is sched_.

import std;

const SCHED_NONE: i64 = 0 - 1;
const SCHED_ASK: i64 = 0;
const SCHED_OBSERVE: i64 = 1;
const SCHED_MAX_NODES: i64 = 32;

// Status of sched_run and sched_progress_status.
const SCHED_OK: i64 = 0;
const SCHED_CANCELLED: i64 = 1;
const SCHED_INVALID: i64 = 0 - 1;

// ── the two callback shapes, for context ───────────────────────────────────
// 0: ggml_backend_sched_eval_callback(t, ask, user_data) -> bool
// 1: ggml_backend_eval_callback(node_index, t1, t2, user_data) -> bool
const SCHED_CB_N: i64 = 2;

const SCHED_CB_NAME: const u8[:][2] = [
    "ggml_backend_sched_eval_callback",
    "ggml_backend_eval_callback"
];

const SCHED_CB_ARITY: i64[2] = [3, 4];
const SCHED_CB_RET: i64[2] = [1, 1];

// Five parameter slots, padded -1. Classes as in the bkh_ slice.
const SCHED_CB_PAR: i64[10] = [
    5, 1, 5, 0 - 1, 0 - 1,
    2, 5, 5, 5, 0 - 1
];

// ── log levels ─────────────────────────────────────────────────────────────
const SCHED_LEVEL_N: i64 = 6;

const SCHED_LEVEL_NAME: const u8[:][6] = [
    "GGML_LOG_LEVEL_NONE",
    "GGML_LOG_LEVEL_DEBUG",
    "GGML_LOG_LEVEL_INFO",
    "GGML_LOG_LEVEL_WARN",
    "GGML_LOG_LEVEL_ERROR",
    "GGML_LOG_LEVEL_CONT"
];

const SCHED_LEVEL_VAL: i64[6] = [0, 1, 2, 3, 4, 5];

// ggml_log_callback(level, text, user_data) -> void.
const SCHED_LOG_CB_ARITY: i64 = 3;
const SCHED_LOG_CB_VOID: i64 = 1;

fn sched_copy(out: u8[:], s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && n < out.len {
        out[n] = s[n];
        n = n + 1;
    }
    return n;
}

// Bit i of mask, without relying on the shift operator.
fn sched_bit(mask: i64, i: i64) -> i64 {
    var m: i64 = mask;
    var k: i64 = 0;
    while k < i {
        m = m / 2;
        k = k + 1;
    }
    return m % 2;
}

fn sched_push(ev: i64[:], n: i64, v: i64) -> i64 {
    if n < ev.len {
        ev[n] = v;
    }
    return n + 1;
}

// ---- exports --------------------------------------------------------------

// Runs the scheduler's eval loop over one split graph of n nodes.
//
//   want_mask   bit i set: the caller wants to observe node i
//   cancel_at   index whose observe callback returns false, or -1
//
// Writes into ev[1..] the callback events as index * 2 + phase (0 ask,
// 1 observe), and into comp[1..] the computed ranges as start * 256 + end
// (end exclusive). ev[0] and comp[0] receive the counts. Returns SCHED_OK,
// SCHED_CANCELLED, or SCHED_INVALID for arguments outside the contract.
export fn sched_run(n: i64, want_mask: i64, cancel_at: i64, ev: i64[:], comp: i64[:]) -> i64 {
    if n < 0 || n > SCHED_MAX_NODES || ev.len < 1 || comp.len < 1 {
        return SCHED_INVALID;
    }
    if cancel_at < 0 - 1 || cancel_at >= n {
        return SCHED_INVALID;
    }
    var ne: i64 = 1;
    var nc: i64 = 1;
    var status: i64 = SCHED_OK;
    var j0: i64 = 0;
    while j0 < n {
        var need: i64 = sched_bit(want_mask, j0);
        ne = sched_push(ev, ne, j0 * 2 + SCHED_ASK);
        var j1: i64 = j0;
        while need == 0 && j1 < n - 1 {
            j1 = j1 + 1;
            need = sched_bit(want_mask, j1);
            ne = sched_push(ev, ne, j1 * 2 + SCHED_ASK);
        }
        nc = sched_push(comp, nc, j0 * 256 + (j1 + 1));
        if need == 1 {
            ne = sched_push(ev, ne, j1 * 2 + SCHED_OBSERVE);
            if cancel_at == j1 {
                status = SCHED_CANCELLED;
                j0 = n;
            } else {
                j0 = j1 + 1;
            }
        } else {
            j0 = j1 + 1;
        }
    }
    ev[0] = ne - 1;
    comp[0] = nc - 1;
    return status;
}

export fn sched_max_nodes() -> i64 {
    return SCHED_MAX_NODES;
}

export fn sched_phase_ask() -> i64 {
    return SCHED_ASK;
}

export fn sched_phase_observe() -> i64 {
    return SCHED_OBSERVE;
}

// An ask that answers "do not observe" is not a cancellation.
export fn sched_ask_false_cancels() -> i64 {
    return 0;
}

// One synchronize per computed chunk, not per node.
export fn sched_sync_per_chunk() -> i64 {
    return 1;
}

export fn sched_cb_count() -> i64 {
    return SCHED_CB_N;
}

export fn sched_cb_name(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SCHED_CB_N {
        return SCHED_NONE;
    }
    return sched_copy(out, SCHED_CB_NAME[id]);
}

export fn sched_cb_shape(id: i64, out: i64[:]) -> i64 {
    if id < 0 || id >= SCHED_CB_N {
        return SCHED_NONE;
    }
    var n: i64 = 0;
    if n < out.len {
        out[n] = SCHED_CB_ARITY[id];
        n = n + 1;
    }
    if n < out.len {
        out[n] = SCHED_CB_RET[id];
        n = n + 1;
    }
    var j: i64 = 0;
    while j < SCHED_CB_ARITY[id] && n < out.len {
        out[n] = SCHED_CB_PAR[id * 5 + j];
        n = n + 1;
        j = j + 1;
    }
    return n;
}

export fn sched_level_count() -> i64 {
    return SCHED_LEVEL_N;
}

export fn sched_level_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= SCHED_LEVEL_N {
        return SCHED_NONE;
    }
    return sched_copy(out, SCHED_LEVEL_NAME[i]);
}

export fn sched_level_value(i: i64) -> i64 {
    if i < 0 || i >= SCHED_LEVEL_N {
        return SCHED_NONE;
    }
    return SCHED_LEVEL_VAL[i];
}

export fn sched_log_cb_arity() -> i64 {
    return SCHED_LOG_CB_ARITY;
}

export fn sched_log_cb_void() -> i64 {
    return SCHED_LOG_CB_VOID;
}

// Where a log line goes: 0 the registered callback, 1 stderr. ggml_log_set
// with a null callback, or no call at all, leaves everything on stderr; a
// non-null callback receives it instead.
// ggml_log_set(nullptr) does not leave the slot empty: the library reinstalls its
// built-in default callback, which writes to stderr. The slot is therefore
// never null after any call, and a null argument is a request for the default,
// not a clear.
export fn sched_log_null_restores_default() -> i64 {
    return 1;
}

export fn sched_log_route(set_called: i64, null_cb: i64) -> i64 {
    if set_called != 0 && null_cb == 0 {
        return 0;
    }
    return 1;
}

// Progress callbacks during a weight load.
//
//   has_cb   0 when llama_model_params::progress_callback is null
//   n_events number of tensor-progress points the loader reaches
//   abort_at index that returns false, or -1
//
// Returns the number of progress callback invocations, or -1 for arguments
// outside the contract. The contract: a null callback is never invoked; each
// point is invoked once with progress in [0, 1]; false stops the load there;
// a load that is not stopped ends with one extra call at exactly 1.0.
export fn sched_progress_calls(has_cb: i64, n_events: i64, abort_at: i64) -> i64 {
    if n_events < 0 || abort_at < 0 - 1 || abort_at >= n_events {
        return SCHED_INVALID;
    }
    if has_cb == 0 {
        return 0;
    }
    if abort_at >= 0 {
        return abort_at + 1;
    }
    return n_events + 1;
}

// 0 when the load completes, 1 when a progress callback stopped it, -1 invalid.
export fn sched_progress_status(has_cb: i64, n_events: i64, abort_at: i64) -> i64 {
    if n_events < 0 || abort_at < 0 - 1 || abort_at >= n_events {
        return SCHED_INVALID;
    }
    if has_cb == 0 {
        return SCHED_OK;
    }
    if abort_at >= 0 {
        return SCHED_CANCELLED;
    }
    return SCHED_OK;
}

// The final call is at 1.0 and is made even when the load had no tensor
// progress points, as long as a callback is installed.
export fn sched_progress_final_value() -> f64 {
    return 1.0;
}

// The final call is skipped when an earlier callback aborted the load.
export fn sched_progress_final_after_abort() -> i64 {
    return 0;
}
