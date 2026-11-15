// P10-02 S1: off-line decisions of the maintained llama-bench tool.
//
// Native source: upstream/llama.cpp/tools/llama-bench/llama-bench.cpp at the
// base commit 137bc40c. Everything ported here runs before or after a model is
// loaded and needs no model, no backend and no thread:
//
//   output_format_str / output_format_from_str        219-252
//   split_mode_str / lazy_mode_str                    254-287
//   parse_int_range                                   293-335
//   avg<T> / stdev<T>                                 102-121
//   cmd_params_defaults (the scalar half)             380-423
//   test::get_field_type                              1631-1650
//   csv_printer::escape_csv                           1759-1774
//   escape_json / format_json_value                   1784-1817
//   markdown_printer::get_field_width                 1867-1920
//   markdown_printer::get_field_display_name          1922-1972
//   markdown_printer::print_test (size/params/test/t/s rows) 2065-2105
//   sql_printer::get_sql_field_type                   2113-2127
//   tensor_split row                                  1652-1668
//
// The prefix is lb_. Numbers are written by this module; the only borrowed
// primitive is "%.2f", declared extern and supplied by the gate (the same
// arrangement as gs_json_fmt_e in the accepted Jinja/JSON modules), because
// decimal rounding is the C library and not a decision of the tool.
//
// Decisions that are deliberately exact:
//   - format names are lower case and compared with ==, so "MD" is refused;
//   - get_field_type checks its integer list first, so no_kv_offload is INT
//     even though it also appears in the boolean list, and f16_kv is BOOL
//     although no printed field has that name;
//   - escape_json walks a signed char: a byte above 0x7f takes the control
//     branch and is formatted into an 8-byte buffer, so it emits the seven
//     bytes "\ufffff" (the \u%04x result of a negative int is truncated);
//   - avg/stdev of uint64 samples use integer arithmetic, and stdev truncates
//     sqrt to uint64; the gate keeps samples where every intermediate stays
//     below 2^63, which is the domain the tool measures in;
//   - parse_int_range accepts ',' as a separator, '-' as first/last and '+'
//     or '*' as step operators; '|' also passes the character class and then
//     fails with "invalid range format"; a first value above the last one
//     yields an empty list, not an error; an empty string yields an empty
//     list; a step that does not advance yields "invalid range".

import math;

extern "fg637_fmt2" fn fmt2(out: u8[:], x: f64) -> i64;

const LB_STRING: i64 = 0;
const LB_BOOL: i64 = 1;
const LB_INT: i64 = 2;
const LB_FLOAT: i64 = 3;

// ── small text helpers ─────────────────────────────────────────────────────

fn lb_copy(s: const u8[:], out: u8[:], at0: i64) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len && at < out.len {
        out[at] = s[i];
        at = at + 1;
        i = i + 1;
    }
    return at;
}

fn lb_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn lb_eq(s: const u8[:], lit: const u8[:]) -> bool {
    return s == lit;
}

// Signed decimal, the way std::to_string writes an integer.
fn lb_put_int(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        at = lb_putc(out, at, 45);
        if v == (0 - 9223372036854775807 - 1) {
            return lb_copy("9223372036854775808", out, at);
        }
        v = 0 - v;
    }
    if v == 0 {
        return lb_putc(out, at, 48);
    }
    var buf: i64[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while v > 0 {
        buf[n] = v % 10;
        v = v / 10;
        n = n + 1;
    }
    var i: i64 = n;
    while i > 0 {
        i = i - 1;
        at = lb_putc(out, at, (48 + buf[i]) as u8);
    }
    return at;
}

fn lb_put_hex4(out: u8[:], at0: i64, v: i64) -> i64 {
    var at: i64 = at0;
    var d: i64 = 12;
    while d >= 0 {
        let nib: i64 = (v / (1 << d)) % 16;
        if nib < 10 {
            at = lb_putc(out, at, (48 + nib) as u8);
        } else {
            at = lb_putc(out, at, (87 + nib) as u8);
        }
        d = d - 4;
    }
    return at;
}

// ── output format selection (219-252) ──────────────────────────────────────

fn lb_out_from(s: const u8[:]) -> i64 {
    if lb_eq(s, "none") {
        return 0;
    }
    if lb_eq(s, "csv") {
        return 1;
    }
    if lb_eq(s, "json") {
        return 2;
    }
    if lb_eq(s, "jsonl") {
        return 3;
    }
    if lb_eq(s, "md") {
        return 4;
    }
    if lb_eq(s, "sql") {
        return 5;
    }
    return 0 - 1;
}

fn lb_out_name(v: i64) -> const u8[:] {
    if v == 0 {
        return "none";
    }
    if v == 1 {
        return "csv";
    }
    if v == 2 {
        return "json";
    }
    if v == 3 {
        return "jsonl";
    }
    if v == 4 {
        return "md";
    }
    if v == 5 {
        return "sql";
    }
    return "";
}

export fn lb_out_from_str(s: const u8[:]) -> i64 {
    return lb_out_from(s);
}

export fn lb_out_str(v: i64, out: u8[:]) -> i64 {
    let n: const u8[:] = lb_out_name(v);
    if n.len == 0 {
        return 0 - 1;
    }
    return lb_copy(n, out, 0);
}

// ── split and lazy mode names (254-287) ────────────────────────────────────

fn lb_split_name(v: i64) -> const u8[:] {
    if v == 0 {
        return "none";
    }
    if v == 1 {
        return "layer";
    }
    if v == 2 {
        return "row";
    }
    if v == 3 {
        return "tensor";
    }
    return "";
}

fn lb_lazy_name(v: i64) -> const u8[:] {
    if v == 0 {
        return "off";
    }
    if v == 1 {
        return "auto";
    }
    if v == 2 {
        return "on";
    }
    return "";
}

export fn lb_split_str(v: i64, out: u8[:]) -> i64 {
    let n: const u8[:] = lb_split_name(v);
    if n.len == 0 {
        return 0 - 1;
    }
    return lb_copy(n, out, 0);
}

export fn lb_lazy_str(v: i64, out: u8[:]) -> i64 {
    let n: const u8[:] = lb_lazy_name(v);
    if n.len == 0 {
        return 0 - 1;
    }
    return lb_copy(n, out, 0);
}

// ── aggregation (102-121) ──────────────────────────────────────────────────

fn lb_mean_u64(vals: const i64[:]) -> i64 {
    if vals.len == 0 {
        return 0;
    }
    var sum: i64 = 0;
    var i: i64 = 0;
    while i < vals.len {
        sum = sum + vals[i];
        i = i + 1;
    }
    return sum / vals.len;
}

fn lb_sd_u64(vals: const i64[:]) -> i64 {
    if vals.len <= 1 {
        return 0;
    }
    let mean: i64 = lb_mean_u64(vals);
    var sq_sum: i64 = 0;
    var i: i64 = 0;
    while i < vals.len {
        sq_sum = sq_sum + vals[i] * vals[i];
        i = i + 1;
    }
    // Native: sq_sum/(n-1) - mean*mean*n/(n-1), every division truncating.
    let a: i64 = sq_sum / (vals.len - 1);
    let b: i64 = (mean * mean * vals.len) / (vals.len - 1);
    let d: f64 = a - b;
    return sqrt(d) as i64;
}

fn lb_ts_one(t: i64, ntok: i64) -> f64 {
    return 1000000000.0 * ntok / t;
}

fn lb_mean_f64(vals: const i64[:], ntok: i64) -> f64 {
    if vals.len == 0 {
        return 0.0;
    }
    var sum: f64 = 0.0;
    var i: i64 = 0;
    while i < vals.len {
        sum = sum + lb_ts_one(vals[i], ntok);
        i = i + 1;
    }
    return sum / vals.len;
}

fn lb_sd_f64(vals: const i64[:], ntok: i64) -> f64 {
    if vals.len <= 1 {
        return 0.0;
    }
    let mean: f64 = lb_mean_f64(vals, ntok);
    var sq_sum: f64 = 0.0;
    var i: i64 = 0;
    while i < vals.len {
        let t: f64 = lb_ts_one(vals[i], ntok);
        sq_sum = sq_sum + t * t;
        i = i + 1;
    }
    let n: f64 = vals.len;
    return sqrt(sq_sum / (n - 1.0) - mean * mean * n / (n - 1.0));
}

export fn lb_avg_u64(vals: const i64[:]) -> i64 {
    return lb_mean_u64(vals);
}

export fn lb_stdev_u64(vals: const i64[:]) -> i64 {
    return lb_sd_u64(vals);
}

export fn lb_avg_ts(samples: const i64[:], ntok: i64) -> f64 {
    return lb_mean_f64(samples, ntok);
}

export fn lb_stdev_ts(samples: const i64[:], ntok: i64) -> f64 {
    return lb_sd_f64(samples, ntok);
}

// ── test::get_field_type (1631-1650) ───────────────────────────────────────

fn lb_type_of(field: const u8[:]) -> i64 {
    if lb_eq(field, "build_number") || lb_eq(field, "n_batch") || lb_eq(field, "n_ubatch") || lb_eq(field, "n_threads") ||
        lb_eq(field, "poll") || lb_eq(field, "model_size") || lb_eq(field, "model_n_params") || lb_eq(field, "n_gpu_layers") ||
        lb_eq(field, "main_gpu") || lb_eq(field, "n_prompt") || lb_eq(field, "n_gen") || lb_eq(field, "n_depth") ||
        lb_eq(field, "avg_ns") || lb_eq(field, "stddev_ns") || lb_eq(field, "no_op_offload") || lb_eq(field, "n_cpu_moe") ||
        lb_eq(field, "fit_target") || lb_eq(field, "fit_min_ctx") || lb_eq(field, "flash_attn") {
        return LB_INT;
    }
    if lb_eq(field, "f16_kv") || lb_eq(field, "no_kv_offload") || lb_eq(field, "cpu_strict") ||
        lb_eq(field, "embeddings") || lb_eq(field, "no_host") {
        return LB_BOOL;
    }
    if lb_eq(field, "avg_ts") || lb_eq(field, "stddev_ts") {
        return LB_FLOAT;
    }
    return LB_STRING;
}

export fn lb_field_type(field: const u8[:]) -> i64 {
    return lb_type_of(field);
}

// ── CSV and JSON escaping (1770-1812) ──────────────────────────────────────

export fn lb_csv_escape(s: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = lb_putc(out, 0, 34);
    var i: i64 = 0;
    while i < s.len {
        if s[i] == 34 {
            at = lb_putc(out, at, 34);
        }
        at = lb_putc(out, at, s[i]);
        i = i + 1;
    }
    return lb_putc(out, at, 34);
}

fn lb_esc_json(s: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    while i < s.len {
        let c: i64 = s[i];
        if c == 34 {
            at = lb_putc(out, at, 92);
            at = lb_putc(out, at, 34);
        } else if c == 92 {
            at = lb_putc(out, at, 92);
            at = lb_putc(out, at, 92);
        } else if c <= 31 {
            at = lb_putc(out, at, 92);
            at = lb_putc(out, at, 117);
            at = lb_put_hex4(out, at, c);
        } else if c >= 128 {
            // The native buffer is 8 bytes: "\u" plus the eight hex digits of a
            // negative int is truncated to seven bytes, "\ufffff".
            at = lb_copy("\\ufffff", out, at);
        } else {
            at = lb_putc(out, at, s[i]);
        }
        i = i + 1;
    }
    return at;
}

export fn lb_json_escape(s: const u8[:], out: u8[:]) -> i64 {
    return lb_esc_json(s, out);
}

fn lb_val_json(field: const u8[:], value: const u8[:], out: u8[:]) -> i64 {
    let t: i64 = lb_type_of(field);
    if t == LB_STRING {
        var at: i64 = lb_putc(out, 0, 34);
        let n: i64 = lb_esc_json(value, out[at..]);
        at = at + n;
        return lb_putc(out, at, 34);
    }
    if t == LB_BOOL {
        if lb_eq(value, "0") {
            return lb_copy("false", out, 0);
        }
        return lb_copy("true", out, 0);
    }
    return lb_copy(value, out, 0);
}

export fn lb_json_value(field: const u8[:], value: const u8[:], out: u8[:]) -> i64 {
    return lb_val_json(field, value, out);
}

// ── markdown table geometry (1867-1972) ────────────────────────────────────

fn lb_mdw(field: const u8[:]) -> i64 {
    if lb_eq(field, "model") {
        return 0 - 30;
    }
    if lb_eq(field, "t/s") {
        return 20;
    }
    if lb_eq(field, "size") || lb_eq(field, "params") {
        return 10;
    }
    if lb_eq(field, "n_gpu_layers") {
        return 3;
    }
    if lb_eq(field, "n_threads") {
        return 7;
    }
    if lb_eq(field, "n_batch") {
        return 7;
    }
    if lb_eq(field, "n_ubatch") {
        return 8;
    }
    if lb_eq(field, "type_k") || lb_eq(field, "type_v") {
        return 6;
    }
    if lb_eq(field, "split_mode") {
        return 6;
    }
    if lb_eq(field, "load_mode") {
        return 10;
    }
    if lb_eq(field, "flash_attn") {
        return 3;
    }
    if lb_eq(field, "devices") {
        return 0 - 12;
    }
    if lb_eq(field, "test") {
        return 15;
    }
    if lb_eq(field, "no_op_offload") {
        return 4;
    }
    if lb_eq(field, "no_host") {
        return 4;
    }
    var width: i64 = field.len;
    if width < 10 {
        width = 10;
    }
    if lb_type_of(field) == LB_STRING {
        return 0 - width;
    }
    return width;
}

export fn lb_md_width(field: const u8[:]) -> i64 {
    return lb_mdw(field);
}

fn lb_mdn(field: const u8[:]) -> const u8[:] {
    if lb_eq(field, "n_gpu_layers") {
        return "ngl";
    }
    if lb_eq(field, "split_mode") {
        return "sm";
    }
    if lb_eq(field, "n_threads") {
        return "threads";
    }
    if lb_eq(field, "no_kv_offload") {
        return "nkvo";
    }
    if lb_eq(field, "flash_attn") {
        return "fa";
    }
    if lb_eq(field, "load_mode") {
        return "lm";
    }
    if lb_eq(field, "embeddings") {
        return "embd";
    }
    if lb_eq(field, "no_op_offload") {
        return "nopo";
    }
    if lb_eq(field, "no_host") {
        return "noh";
    }
    if lb_eq(field, "devices") {
        return "dev";
    }
    if lb_eq(field, "tensor_split") {
        return "ts";
    }
    if lb_eq(field, "tensor_buft_overrides") {
        return "ot";
    }
    if lb_eq(field, "fit_target") {
        return "fitt";
    }
    if lb_eq(field, "fit_min_ctx") {
        return "fitc";
    }
    return field;
}

export fn lb_md_name(field: const u8[:], out: u8[:]) -> i64 {
    return lb_copy(lb_mdn(field), out, 0);
}

// ── markdown rows (2065-2105) ──────────────────────────────────────────────

fn lb_row_size(bytes: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    if bytes < 1073741824 {
        let x: f64 = bytes / 1024.0 / 1024.0;
        at = at + fmt2(out[at..], x);
        return lb_copy(" MiB", out, at);
    }
    let x: f64 = bytes / 1024.0 / 1024.0 / 1024.0;
    at = at + fmt2(out[at..], x);
    return lb_copy(" GiB", out, at);
}

export fn lb_md_size(bytes: i64, out: u8[:]) -> i64 {
    return lb_row_size(bytes, out);
}

fn lb_row_params(n: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    if n < 1000000000 {
        let x: f64 = n / 1000000.0;
        at = at + fmt2(out[at..], x);
        return lb_copy(" M", out, at);
    }
    let x: f64 = n / 1000000000.0;
    at = at + fmt2(out[at..], x);
    return lb_copy(" B", out, at);
}

export fn lb_md_params(n: i64, out: u8[:]) -> i64 {
    return lb_row_params(n, out);
}

fn lb_row_test(np: i64, ng: i64, nd: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    if np > 0 && ng == 0 {
        at = lb_copy("pp", out, at);
        at = lb_put_int(out, at, np);
    } else if ng > 0 && np == 0 {
        at = lb_copy("tg", out, at);
        at = lb_put_int(out, at, ng);
    } else {
        at = lb_copy("pp", out, at);
        at = lb_put_int(out, at, np);
        at = lb_copy("+tg", out, at);
        at = lb_put_int(out, at, ng);
    }
    if nd > 0 {
        at = lb_copy(" @ d", out, at);
        at = lb_put_int(out, at, nd);
    }
    return at;
}

export fn lb_md_test(np: i64, ng: i64, nd: i64, out: u8[:]) -> i64 {
    return lb_row_test(np, ng, nd, out);
}

fn lb_row_ts(avg: f64, sd: f64, out: u8[:]) -> i64 {
    var at: i64 = fmt2(out, avg);
    at = lb_copy(" ± ", out, at);
    at = at + fmt2(out[at..], sd);
    return at;
}

export fn lb_md_ts(avg: f64, sd: f64, out: u8[:]) -> i64 {
    return lb_row_ts(avg, sd, out);
}

// ── SQL column types (2113-2127) ───────────────────────────────────────────

export fn lb_sql_type(field: const u8[:], out: u8[:]) -> i64 {
    let t: i64 = lb_type_of(field);
    if t == LB_STRING {
        return lb_copy("TEXT", out, 0);
    }
    if t == LB_BOOL || t == LB_INT {
        return lb_copy("INTEGER", out, 0);
    }
    return lb_copy("REAL", out, 0);
}

// ── tensor_split row (1652-1668) ───────────────────────────────────────────

export fn lb_tensor_split(vals: const f64[:], ndev: i64, out: u8[:]) -> i64 {
    var max_nonzero: i64 = 0;
    var i: i64 = 0;
    while i < ndev && i < vals.len {
        if vals[i] > 0.0 {
            max_nonzero = i;
        }
        i = i + 1;
    }
    var at: i64 = 0;
    i = 0;
    while i <= max_nonzero && i < vals.len {
        at = at + fmt2(out[at..], vals[i]);
        if i < max_nonzero {
            at = lb_putc(out, at, 47);
        }
        i = i + 1;
    }
    return at;
}

// ── parse_int_range (293-335) ──────────────────────────────────────────────
//
// Return: count >= 0, or -1 "invalid range format", -2 "invalid range",
// -3 std::stoi out_of_range (the native call throws std::out_of_range).

fn lb_is_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

// 0 on success with the value in val[0], -1 when no integer starts here,
// -3 on the std::stoi out_of_range path. A negative value is a value, not a
// status, so the result travels through val.
fn lb_read_int(s: const u8[:], at0: i64, limit: i64, signed: i64, val: i64[:]) -> i64 {
    var at: i64 = at0;
    var neg: i64 = 0;
    if signed != 0 && at < limit && s[at] == 45 {
        neg = 1;
        at = at + 1;
    }
    if at >= limit || !lb_is_digit(s[at]) {
        return 0 - 1;
    }
    var v: i64 = 0;
    while at < limit && lb_is_digit(s[at]) {
        let d: i64 = (s[at] as i64) - 48;
        if v > (9223372036854775807 - d) / 10 {
            return 0 - 3;
        }
        v = v * 10 + d;
        at = at + 1;
    }
    if neg != 0 {
        v = 0 - v;
    }
    val[0] = v;
    return 0;
}

export fn lb_range(s: const u8[:], allow_neg: i64, out: i64[:]) -> i64 {
    var at: i64 = 0;
    var n: i64 = 0;
    var tmp: i64[1] = [0];
    if s.len == 0 {
        return 0;
    }
    while at < s.len {
        // ^(-?\d+)(?:-(\d+)(?:([+*|])(\d+))?)?(?:,|$)
        let rs: i64 = lb_read_int(s, at, s.len, allow_neg, tmp);
        if rs != 0 {
            return rs;
        }
        let first: i64 = tmp[0];
        var p: i64 = at;
        if allow_neg != 0 && p < s.len && s[p] == 45 {
            p = p + 1;
        }
        while p < s.len && lb_is_digit(s[p]) {
            p = p + 1;
        }
        var last: i64 = first;
        var op: u8 = 43;
        var step: i64 = 1;
        if p < s.len && s[p] == 45 {
            let qs: i64 = lb_read_int(s, p + 1, s.len, 0, tmp);
            if qs != 0 {
                return qs;
            }
            last = tmp[0];
            p = p + 1;
            while p < s.len && lb_is_digit(s[p]) {
                p = p + 1;
            }
            if p < s.len && (s[p] == 43 || s[p] == 42 || s[p] == 124) {
                op = s[p];
                let sts: i64 = lb_read_int(s, p + 1, s.len, 0, tmp);
                if sts != 0 {
                    return sts;
                }
                step = tmp[0];
                p = p + 1;
                while p < s.len && lb_is_digit(s[p]) {
                    p = p + 1;
                }
            }
        }
        if p < s.len && s[p] != 44 {
            return 0 - 1;
        }
        var i: i64 = first;
        while i <= last {
            if n < out.len {
                out[n] = i;
            }
            n = n + 1;
            let prev: i64 = i;
            if op == 43 {
                i = i + step;
            } else if op == 42 {
                i = i * step;
            } else {
                return 0 - 1;
            }
            if i <= prev {
                return 0 - 2;
            }
        }
        if p < s.len {
            p = p + 1;
        }
        at = p;
    }
    return n;
}

// ── cmd_params_defaults, scalar half (380-423) ─────────────────────────────
//
// Indexes name the native field. n_threads is excluded because it is
// common_cpu_get_num_math() on the host; cpu_mask has its own export.
//
//   0 n_prompt        7 n_cpu_moe      14 embeddings     21 output_format
//   1 n_gen           8 split_mode     15 no_op_offload  22 output_stderr
//   2 n_depth         9 load_mode      16 no_host        23 verbose
//   3 n_batch        10 lazy_mode      17 fit_target     24 progress
//   4 n_ubatch       11 main_gpu       18 fit_min_ctx    25 no_warmup
//   5 poll           12 no_kv_offload  19 reps           26 type_k
//   6 n_gpu_layers   13 flash_attn     20 delay          27 type_v

export fn lb_defaults(out: i64[:]) -> i64 {
    if out.len < 28 {
        return 0 - 1;
    }
    out[0] = 512;
    out[1] = 128;
    out[2] = 0;
    out[3] = 2048;
    out[4] = 512;
    out[5] = 50;
    out[6] = 0 - 1;
    out[7] = 0;
    out[8] = 1;
    out[9] = 0 - 1;
    out[10] = 1;
    out[11] = 0;
    out[12] = 0;
    out[13] = 0 - 1;
    out[14] = 0;
    out[15] = 0;
    out[16] = 0;
    out[17] = 0;
    out[18] = 0;
    out[19] = 5;
    out[20] = 0;
    out[21] = 4;
    out[22] = 0;
    out[23] = 0;
    out[24] = 0;
    out[25] = 0;
    out[26] = 1;
    out[27] = 1;
    return 28;
}

export fn lb_default_cpu_mask(out: u8[:]) -> i64 {
    return lb_copy("0x0", out, 0);
}
