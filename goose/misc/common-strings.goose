// Common string and format helpers, ported from common/common.cpp:
// string_strip, string_split, string_join, string_replace_all, string_repeat,
// string_from (bool and vector<int>), string_lcs, string_process_escapes and
// string_parse_kv_override.
//
// Owned decisions:
//   - strip removes the C-locale whitespace set from both ends and nothing
//     else; the whole input being whitespace yields an empty string;
//   - split always returns at least one part, keeps empty parts at both ends
//     and between adjacent delimiters, and the last part runs to the end;
//     an empty delimiter is refused here because the native loop cannot
//     advance on it;
//   - join writes the separator between parts and nothing around them;
//   - replace_all walks left to right, does not rescan replacement text, and
//     an empty search leaves the string untouched;
//   - repeat of a zero count is empty;
//   - from(bool) is "true"/"false" and from(vector<int>) is "[ " + items
//     separated by ", " + " ]", so the empty vector is "[  ]" with two spaces;
//   - lcs is the longest common *substring*; the first longest one in row-major
//     order wins, which is what the native dp update order gives;
//   - process_escapes maps \n \r \t ' " \\ to themselves, and \xHH only when
//     strtol would consume exactly the two bytes: two hex digits, or one sign
//     or blank followed by one hex digit. Anything else keeps the backslash and
//     the byte after it, and the hex digits are then ordinary text;
//   - parse_kv_override splits at the first '=', refuses a key of 128 bytes or
//     more, reads int:/float:/bool:/str:, requires "true" or "false" exactly,
//     refuses a string value longer than 127 bytes and refuses any other tag.
//
// Not owned: the stray kv override storage (llama_model_kv_override), the
// locale itself (the gate runs with LC_ALL=C), the logging of the refusal, and
// bytes above 127 in strip, where native calls isspace on a possibly negative
// char, which is undefined; the corpus keeps strip on ASCII.

extern "gs_json_strtod" fn libc_strtod(s: const u8[:]) -> f64;

fn cs_is_space(c: u8) -> bool {
    return c == 32 || c == 9 || c == 10 || c == 11 || c == 12 || c == 13;
}

fn cs_is_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

fn cs_is_hex(c: u8) -> bool {
    return (c >= 48 && c <= 57) || (c >= 97 && c <= 102) || (c >= 65 && c <= 70);
}

fn cs_eq(a: const u8[:], b: const u8[:]) -> bool {
    return a == b;
}

fn cs_starts(a: const u8[:], p: const u8[:]) -> bool {
    if p.len > a.len {
        return false;
    }
    var i: i64 = 0;
    while i < p.len {
        if a[i] != p[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// First occurrence of needle in s[from..], -1 when there is none.
fn cs_find(s: const u8[:], pos_from: i64, needle: const u8[:]) -> i64 {
    if needle.len == 0 {
        return 0 - 1;
    }
    if needle.len > s.len - pos_from {
        return 0 - 1;
    }
    var i: i64 = pos_from;
    while i + needle.len <= s.len {
        var k: i64 = 0;
        var hit: i64 = 1;
        while k < needle.len {
            if s[i + k] != needle[k] {
                hit = 0;
                break;
            }
            k = k + 1;
        }
        if hit == 1 {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn cs_copy(s: const u8[:], pos_from: i64, len: i64, out: u8[:], at: i64) -> i64 {
    var i: i64 = 0;
    while i < len {
        out[at + i] = s[pos_from + i];
        i = i + 1;
    }
    return at + len;
}

export fn cs_strip(s: const u8[:], out: u8[:]) -> i64 {
    var a: i64 = 0;
    var b: i64 = s.len;
    while a < b && cs_is_space(s[a]) {
        a = a + 1;
    }
    while b > a && cs_is_space(s[b - 1]) {
        b = b - 1;
    }
    return cs_copy(s, a, b - a, out, 0);
}

// Parts are written back to back into out, their lengths into plens.
// Returns the part count, or -1 when the empty delimiter is asked for or the
// destination is too small.
export fn cs_split(s: const u8[:], delim: const u8[:], out: u8[:], plens: i64[:]) -> i64 {
    if delim.len == 0 {
        return 0 - 1;
    }
    var at: i64 = 0;
    var n: i64 = 0;
    var start: i64 = 0;
    while true {
        var m: i64 = cs_find(s, start, delim);
        if m < 0 {
            break;
        }
        if at + (m - start) > out.len || n >= plens.len {
            return 0 - 1;
        }
        at = cs_copy(s, start, m - start, out, at);
        plens[n] = m - start;
        n = n + 1;
        start = m + delim.len;
    }
    if at + (s.len - start) > out.len || n >= plens.len {
        return 0 - 1;
    }
    at = cs_copy(s, start, s.len - start, out, at);
    plens[n] = s.len - start;
    n = n + 1;
    return n;
}

export fn cs_join(parts: const u8[:], plens: const i64[:], n: i64, sep: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var off: i64 = 0;
    var i: i64 = 0;
    while i < n {
        if i > 0 {
            at = cs_copy(sep, 0, sep.len, out, at);
        }
        at = cs_copy(parts, off, plens[i], out, at);
        off = off + plens[i];
        i = i + 1;
    }
    return at;
}

export fn cs_replace_all(s: const u8[:], search: const u8[:], repl: const u8[:], out: u8[:]) -> i64 {
    if search.len == 0 {
        return cs_copy(s, 0, s.len, out, 0);
    }
    var at: i64 = 0;
    var last: i64 = 0;
    while true {
        var m: i64 = cs_find(s, last, search);
        if m < 0 {
            break;
        }
        at = cs_copy(s, last, m - last, out, at);
        at = cs_copy(repl, 0, repl.len, out, at);
        last = m + search.len;
    }
    return cs_copy(s, last, s.len - last, out, at);
}

export fn cs_repeat(s: const u8[:], n: i64, out: u8[:]) -> i64 {
    if n <= 0 {
        return 0;
    }
    var at: i64 = 0;
    var i: i64 = 0;
    while i < n {
        at = cs_copy(s, 0, s.len, out, at);
        i = i + 1;
    }
    return at;
}

export fn cs_from_bool(v: i64, out: u8[:]) -> i64 {
    if v != 0 {
        return cs_copy("true", 0, 4, out, 0);
    }
    return cs_copy("false", 0, 5, out, 0);
}

fn cs_put_int(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        out[at] = 45;
        at = at + 1;
        if v == (0 - 9223372036854775807 - 1) {
            // The magnitude of the smallest i64 does not fit in i64.
            return cs_copy("9223372036854775808", 0, 19, out, at);
        }
        v = 0 - v;
    }
    if v == 0 {
        out[at] = 48;
        return at + 1;
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
        out[at] = (48 + buf[i]) as u8;
        at = at + 1;
    }
    return at;
}

export fn cs_from_ints(vals: const i64[:], out: u8[:]) -> i64 {
    var at: i64 = cs_copy("[ ", 0, 2, out, 0);
    var i: i64 = 0;
    while i < vals.len {
        if i > 0 {
            at = cs_copy(", ", 0, 2, out, at);
        }
        at = cs_put_int(out, at, vals[i]);
        i = i + 1;
    }
    return cs_copy(" ]", 0, 2, out, at);
}

// dp is (a.len + 1) * (b.len + 1) i64 slots; it is written, not read.
export fn cs_lcs(a: const u8[:], b: const u8[:], out: u8[:], dp: i64[:]) -> i64 {
    if a.len == 0 || b.len == 0 {
        return 0;
    }
    var w: i64 = b.len + 1;
    var i: i64 = 0;
    while i <= a.len {
        dp[i * w] = 0;
        i = i + 1;
    }
    i = 0;
    while i <= b.len {
        dp[i] = 0;
        i = i + 1;
    }
    var best: i64 = 0;
    var best_i: i64 = 0;
    i = 1;
    while i <= a.len {
        var j: i64 = 1;
        while j <= b.len {
            if a[i - 1] == b[j - 1] {
                var v: i64 = dp[(i - 1) * w + (j - 1)] + 1;
                dp[i * w + j] = v;
                if v > best {
                    best = v;
                    best_i = i;
                }
            } else {
                dp[i * w + j] = 0;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return cs_copy(a, best_i - best, best, out, 0);
}

// strtol on a two-byte buffer consumes both bytes exactly when the first is a
// hex digit, a blank or a sign and the second is a hex digit. "0x" does not
// qualify: strtol stops after the zero.
fn cs_hex_pair_ok(c1: u8, c2: u8) -> bool {
    if !cs_is_hex(c2) {
        return false;
    }
    return cs_is_hex(c1) || cs_is_space(c1) || c1 == 43 || c1 == 45;
}

// The value strtol produces from the pair: two hex digits, or one blank and a
// digit, or a sign and a digit.
fn cs_hex_run(c1: u8, c2: u8) -> i64 {
    if cs_is_hex(c1) {
        return cs_hex_val(c1) * 16 + cs_hex_val(c2);
    }
    if c1 == 45 {
        return 0 - cs_hex_val(c2);
    }
    return cs_hex_val(c2);
}

fn cs_hex_val(c: u8) -> i64 {
    if c >= 48 && c <= 57 {
        return c - 48;
    }
    if c >= 97 && c <= 102 {
        return c - 87;
    }
    return c - 55;
}

export fn cs_process_escapes(input: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    while i < input.len {
        if input[i] == 92 && i + 1 < input.len {
            i = i + 1;
            var c: u8 = input[i];
            if c == 110 {
                out[at] = 10;
            } else if c == 114 {
                out[at] = 13;
            } else if c == 116 {
                out[at] = 9;
            } else if c == 39 {
                out[at] = 39;
            } else if c == 34 {
                out[at] = 34;
            } else if c == 92 {
                out[at] = 92;
            } else if c == 120 && i + 2 < input.len && cs_hex_pair_ok(input[i + 1], input[i + 2]) {
                out[at] = cs_hex_run(input[i + 1], input[i + 2]) as u8;
                i = i + 2;
            } else {
                out[at] = 92;
                at = at + 1;
                out[at] = c;
            }
            at = at + 1;
        } else {
            out[at] = input[i];
            at = at + 1;
        }
        i = i + 1;
    }
    return at;
}

// atol: leading blanks, an optional sign, decimal digits, stop at the first
// other byte; saturation at the i64 limits like strtol.
fn cs_atol(s: const u8[:], pos_from: i64) -> i64 {
    var i: i64 = pos_from;
    while i < s.len && cs_is_space(s[i]) {
        i = i + 1;
    }
    var neg: i64 = 0;
    if i < s.len && (s[i] == 43 || s[i] == 45) {
        if s[i] == 45 {
            neg = 1;
        }
        i = i + 1;
    }
    var lim: u64 = 9223372036854775807;
    if neg == 1 {
        lim = 9223372036854775808;
    }
    var acc: u64 = 0;
    var over: i64 = 0;
    while i < s.len && cs_is_digit(s[i]) {
        var d: u64 = s[i] - 48;
        if acc > (lim - d) / 10 {
            over = 1;
        } else {
            acc = acc * 10 + d;
        }
        i = i + 1;
    }
    if over == 1 {
        acc = lim;
    }
    if neg == 1 {
        if acc == 9223372036854775808 {
            return 0 - 9223372036854775807 - 1;
        }
        return 0 - (acc as i64);
    }
    return acc as i64;
}

// res[0] tag (0 int, 1 float, 2 bool, 3 str), res[1] int value, res[2] bool,
// res[3] key length, res[4] string length. out_f64[0] carries the float value.
// Returns 0 ok, 1 malformed, 2 invalid bool, 3 invalid type, 4 string too long.
export fn cs_kv(
    data: const u8[:],
    out_key: u8[:],
    out_str: u8[:],
    res: i64[:],
    out_f64: f64[:]
) -> i64 {
    var sep: i64 = cs_find(data, 0, "=");
    if sep < 0 || sep >= 128 {
        return 1;
    }
    if sep > out_key.len {
        return 1;
    }
    cs_copy(data, 0, sep, out_key, 0);
    res[3] = sep;

    var v: i64 = sep + 1;
    var rest: const u8[:] = data[v..];

    if cs_starts(rest, "int:") {
        res[0] = 0;
        res[1] = cs_atol(data, v + 4);
        return 0;
    }
    if cs_starts(rest, "float:") {
        res[0] = 1;
        out_f64[0] = libc_strtod(rest[6..]);
        return 0;
    }
    if cs_starts(rest, "bool:") {
        res[0] = 2;
        var b: const u8[:] = rest[5..];
        if cs_eq(b, "true") {
            res[2] = 1;
            return 0;
        }
        if cs_eq(b, "false") {
            res[2] = 0;
            return 0;
        }
        return 2;
    }
    if cs_starts(rest, "str:") {
        res[0] = 3;
        var t: const u8[:] = rest[4..];
        if t.len > 127 {
            return 4;
        }
        res[4] = cs_copy(t, 0, t.len, out_str, 0);
        return 0;
    }
    return 3;
}
