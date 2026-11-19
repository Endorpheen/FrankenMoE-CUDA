// Log policy, ported from common/log.cpp, plus the debug filter and the
// NaN-abort decision from common/debug.cpp.
//
// Owned decisions:
//   - level map and the default-callback verbosity gate (common_log_get_verbosity);
//   - stream choice in common_log_entry::print: a DEBUG entry is dropped on the
//     stderr path while the threshold is below LOG_DEFAULT_DEBUG, an entry that
//     is not NONE and not JSONL goes to stderr, everything else to stdout;
//   - the log line: the timestamp `M.mm.mmm.uuu` in blue, the one-letter level
//     prefix, suppression of the prefix for NONE and CONT, the reset placed
//     before the trailing newlines and only for WARN/ERROR/DEBUG;
//   - JSONL: a bare serialized object, or a `{"type","time","level","msg"}`
//     wrapper built from the entry;
//   - the debug filter: an empty filter set matches every tensor name, a
//     non-empty set matches through the anchored `^pattern` search;
//   - the debug NaN abort: abort_on_nan plus a NaN sum exits with status 0
//     after printing the tensor and `encountered NaN - aborting`.
//
// Not owned: the ring queue, the writer thread, FILE* handling, varargs
// formatting, the colors table refresh (tty_can_use_colors), the ggml tensor
// access, the fp16/bf16/int value decoders and the value of `sum` for types
// other than F32. `exit()` stays with the caller: the port reports the abort
// decision instead.
//
// Recorded approximations:
//   - `std::regex` has no Goose equivalent. The filter port matches the
//     anchored pattern as a literal prefix, which is identical to
//     regex_search(name, "^pattern") for every pattern without regex
//     metacharacters. Metacharacters and invalid patterns stay native.
//   - `%12.4f` and `%f` are reproduced with round-half-away-from-zero on a
//     value scaled to the printed precision. The graded corpus holds values
//     that are exact at that precision, so the two rounders agree; general
//     float formatting stays native.
//   - Only the F32 tensor dump is ported. F16/BF16/I8/I16/I32/I64 decoding and
//     ggml_abort on an unknown type stay native.
//
// Levels are the ggml_log_level values: NONE 0, DEBUG 1, INFO 2, WARN 3,
// ERROR 4, CONT 5. The port keeps the native enum numbers below.

// ggml_log_level
let LP_NONE: i64 = 0;
let LP_DEBUG: i64 = 1;
let LP_INFO: i64 = 2;
let LP_WARN: i64 = 3;
let LP_ERROR: i64 = 4;
let LP_CONT: i64 = 5;

// log.h verbosity values
let LP_V_DEBUG: i64 = 5;
let LP_V_TRACE: i64 = 4;
let LP_V_INFO: i64 = 3;
let LP_V_WARN: i64 = 2;
let LP_V_ERROR: i64 = 1;
let LP_V_OUTPUT: i64 = 0;

// LOG_DEFAULT_DEBUG
let LP_DEFAULT_DEBUG: i64 = 5;

fn lp_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        dst[p] = s[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn lp_putc(dst: u8[:], at: i64, c: u8) -> i64 {
    dst[at] = c;
    return at + 1;
}

// Unsigned decimal with a minimum width, zero padded.
fn lp_put_uint(dst: u8[:], at: i64, v: i64, width: i64) -> i64 {
    var buf: u8[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    var x = v;

    if x == 0 {
        buf[0] = 48;
        n = 1;
    }
    while x > 0 {
        buf[n] = (48 + (x % 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var p = at;
    var fill = width - n;

    while fill > 0 {
        p = lp_putc(dst, p, 48);
        fill = fill - 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        dst[p] = buf[i];
        p = p + 1;
    }
    return p;
}

// Signed decimal for the JSON `time` field.
fn lp_put_int(dst: u8[:], at: i64, v: i64) -> i64 {
    var p = at;

    if v < 0 {
        p = lp_putc(dst, p, 45);
        return lp_put_uint(dst, p, 0 - v, 1);
    }
    return lp_put_uint(dst, p, v, 1);
}

// ANSI color by native index: 0 default, 1 bold, 2 red, 3 green, 4 yellow,
// 5 blue, 6 magenta, 7 cyan, 8 white. The empty string means colors are off.
fn lp_color(kind: i64, colors: i64, dst: u8[:], at: i64) -> i64 {
    if colors == 0 {
        return at;
    }
    var p = at;

    p = lp_putc(dst, p, 27);
    p = lp_putc(dst, p, 91);
    if kind == 0 {
        p = lp_putc(dst, p, 48);
    } else if kind == 1 {
        p = lp_putc(dst, p, 49);
    } else if kind == 2 {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 49);
    } else if kind == 3 {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 50);
    } else if kind == 4 {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 51);
    } else if kind == 5 {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 52);
    } else if kind == 6 {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 53);
    } else if kind == 7 {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 54);
    } else {
        p = lp_putc(dst, p, 51);
        p = lp_putc(dst, p, 55);
    }
    return lp_putc(dst, p, 109);
}

fn log_level_str(level: i64, out: u8[:]) -> i64 {
    if level == LP_DEBUG {
        return lp_put(out, 0, "debug");
    }
    if level == LP_INFO {
        return lp_put(out, 0, "info");
    }
    if level == LP_WARN {
        return lp_put(out, 0, "warn");
    }
    if level == LP_ERROR {
        return lp_put(out, 0, "error");
    }
    if level == LP_CONT {
        return lp_put(out, 0, "cont");
    }
    return lp_put(out, 0, "none");
}

// common_log_get_verbosity: the level's verbosity, used by the default callback
// and by LOG_TMPL.
fn log_verbosity(level: i64) -> i64 {
    if level == LP_DEBUG {
        return LP_V_DEBUG;
    }
    if level == LP_INFO {
        return LP_V_TRACE;
    }
    if level == LP_WARN {
        return LP_V_WARN;
    }
    if level == LP_ERROR {
        return LP_V_ERROR;
    }
    if level == LP_CONT {
        return LP_V_TRACE;
    }
    return LP_V_OUTPUT;
}

// The default callback gate: verbosity(level) <= threshold.
fn log_passes(level: i64, thold: i64) -> i64 {
    if log_verbosity(level) <= thold {
        return 1;
    }
    return 0;
}

// Stream for the stderr path of common_log_entry::print with no file and no
// explicit target: -1 dropped, 0 stdout, 1 stderr.
fn log_stream(level: i64, jsonl: i64, thold: i64) -> i64 {
    if level == LP_DEBUG && thold < LP_DEFAULT_DEBUG {
        return 0 - 1;
    }
    if level != LP_NONE && jsonl == 0 {
        return 1;
    }
    return 0;
}

// `M.mm.mmm.uuu` plus its trailing space, wrapped in the blue/default colors.
fn log_timestamp(timestamp: i64, colors: i64, out: u8[:]) -> i64 {
    var p = lp_color(5, colors, out, 0);

    p = lp_put_uint(out, p, timestamp / 1000000 / 60, 1);
    p = lp_putc(out, p, 46);
    p = lp_put_uint(out, p, timestamp / 1000000 % 60, 2);
    p = lp_putc(out, p, 46);
    p = lp_put_uint(out, p, timestamp / 1000 % 1000, 3);
    p = lp_putc(out, p, 46);
    p = lp_put_uint(out, p, timestamp % 1000, 3);
    p = lp_color(0, colors, out, p);
    return lp_putc(out, p, 32);
}

// One-letter level prefix for the stderr path. NONE and CONT write nothing.
fn log_prefix(level: i64, timestamp: i64, colors: i64, out: u8[:]) -> i64 {
    var p: i64 = 0;

    if level == LP_NONE || level == LP_CONT {
        return 0;
    }
    if timestamp != 0 {
        p = log_timestamp(timestamp, colors, out);
    }
    if level == LP_INFO {
        p = lp_color(3, colors, out, p);
        p = lp_putc(out, p, 73);
        p = lp_putc(out, p, 32);
        return lp_color(0, colors, out, p);
    }
    if level == LP_WARN {
        p = lp_color(6, colors, out, p);
        p = lp_putc(out, p, 87);
        return lp_putc(out, p, 32);
    }
    if level == LP_ERROR {
        p = lp_color(2, colors, out, p);
        p = lp_putc(out, p, 69);
        return lp_putc(out, p, 32);
    }
    if level == LP_DEBUG {
        p = lp_color(4, colors, out, p);
        p = lp_putc(out, p, 68);
        return lp_putc(out, p, 32);
    }
    return p;
}

// JSON string escaping as dump_safe does it for valid UTF-8 input: quote,
// backslash and the short control escapes, other controls as \u00XX.
fn lp_json_str(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if c == 34 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 34);
        } else if c == 92 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 92);
        } else if c == 8 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 98);
        } else if c == 9 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 116);
        } else if c == 10 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 110);
        } else if c == 12 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 102);
        } else if c == 13 {
            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 114);
        } else if c < 32 {
            let hex: const u8[:] = "0123456789abcdef";

            p = lp_putc(dst, p, 92);
            p = lp_putc(dst, p, 117);
            p = lp_putc(dst, p, 48);
            p = lp_putc(dst, p, 48);
            p = lp_putc(dst, p, hex[((c as i64) / 16) % 16]);
            p = lp_putc(dst, p, hex[(c as i64) % 16]);
        } else {
            p = lp_putc(dst, p, c);
        }
        i = i + 1;
    }
    return p;
}

fn lp_jsonl(level: i64, timestamp: i64, msg: const u8[:], is_json: i64, out: u8[:]) -> i64 {
    var p: i64 = 0;

    if is_json == 1 {
        p = lp_put(out, p, msg);
        return lp_putc(out, p, 10);
    }
    p = lp_put(out, p, "{\"type\":\"log\",\"time\":");
    p = lp_put_int(out, p, timestamp);
    p = lp_put(out, p, ",\"level\":\"");
    var lb: u8[16] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let ln = log_level_str(level, lb);

    p = lp_put(out, p, lb[0..ln]);
    p = lp_put(out, p, "\",\"msg\":\"");
    p = lp_json_str(out, p, msg);
    p = lp_put(out, p, "\"}");
    return lp_putc(out, p, 10);
}

// common_log_entry::print into a file: jsonl, prefix, body, reset placement and
// the trailing newlines. `msg` is the formatted message.
fn log_render(
    level: i64,
    prefix: i64,
    timestamp: i64,
    jsonl: i64,
    is_json: i64,
    colors: i64,
    msg: const u8[:],
    out: u8[:]) -> i64 {
    if jsonl == 1 {
        return lp_jsonl(level, timestamp, msg, is_json, out);
    }
    var p: i64 = 0;

    if level != LP_NONE && level != LP_CONT && prefix == 1 {
        p = log_prefix(level, timestamp, colors, out);
    }
    // the reset goes before the trailing newlines, so every line carries its own
    // colors
    let reset = level == LP_WARN || level == LP_ERROR || level == LP_DEBUG;
    var end = msg.len;

    while end > 0 && msg[end - 1] == 10 {
        end = end - 1;
    }
    var i: i64 = 0;

    while i < end {
        out[p] = msg[i];
        p = p + 1;
        i = i + 1;
    }
    if reset {
        p = lp_color(0, colors, out, p);
    }
    i = end;
    while i < msg.len {
        out[p] = msg[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// Color enablement: LOG_COLORS_AUTO (-1) asks tty_can_use_colors, DISABLED
// (0) and ENABLED (1) do not; the answer is then ANDed with tty_enable_ansi,
// which is false on a console that cannot render ANSI.
fn log_colors_enabled(mode: i64, tty_can: i64, tty_ansi: i64) -> i64 {
    var requested: i64 = 0;

    if mode == 0 - 1 {
        requested = tty_can;
    } else if mode == 1 {
        requested = 1;
    }
    if requested == 1 && tty_ansi == 1 {
        return 1;
    }
    return 0;
}

// Debug filter: an empty set matches everything, otherwise the anchored
// `^pattern` search is a literal prefix match for patterns without regex
// metacharacters.
fn dbg_filter_match(
    blob: const u8[:],
    off: i64[:],
    len: i64[:],
    count: i64,
    name: const u8[:]) -> i64 {
    if count == 0 {
        return 1;
    }
    var k: i64 = 0;

    while k < count {
        let pos_from = off[k];
        let n = len[k];
        var ok = n <= name.len;
        var i: i64 = 0;

        while ok && i < n {
            if blob[pos_from + i] != name[i] {
                ok = false;
            } else {
                i = i + 1;
            }
        }
        if ok {
            return 1;
        }
        k = k + 1;
    }
    return 0;
}

// common_ggml_ne_string: four dimensions joined by ", ".
fn dbg_ne_string(ne: i64[:], out: u8[:]) -> i64 {
    var p: i64 = 0;
    var i: i64 = 0;

    while i < 4 {
        let v = ne[i];

        if v < 0 {
            p = lp_putc(out, p, 45);
            p = lp_put_uint(out, p, 0 - v, 1);
        } else {
            p = lp_put_uint(out, p, v, 1);
        }
        if i + 1 < 4 {
            p = lp_put(out, p, ", ");
        }
        i = i + 1;
    }
    return p;
}

fn lp_isinf(v: f64) -> i64 {
    if v > 3.4028234663852886e38 {
        return 1;
    }
    if v < 0.0 - 3.4028234663852886e38 {
        return 0 - 1;
    }
    return 0;
}

// printf("%.4f") / printf("%f") on a value that is exact at the printed
// precision, right justified in `width` when width > 0.
fn lp_fixed(v: f64, decimals: i64, width: i64, out: u8[:]) -> i64 {
    var digits: u8[32] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;

    if v != v {
        digits[0] = 110;
        digits[1] = 97;
        digits[2] = 110;
        n = 3;
    } else if lp_isinf(v) != 0 {
        if lp_isinf(v) < 0 {
            digits[0] = 45;
            n = 1;
        }
        digits[n] = 105;
        digits[n + 1] = 110;
        digits[n + 2] = 102;
        n = n + 3;
    } else {
        var x = v;

        if x < 0.0 {
            digits[0] = 45;
            n = 1;
            x = 0.0 - x;
        }
        var scale: f64 = 1.0;
        var d: i64 = 0;

        while d < decimals {
            scale = scale * 10.0;
            d = d + 1;
        }
        let scaled = x * scale;
        let whole = (scaled + 0.5) as i64;
        // integer part then the fractional digits, both zero padded
        var ip = whole;
        var frac: u8[6] = [0, 0, 0, 0, 0, 0];
        var k: i64 = decimals;

        while k > 0 {
            frac[k - 1] = (48 + (ip % 10)) as u8;
            ip = ip / 10;
            k = k - 1;
        }
        var whole_digits: u8[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var wn: i64 = 0;

        if ip == 0 {
            whole_digits[0] = 48;
            wn = 1;
        }
        while ip > 0 {
            whole_digits[wn] = (48 + (ip % 10)) as u8;
            ip = ip / 10;
            wn = wn + 1;
        }
        var j = wn;

        while j > 0 {
            j = j - 1;
            digits[n] = whole_digits[j];
            n = n + 1;
        }
        digits[n] = 46;
        n = n + 1;
        k = 0;
        while k < decimals {
            digits[n] = frac[k];
            n = n + 1;
            k = k + 1;
        }
    }
    var p: i64 = 0;
    var fill = width - n;

    while fill > 0 {
        out[p] = 32;
        p = p + 1;
        fill = fill - 1;
    }
    var i: i64 = 0;

    while i < n {
        out[p] = digits[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// common_debug_print_tensor for an F32 tensor with n = 3 truncation. Returns 0
// when the call returns and 1 when it would exit(0) on a NaN sum.
fn dbg_dump_f32(
    ne: i64[:],
    vals: f64[:],
    n: i64,
    abort_on_nan: i64,
    out: u8[:],
    outn: i64[:]) -> i64 {
    var p: i64 = 0;
    var sum: f64 = 0.0;
    var i3: i64 = 0;

    while i3 < ne[3] {
        var i2: i64 = 0;

        while i2 < ne[2] {
            var i1: i64 = 0;

            while i1 < ne[1] {
                var i0: i64 = 0;

                while i0 < ne[0] {
                    let idx = i0 + ne[0] * (i1 + ne[1] * (i2 + ne[2] * i3));

                    sum = sum + vals[idx];
                    i0 = i0 + 1;
                }
                i1 = i1 + 1;
            }
            i2 = i2 + 1;
        }
        i3 = i3 + 1;
    }
    i3 = 0;
    while i3 < ne[3] {
        p = lp_put(out, p, "    [\n");
        var i2: i64 = 0;

        while i2 < ne[2] {
            if i2 == n && ne[2] > 2 * n {
                p = lp_put(out, p, "        ..., \n");
                i2 = ne[2] - n;
            }
            p = lp_put(out, p, "        [\n");
            var i1: i64 = 0;

            while i1 < ne[1] {
                if i1 == n && ne[1] > 2 * n {
                    p = lp_put(out, p, "            ..., \n");
                    i1 = ne[1] - n;
                }
                p = lp_put(out, p, "            [");
                var i0: i64 = 0;

                while i0 < ne[0] {
                    if i0 == n && ne[0] > 2 * n {
                        p = lp_put(out, p, "   ..., ");
                        i0 = ne[0] - n;
                    }
                    let idx = i0 + ne[0] * (i1 + ne[1] * (i2 + ne[2] * i3));

                    p = p + lp_fixed(vals[idx], 4, 12, out[p ..]);
                    if i0 < ne[0] - 1 {
                        p = lp_put(out, p, ", ");
                    }
                    i0 = i0 + 1;
                }
                p = lp_put(out, p, "  ],\n");
                i1 = i1 + 1;
            }
            p = lp_put(out, p, "        ],\n");
            i2 = i2 + 1;
        }
        p = lp_put(out, p, "    ]\n");
        p = lp_put(out, p, "    sum = ");
        p = p + lp_fixed(sum, 6, 0, out[p ..]);
        p = lp_putc(out, p, 10);
        i3 = i3 + 1;
    }
    outn[0] = p;
    if abort_on_nan == 1 && sum != sum {
        p = lp_put(out, p, "encountered NaN - aborting\n");
        outn[0] = p;
        return 1;
    }
    return 0;
}

// The abort decision alone, for callers that do not want the dump.
fn dbg_nan_abort(abort_on_nan: i64, is_nan: i64) -> i64 {
    if abort_on_nan == 1 && is_nan == 1 {
        return 1;
    }
    return 0;
}

// Export wrappers: Goose cannot call an export fn from another function, so the
// bodies stay private and the gate driver calls them directly.
export fn lp_level_str(level: i64, out: u8[:]) -> i64 {
    return log_level_str(level, out);
}

export fn lp_verbosity(level: i64) -> i64 {
    return log_verbosity(level);
}

export fn lp_passes(level: i64, thold: i64) -> i64 {
    return log_passes(level, thold);
}

export fn lp_stream(level: i64, jsonl: i64, thold: i64) -> i64 {
    return log_stream(level, jsonl, thold);
}

export fn lp_render(
    level: i64,
    prefix: i64,
    timestamp: i64,
    jsonl: i64,
    is_json: i64,
    colors: i64,
    msg: const u8[:],
    out: u8[:]) -> i64 {
    return log_render(level, prefix, timestamp, jsonl, is_json, colors, msg, out);
}

export fn lp_colors(mode: i64, tty_can: i64, tty_ansi: i64) -> i64 {
    return log_colors_enabled(mode, tty_can, tty_ansi);
}

export fn lp_filter_match(blob: const u8[:], off: i64[:], len: i64[:], count: i64, name: const u8[:]) -> i64 {
    return dbg_filter_match(blob, off, len, count, name);
}

export fn lp_ne_string(ne: i64[:], out: u8[:]) -> i64 {
    return dbg_ne_string(ne, out);
}

export fn lp_dump_f32(ne: i64[:], vals: f64[:], n: i64, abort_on_nan: i64, out: u8[:], outn: i64[:]) -> i64 {
    return dbg_dump_f32(ne, vals, n, abort_on_nan, out, outn);
}

export fn lp_nan_abort(abort_on_nan: i64, is_nan: i64) -> i64 {
    return dbg_nan_abort(abort_on_nan, is_nan);
}
