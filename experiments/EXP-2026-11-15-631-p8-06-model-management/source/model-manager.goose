// Model manager state and LRU scheduling (P8-06 S1) in Goose, from
// tools/server/server-models.cpp and tools/server/server-models.h.
//
// Owned decisions, in the native order:
//   1. status/source names as /v1/models and the SSE stream spell them;
//   2. the four meta predicates (is_ready, is_running, is_ready_or_sleep,
//      is_failed);
//   3. what unload() does for each status: cancel a download, stop a running
//      child with the exit command, force-kill a child that is still coming up,
//      or nothing at all;
//   4. remove()'s two admission gates and its wait predicate;
//   5. request_stop()'s "already mapped / already stopping" gate;
//   6. update_status(): the fields it writes, when it drops the stop mark, and
//      which members the SSE data carries;
//   7. the architecture modality merge from a child's loaded_info: a value that
//      is not an array of strings changes nothing;
//   8. the scheduler: capacity, the LRU victim walk over the name-sorted
//      mapping, and tick()'s eviction count.
//
// The mapping is data: `names` is one buffer and `off`/`ln` address one model
// name each. Rows arrive in insertion order; every walk the native code takes
// over `std::map` is reproduced by sorting the row indices by name, exactly
// like the red-black tree the native code iterates.
//
// Status codes are the native enum: 0 downloading, 1 incomplete, 2 downloaded,
// 3 unloaded, 4 loading, 5 loaded, 6 sleeping.
// Source codes are the native enum: 0 preset, 1 models_dir, 2 cache.

import std;

const MG_MAX: i64 = 64;

const MG_DOWNLOADING: i64 = 0;
const MG_INCOMPLETE: i64 = 1;
const MG_DOWNLOADED: i64 = 2;
const MG_UNLOADED: i64 = 3;
const MG_LOADING: i64 = 4;
const MG_LOADED: i64 = 5;
const MG_SLEEPING: i64 = 6;

const MG_SRC_PRESET: i64 = 0;
const MG_SRC_MODELS_DIR: i64 = 1;
const MG_SRC_CACHE: i64 = 2;

// ── byte helpers ───────────────────────────────────────────────────────────

fn mg_put_range(dst: u8[:], at: i64, s: const u8[:], from: i64, n: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < n {
        if p < dst.len {
            dst[p] = s[from + i];
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn mg_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return mg_put_range(dst, at, s, 0, s.len);
}

fn mg_putc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn mg_put_i(dst: u8[:], at: i64, v: i64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;
    var q = at;

    if x < 0 {
        q = mg_putc(dst, q, 45);
        x = 0 - x;
    }
    if x == 0 {
        return mg_putc(dst, q, 48);
    }
    while x > 0 {
        buf[n] = (48 + x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        q = mg_putc(dst, q, buf[i]);
    }
    return q;
}

// the byte order std::string comparison uses, so the row walk matches std::map
fn mg_cmp(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> i64 {
    var n = al;

    if bl < n {
        n = bl;
    }
    var i: i64 = 0;

    while i < n {
        if a[ao + i] < b[bo + i] {
            return 0 - 1;
        }
        if a[ao + i] > b[bo + i] {
            return 1;
        }
        i = i + 1;
    }
    // a proper prefix sorts first
    if al < bl {
        return 0 - 1;
    }
    if al > bl {
        return 1;
    }
    return 0;
}

// indices 0..n-1 in std::map (byte-lexicographic) name order
fn mg_sort_rows(n: i64, names: const u8[:], off: i64[:], ln: i64[:], perm: i64[:]) -> i64 {
    var i: i64 = 0;

    while i < n {
        var j = i;
        var placed: i64 = 0;

        while j > 0 && placed == 0 {
            let a = perm[j - 1];
            let b = i;

            if mg_cmp(names, off[a], ln[a], names, off[b], ln[b]) <= 0 {
                placed = 1;
            } else {
                perm[j] = a;
                j = j - 1;
            }
        }
        perm[j] = i;
        i = i + 1;
    }
    return n;
}

fn mg_eq(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    return mg_cmp(a, ao, al, b, bo, bl) == 0;
}

// ── names and predicates ───────────────────────────────────────────────────

// server-models.h 41-53
fn mg_status_str_body(st: i64, out: u8[:]) -> i64 {
    if st == MG_DOWNLOADING {
        return mg_puts(out, 0, "downloading");
    }
    if st == MG_INCOMPLETE {
        return mg_puts(out, 0, "incomplete");
    }
    if st == MG_DOWNLOADED {
        return mg_puts(out, 0, "downloaded");
    }
    if st == MG_UNLOADED {
        return mg_puts(out, 0, "unloaded");
    }
    if st == MG_LOADING {
        return mg_puts(out, 0, "loading");
    }
    if st == MG_LOADED {
        return mg_puts(out, 0, "loaded");
    }
    if st == MG_SLEEPING {
        return mg_puts(out, 0, "sleeping");
    }
    return mg_puts(out, 0, "unknown");
}

export fn mg_status_str(st: i64, out: u8[:]) -> i64 {
    return mg_status_str_body(st, out);
}

// server-models.h 55-63
fn mg_source_str_body(src: i64, out: u8[:]) -> i64 {
    if src == MG_SRC_PRESET {
        return mg_puts(out, 0, "preset");
    }
    if src == MG_SRC_MODELS_DIR {
        return mg_puts(out, 0, "models_dir");
    }
    if src == MG_SRC_CACHE {
        return mg_puts(out, 0, "cache");
    }
    return mg_puts(out, 0, "unknown");
}

export fn mg_source_str(src: i64, out: u8[:]) -> i64 {
    return mg_source_str_body(src, out);
}

fn mg_is_ready(st: i64) -> i64 {
    if st == MG_LOADED {
        return 1;
    }
    return 0;
}

// loaded, loading or sleeping: it holds a models_max slot
fn mg_is_running(st: i64) -> i64 {
    if st == MG_LOADED || st == MG_LOADING || st == MG_SLEEPING {
        return 1;
    }
    return 0;
}

fn mg_is_ready_or_sleep(st: i64) -> i64 {
    if st == MG_LOADED || st == MG_SLEEPING {
        return 1;
    }
    return 0;
}

// unloaded with a non-zero exit code
fn mg_is_failed(st: i64, exit_code: i64) -> i64 {
    if st == MG_UNLOADED && exit_code != 0 {
        return 1;
    }
    return 0;
}

// server-models.h 105-124
// out[0..3] = is_ready, is_running, is_ready_or_sleep, is_failed
fn mg_pred_body(status: i64, exit_code: i64, out: i64[:]) -> i64 {
    out[0] = mg_is_ready(status);
    out[1] = mg_is_running(status);
    out[2] = mg_is_ready_or_sleep(status);
    out[3] = mg_is_failed(status, exit_code);
    return 4;
}

export fn mg_pred(status: i64, exit_code: i64, out: i64[:]) -> i64 {
    return mg_pred_body(status, exit_code, out);
}

// ── unload() / remove() ────────────────────────────────────────────────────

// unload() 1518-1543
// out[0] action: 0 nothing, 1 cancel the download and wait, 2 stop the child,
//                 3 "not running" warning
// out[1] send_exit: the child gets the exit command on its command pipe
// out[2] terminate: force-kill first (only for a child that is still loading)
fn mg_unload_body(mapped: i64, status: i64, out: i64[:]) -> i64 {
    var action: i64 = 0;
    var send_exit: i64 = 0;
    var terminate: i64 = 0;

    if mapped != 0 {
        if status == MG_DOWNLOADING {
            action = 1;
        } else if mg_is_running(status) != 0 {
            action = 2;
            if status == MG_LOADING {
                terminate = 1;
                send_exit = 0;
            } else {
                send_exit = 1;
            }
        } else {
            action = 3;
        }
    }
    out[0] = action;
    out[1] = send_exit;
    out[2] = terminate;
    return 3;
}

export fn mg_unload_action(mapped: i64, status: i64, out: i64[:]) -> i64 {
    return mg_unload_body(mapped, status, out);
}

// remove() 1668-1712 refuses a name that is not mapped and one that is not from
// the cache.
// out[0] verdict: 0 accepted, 1 "not found", 2 "not removable (not from cache)"
fn mg_remove_body(mapped: i64, source: i64, out: i64[:]) -> i64 {
    var verdict: i64 = 0;

    if mapped == 0 {
        verdict = 1;
    } else if source != MG_SRC_CACHE {
        verdict = 2;
    }
    out[0] = verdict;
    return 1;
}

export fn mg_remove_gate(mapped: i64, source: i64, out: i64[:]) -> i64 {
    return mg_remove_body(mapped, source, out);
}

// request_stop() 1487-1494: only ask the monitor when the name is mapped and no stop is
// already in flight for it
export fn mg_stop_gate(mapped: i64, stopping: i64) -> i64 {
    if mapped == 0 {
        return 0;
    }
    if stopping != 0 {
        return 0;
    }
    return 1;
}

// The three waits the manager parks on, one predicate each:
//   0 unload() cancelling a download: the download left the DOWNLOADING state
//   1 unload_lru(): the evicted instance is UNLOADED
//   2 remove(): the instance is UNLOADED or its download finished
export fn mg_wait_pred(purpose: i64, status: i64) -> i64 {
    if purpose == 0 {
        if status != MG_DOWNLOADING {
            return 1;
        }
        return 0;
    }
    if purpose == 1 {
        if status == MG_UNLOADED {
            return 1;
        }
        return 0;
    }
    if status == MG_UNLOADED || status == MG_DOWNLOADED {
        return 1;
    }
    return 0;
}

// ── update_status() ────────────────────────────────────────────────────────

// update_status() 1571-1629
// out[0] applied status, or -1 when the name is not mapped (the meta is left
//        alone: native writes happen inside the mapping lookup)
// out[1] applied exit code, or -1 when not mapped
// out[2] the stop mark after the update
// out[3..5] the SSE data members: exit_code, info, progress
fn mg_update_body(mapped: i64, args_status: i64, args_exit: i64, has_info: i64, has_progress: i64, stopping: i64, out: i64[:]) -> i64 {
    var stop_mark = stopping;

    if args_status == MG_UNLOADED {
        stop_mark = 0;
    }
    if mapped != 0 {
        out[0] = args_status;
        out[1] = args_exit;
        out[2] = stop_mark;
    } else {
        out[0] = 0 - 1;
        out[1] = 0 - 1;
        out[2] = stopping;
    }
    // the SSE object is built outside the lookup, so it is sent either way
    out[3] = 0;
    if args_status == MG_UNLOADED {
        out[3] = 1;
    }
    out[4] = has_info;
    out[5] = has_progress;
    return 6;
}

export fn mg_update_fields(mapped: i64, args_status: i64, args_exit: i64, has_info: i64, has_progress: i64, stopping: i64, out: i64[:]) -> i64 {
    return mg_update_body(mapped, args_status, args_exit, has_info, has_progress, stopping, out);
}

// update_status() 1583-1604
// the modality merge from a child's loaded_info: the value replaces the current
// list only when it is an array whose every element is a string. An empty array
// counts as a valid value. `kind_state` is 0 absent, 1 an array, 2 a non-array.
export fn mg_arch_take(has_arch: i64, kind_state: i64, all_strings: i64) -> i64 {
    if has_arch == 0 {
        return 0;
    }
    if kind_state != 1 {
        return 0;
    }
    if all_strings == 0 {
        return 0;
    }
    return 1;
}

// ── the scheduler ──────────────────────────────────────────────────────────

// has_capacity 267-272: models_max slots and how many models hold one
export fn mg_capacity(models_max: i64, n_running: i64) -> i64 {
    if models_max <= 0 {
        return 1;
    }
    if n_running < models_max {
        return 1;
    }
    return 0;
}

// pick_victim 274-295: the row the LRU walk gives up, or -1 when every remaining
// model is busy, still
// coming up, already stopping or wanted by a queued request.
fn mg_victim_body(n: i64, names: const u8[:], off: i64[:], ln: i64[:],
                  status: i64[:], req_count: i64[:], last_used: i64[:],
                  stopping: i64[:], queued: i64[:]) -> i64 {
    var perm: i64[MG_MAX] = [0; MG_MAX];
    mg_sort_rows(n, names, off, ln, perm);

    var victim: i64 = 0 - 1;
    var victim_last_used: i64 = 0;
    var i: i64 = 0;

    while i < n {
        let r = perm[i];
        var skip: i64 = 0;

        if req_count[r] != 0 {
            skip = 1;
        }
        if mg_is_ready_or_sleep(status[r]) == 0 {
            skip = 1;
        }
        if stopping[r] != 0 {
            skip = 1;
        }
        if queued[r] != 0 {
            skip = 1;
        }
        if skip == 0 {
            if victim < 0 || last_used[r] < victim_last_used {
                victim = r;
                victim_last_used = last_used[r];
            }
        }
        i = i + 1;
    }
    return victim;
}

export fn mg_victim(n: i64, names: const u8[:], off: i64[:], ln: i64[:],
                    status: i64[:], req_count: i64[:], last_used: i64[:],
                    stopping: i64[:], queued: i64[:]) -> i64 {
    return mg_victim_body(n, names, off, ln, status, req_count, last_used, stopping, queued);
}

// tick 356-393: evict idle models while queued requests outnumber the slots that are
// free or being freed. The victims are written to `out` in eviction order and
// their stop mark is set, exactly as request_stop() does. Returns the number of
// victims. `qrow` holds the mapping row of a queue entry, -1 for a name the
// mapping does not know yet.
fn mg_tick_body(n: i64, names: const u8[:], off: i64[:], ln: i64[:],
                status: i64[:], req_count: i64[:], last_used: i64[:],
                stopping: i64[:], models_max: i64,
                nq: i64, qrow: i64[:], q_loading: i64[:], out: i64[:]) -> i64 {
    if models_max <= 0 {
        return 0;
    }
    if nq <= 0 {
        return 0;
    }

    var n_running: i64 = 0;
    var n_stopping: i64 = 0;
    var i: i64 = 0;

    while i < n {
        if mg_is_running(status[i]) != 0 {
            n_running = n_running + 1;
            if stopping[i] != 0 {
                n_stopping = n_stopping + 1;
            }
        }
        i = i + 1;
    }

    var n_needed: i64 = 0;
    var n_claimed: i64 = 0;
    var q: i64 = 0;

    while q < nq {
        if q_loading[q] == 0 {
            n_needed = n_needed + 1;
            q = q + 1;
        } else {
            let r = qrow[q];
            if r >= 0 && mg_is_running(status[r]) == 0 {
                n_claimed = n_claimed + 1;
            }
            q = q + 1;
        }
    }

    var n_free = models_max - n_running + n_stopping - n_claimed;
    var n_evicted: i64 = 0;

    while n_free < n_needed {
        // `find(m.first)` in pick_victim: any queue entry names this model
        var queued: i64[MG_MAX] = [0; MG_MAX];
        var k: i64 = 0;

        while k < nq {
            let r = qrow[k];
            if r >= 0 {
                queued[r] = 1;
            }
            k = k + 1;
        }
        let victim = mg_victim_body(n, names, off, ln, status, req_count, last_used, stopping, queued);
        if victim < 0 {
            return n_evicted; // every remaining model is busy
        }
        stopping[victim] = 1;
        if n_evicted < out.len {
            out[n_evicted] = victim;
        }
        n_evicted = n_evicted + 1;
        n_free = n_free + 1;
    }
    return n_evicted;
}

export fn mg_tick(n: i64, names: const u8[:], off: i64[:], ln: i64[:],
                  status: i64[:], req_count: i64[:], last_used: i64[:],
                  stopping: i64[:], models_max: i64,
                  nq: i64, qrow: i64[:], q_loading: i64[:], out: i64[:]) -> i64 {
    return mg_tick_body(n, names, off, ln, status, req_count, last_used, stopping, models_max, nq, qrow, q_loading, out);
}

// unload_lru 1295-1318: the log line printed for a refusal, so the gate can
// compare the text the operator sees as well: "models_max limit reached,
// removing LRU name=<name>" or "" when there is a slot.
export fn mg_lru_log(models_max: i64, n_running: i64, name: const u8[:], out: u8[:]) -> i64 {
    if models_max <= 0 {
        return 0;
    }
    if n_running < models_max {
        return 0;
    }
    var p = mg_puts(out, 0, "models_max limit reached, removing LRU name=");
    p = mg_puts(out, p, name);
    return p;
}
