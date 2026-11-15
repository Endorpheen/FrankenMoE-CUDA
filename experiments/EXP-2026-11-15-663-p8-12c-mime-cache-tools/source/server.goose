import math;
import std;
// ---- composed from goose/server/model-manager.goose ----
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

// ---- composed from goose/server/jsonparse.goose ----
// JSON text parser in Goose, from the behaviour of common_json::parse
// (nlohmann ordered_json): the RFC 8259 grammar with the library's number
// classification, string escape rules, duplicate-key policy and error
// classes. The tree is emitted flat; the JSON container type stays native.
//
// Node kinds: 0 null, 1 bool, 2 integer (i64), 3 unsigned above i64, 4 float,
// 5 string, 6 array, 7 object.
//
// Error classes: 101 parse_error, 406 out_of_range (number overflow). The line
// and column follow the library's get_position(): 1-based line, and a column
// counted from the first byte of that line (0 on a line that starts at the
// offending byte).



const K_NULL: i64 = 0;
const K_BOOL: i64 = 1;
const K_INT: i64 = 2;
const K_UINT: i64 = 3;
const K_FLOAT: i64 = 4;
const K_STRING: i64 = 5;
const K_ARRAY: i64 = 6;
const K_OBJECT: i64 = 7;
const E_PARSE: i64 = 101;
const E_RANGE: i64 = 406;
// strtod, through the C library: the one primitive the parser borrows
extern "gs_json_strtod" fn libc_strtod(s: const u8[:]) -> f64;

fn app(dst: u8[:], at: i64, s: const u8[:], off: i64, len: i64) -> i64 {
    var k: i64 = 0;

    while k < len {
        if at + k < dst.len {
            dst[at + k] = s[off + k];
        }
        k = k + 1;
    }
    return at + len;
}

fn appc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn put_utf8(dst: u8[:], at: i64, cp: i64) -> i64 {
    var p: i64 = at;

    if cp < 128 {
        return appc(dst, p, cp as u8);
    }

    if cp < 2048 {
        p = appc(dst, p, (192 + (cp / 64)) as u8);
        return appc(dst, p, (128 + (cp % 64)) as u8);
    }

    if cp < 65536 {
        p = appc(dst, p, (224 + (cp / 4096)) as u8);
        p = appc(dst, p, (128 + ( (cp / 64) % 64)) as u8);
        return appc(dst, p, (128 + (cp % 64)) as u8);
    }
    p = appc(dst, p, (240 + (cp / 262144)) as u8);
    p = appc(dst, p, (128 + ( (cp / 4096) % 64)) as u8);
    p = appc(dst, p, (128 + ( (cp / 64) % 64)) as u8);
    return appc(dst, p, (128 + (cp % 64)) as u8);
}

fn hexv(c: u8) -> i64 {
    if c >= '0' && c <= '9' {
        return (c - 48) as i64;
    }

    if c >= 'a' && c <= 'f' {
        return (c - 87) as i64;
    }

    if c >= 'A' && c <= 'F' {
        return (c - 55) as i64;
    }
    return -1;
}

fn is_ws(c: u8) -> bool {
    if c == (32 as u8) || c == (9 as u8) || c == (10 as u8) || c == (13 as u8) {
        return true;
    }
    return false;
}

fn is_dig(c: u8) -> bool {
    return c >= '0' && c <= '9';
}
// state: 0 nodes, 1 err code, 2 err line, 3 err col, 4 pos, 5 entries,
//        6 (unused), 7 depth, 8 str off, 9 str len, 10 sbuf cursor, 11 block cursor
// consumed = bytes read, the way the library counts them: one per get(), so an
// end-of-input error is one past the last byte

fn err_cons(text: const u8[:], st: i64[:], code: i64, consumed: i64) -> i64 {
    var line: i64 = 1;
    var ls: i64 = 0;
    var i: i64 = 0;
    let lim: i64 = if consumed < text.len {
        consumed
    } else {
        text.len
    }
    ;

    while i < lim {
        if text[i] == (10 as u8) {
            line = line + 1;
            ls = i + 1;
        }
        i = i + 1;
    }
    st[1] = code;
    st[2] = line;
    st[3] = consumed - ls;
    return -1;
}
// the byte at the cursor is at fault: the library has already consumed it

fn err_next(text: const u8[:], st: i64[:], code: i64) -> i64 {
    return err_cons(text, st, code, st[4] + 1);
}

fn err_eof(text: const u8[:], st: i64[:], code: i64) -> i64 {
    return err_cons(text, st, code, text.len + 1);
}

fn skip_ws(text: const u8[:], st: i64[:]) -> i64 {
    while st[4] < text.len && is_ws(text[st[4]]) {
        st[4] = st[4] + 1;
    }
    return st[4];
}

fn new_node(
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    st: i64[:]) -> i64 {
    let id = st[0];
    st[0] = id + 1;
    jk[id] = -1;
    jv[id] = 0;
    jd[id] = 0.0;
    js[id] = 0;
    jl[id] = 0;
    jf[id] = 0;
    jn[id] = 0;
    jh[id] = -1;
    return id;
}
// prepend an entry to node's list

fn add_entry(
    node: i64,
    val: i64,
    koff: i64,
    klen: i64,
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    jh: i64[:],
    st: i64[:]) -> i64 {
    let ei = st[5];
    st[5] = ei + 1;
    e_val[ei] = val;
    e_koff[ei] = koff;
    e_klen[ei] = klen;
    e_next[ei] = jh[node];
    jh[node] = ei;
    return ei;
}
// replace the value of an existing key, keeping its position

fn set_entry(
    node: i64,
    val: i64,
    koff: i64,
    klen: i64,
    sbuf: const u8[:],
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    jh: i64[:],
    st: i64[:]) -> i64 {
    let key = sbuf[koff .. koff + klen];
    var e: i64 = jh[node];

    while e >= 0 {
        if e_klen[e] == key.len {
            if eqb(sbuf[e_koff[e] .. e_koff[e] + e_klen[e]], key) {
                e_val[e] = val;
                return e;
            }
        }
        e = e_next[e];
    }
    return add_entry(node, val, koff, klen, e_val, e_koff, e_klen, e_next, jh, st);
}

fn eqb(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}
recursive fn parse_value(
    depth: i64,
    text: const u8[:],
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    sbuf: u8[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    st: i64[:]) -> i64 {
    skip_ws(text, st);
    let p = st[4];

    if p >= text.len {
        return err_eof(text, st, E_PARSE);
    }
    let c = text[p];

    if c == '{' {
        return parse_object(
            depth + 1,
            text,
            jk,
            jv,
            jd,
            js,
            jl,
            jf,
            jn,
            jh,
            e_val,
            e_koff,
            e_klen,
            e_next,
            sbuf,
            jkey,
            jkl,
            jchild,
            st);
    }

    if c == '[' {
        return parse_array(
            depth + 1,
            text,
            jk,
            jv,
            jd,
            js,
            jl,
            jf,
            jn,
            jh,
            e_val,
            e_koff,
            e_klen,
            e_next,
            sbuf,
            jkey,
            jkl,
            jchild,
            st);
    }

    if c == '"' {
        let off, len = parse_string(text, sbuf, st);

        if st[1] != 0 {
            return -1;
        }
        let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);
        jk[id] = K_STRING;
        js[id] = off;
        jl[id] = len;
        return id;
    }

    if c == 't' {
        let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);
        jk[id] = K_BOOL;
        jv[id] = 1;
        st[4] = p + 1;

        if !lit_rest(text, st, "rue") {
            return -1;
        }
        return id;
    }

    if c == 'f' {
        let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);
        jk[id] = K_BOOL;
        jv[id] = 0;
        st[4] = p + 1;

        if !lit_rest(text, st, "alse") {
            return -1;
        }
        return id;
    }

    if c == 'n' {
        let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);
        jk[id] = K_NULL;
        st[4] = p + 1;

        if !lit_rest(text, st, "ull") {
            return -1;
        }
        return id;
    }

    if c == '-' || is_dig(c) {
        return parse_number(text, jk, jv, jd, js, jl, jf, jn, jh, st);
    }
    return err_next(text, st, E_PARSE);
}

fn lit_rest(text: const u8[:], st: i64[:], rest: const u8[:]) -> bool {
    var i: i64 = 0;

    while i < rest.len {
        let p = st[4];

        if p >= text.len {
            err_eof(text, st, E_PARSE);
            return false;
        }

        if text[p] != rest[i] {
            err_next(text, st, E_PARSE);
            return false;
        }
        st[4] = p + 1;
        i = i + 1;
    }
    // the next byte must not continue a literal
    let p = st[4];

    if p < text.len {
        let c = text[p];

        if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || is_dig(c) {
            err_next(text, st, E_PARSE);
            return false;
        }
    }
    return true;
}
recursive fn parse_object(
    depth: i64,
    text: const u8[:],
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    sbuf: u8[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    st: i64[:]) -> i64 {
    let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);
    jk[id] = K_OBJECT;
    st[4] = st[4] + 1;
    // '{'
    skip_ws(text, st);

    if st[4] < text.len && text[st[4]] == '}' {
        st[4] = st[4] + 1;
        return id;
    }
    var looping: bool = true;

    while looping {
        skip_ws(text, st);

        if st[4] >= text.len {
            return err_eof(text, st, E_PARSE);
        }

        if text[st[4]] != '"' {
            return err_next(text, st, E_PARSE);
        }
        let koff, klen = parse_string(text, sbuf, st);

        if st[1] != 0 {
            return -1;
        }
        skip_ws(text, st);

        if st[4] >= text.len {
            return err_eof(text, st, E_PARSE);
        }

        if text[st[4]] != ':' {
            return err_next(text, st, E_PARSE);
        }
        st[4] = st[4] + 1;
        let child = parse_value(
            depth,
            text,
            jk,
            jv,
            jd,
            js,
            jl,
            jf,
            jn,
            jh,
            e_val,
            e_koff,
            e_klen,
            e_next,
            sbuf,
            jkey,
            jkl,
            jchild,
            st);

        if st[1] != 0 {
            return -1;
        }
        set_entry(id, child, koff, klen, sbuf, e_val, e_koff, e_klen, e_next, jh, st);
        skip_ws(text, st);

        if st[4] >= text.len {
            return err_eof(text, st, E_PARSE);
        }
        let c = text[st[4]];

        if c == '}' {
            st[4] = st[4] + 1;
            looping = false;
        } else

        if c == ',' {
            st[4] = st[4] + 1;
        } else {
            skip_token(text, jk, jv, jd, js, jl, jf, jn, jh, e_val, e_koff, e_klen, e_next, sbuf, st);
            return err_cons(text, st, E_PARSE, st[4]);
        }
    }
    return id;
}
recursive fn parse_array(
    depth: i64,
    text: const u8[:],
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    sbuf: u8[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    st: i64[:]) -> i64 {
    let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);
    jk[id] = K_ARRAY;
    st[4] = st[4] + 1;
    // '['
    skip_ws(text, st);

    if st[4] < text.len && text[st[4]] == ']' {
        st[4] = st[4] + 1;
        return id;
    }
    var looping: bool = true;

    while looping {
        let child = parse_value(
            depth,
            text,
            jk,
            jv,
            jd,
            js,
            jl,
            jf,
            jn,
            jh,
            e_val,
            e_koff,
            e_klen,
            e_next,
            sbuf,
            jkey,
            jkl,
            jchild,
            st);

        if st[1] != 0 {
            return -1;
        }
        add_entry(id, child, 0, 0, e_val, e_koff, e_klen, e_next, jh, st);
        skip_ws(text, st);

        if st[4] >= text.len {
            return err_eof(text, st, E_PARSE);
        }
        let c = text[st[4]];

        if c == ']' {
            st[4] = st[4] + 1;
            looping = false;
        } else

        if c == ',' {
            st[4] = st[4] + 1;
        } else {
            skip_token(text, jk, jv, jd, js, jl, jf, jn, jh, e_val, e_koff, e_klen, e_next, sbuf, st);
            return err_cons(text, st, E_PARSE, st[4]);
        }
    }
    return id;
}
// string body; leaves (off,len) in st[8], st[9]

fn parse_string(text: const u8[:], sbuf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[10];
    var p: i64 = st[4] + 1;
    // past '"'
    var end: i64 = -1;

    while p < text.len {
        let c = text[p];

        if c == '"' {
            end = p;
            p = p + 1;
        } else

        if c == '\\' {
            p = p + 1;

            if p >= text.len {
                return err_eof(text, st, E_PARSE), 0;
            }
            let e = text[p];

            if e == '"' || e == '\\' || e == '/' {
                st[10] = appc(sbuf, st[10], e);
                p = p + 1;
            } else

            if e == 'b' {
                st[10] = appc(sbuf, st[10], 8 as u8);
                p = p + 1;
            } else

            if e == 'f' {
                st[10] = appc(sbuf, st[10], 12 as u8);
                p = p + 1;
            } else

            if e == 'n' {
                st[10] = appc(sbuf, st[10], 10 as u8);
                p = p + 1;
            } else

            if e == 'r' {
                st[10] = appc(sbuf, st[10], 13 as u8);
                p = p + 1;
            } else

            if e == 't' {
                st[10] = appc(sbuf, st[10], 9 as u8);
                p = p + 1;
            } else

            if e == 'u' {
                let cp, np = hex4(text, p + 1, st);

                if st[1] != 0 {
                    return -1, 0;
                }
                p = np;

                if cp >= 0xD800 && cp <= 0xDBFF {
                    if p < text.len && text[p] == '\\' {
                        if p + 1 >= text.len {
                            return err_eof(text, st, E_PARSE), 0;
                        }

                        if text[p + 1] != 'u' {
                            return err_cons(text, st, E_PARSE, p + 2), 0;
                        }
                        let lo, np2 = hex4(text, p + 2, st);

                        if st[1] != 0 {
                            return -1, 0;
                        }

                        if ! (lo >= 0xDC00 && lo <= 0xDFFF) {
                            return err_cons(text, st, E_PARSE, np2), 0;
                        }
                        let full = 65536 + (cp - 0xD800) * 1024 + (lo - 0xDC00);
                        st[10] = put_utf8(sbuf, st[10], full);
                        p = np2;
                    } else

                    if p >= text.len {
                        return err_eof(text, st, E_PARSE), 0;
                    } else {
                        return err_cons(text, st, E_PARSE, p + 1), 0;
                    }
                } else

                if cp >= 0xDC00 && cp <= 0xDFFF {
                    return err_cons(text, st, E_PARSE, p), 0;
                } else {
                    st[10] = put_utf8(sbuf, st[10], cp);
                }
            } else {
                return err_cons(text, st, E_PARSE, p + 1), 0;
            }
        } else

        if c < 0x20 {
            // the control byte is consumed before it is rejected
            p = p + 1;
            return err_cons(text, st, E_PARSE, p), 0;
        } else {
            let uk, ub = utf8_at(text, p);

            if uk != 0 {
                return err_cons(text, st, E_PARSE, p + ub), 0;
            }
            st[10] = app(sbuf, st[10], text, p, ub);
            p = p + ub;
        }

        if end >= 0 {
            break;
        }
    }

    if end < 0 {
        return err_eof(text, st, E_PARSE), 0;
    }
    let off = at;
    let len = st[10] - at;
    st[8] = off;
    st[9] = len;
    st[4] = end + 1;
    return off, len;
}

fn hex4(text: const u8[:], p0: i64, st: i64[:]) -> i64, i64 {
    var p: i64 = p0;
    var v: i64 = 0;
    var i: i64 = 0;

    while i < 4 {
        if p >= text.len {
            return err_eof(text, st, E_PARSE), p;
        }
        let d = hexv(text[p]);

        if d < 0 {
            return err_cons(text, st, E_PARSE, p + 1), p;
        }
        v = v * 16 + d;
        p = p + 1;
        i = i + 1;
    }
    return v, p;
}
// 0 valid, 1 incomplete, 2 invalid
// 0 valid, 1 incomplete, 2 invalid; the count is bytes up to and including the
// byte that decided it

fn utf8_at(text: const u8[:], p: i64) -> i64, i64 {
    let b0 = text[p];

    if b0 < 128 {
        return 0, 1;
    }

    if b0 < 194 {
        return 2, 1;
    }

    if b0 < 224 {
        if p + 1 >= text.len {
            return 1, text.len - p;
        }

        if (text[p + 1] & 192) != 128 {
            return 2, 2;
        }
        return 0, 2;
    }

    if b0 < 240 {
        var i: i64 = 1;

        while i < 3 {
            if p + i >= text.len {
                return 1, i;
            }

            if (text[p + i] & 192) != 128 {
                return 2, i + 1;
            }
            i = i + 1;
        }
        return 0, 3;
    }

    if b0 < 245 {
        var i: i64 = 1;

        while i < 4 {
            if p + i >= text.len {
                return 1, i;
            }

            if (text[p + i] & 192) != 128 {
                return 2, i + 1;
            }
            i = i + 1;
        }
        return 0, 4;
    }
    return 2, 1;
}

fn parse_number(
    text: const u8[:],
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    st: i64[:]) -> i64 {
    let start = st[4];
    var p: i64 = start;
    var neg: bool = false;

    if text[p] == '-' {
        neg = true;
        p = p + 1;
    }

    if p >= text.len {
        return err_eof(text, st, E_PARSE);
    }
    // integer part
    var ov: bool = false;
    var mag: u64 = 0;
    let first = text[p];

    if first == '0' {
        p = p + 1;
    } else

    if is_dig(first) {
        while p < text.len && is_dig(text[p]) {
            let d = (text[p] - 48) as u64;

            if mag > 1844674407370955161 {
                ov = true;
            } else

            if mag == 1844674407370955161 && d > 5 {
                ov = true;
            } else

            if !ov {
                mag = mag * 10 + d;
            }
            p = p + 1;
        }
    } else {
        // a bare '-' is an invalid number at the byte that must follow
        return err_cons(text, st, E_PARSE, p + 1);
    }
    var isflt: bool = ov;

    if p < text.len && text[p] == '.' {
        p = p + 1;

        if p >= text.len || !is_dig(text[p]) {
            return err_cons(text, st, E_PARSE, p + 1);
        }

        while p < text.len && is_dig(text[p]) {
            p = p + 1;
        }
        isflt = true;
    }

    if p < text.len && (text[p] == 'e' || text[p] == 'E') {
        p = p + 1;

        if p < text.len && (text[p] == '+' || text[p] == '-') {
            p = p + 1;
        }

        if p >= text.len || !is_dig(text[p]) {
            return err_cons(text, st, E_PARSE, p + 1);
        }

        while p < text.len && is_dig(text[p]) {
            p = p + 1;
        }
        isflt = true;
    }
    st[4] = p;
    let id = new_node(jk, jv, jd, js, jl, jf, jn, jh, st);

    if !isflt {
        if neg {
            // -2^63 is the smallest signed value

            if mag <= 9223372036854775808 {
                if mag == 9223372036854775808 {
                    jk[id] = K_INT;
                    jv[id] = -9223372036854775807 - 1;
                } else {
                    jk[id] = K_INT;
                    jv[id] = 0 - (mag as i64);
                }
                return id;
            }
            isflt = true;
        } else {
            if mag <= 9223372036854775807 {
                jk[id] = K_INT;
                jv[id] = mag as i64;
                return id;
            }
            jk[id] = K_UINT;
            jv[id] = mag as i64;
            return id;
        }
    }
    let tok = text[start .. p];
    let d = libc_strtod(tok);

    if is_inf(d) || is_nan(d) {
        return err_cons(text, st, E_RANGE, st[4]);
    }
    jk[id] = K_FLOAT;
    jd[id] = d;
    return id;
}
// advance past one token without validating it: the library reads the next
// token before it decides that a ',' or ']' / '}' was expected

fn skip_token(
    text: const u8[:],
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    sbuf: u8[:],
    st: i64[:]) -> i64 {
    skip_ws(text, st);
    let p = st[4];

    if p >= text.len {
        return st[4];
    }
    let c = text[p];

    if c == '"' {
        // skip the raw string, escapes included
        var q: i64 = p + 1;
        var stop: bool = false;

        while q < text.len && !stop {
            if text[q] == '\\' {
                q = q + 2;
            } else

            if text[q] == '"' {
                q = q + 1;
                stop = true;
            } else {
                q = q + 1;
            }
        }
        st[4] = q;
        return st[4];
    }

    if is_dig(c) || c == '-' {
        var q: i64 = p;

        while q < text.len && (is_dig(text[q]) || text[q] == '-' || text[q] == '+' || text[q] == '.' || text[q] == 'e' || text[q] == 'E') {
            q = q + 1;
        }
        st[4] = q;
        return st[4];
    }

    if (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') {
        var q: i64 = p;

        while q < text.len && ( ( (text[q] >= 'a') && (text[q] <= 'z')) || ( (text[q] >= 'A') && (text[q] <= 'Z'))) {
            q = q + 1;
        }
        st[4] = q;
        return st[4];
    }
    st[4] = p + 1;
    return st[4];
}
// materialise the child blocks: reverse each list, then copy it in document order

export fn json_parse(
    text: const u8[:],
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jh: i64[:],
    e_val: i64[:],
    e_koff: i64[:],
    e_klen: i64[:],
    e_next: i64[:],
    sbuf: u8[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    st: i64[:]) -> i64 {
    st[0] = 0;
    st[1] = 0;
    st[2] = 0;
    st[3] = 0;
    st[4] = 0;
    st[5] = 0;
    st[7] = 0;
    st[8] = 0;
    st[9] = 0;
    st[10] = 0;
    st[11] = 0;
    let root = parse_value(
        0,
        text,
        jk,
        jv,
        jd,
        js,
        jl,
        jf,
        jn,
        jh,
        e_val,
        e_koff,
        e_klen,
        e_next,
        sbuf,
        jkey,
        jkl,
        jchild,
        st);

    if st[1] != 0 {
        return -1;
    }
    // trailing garbage is an error
    skip_ws(text, st);

    if st[4] < text.len {
        err_next(text, st, E_PARSE);
        return -1;
    }
    var i: i64 = 0;

    while i < st[0] {
        let k = jk[i];

        if k == K_ARRAY || k == K_OBJECT {
            // reverse
            var prev: i64 = -1;
            var e: i64 = jh[i];

            while e >= 0 {
                let nx = e_next[e];
                e_next[e] = prev;
                prev = e;
                e = nx;
            }
            jh[i] = prev;
            // copy in order
            jf[i] = st[11];
            var cnt: i64 = 0;
            e = prev;

            while e >= 0 {
                jchild[st[11]] = e_val[e];
                jkey[st[11]] = e_koff[e];
                jkl[st[11]] = e_klen[e];
                st[11] = st[11] + 1;
                cnt = cnt + 1;
                e = e_next[e];
            }
            jn[i] = cnt;
        }
        i = i + 1;
    }
    return root;
}

// ---- composed from goose/server/tools-pure.goose ----
// P8-07 preparatory slice: local tool definitions, argument validation and
// result formation for the seven tools_io_basic tools.
//
// Native source: upstream/llama.cpp/tools/server/server-tools.cpp (lines cited
// per function below); the JSON is parsed with the accepted parser
// (goose/json/json-parser.goose, called through its export ABI).
//
// Scope: the pure parts only. No file system, no subprocess, no regex engine.
// The content, size, entry list and match list a tool would obtain from
// tools_io are parameters of these functions, so every branch is testable
// without a substrate. What still needs the substrate is listed at the end of
// TOOLS-ANALYSIS.md.
//
// Flat layouts:
//   st[0] status: 0 ok, -1 unparsable params or not an object (native: 400
//   invalid request), -2 a required member is missing (native: params.at throws,
//   400), -3 a tool error (native: 200 with {"error": ...}, message in `msg`);
//   st[1] tool index.
//   Every string a validator keeps is appended to `sbuf` and recorded as
//   (offset, length) pairs from st[20] upward: st[20]/st[21] first string,
//   st[22]/st[23] second, ...
//   Per-tool numbers stay in st[10..19] (documented per case below).



const TOOL_READ_FILE: i64 = 0;
const TOOL_FILE_GLOB: i64 = 1;
const TOOL_GREP: i64 = 2;
const TOOL_EXEC: i64 = 3;
const TOOL_WRITE_FILE: i64 = 4;
const TOOL_EDIT_FILE: i64 = 5;
const TOOL_GET_INFO: i64 = 6;
const TOOL_COUNT: i64 = 7;

// Limits, exactly as the native constants.
const LIMIT_READ_SIZE: i64 = 16 * 1024;              // server-tools.cpp:866
const LIMIT_READ_SIZE_BASE64: i64 = 32 * 1024 * 1024; // :867
const LIMIT_SEARCH_RESULTS: i64 = 100;               // :973
const LIMIT_GREP_RESULTS: i64 = 100;                 // :1090
const LIMIT_EXEC_OUTPUT: i64 = 16 * 1024;            // :1242
const LIMIT_EXEC_TIMEOUT: i64 = 60;                  // :1243
const LIMIT_INFO_OUTPUT: i64 = 4096;                 // :1697
const LIMIT_INFO_TIMEOUT: i64 = 5;                   // :1698

const TP_K_BOOL: i64 = 1;
const TP_K_INT: i64 = 2;
const TP_K_UINT: i64 = 3;
const TP_K_STRING: i64 = 5;
const TP_K_ARRAY: i64 = 6;
const TP_K_OBJECT: i64 = 7;

extern "json_parse" fn json_parse_abi(
    text: const u8[:],
    jk: i64[:], jv: i64[:], jd: f64[:], js: i64[:], jl: i64[:], jf: i64[:],
    jn: i64[:], jh: i64[:],
    e_val: i64[:], e_koff: i64[:], e_klen: i64[:], e_next: i64[:],
    sbuf: u8[:], jkey: i64[:], jkl: i64[:], jchild: i64[:], st: i64[:]) -> i64;

// ── tool table ──────────────────────────────────────────────────────────────

// The names in registration order (server-tools.cpp:1957-1963).
fn tool_name_impl(idx: i64, out: u8[:]) -> i64 {
    if idx == TOOL_READ_FILE {
        return tp_put(out, 0, "read_file");
    }
    if idx == TOOL_FILE_GLOB {
        return tp_put(out, 0, "file_glob_search");
    }
    if idx == TOOL_GREP {
        return tp_put(out, 0, "grep_search");
    }
    if idx == TOOL_EXEC {
        return tp_put(out, 0, "exec_shell_command");
    }
    if idx == TOOL_WRITE_FILE {
        return tp_put(out, 0, "write_file");
    }
    if idx == TOOL_EDIT_FILE {
        return tp_put(out, 0, "edit_file");
    }
    if idx == TOOL_GET_INFO {
        return tp_put(out, 0, "get_info");
    }
    return 0;
}

export fn tool_name(idx: i64, out: u8[:]) -> i64 {
    return tool_name_impl(idx, out);
}

// Dispatch by name; -1 means the native find_tool would throw (HTTP 404).
export fn tool_index(name: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < TOOL_COUNT {
        var buf: u8[32] = [0; 32];
        let n = tool_name_impl(i, buf);

        if tp_eq(name, buf[0 .. n]) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// permission_write / uses_cwd / support_stream flags (server-tools.cpp:869-1963).
export fn tool_flags(idx: i64) -> i64 {
    // bit 0 write permission, bit 1 uses cwd, bit 2 supports streaming
    if idx == TOOL_EXEC {
        return 7;
    }
    if idx == TOOL_READ_FILE || idx == TOOL_FILE_GLOB || idx == TOOL_GREP || idx == TOOL_GET_INFO {
        return 2;
    }
    if idx == TOOL_WRITE_FILE || idx == TOOL_EDIT_FILE {
        return 3;
    }
    return 0;
}

fn tp_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        if at + i < dst.len {
            dst[at + i] = s[i];
        }
        i = i + 1;
    }
    return at + s.len;
}

fn tp_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn tp_uint(dst: u8[:], at_in: i64, v: i64) -> i64 {
    var at = at_in;
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;

    if x < 0 {
        x = 0;
    }
    if x == 0 {
        buf[0] = 48;
        n = 1;
    }
    while x > 0 {
        buf[n] = (48 + (x - (x / 10) * 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n - 1;

    while i >= 0 {
        if at < dst.len {
            dst[at] = buf[i];
        }
        at = at + 1;
        i = i - 1;
    }
    return at;
}

// ── params access, mirroring server-common.h json_value ─────────────────────

struct TPNode {
    jk: i64[:],
    jv: i64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    sbuf: const u8[:],
}

fn tp_pget(p: const TPNode, obj: i64, key: const u8[:]) -> i64 {
    let base = p.jf[obj];
    let n = p.jn[obj];
    var k: i64 = 0;

    while k < n {
        let ko = p.jkey[base + k];
        let kl = p.jkl[base + k];

        if kl == key.len {
            var same = true;
            var i: i64 = 0;

            while i < kl {
                if p.sbuf[ko + i] != key[i] {
                    same = false;
                    i = kl;
                } else {
                    i = i + 1;
                }
            }
            if same {
                return p.jchild[base + k];
            }
        }
        k = k + 1;
    }
    return 0 - 1;
}

// json_value<T>: a missing or null member is the default, a wrong type warns and
// is also the default (server-common.h:41).
fn tp_pget_int(p: const TPNode, obj: i64, key: const u8[:], dflt: i64) -> i64 {
    let n = tp_pget(p, obj, key);

    if n < 0 {
        return dflt;
    }
    if p.jk[n] == TP_K_INT || p.jk[n] == TP_K_UINT {
        return p.jv[n];
    }
    return dflt;
}

fn tp_pget_bool(p: const TPNode, obj: i64, key: const u8[:], dflt: i64) -> i64 {
    let n = tp_pget(p, obj, key);

    if n < 0 {
        return dflt;
    }
    if p.jk[n] == TP_K_BOOL {
        return p.jv[n];
    }
    return dflt;
}

// 1 when the member is absent or null (the native params.at would throw).
fn p_missing(p: const TPNode, obj: i64, key: const u8[:]) -> i64 {
    if tp_pget(p, obj, key) < 0 {
        return 1;
    }
    return 0;
}

// (offset, length) of a string member, or (-1, 0) when absent.
fn tp_pget_str(p: const TPNode, obj: i64, key: const u8[:]) -> i64, i64 {
    let n = tp_pget(p, obj, key);

    if n < 0 || p.jk[n] != TP_K_STRING {
        return 0 - 1, 0;
    }
    return p.js[n], p.jl[n];
}

fn tp_copy(dst: u8[:], at_in: i64, src: const u8[:], off: i64, len: i64) -> i64 {
    var at = at_in;
    var i: i64 = 0;

    while i < len {
        if at + i < dst.len {
            dst[at + i] = src[off + i];
        }
        i = i + 1;
    }
    return at + len;
}

fn keep_str(st: i64[:], sbuf: u8[:], src: const u8[:], off: i64, len: i64, slot: i64) -> i64 {
    let at = st[18];
    let n = tp_copy(sbuf, at, src, off, len);

    st[18] = n;
    st[20 + 2 * slot] = at;
    st[21 + 2 * slot] = len;
    return 0;
}

fn keep_lit(st: i64[:], sbuf: u8[:], lit: const u8[:], slot: i64) -> i64 {
    let at = st[18];
    let n = tp_put(sbuf, at, lit);

    st[18] = n;
    st[20 + 2 * slot] = at;
    st[21 + 2 * slot] = lit.len;
    return 0;
}

fn set_str(st: i64[:], slot: i64, off: i64, len: i64) -> i64 {
    st[20 + 2 * slot] = off;
    st[21 + 2 * slot] = len;
    return 0;
}

fn set_msg(st: i64[:], msg: u8[:], text: const u8[:]) -> i64 {
    let n = tp_put(msg, 0, text);
    st[19] = n;
    return 0;
}

fn set_msg_prefixed(st: i64[:], msg: u8[:], prefix: const u8[:], arg: const u8[:], off: i64,
                    len: i64, tail: const u8[:]) -> i64 {
    var p = tp_put(msg, 0, prefix);

    p = tp_copy(msg, p, arg, off, len);
    p = tp_put(msg, p, tail);
    st[19] = p;
    return 0;
}

// ── validation ──────────────────────────────────────────────────────────────

// One call: parse the params text and validate them for the tool index.
// Returns st[0] (0 ok, -1 bad params, -2 missing required, -3 tool error).
export fn tool_validate(
    idx: i64,
    params: const u8[:],
    st: i64[:],
    sbuf: u8[:],
    msg: u8[:]) -> i64 {
    var jk: i64[64] = [0; 64];
    var jv: i64[64] = [0; 64];
    var jd: f64[64] = [0.0; 64];
    var js: i64[64] = [0; 64];
    var jl: i64[64] = [0; 64];
    var jf: i64[64] = [0; 64];
    var jn: i64[64] = [0; 64];
    var jh: i64[64] = [0; 64];
    var ev: i64[64] = [0; 64];
    var ek: i64[64] = [0; 64];
    var el: i64[64] = [0; 64];
    var en: i64[64] = [0; 64];
    var sb: u8[8192] = [0; 8192];
    var key: i64[64] = [0; 64];
    var kl: i64[64] = [0; 64];
    var jc: i64[64] = [0; 64];
    var pst: i64[16] = [0; 16];

    st[0] = 0;
    st[1] = idx;
    st[18] = 0;
    st[19] = 0;

    let root = json_parse_abi(params, jk, jv, jd, js, jl, jf, jn, jh,
                             ev, ek, el, en, sb, key, kl, jc, pst);

    if root < 0 || jk[root] != TP_K_OBJECT {
        st[0] = 0 - 1;
        return st[0];
    }
    let p = TPNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };

    if idx == TOOL_READ_FILE {
        return tv_read_file(p, root, st, sbuf, msg);
    }
    if idx == TOOL_FILE_GLOB {
        return tv_file_glob(p, root, st, sbuf, msg);
    }
    if idx == TOOL_GREP {
        return tv_grep(p, root, st, sbuf, msg);
    }
    if idx == TOOL_EXEC {
        return tv_exec(p, root, st, sbuf, msg);
    }
    if idx == TOOL_WRITE_FILE {
        return tv_write_file(p, root, st, sbuf, msg);
    }
    if idx == TOOL_EDIT_FILE {
        return tv_edit_file(p, root, st, sbuf, msg);
    }
    if idx == TOOL_GET_INFO {
        return 0;
    }
    st[0] = 0 - 1;
    return st[0];
}

// read_file (server-tools.cpp:898-967): path required; start_line default 1,
// end_line default -1 (unlimited), append_loc false, resp_type is a header and
// never a model argument (it is read here only to model the base64 branch).
fn tv_read_file(p: const TPNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path");
        return st[0];
    }
    let po, pl = tp_pget_str(p, root, "path");

    if po < 0 {
        st[0] = 0 - 1;
        set_msg(st, msg, "path must be a string");
        return st[0];
    }
    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    st[10] = tp_pget_int(p, root, "start_line", 1);
    st[11] = tp_pget_int(p, root, "end_line", 0 - 1);
    st[12] = tp_pget_bool(p, root, "append_loc", 0);
    st[13] = 0;
    let ro, rl = tp_pget_str(p, root, "resp_type");

    if ro >= 0 && rl == 6 && tp_eq(p.sbuf[ro .. ro + rl], "base64") {
        st[13] = 1;
    }
    return 0;
}

// file_glob_search (server-tools.cpp:1015-1085): path required; include "**";
// type is file/dir/all; max_depth clamped at 0; limit >= 1 and capped at 100.
fn tv_file_glob(p: const TPNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path");
        return st[0];
    }
    let po, pl = tp_pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let io, il = tp_pget_str(p, root, "include");

    if io < 0 {
        keep_lit(st, sbuf, "**", 1);
    } else {
        keep_str(st, sbuf, p.sbuf, io, il, 1);
    }
    let eo, el = tp_pget_str(p, root, "exclude");

    if eo < 0 {
        keep_str(st, sbuf, p.sbuf, 0, 0, 2);
    } else {
        keep_str(st, sbuf, p.sbuf, eo, el, 2);
    }
    let to, tl = tp_pget_str(p, root, "type");
    var kind: i64 = 0; // 0 file, 1 dir, 2 all

    if to >= 0 {
        if tp_eq(p.sbuf[to .. to + tl], "dir") {
            kind = 1;
        } else {
            if tp_eq(p.sbuf[to .. to + tl], "all") {
                kind = 2;
            } else {
                if tp_eq(p.sbuf[to .. to + tl], "file") == false {
                    st[0] = 0 - 3;
                    set_msg_prefixed(st, msg, "invalid type: ", p.sbuf, to, tl,
                                     " (expected \"file\", \"dir\" or \"all\")");
                    return st[0];
                }
            }
        }
        keep_str(st, sbuf, p.sbuf, to, tl, 3);
    } else {
        keep_str(st, sbuf, p.sbuf, 0, 0, 3);
    }
    st[14] = kind;
    let md = tp_pget_int(p, root, "max_depth", 0);

    if md < 0 {
        st[15] = 0;
    } else {
        st[15] = md;
    }
    let lim = tp_pget_int(p, root, "limit", LIMIT_SEARCH_RESULTS);

    if lim < 1 {
        st[0] = 0 - 3;
        var p2 = tp_put(msg, 0, "invalid limit: ");

        p2 = tp_uint(msg, p2, lim);
        p2 = tp_put(msg, p2, " (expected 1 or more)");
        st[19] = p2;
        return st[0];
    }
    if lim > LIMIT_SEARCH_RESULTS {
        st[16] = LIMIT_SEARCH_RESULTS;
    } else {
        st[16] = lim;
    }
    return 0;
}

// grep_search (server-tools.cpp:1130-1231): path and pattern required; include
// "**"; literal/ignore_case false; context_lines >= 0.
fn tv_grep(p: const TPNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 || p_missing(p, root, "pattern") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path or pattern");
        return st[0];
    }
    let po, pl = tp_pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let pro, prl = tp_pget_str(p, root, "pattern");

    keep_str(st, sbuf, p.sbuf, pro, prl, 1);
    let io, il = tp_pget_str(p, root, "include");

    if io < 0 {
        keep_lit(st, sbuf, "**", 2);
    } else {
        keep_str(st, sbuf, p.sbuf, io, il, 2);
    }
    let eo, el = tp_pget_str(p, root, "exclude");

    if eo < 0 {
        keep_str(st, sbuf, p.sbuf, 0, 0, 3);
    } else {
        keep_str(st, sbuf, p.sbuf, eo, el, 3);
    }
    st[10] = tp_pget_bool(p, root, "return_line_numbers", 0);
    st[11] = tp_pget_bool(p, root, "literal", 0);
    st[12] = tp_pget_bool(p, root, "ignore_case", 0);
    let c = tp_pget_int(p, root, "context_lines", 0);

    if c < 0 {
        st[13] = 0;
    } else {
        st[13] = c;
    }
    return 0;
}

// exec_shell_command (server-tools.cpp:1273-1319): command required; timeout
// default 10 capped at 60; max_output_size default and cap 16 KiB.
fn tv_exec(p: const TPNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "command") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: command");
        return st[0];
    }
    let co, cl = tp_pget_str(p, root, "command");

    keep_str(st, sbuf, p.sbuf, co, cl, 0);
    let t = tp_pget_int(p, root, "timeout", 10);

    if t > LIMIT_EXEC_TIMEOUT {
        st[10] = LIMIT_EXEC_TIMEOUT;
    } else {
        st[10] = t;
    }
    let m = tp_pget_int(p, root, "max_output_size", LIMIT_EXEC_OUTPUT);

    if m > LIMIT_EXEC_OUTPUT {
        st[11] = LIMIT_EXEC_OUTPUT;
    } else {
        st[11] = m;
    }
    return 0;
}

// write_file (server-tools.cpp:1350-1359): path and content required.
fn tv_write_file(p: const TPNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 || p_missing(p, root, "content") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path or content");
        return st[0];
    }
    let po, pl = tp_pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let co, cl = tp_pget_str(p, root, "content");

    keep_str(st, sbuf, p.sbuf, co, cl, 1);
    st[10] = cl;
    return 0;
}

// edit_file (server-tools.cpp:1407-1502): path and a non-empty edits array;
// every edit needs a non-empty old_text and a new_text string. The edit pairs
// are copied into sbuf; st[10] is the edit count and st[30 + 4*i ...] holds
// (old off, old len, new off, new len).
fn tv_edit_file(p: const TPNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 || p_missing(p, root, "edits") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path or edits");
        return st[0];
    }
    let po, pl = tp_pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let e = tp_pget(p, root, "edits");

    if e < 0 || p.jk[e] != TP_K_ARRAY || p.jn[e] == 0 {
        st[0] = 0 - 3;
        set_msg(st, msg, "\"edits\" must be a non-empty array");
        return st[0];
    }
    var at: i64 = st[18];
    var i: i64 = 0;

    while i < p.jn[e] {
        let item = p.jchild[p.jf[e] + i];
        var bad = false;

        if p.jk[item] != TP_K_OBJECT {
            bad = true;
        }
        let oo, ol = tp_pget_str(p, item, "old_text");

        if bad || oo < 0 {
            st[0] = 0 - 3;
            var m = tp_put(msg, 0, "edits[");

            m = tp_uint(msg, m, i);
            m = tp_put(msg, m, "].old_text must not be empty");
            st[19] = m;
            return st[0];
        }
        if ol == 0 {
            st[0] = 0 - 3;
            var m2 = tp_put(msg, 0, "edits[");

            m2 = tp_uint(msg, m2, i);
            m2 = tp_put(msg, m2, "].old_text must not be empty");
            st[19] = m2;
            return st[0];
        }
        let no, nl = tp_pget_str(p, item, "new_text");
        var no2 = no;
        var nl2 = nl;

        if no < 0 {
            no2 = 0;
            nl2 = 0;
        }
        st[30 + 4 * i + 0] = at;
        st[30 + 4 * i + 1] = ol;
        at = tp_copy(sbuf, at, p.sbuf, oo, ol);
        st[30 + 4 * i + 2] = at;
        st[30 + 4 * i + 3] = nl2;
        at = tp_copy(sbuf, at, p.sbuf, no2, nl2);
        i = i + 1;
    }
    st[10] = p.jn[e];
    st[18] = at;
    return 0;
}

// ── uniqueness and overlap, against the ORIGINAL content ────────────────────
// The fuzzy fallback of the native code (normalize_for_fuzzy_match) is out of
// this slice; only exact matching is implemented, and the analysis says so.
fn count_occurrences(hay: const u8[:], needle: const u8[:]) -> i64 {
    if needle.len == 0 || hay.len < needle.len {
        return 0;
    }
    var n: i64 = 0;
    var i: i64 = 0;

    while i + needle.len <= hay.len {
        var same = true;
        var k: i64 = 0;

        while k < needle.len {
            if hay[i + k] != needle[k] {
                same = false;
                k = needle.len;
            } else {
                k = k + 1;
            }
        }
        if same {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

fn find_at(hay: const u8[:], needle: const u8[:]) -> i64 {
    if needle.len == 0 || hay.len < needle.len {
        return 0 - 1;
    }
    var i: i64 = 0;

    while i + needle.len <= hay.len {
        var same = true;
        var k: i64 = 0;

        while k < needle.len {
            if hay[i + k] != needle[k] {
                same = false;
                k = needle.len;
            } else {
                k = k + 1;
            }
        }
        if same {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// Validate the edits of the last tool_validate call against the file content.
// Returns 0 or -3, with the native message in `msg`. st[11..] receives the
// match positions sorted by index (st[11 + 2*i] index, [12 + 2*i] length).
export fn tool_edit_check(
    st: i64[:],
    sbuf: const u8[:],
    content: const u8[:],
    msg: u8[:]) -> i64 {
    let n = st[10];
    let po = st[20];
    let pl = st[21];
    var order: i64[64] = [0; 64];
    var pos: i64[64] = [0; 64];
    var i: i64 = 0;

    while i < n {
        let oo = st[30 + 4 * i + 0];
        let ol = st[30 + 4 * i + 1];
        let occ = count_occurrences(content, sbuf[oo .. oo + ol]);

        if occ == 0 {
            st[0] = 0 - 3;
            var m = tp_put(msg, 0, "could not find edits[");

            m = tp_uint(msg, m, i);
            m = tp_put(msg, m, "].old_text in ");
            m = tp_copy(msg, m, sbuf, po, pl);
            m = tp_put(msg, m, ", it must match the file's current content exactly");
            st[19] = m;
            return st[0];
        }
        if occ > 1 {
            st[0] = 0 - 3;
            var m2 = tp_put(msg, 0, "found ");

            m2 = tp_uint(msg, m2, occ);
            m2 = tp_put(msg, m2, " occurrences of edits[");
            m2 = tp_uint(msg, m2, i);
            m2 = tp_put(msg, m2, "].old_text in ");
            m2 = tp_copy(msg, m2, sbuf, po, pl);
            m2 = tp_put(msg, m2, ", it must be unique");
            st[19] = m2;
            return st[0];
        }
        order[i] = i;
        pos[i] = find_at(content, sbuf[oo .. oo + ol]);
        i = i + 1;
    }
    // insertion sort by position (n is small)
    i = 1;
    while i < n {
        var j = i;

        while j > 0 && pos[order[j - 1]] > pos[order[j]] {
            let t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
            j = j - 1;
        }
        i = i + 1;
    }
    i = 1;
    while i < n {
        let a = order[i - 1];
        let b = order[i];

        if pos[a] + st[30 + 4 * a + 1] > pos[b] {
            st[0] = 0 - 3;
            var m3 = tp_put(msg, 0, "edits[");

            m3 = tp_uint(msg, m3, a);
            m3 = tp_put(msg, m3, "] and edits[");
            m3 = tp_uint(msg, m3, b);
            m3 = tp_put(msg, m3, "] overlap in ");
            m3 = tp_copy(msg, m3, sbuf, po, pl);
            m3 = tp_put(msg, m3, "; merge them into one edit or target disjoint regions");
            st[19] = m3;
            return st[0];
        }
        i = i + 1;
    }
    i = 0;
    while i < n {
        st[11 + 2 * i] = pos[i];
        st[12 + 2 * i] = st[30 + 4 * i + 1];
        i = i + 1;
    }
    return 0;
}

// ── read_file text, pure over the content bytes ─────────────────────────────

fn tp_esc(dst: u8[:], at_in: i64, s: const u8[:]) -> i64 {
    var at = at_in;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if c == 34 {
            at = tp_put(dst, at, "\\\"");
        } else {
            if c == 92 {
                at = tp_put(dst, at, "\\\\");
            } else {
                if c == 10 {
                    at = tp_put(dst, at, "\\n");
                } else {
                    if c == 13 {
                        at = tp_put(dst, at, "\\r");
                    } else {
                        if c == 9 {
                            at = tp_put(dst, at, "\\t");
                        } else {
                            if c < 32 {
                                at = tp_put(dst, at, "\\u00");
                                at = tp_chr(dst, at, (48 + ((c >> 4) & 15)) as u8);
                                at = tp_chr(dst, at, (48 + (c & 15)) as u8);
                            } else {
                                at = tp_chr(dst, at, c);
                            }
                        }
                    }
                }
            }
        }
        i = i + 1;
    }
    return at;
}

fn tp_chr(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

// server-tools.cpp:920-966: 1-based inclusive range, -1 for "to the end",
// an optional "N→" prefix, and a 16 KiB ceiling with "[output truncated]".
// `start_line` < 1 behaves like 1 (the loop skips nothing earlier).
export fn tool_read_text(
    content: const u8[:],
    start_line: i64,
    end_line: i64,
    append_loc: i64,
    out: u8[:]) -> i64 {
    var p: i64 = 0;
    var lineno: i64 = 0;
    var at: i64 = 0;
    var stopped = false;

    while at < content.len && !stopped {
        // one line, without the newline
        var e = at;

        while e < content.len && content[e] != 10 {
            e = e + 1;
        }
        lineno = lineno + 1;

        if lineno >= start_line {
            if end_line != 0 - 1 && lineno > end_line {
                stopped = true;
            } else {
                var line_at = p;

                if append_loc != 0 {
                    line_at = tp_uint(out, line_at, lineno);
                    line_at = tp_put(out, line_at, "\xe2\x86\x92");
                }
                line_at = tp_copy(out, line_at, content, at, e - at);
                line_at = tp_chr(out, line_at, 10);
                if line_at > LIMIT_READ_SIZE {
                    // the native adds the marker once the ceiling would be crossed
                    p = tp_put(out, p, "[output truncated]");
                    stopped = true;
                } else {
                    p = line_at;
                }
            }
        } else {
            if end_line != 0 - 1 && lineno > end_line {
                stopped = true;
            }
        }
        at = e + 1;
    }
    return p;
}

// server-tools.cpp:908-917, 924-928: the size guards before reading.
// Returns 0 when the read may proceed, 1 when it is refused (message in `msg`).
export fn tool_read_guard(
    file_size: i64,
    end_line: i64,
    as_base64: i64,
    path: const u8[:],
    stat_ok: i64,
    msg: u8[:]) -> i64 {
    var p = 0;

    if stat_ok == 0 {
        p = tp_put(msg, 0, "cannot stat file: ");
        p = tp_copy(msg, p, path, 0, path.len);
        return 1;
    }
    if as_base64 != 0 {
        if file_size > LIMIT_READ_SIZE_BASE64 {
            p = tp_put(msg, 0, "file too large (");
            p = tp_uint(msg, p, file_size);
            p = tp_put(msg, p, " bytes, max ");
            p = tp_uint(msg, p, LIMIT_READ_SIZE_BASE64);
            p = tp_put(msg, p, ")");
            return 1;
        }
        return 0;
    }
    if file_size > LIMIT_READ_SIZE && end_line == 0 - 1 {
        p = tp_put(msg, 0, "file too large (");
        p = tp_uint(msg, p, file_size);
        p = tp_put(msg, p, " bytes, max ");
        p = tp_uint(msg, p, LIMIT_READ_SIZE);
        p = tp_put(msg, p, "). Use start_line/end_line to read a portion.");
        return 1;
    }
    return 0;
}

// server-tools.cpp:1157-1165: the literal-mode escaping of a regex pattern.
export fn tool_literal_regex(pat: const u8[:], out: u8[:]) -> i64 {
    var p: i64 = 0;
    var i: i64 = 0;

    while i < pat.len {
        let c = pat[i];

        if c == 92 || c == 94 || c == 36 || c == 46 || c == 124 || c == 63 || c == 42 ||
           c == 43 || c == 40 || c == 41 || c == 91 || c == 93 || c == 123 || c == 125 {
            p = tp_chr(out, p, 92);
        }
        p = tp_chr(out, p, c);
        i = i + 1;
    }
    return p;
}

// server-tools.cpp:1303-1310: the exec tail.
export fn tool_exec_tail(
    output: const u8[:],
    exit_code: i64,
    timed_out: i64,
    out: u8[:]) -> i64 {
    var p = tp_copy(out, 0, output, 0, output.len);

    p = tp_put(out, p, "\n[exit code: ");
    p = tp_uint(out, p, exit_code);
    p = tp_chr(out, p, 93);
    if timed_out != 0 {
        p = tp_put(out, p, " [exit due to timed out]");
    }
    return p;
}

// ── result JSON, in the native key order ────────────────────────────────────
// nlohmann ordered_json keeps insertion order, so the emitters follow the
// object literals in the source exactly.

export fn tool_json_error(msg: const u8[:], out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"error\":\"");

    p = tp_esc(out, p, msg);
    p = tp_put(out, p, "\"}");
    return p;
}

export fn tool_json_plain(text: const u8[:], out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"plain_text_response\":\"");

    p = tp_esc(out, p, text);
    p = tp_put(out, p, "\"}");
    return p;
}

export fn tool_json_write(path: const u8[:], bytes: i64, out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"result\":\"file written successfully\",\"path\":\"");

    p = tp_esc(out, p, path);
    p = tp_put(out, p, "\",\"bytes\":");
    p = tp_uint(out, p, bytes);
    p = tp_chr(out, p, 125);
    return p;
}

export fn tool_json_edit(path: const u8[:], edits: i64, out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"result\":\"file edited successfully\",\"path\":\"");

    p = tp_esc(out, p, path);
    p = tp_put(out, p, "\",\"edits_applied\":");
    p = tp_uint(out, p, edits);
    p = tp_chr(out, p, 125);
    return p;
}

export fn tool_json_info(os: const u8[:], cwd: const u8[:], out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"os\":\"");

    p = tp_esc(out, p, os);
    p = tp_put(out, p, "\",\"cwd\":\"");
    p = tp_esc(out, p, cwd);
    p = tp_put(out, p, "\"}");
    return p;
}

export fn tool_json_base64(size_bytes: i64, out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"base64\":\"");

    p = tp_put(out, p, "\",\"size_bytes\":");
    p = tp_uint(out, p, size_bytes);
    p = tp_chr(out, p, 125);
    return p;
}

// server-tools.cpp:1060-1075: the glob summary tail.
export fn tool_glob_tail(
    total: i64,
    shown: i64,
    truncated: i64,
    out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "\n---\nTotal matches: ");

    p = tp_uint(out, p, total);
    p = tp_chr(out, p, 10);
    if total > shown {
        p = tp_chr(out, p, 91);
        p = tp_uint(out, p, shown);
        p = tp_put(out, p, " results limit reached (");
        p = tp_uint(out, p, total);
        p = tp_put(out, p, " total matches). Refine the glob pattern to narrow the search.]\n");
    }
    if truncated != 0 {
        p = tp_put(out, p, "[results truncated: time budget or unreadable directory]\n");
    }
    return p;
}

// ---- composed from goose/server/web-assets.goose ----
// P8-09 S2: the web UI serve decision - which asset a request gets, under
// which caching policy, with which headers.
//
// Native source: upstream/llama.cpp/tools/server/server-http.cpp at base commit
// ec50a32d, the embedded-asset branch of server_http_context::init:
//
//   handle_gzip_header        the 415 gate and Content-Encoding          341-355
//   cache_immutable / revalidate                                      357-360
//   serve_asset_cached        ETag, 304, isolation, Cache-Control      362-387
//   serve_asset_nocache       no ETag, no isolation                    389-401
//   index registration        "/" and "/index.html"                    403-404
//   no_cache_names            sw.js, manifest, version, build.json     409-414
//   remaining registrations   prefix + "/" + name, index skipped        416-425
//
// The module decides, for one request path and the build's asset table, the
// response shape: status, which asset, which of the five response headers are
// set, the gzip encoding flag, the isolation flag and the bytes of
// Cache-Control. Native rules, in native order:
//
//   - a route is prefix + "/" + name, an exact whole-path match; "/" and
//     "/index.html" are both the index asset;
//   - an unknown path is 404 and carries no headers at all. The native handlers
//     are registered per route, so an unregistered path never enters one: the
//     gzip gate below is not consulted and no Content-Encoding is set;
//   - a registered asset in a gzip build with no "gzip" in Accept-Encoding is
//     415 text/plain, before the ETag is looked at;
//   - the two cached shapes (the index and every immutable asset) get ETag, and
//     then If-None-Match equal to the ETag or to "W/" + ETag turns the answer
//     into 304 and returns immediately: a 304 therefore has ETag and
//     Content-Encoding but NO Cache-Control and NO cross-origin isolation;
//   - the four no_cache_names use serve_asset_nocache: no ETag at all, so their
//     If-None-Match is ignored and they always answer 200;
//   - only the index asset gets Cross-Origin-Embedder-Policy / -Opener-Policy;
//   - Cache-Control is "no-cache" for the index and for no_cache_names, and
//     "public, max-age=31536000, immutable" for everything else;
//   - Content-Encoding: gzip is set whenever the gzip build accepted the
//     request, including on 304; a non-gzip build never sets it;
//   - Content-Type is the asset type on 200 and text/plain on 415.
//
// The asset table arrives as one packed name blob plus per-asset offsets, and a
// parallel ETag blob plus offsets: the runtime owns the table, this module owns
// the decision. Header text goes into a caller buffer, never a static pointer.
//
// The prefix is web_. Not owned: sockets, threads, TLS, the mount point for
// --path (httplib does the file I/O there), UUID/templating, CORS, percent
// decoding, request queuing and the API routes.


// Response header bits, in the order native sets them.
const WEB_H_ETAG: i64 = 1;
const WEB_H_ISOLATION: i64 = 2;
const WEB_H_CACHE: i64 = 4;
const WEB_H_ENCODING: i64 = 8;
const WEB_H_TYPE: i64 = 16;

const WEB_OK: i64 = 200;
const WEB_NOT_MODIFIED: i64 = 304;
const WEB_NOT_FOUND: i64 = 404;
const WEB_UNSUPPORTED_MEDIA: i64 = 415;

const WEB_POLICY_IMMUTABLE: i64 = 0;
const WEB_POLICY_REVALIDATE: i64 = 1;
const WEB_POLICY_NOCACHE: i64 = 2;

const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000, immutable";
const WEB_CACHE_NO_CACHE: const u8[:] = "no-cache";

fn web_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn web_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// An asset name out of the packed table: names[off[i] .. off[i+1]] with the
// last entry ending at names.len. off has cnt + 1 entries.
fn web_name(names: const u8[:], off: const i64[:], i: i64) -> const u8[:] {
    let from: i64 = off[i];
    let to: i64 = off[i + 1];

    if from < 0 || to < from || to > names.len {
        return names[0 .. 0];
    }
    return names[from .. to];
}

fn web_name_is(names: const u8[:], off: const i64[:], i: i64, lit: const u8[:]) -> bool {
    return web_eq(web_name(names, off, i), lit);
}

// Index of "index.html" in the table, -1 when the bundle has none.
fn web_index_of(names: const u8[:], off: const i64[:], cnt: i64) -> i64 {
    var i: i64 = 0;

    while i < cnt {
        if web_name_is(names, off, i, "index.html") {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn web_starts(path: const u8[:], pre: const u8[:]) -> bool {
    if path.len < pre.len {
        return false;
    }
    var i: i64 = 0;

    while i < pre.len {
        if path[i] != pre[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// The native route table, as one decision: the asset index for a whole request
// path, or -1 for an unregistered path. "/" and "/index.html" resolve to the
// index asset; every other asset needs an exact prefix + "/" + name match.
// Goose refuses a call to an export fn from another function, so the body is
// private and web_serve below calls it directly; the export wrapper is the
// public surface.
fn web_route_idx(
    path: const u8[:],
    prefix: const u8[:],
    names: const u8[:],
    off: const i64[:],
    cnt: i64) -> i64 {
    if !web_starts(path, prefix) || path.len < prefix.len {
        return 0 - 1;
    }
    let rest = path[prefix.len .. path.len];

    // every registered route is prefix + "/" + something, index included
    if rest.len < 1 || rest[0] != 47 {
        return 0 - 1;
    }
    let idx = web_index_of(names, off, cnt);

    if rest.len == 1 {
        return idx;
    }
    if web_eq(rest, "/index.html") {
        return idx;
    }
    var i: i64 = 0;
    while i < cnt {
        // index.html is already handled above, so the table entry matches only
        // the unusable duplicate spelling
        if rest.len == web_name(names, off, i).len + 1 {
            if web_eq(rest[1 .. rest.len], web_name(names, off, i)) {
                return i;
            }
        }
        i = i + 1;
    }
    return 0 - 1;
}

export fn web_route(
    path: const u8[:],
    prefix: const u8[:],
    names: const u8[:],
    off: const i64[:],
    cnt: i64) -> i64 {
    return web_route_idx(path, prefix, names, off, cnt);
}

fn web_is(target: i64, id: i64) -> bool {
    return target == id;
}

// Cache policy id of an asset name, exactly the native registration split.
fn web_policy_of(names: const u8[:], off: const i64[:], idx: i64) -> i64 {
    if web_name_is(names, off, idx, "index.html") {
        return WEB_POLICY_REVALIDATE;
    }
    if web_name_is(names, off, idx, "sw.js")
        || web_name_is(names, off, idx, "manifest.webmanifest")
        || web_name_is(names, off, idx, "_app/version.json")
        || web_name_is(names, off, idx, "build.json") {
        return WEB_POLICY_NOCACHE;
    }
    return WEB_POLICY_IMMUTABLE;
}

// Accept-Encoding contains "gzip" (the native std::string::find).
fn web_accepts_gzip(accept: const u8[:]) -> bool {
    var i: i64 = 0;
    while i + 4 <= accept.len {
        if accept[i] == 103 && accept[i + 1] == 122 && accept[i + 2] == 105 && accept[i + 3] == 112 {
            return true;
        }
        i = i + 1;
    }
    return false;
}

// If-None-Match matches the asset ETag: exact, or the weak form "W/" + etag.
fn web_etag_matches(inm: const u8[:], etag: const u8[:]) -> bool {
    if inm.len == 0 {
        return false;
    }
    if web_eq(inm, etag) {
        return true;
    }
    if inm.len == etag.len + 2 && inm[0] == 87 && inm[1] == 47 && web_eq(inm[2 .. inm.len], etag) {
        return true;
    }
    return false;
}

fn web_etag_text(etags: const u8[:], eoff: const i64[:], i: i64) -> const u8[:] {
    let from: i64 = eoff[i];
    let to: i64 = eoff[i + 1];

    if from < 0 || to < from || to > etags.len {
        return etags[0 .. 0];
    }
    return etags[from .. to];
}

// The whole serve decision for one request path.
//
//   out[0] status          200, 304, 404 or 415
//   out[1] asset index     -1 when the path is not registered
//   out[2] header mask     WEB_H_* bits that native actually sets
//   out[3] encoding        1 when Content-Encoding: gzip is set
//   out[4] isolation       1 when the two cross-origin policies are set
//   out[5] policy          WEB_POLICY_* id; web_cache_text turns it into the
//                          Cache-Control bytes (unused on 304 and 404)
//
// The asset count is read from off.len - 1, so the tables cannot disagree with
// their own offsets. Returns out[0].
export fn web_serve(
    path: const u8[:],
    prefix: const u8[:],
    names: const u8[:],
    off: const i64[:],
    etags: const u8[:],
    eoff: const i64[:],
    use_gzip: i64,
    accept: const u8[:],
    inm: const u8[:],
    out: i64[:]) -> i64 {
    let cnt: i64 = off.len - 1;

    out[0] = WEB_NOT_FOUND;
    out[1] = 0 - 1;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
    out[5] = 0 - 1;

    // an unregistered path never reaches a native handler: 404, no headers
    let idx = web_route_idx(path, prefix, names, off, cnt);

    if idx < 0 {
        return out[0];
    }
    out[1] = idx;

    // the gzip gate of the registered handler
    if use_gzip != 0 {
        if !web_accepts_gzip(accept) {
            out[0] = WEB_UNSUPPORTED_MEDIA;
            out[2] = WEB_H_TYPE;
            return out[0];
        }
        out[2] = WEB_H_ENCODING;
        out[3] = 1;
    }

    let policy = web_policy_of(names, off, idx);

    // serve_asset_nocache has no ETag and no 304 branch
    if policy != WEB_POLICY_NOCACHE {
        out[2] = out[2] | WEB_H_ETAG;
        if web_etag_matches(inm, web_etag_text(etags, eoff, idx)) {
            out[0] = WEB_NOT_MODIFIED;
            return out[0];
        }
    }
    if web_name_is(names, off, idx, "index.html") {
        out[2] = out[2] | WEB_H_ISOLATION;
        out[4] = 1;
    }
    out[2] = out[2] | WEB_H_CACHE | WEB_H_TYPE;
    out[0] = WEB_OK;
    out[5] = policy;
    return out[0];
}

// Cache-Control text for a policy id, so a caller that only needs the header
// does not duplicate the two literals.
export fn web_cache_text(policy: i64, out: u8[:]) -> i64 {
    if policy == WEB_POLICY_IMMUTABLE {
        return web_put(out, 0, WEB_CACHE_IMMUTABLE);
    }
    if policy == WEB_POLICY_REVALIDATE || policy == WEB_POLICY_NOCACHE {
        return web_put(out, 0, WEB_CACHE_NO_CACHE);
    }
    return 0 - 1;
}

// Isolation decision on its own: only the index asset carries the two headers.
export fn web_isolation_of(names: const u8[:], off: const i64[:], idx: i64) -> i64 {
    if idx < 0 {
        return 0;
    }
    if web_name_is(names, off, idx, "index.html") {
        return 1;
    }
    return 0;
}

// ---- composed from goose/server/mime-table.goose ----
// P8-09 S1: the embedded UI asset table's naming and content types.
//
// Native source: upstream/llama.cpp/tools/ui/embed.cpp at base commit
// ec50a32d. The file is the generator for ui.cpp/ui.h, which embed the web UI
// as C arrays and are what the server serves; the three text decisions in it
// are owned here:
//
//   mime_from_ext   the extension table and the octet-stream fallback   20-40
//   path_basename   the last slash of an asset path                    113-116
//   required checks the eight labels a complete UI bundle must satisfy 178-196
//
// The rules exactly as native applies them:
//   - the extension is everything after the LAST '.' of the WHOLE asset name,
//     not of the basename: for the asset "_app/x/y" the last dot may live in a
//     directory component, and then the table sees "app/x/y" and misses;
//   - the extension compare is byte exact and case sensitive: "INDEX.HTML" and
//     ".HTML" are octet-stream, only lower-case names hit the table;
//   - a name with no '.' at all is octet-stream;
//   - an empty extension ("index.") is octet-stream;
//   - jpg and jpeg share one type, and both take precedence over nothing else;
//   - the required checks run on the basename (path_basename), so
//     "_app/immutable/index.html" satisfies the "index.html" label;
//   - bundle/workbox are prefix+suffix matches, not globs: "bundle.css.js"
//     satisfies both the .js label and, if it also ends in .css, no;
//     "x-bundle.js" satisfies neither because the prefix is not at the start.
//
// Asset type strings are 11 distinct byte strings plus the fallback; the ids
// are the table order below and every export resolves ids back to bytes, so no
// pointer into static storage crosses the boundary.
//
// The prefix is mime_. Not owned: walking the asset directory, gzip staging,
// emitting ui.cpp, FNV/ETag (S3), the serve decisions (S2).


const MIME_NONE: i64 = 0 - 1;

// Content-type ids. Application/octet-stream is also the fallback.
const MIME_HTML: i64 = 0;
const MIME_CSS: i64 = 1;
const MIME_JS: i64 = 2;
const MIME_JSON: i64 = 3;
const MIME_WEBMANIFEST: i64 = 4;
const MIME_SVG: i64 = 5;
const MIME_PNG: i64 = 6;
const MIME_JPEG: i64 = 7;
const MIME_ICO: i64 = 8;
const MIME_WOFF: i64 = 9;
const MIME_WOFF2: i64 = 10;
const MIME_OCTET: i64 = 11;

const MIME_N: i64 = 12;
const MIME_TEXT: const u8[:][12] = [
    "text/html; charset=utf-8",
    "text/css",
    "application/javascript",
    "application/json",
    "application/manifest+json",
    "image/svg+xml",
    "image/png",
    "image/jpeg",
    "image/x-icon",
    "font/woff",
    "font/woff2",
    "application/octet-stream"
];

// The twelve extension spellings of the native if-chain, in table order.
// jpg and jpeg both map to MIME_JPEG.
const MIME_EXT_N: i64 = 12;
const MIME_EXT: const u8[:][12] = [
    "html",
    "css",
    "js",
    "json",
    "webmanifest",
    "svg",
    "png",
    "jpg",
    "jpeg",
    "ico",
    "woff",
    "woff2"
];
const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10];

fn mime_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn mime_slice_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// name.rfind('.') from the end; -1 when the name has no dot.
fn mime_last_dot(name: const u8[:]) -> i64 {
    var i: i64 = name.len - 1;

    while i >= 0 {
        if name[i] == 46 {
            return i;
        }
        i = i - 1;
    }
    return 0 - 1;
}

fn mime_of_name(name: const u8[:]) -> i64 {
    let dot = mime_last_dot(name);

    if dot < 0 {
        return MIME_OCTET;
    }
    let ext = name[dot + 1 .. name.len];

    var i: i64 = 0;
    while i < MIME_EXT_N {
        if mime_slice_eq(ext, MIME_EXT[i]) {
            return MIME_EXT_ID[i];
        }
        i = i + 1;
    }
    return MIME_OCTET;
}

// Content-type id for an asset name, exactly mime_from_ext.
export fn mime_of(name: const u8[:]) -> i64 {
    return mime_of_name(name);
}

// The type string for an id; -1 on an unknown id so the caller keeps its own
// buffer rather than receiving a pointer into the table.
export fn mime_text(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= MIME_N {
        return MIME_NONE;
    }
    return mime_put(out, 0, MIME_TEXT[id]);
}

// path_basename: everything after the last '/', the whole name when there is
// no slash. An empty name stays empty.
export fn mime_base(name: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = name.len - 1;

    while at >= 0 {
        if name[at] == 47 {
            at = at + 1;
            break;
        }
        at = at - 1;
    }
    if at < 0 {
        at = 0;
    }
    return mime_put(out, 0, name[at .. name.len]);
}

fn mime_starts(base: const u8[:], pre: const u8[:]) -> bool {
    if base.len < pre.len {
        return false;
    }
    var i: i64 = 0;

    while i < pre.len {
        if base[i] != pre[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn mime_ends(base: const u8[:], suf: const u8[:]) -> bool {
    if base.len < suf.len {
        return false;
    }
    return mime_slice_eq(base[base.len - suf.len .. base.len], suf);
}

// The eight required-bundle labels of the native checks[] table, as a bitmask;
// bit i is set when the basename satisfies label i. The labels are ordered as
// in embed.cpp so a missing-label report keeps the native order.
const MIME_REQ_N: i64 = 8;
export fn mime_required(base: const u8[:]) -> i64 {
    var mask: i64 = 0;

    if mime_slice_eq(base, "index.html") {
        mask = mask | 1;
    }
    if mime_slice_eq(base, "manifest.webmanifest") {
        mask = mask | 2;
    }
    if mime_slice_eq(base, "sw.js") {
        mask = mask | 4;
    }
    if mime_slice_eq(base, "build.json") {
        mask = mask | 8;
    }
    if mime_slice_eq(base, "version.json") {
        mask = mask | 16;
    }
    if mime_starts(base, "bundle") && mime_ends(base, ".js") {
        mask = mask | 32;
    }
    if mime_starts(base, "bundle") && mime_ends(base, ".css") {
        mask = mask | 64;
    }
    if mime_starts(base, "workbox") && mime_ends(base, ".js") {
        mask = mask | 128;
    }
    return mask;
}

// The label text for a bit index, so the caller can print the native missing
// list without owning the strings.
export fn mime_required_label(bit: i64, out: u8[:]) -> i64 {
    if bit < 0 || bit >= MIME_REQ_N {
        return MIME_NONE;
    }
    if bit == 0 {
        return mime_put(out, 0, "index.html");
    }
    if bit == 1 {
        return mime_put(out, 0, "manifest.webmanifest");
    }
    if bit == 2 {
        return mime_put(out, 0, "sw.js");
    }
    if bit == 3 {
        return mime_put(out, 0, "build.json");
    }
    if bit == 4 {
        return mime_put(out, 0, "version.json");
    }
    if bit == 5 {
        return mime_put(out, 0, "bundle[hash].js");
    }
    if bit == 6 {
        return mime_put(out, 0, "bundle[hash].css");
    }
    return mime_put(out, 0, "workbox[hash].js");
}

// ---- composed from goose/server/cors-policy.goose ----
// P8-09 S4: the CORS decision of the server's pre-routing handler, plus the
// localhost origin test it uses.
//
// Native source: upstream/llama.cpp/tools/server/server-http.cpp at base commit
// 050354e9:
//
//   origin_is_localhost              50-58   host == localhost / 127.0.0.1 / ::1
//   cors_origins == "localhost"     281-288   echo only a localhost origin
//   cors_credentials && origins "*" 278-280   echo any origin
//   static origin                   289-291   set the configured literal
//   OPTIONS preflight               292-299   credentials/methods/headers, blank
//                                             text/html body, handled early
//
// The module owns the branch decision and the three literals that belong to it:
// the credentials text, the preflight content type and the blank body. The
// allowed methods and allowed headers are configured values the native code
// copies verbatim, so they stay with the caller and are not an export here.
//
// Native rules, in native order:
//
//   - credentials set and cors_origins == "*" is the wildcard-with-credentials
//     case: the Origin header is echoed back as Access-Control-Allow-Origin even
//     when it is empty, so the header is always set and may carry zero bytes;
//   - cors_origins == "localhost": the header is set to the Origin value only
//     when the origin is non-empty and its host is localhost / 127.0.0.1 / ::1;
//     a non-empty origin that fails the test sets no header and logs a warning;
//     an empty origin is silent;
//   - every other value of cors_origins is a static Access-Control-Allow-Origin,
//     set unconditionally, empty string included;
//   - the preflight branch is an exact "OPTIONS" method match (case sensitive);
//     it sets credentials, methods and headers, sets an empty text/html body and
//     returns "handled" so no route runs.
//
// origin_is_localhost native behaviour: common_http_parse_url(origin) may throw
// (no scheme, unsupported scheme, bad port); the catch is the only other exit
// and it returns false. The host is then compared byte-for-byte against the
// three literals, case sensitively, so "LOCALHOST" and "localhost." are false
// and a bracketed IPv6 literal is unbracketed by the parser before the compare.
//
// That parse is owned by goose/server/http-url-policy.goose. This module never
// parses a URL: it takes the parsed host and a "parsed" flag, so a caller that
// caught the native exception passes parsed = 0 and gets the native false. The
// port therefore cannot re-derive the host slice and cannot disagree with the
// URL policy module.
//
// The prefix is cors_. Not owned: sockets, the Origin header lookup, the
// response object, the warning log line, header ordering, and the placeholder
// response body object.


const CORS_CREDENTIALS_TRUE: const u8[:] = "true";
const CORS_CREDENTIALS_FALSE: const u8[:] = "false";
const CORS_PREFLIGHT_TYPE: const u8[:] = "text/html";

// out[at ..] = s, and -1 when the caller buffer cannot hold the text.
fn cors_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    if at < 0 || at + s.len > out.len {
        return 0 - 1;
    }
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn cors_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// The host comparison of origin_is_localhost, on the host slice that
// http-url-policy already produced. parsed = 0 is the native catch: false.
export fn cors_is_localhost(host: const u8[:], parsed: i64) -> i64 {
    if parsed == 0 {
        return 0;
    }
    if cors_eq(host, "localhost") {
        return 1;
    }
    if cors_eq(host, "127.0.0.1") {
        return 1;
    }
    if cors_eq(host, "::1") {
        return 1;
    }
    return 0;
}

// Access-Control-Allow-Origin for one request.
//
//   res[0] = 1 when the header is set, 0 when it is left out
//   res[1] = 1 when native logs "(CORS) skip non-localhost origin"
//   out    = the header value bytes
//
// Returns the number of value bytes written to out, or -1 when no header is set
// (a header value may legitimately be zero bytes: the wildcard-with-credentials
// branch echoes an absent Origin). origin_local is cors_is_localhost over the
// parsed host of this same origin.
export fn cors_allow_origin(
    origins: const u8[:],
    credentials: i64,
    origin: const u8[:],
    origin_local: i64,
    out: u8[:],
    res: i64[:]) -> i64 {
    res[0] = 0;
    res[1] = 0;

    if credentials != 0 && cors_eq(origins, "*") {
        let n = cors_put(out, 0, origin);

        // the header is claimed only when its value made it into the buffer
        if n >= 0 {
            res[0] = 1;
        }
        return n;
    }
    if cors_eq(origins, "localhost") {
        if origin.len != 0 && origin_local != 0 {
            let n = cors_put(out, 0, origin);

            if n >= 0 {
                res[0] = 1;
            }
            return n;
        }
        if origin.len != 0 {
            res[1] = 1;
        }
        return 0 - 1;
    }
    let n = cors_put(out, 0, origins);

    if n >= 0 {
        res[0] = 1;
    }
    return n;
}

// The native `req.method == "OPTIONS"`, exact and case sensitive.
export fn cors_is_preflight(method: const u8[:]) -> i64 {
    if cors_eq(method, "OPTIONS") {
        return 1;
    }
    return 0;
}

// Access-Control-Allow-Credentials text, the native `? "true" : "false"`.
export fn cors_credentials_text(credentials: i64, out: u8[:]) -> i64 {
    if credentials != 0 {
        return cors_put(out, 0, CORS_CREDENTIALS_TRUE);
    }
    return cors_put(out, 0, CORS_CREDENTIALS_FALSE);
}

// Content type of the preflight placeholder response.
export fn cors_preflight_type(out: u8[:]) -> i64 {
    return cors_put(out, 0, CORS_PREFLIGHT_TYPE);
}
