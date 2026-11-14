// P9-07: metrics/traces and deterministic workload infrastructure.
//
// Native sources, all in the carrier:
//   runtime/core/src/metrics/decode_trace_sink.cpp  — the shared CSV field encoder
//                                                     (write_csv_field, 19-30) and the two
//                                                     trace row encoders (45-55, 74-83);
//   runtime/core/src/metrics/route_trace_sink.cpp   — the route row encoder including the
//                                                     NaN arm (33-48);
//   runtime/core/src/metrics/csv_metrics_sink.cpp   — the per-token column list (128-131)
//                                                     and the run-summary key list (73-110);
//   runtime/core/src/moe/deterministic_workload.cpp — the capture/replay validation
//                                                     decisions (load 13-79, replay_topk
//                                                     131-146, token 148-159, finish 161-172).
//
// Scope: the text formats and the validation decisions only. No model, no graph, no file
// I/O policy. Float-to-text conversion for one column (`%.6g` in the route weight) stays
// libc through the `mt_fmt_g6` extern; everything else — the CSV escaping, the integer
// fields, the column layout, the workload checks — is computed here.
//
// Owned decisions, in the native order:
//   - a field is quoted iff it holds ',', '"' or '\n'. A '\r' is NOT quoted and travels
//     raw (the stdlib csv writer quotes it; this engine's does not, and the difference is
//     observable in a CRLF name);
//   - inside a quoted field a '"' is doubled (RFC4180) and a '\n' becomes one space, in
//     that order of application;
//   - a route weight the graph did not expose prints the literal `nan`, never `0`, so
//     "unknown" cannot read as "the router gave this expert no mass";
//   - row widths are 9 (compute), 12 (io) and 10 (route) columns; the metrics CSV has 29
//     per-token columns and its summary trailer has 62 key=value tokens;
//   - a deterministic workload header is `BMOE_DETERMINISTIC_WORKLOAD 1`, both parts
//     required; metadata keys are arch/layers/top_k in that order;
//   - a route record is accepted iff `n_ids == (size_t) top_k * (size_t) token_n` with
//     unsigned width (a negative top_k becomes a huge unsigned value, as in the carrier);
//   - a replayed route must match phase/base/batch/layer/top_k/tokens exactly; the
//     replay cursor must stay inside the record count;
//   - a replay token index must be below the captured count; a capture token index must
//     equal the count so the file stays sequential;
//   - finish() checks routes before tokens, so a run short on both reports routes;
//   - a replay reads its prompt from the file and a capture overwrites it: only a replay
//     can mismatch.

import std;

// mt_weight_kind results.
const MT_WEIGHT_NAN_TEXT: i64 = 0;   // route_trace_sink.cpp:38
const MT_WEIGHT_FORMATTED: i64 = 1;  // route_trace_sink.cpp:43

// mt_trace_cols kinds.
const MT_TRACE_COMPUTE: i64 = 0;
const MT_TRACE_IO: i64 = 1;
const MT_TRACE_ROUTE: i64 = 2;

// mt_wl_finish_code results.
const MT_WL_OK: i64 = 0;
const MT_WL_ROUTES: i64 = 1; // route_cursor != routes.size()
const MT_WL_TOKENS: i64 = 2; // emitted != output_tokens.size()

// csv_metrics_sink.cpp:128-131 and 73-110 — the two fixed widths of the metrics CSV.
const MT_METRICS_COLS: i64 = 29;
const MT_SUMMARY_KEYS: i64 = 62;

// The one conversion that is not ported: libc owns binary-to-decimal for a double.
// The shim writes at `at` inside `out` and returns the characters written, so the port
// still owns where the column sits and how wide the surrounding row is.
extern "mt_fmt_g6" fn mt_fmt_g6(out: u8[:], at: i64, x: f64) -> i64;

fn mt_put(out: u8[:], pos: i64, c: u8) -> i64 {
    if pos < out.len {
        out[pos] = c;
    }
    return pos + 1;
}

fn mt_put_lit(out: u8[:], pos: i64, s: const u8[:]) -> i64 {
    var p: i64 = pos;
    for c in s {
        p = mt_put(out, p, c);
    }
    return p;
}

// Unsigned decimal, most significant digit first. 20 digits fit u64; the buffer is padded.
fn mt_put_u64(out: u8[:], pos: i64, v: u64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x: u64 = v;
    if x == 0 {
        buf[0] = '0';
        n = 1;
    }
    while x > 0 {
        buf[n] = '0' + (x % 10) as u8;
        n = n + 1;
        x = x / 10;
    }
    var p: i64 = pos;
    var i: i64 = n - 1;
    while i >= 0 {
        p = mt_put(out, p, buf[i]);
        i = i - 1;
    }
    return p;
}

// Signed decimal, the `%d` / `%lld` width of the native rows.
fn mt_put_i64(out: u8[:], pos: i64, v: i64) -> i64 {
    if v < 0 {
        var p: i64 = mt_put(out, pos, '-');
        return mt_put_u64(out, p, (0 as u64) - (v as u64));
    }
    return mt_put_u64(out, pos, v as u64);
}

// decode_trace_sink.cpp:20 — the exact quote predicate: ',' '"' '\n'. '\r' is absent on
// purpose, and an empty field needs no quotes. Shared by the encoder and the standalone
// export below; the export is a leaf so the compiler has one concrete signature for it.
fn mt_quote_needed(v: const u8[:]) -> i64 {
    for c in v {
        if c == ',' || c == '"' || c == '\n' {
            return 1;
        }
    }
    return 0;
}

// decode_trace_sink.cpp:19-30 — write_csv_field at `at` inside a caller buffer. Returns the
// position after the field; a short buffer is filled but the returned position stays
// truthful. The row encoders call this with their running position, which is why the
// offset is a parameter rather than a sub-slice: an exported function may only have one
// concrete signature, and a temporary view would give it a second one.
fn mt_field_into(v: const u8[:], out: u8[:], at: i64) -> i64 {
    if mt_quote_needed(v) == 0 {
        return mt_put_lit(out, at, v);
    }
    var p: i64 = mt_put(out, at, '"');
    for c in v {
        if c == '"' {
            p = mt_put(out, p, '"');
            p = mt_put(out, p, '"');
        } else if c == '\n' {
            p = mt_put(out, p, ' ');
        } else {
            p = mt_put(out, p, c);
        }
    }
    return mt_put(out, p, '"');
}

// The standalone predicate, for a reader that wants the decision without the bytes.
export fn mt_field_needs_quote(v: const u8[:]) -> i64 {
    return mt_quote_needed(v);
}

// The exported form: the whole field at the start of `out`, for a C host that owns one
// buffer per column.
export fn mt_field_encode(v: const u8[:], out: u8[:]) -> i64 {
    return mt_field_into(v, out, 0);
}

// route_trace_sink.cpp:38,43 — NaN takes the literal arm, anything else the numeric one.
// `w != w` is the only NaN test that needs no library call.
export fn mt_weight_kind(w: f64) -> i64 {
    if w != w {
        return MT_WEIGHT_NAN_TEXT;
    }
    return MT_WEIGHT_FORMATTED;
}

// The row widths the three trace headers pin (decode_trace_sink.cpp:41,70;
// route_trace_sink.cpp:29).
export fn mt_trace_cols(kind: i64) -> i64 {
    if kind == MT_TRACE_COMPUTE {
        return 9;
    }
    if kind == MT_TRACE_IO {
        return 12;
    }
    if kind == MT_TRACE_ROUTE {
        return 10;
    }
    return 0;
}

export fn mt_metrics_cols() -> i64 {
    return MT_METRICS_COLS;
}

export fn mt_summary_keys() -> i64 {
    return MT_SUMMARY_KEYS;
}

// decode_trace_sink.cpp:45-55 — one compute row (nine columns):
//   "%d,%d,%d,%d,%d," + escaped op + "," + escaped name + ",%llu,%llu\n"
export fn mt_compute_row_encode(turn: i64, phase: i64, step: i64, seq: i64, layer: i64, op: const u8[:],
                                name: const u8[:], wall_ns: i64, majflt: i64, out: u8[:]) -> i64 {
    var p: i64 = mt_put_i64(out, 0, turn);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, phase);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, step);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, seq);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, layer);
    p = mt_put(out, p, ',');
    p = mt_field_into(op, out, p);
    p = mt_put(out, p, ',');
    p = mt_field_into(name, out, p);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, wall_ns as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, majflt as u64);
    return mt_put(out, p, '\n');
}

// decode_trace_sink.cpp:74-83 — one io row, all twelve columns integer:
//   "%d,%d,%d,%d,%d,%d,%d,%u,%llu,%llu,%llu,%llu\n"
export fn mt_io_row_encode(turn: i64, phase: i64, step: i64, layer: i64, expert: i64, proj: i64, lane: i64,
                           spec: i64, offset: i64, req_bytes: i64, read_bytes: i64, latency_ns: i64,
                           out: u8[:]) -> i64 {
    var p: i64 = mt_put_i64(out, 0, turn);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, phase);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, step);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, layer);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, expert);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, proj);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, lane);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, spec as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, offset as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, req_bytes as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, read_bytes as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, latency_ns as u64);
    return mt_put(out, p, '\n');
}

// route_trace_sink.cpp:33-48 — one route row. Columns turn..expert, then the weight
// column (nan literal or %.6g), then residency, expert_bytes, dropped.
export fn mt_route_row_encode(turn: i64, phase: i64, step: i64, layer: i64, slot: i64, expert: i64,
                              weight: f64, residency: i64, expert_bytes: i64, dropped: i64,
                              out: u8[:]) -> i64 {
    var p: i64 = mt_put_i64(out, 0, turn);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, phase);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, step);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, layer);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, slot);
    p = mt_put(out, p, ',');
    p = mt_put_i64(out, p, expert);
    p = mt_put(out, p, ',');
    if mt_weight_kind(weight) == MT_WEIGHT_NAN_TEXT {
        p = mt_put_lit(out, p, "nan");
    } else {
        p = p + mt_fmt_g6(out, p, weight);
    }
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, residency as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, expert_bytes as u64);
    p = mt_put(out, p, ',');
    p = mt_put_u64(out, p, dropped as u64);
    return mt_put(out, p, '\n');
}

// A reader's column count: commas outside quotes, plus one. A doubled quote toggles the
// state twice and therefore cannot desynchronise the scan.
export fn mt_count_cols(line: const u8[:]) -> i64 {
    var n: i64 = 1;
    var inq: i64 = 0;
    for c in line {
        if c == '"' {
            inq = 1 - inq;
        } else if c == ',' && inq == 0 {
            n = n + 1;
        }
    }
    return n;
}

// RFC4180 unescape of one raw field: an unquoted field is copied as it is, a quoted one
// loses its quotes, collapses `""` to `"`, and keeps everything else.
export fn mt_unescape_field(field: const u8[:], out: u8[:]) -> i64 {
    if field.len == 0 {
        return 0;
    }
    if field[0] != '"' {
        return mt_put_lit(out, 0, field);
    }
    var p: i64 = 0;
    var i: i64 = 1;
    let last: i64 = field.len - 1;
    while i < last {
        let c: u8 = field[i];
        if c == '"' && i + 1 < last && field[i + 1] == '"' {
            p = mt_put(out, p, '"');
            i = i + 2;
        } else {
            p = mt_put(out, p, c);
            i = i + 1;
        }
    }
    return p;
}

// deterministic_workload.cpp:21 — the header contract: the magic matches and the version
// is exactly 1. A missing or different version is not a workload this engine can replay.
export fn mt_wl_header_ok(magic_match: i64, version: i64) -> i64 {
    if magic_match == 0 {
        return 0;
    }
    if version == 1 {
        return 1;
    }
    return 0;
}

// deterministic_workload.cpp:133 — the replay cursor must stay below the record count;
// the record it points at is then expected to match.
export fn mt_wl_next_route_ok(cursor: i64, count: i64) -> i64 {
    if cursor >= count {
        return 0;
    }
    return 1;
}

// deterministic_workload.cpp:66 — `n_ids != (size_t) r.top_k * (size_t) r.token_n`.
// The multiplication and the comparison are unsigned in the carrier, so a negative top_k
// is a huge count and never matches a small n_ids; this keeps that width.
export fn mt_wl_route_nids_ok(n_ids: i64, top_k: i64, token_n: i64) -> i64 {
    let want: u64 = (top_k as u64) * (token_n as u64);
    if (n_ids as u64) == want {
        return 1;
    }
    return 0;
}

// deterministic_workload.cpp:186-188 — a route line is
//   "route <phase> <base> <batch> <layer> <top_k> <token_n> <n_ids> <ids...>"
// i.e. eight leading words plus the ids. Used to check a saved file without a parser.
export fn mt_wl_route_words(top_k: i64, token_n: i64) -> i64 {
    return 8 + top_k * token_n;
}

// deterministic_workload.cpp:135-136 — the six-field shape match, in the native order.
// Returns 1 for a match, 0 otherwise; the caller reports the record index.
export fn mt_wl_route_shape_ok(exp_phase: i64, exp_base: i64, exp_batch: i64, exp_layer: i64, exp_top_k: i64,
                               exp_token_n: i64, got_phase: i64, got_base: i64, got_batch: i64,
                               got_layer: i64, got_top_k: i64, got_token_n: i64) -> i64 {
    if exp_phase != got_phase || exp_base != got_base || exp_batch != got_batch {
        return 0;
    }
    if exp_layer != got_layer || exp_top_k != got_top_k || exp_token_n != got_token_n {
        return 0;
    }
    return 1;
}

// deterministic_workload.cpp:103-108,136-143 — a route record must have the same top_k and
// token_n as the routing being captured/replayed; the native check is part of the shape
// comparison above, and this predicate is the standalone form the hook uses before it
// takes the ids pointer.
export fn mt_wl_route_take_ok(shape_ok: i64, cursor_ok: i64) -> i64 {
    if cursor_ok == 0 {
        return 0;
    }
    if shape_ok == 0 {
        return 0;
    }
    return 1;
}

// deterministic_workload.cpp:107 — a replay compares the tokenized prompt with the
// captured one; a capture overwrites it and cannot mismatch.
export fn mt_wl_prompt_ok(replay: i64, equal: i64) -> i64 {
    if replay == 0 {
        return 1;
    }
    if equal != 0 {
        return 1;
    }
    return 0;
}

// deterministic_workload.cpp:150-158 — capture appends only at the current length (the
// file must stay a sequential list); replay only reads indices inside the captured count.
export fn mt_wl_token_index_ok(replay: i64, index: i64, count: i64) -> i64 {
    if replay != 0 {
        if index >= count {
            return 0;
        }
        return 1;
    }
    if index == count {
        return 1;
    }
    return 0;
}

// deterministic_workload.cpp:161-172 — routes first, tokens second, first failure wins.
export fn mt_wl_finish_code(route_cursor: i64, route_n: i64, emitted: i64, output_n: i64) -> i64 {
    if route_cursor != route_n {
        return MT_WL_ROUTES;
    }
    if emitted != output_n {
        return MT_WL_TOKENS;
    }
    return MT_WL_OK;
}

// deterministic_workload.cpp:92-99 — arch, layer count and effective top-k must all match;
// one mismatch is a workload/model mismatch.
export fn mt_wl_static_ok(arch_eq: i64, layer_eq: i64, topk_eq: i64) -> i64 {
    if arch_eq != 0 && layer_eq != 0 && topk_eq != 0 {
        return 1;
    }
    return 0;
}
