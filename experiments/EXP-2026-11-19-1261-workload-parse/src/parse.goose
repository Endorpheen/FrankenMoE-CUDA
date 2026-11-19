// The deterministic-workload file format, load() in deterministic_workload.cpp.
// Returns 0 when the text parses, otherwise the index of the first failing check in the native
// order: 1 header (magic AND version - one native condition), 3 metadata, 4 prompt section,
// 5 output section, 6 routes count, 7 route record shape (including n_ids == top_k * token_n),
// 8 truncated route ids. There is no rule 2: the native code fuses magic and version.
// The native error strings name values and are not ported; the harness maps them back to these
// indices, so the decision is what gets compared.
//
// The two readers write their result through an out array instead of packing position and value
// into one integer, which is what the first attempt at this did and why it was thrown away.
//   wl_skip(text, at, pos)        -> pos[0] = next non-space position
//   wl_word(text, at, out, pos)   -> returns the length, pos[0] = position after the word
//   wl_num(text, at, out, pos)    -> returns 1 on success, pos[0] = position after the number,
//                                    out[0] = the value
fn wl_skip(t: const u8[:], at_in: i64, pos: i64[:]) -> i64 {
    var at: i64 = at_in;
    while at < t.len {
        let c = t[at];
        if c == 32 || c == 9 || c == 10 || c == 13 || c == 11 || c == 12 {
            at = at + 1;
        } else {
            pos[0] = at;
            return 0;
        }
    }
    pos[0] = at;
    return 0;
}

fn wl_word(t: const u8[:], at_in: i64, out: u8[:], pos: i64[:]) -> i64 {
    var p: i64[1] = [0];
    wl_skip(t, at_in, p);
    var at: i64 = p[0];
    var n: i64 = 0;
    while at < t.len {
        let c = t[at];
        if c == 32 || c == 9 || c == 10 || c == 13 || c == 11 || c == 12 {
            pos[0] = at;
            return n;
        }
        if n < out.len {
            out[n] = c;
        }
        n = n + 1;
        at = at + 1;
    }
    pos[0] = at;
    return n;
}

fn wl_num(t: const u8[:], at_in: i64, out: i64[:], pos: i64[:]) -> i64 {
    var p: i64[1] = [0];
    wl_skip(t, at_in, p);
    var at: i64 = p[0];
    var neg: i64 = 0;
    if at < t.len && (t[at] == 45 || t[at] == 43) {
        if t[at] == 45 { neg = 1; }
        at = at + 1;
    }
    var v: i64 = 0;
    var any: i64 = 0;
    while at < t.len && t[at] >= 48 && t[at] <= 57 {
        v = v * 10 + (t[at] - 48);
        any = 1;
        at = at + 1;
    }
    pos[0] = at;
    if any == 0 {
        return 0;
    }
    out[0] = v;
    if neg == 1 {
        out[0] = 0 - v;
    }
    return 1;
}

fn wl_is(w: const u8[:], n: i64, lit: const u8[:]) -> i64 {
    if n != lit.len {
        return 0;
    }
    var i: i64 = 0;
    while i < n {
        if w[i] != lit[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn wl_count(t: const u8[:], at_in: i64, count: i64[:], pos: i64[:]) -> i64 {
    var v: i64[1] = [0];
    var v2: i64[1] = [0];
    if wl_num(t, at_in, v, pos) == 0 {
        return 0;
    }
    if v[0] < 0 {
        return 0;
    }
    count[0] = v[0];
    return 1;
}

fn wl_parse(t: const u8[:], marker: i64) -> i64 {
    if marker != 211 {
        return -1;
    }
    var w: u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var pos: i64[1] = [0];
    var v: i64[1] = [0];
    var v2: i64[1] = [0];
    var n: i64 = 0;

    n = wl_word(t, 0, w, pos);
    if wl_is(w, n, "BMOE_DETERMINISTIC_WORKLOAD") == 0 { return 1; }
    // magic and version are ONE native condition ("unsupported header"), so a bad version is rule 1
    if wl_num(t, pos[0], v, pos) == 0 { return 1; }
    if v[0] != 1 { return 1; }

    n = wl_word(t, pos[0], w, pos);
    if wl_is(w, n, "arch") == 0 { return 3; }
    n = wl_word(t, pos[0], w, pos);
    if n == 0 { return 3; }

    n = wl_word(t, pos[0], w, pos);
    if wl_is(w, n, "layers") == 0 { return 3; }
    if wl_num(t, pos[0], v, pos) == 0 { return 3; }
    n = wl_word(t, pos[0], w, pos);
    if wl_is(w, n, "top_k") == 0 { return 3; }
    if wl_num(t, pos[0], v, pos) == 0 { return 3; }

    n = wl_word(t, pos[0], w, pos);
    if wl_is(w, n, "prompt") == 0 { return 4; }
    if wl_count(t, pos[0], v, pos) == 0 { return 4; }
    var i: i64 = 0;
    while i < v[0] {
        if wl_num(t, pos[0], v2, pos) == 0 { return 4; }
        i = i + 1;
    }

    var cnt: i64 = 0;
    n = wl_word(t, pos[0], w, pos);
    if wl_is(w, n, "output") == 0 { return 5; }
    if wl_count(t, pos[0], v, pos) == 0 { return 5; }
    cnt = v[0];
    i = 0;
    while i < cnt {
        if wl_num(t, pos[0], v2, pos) == 0 { return 5; }
        i = i + 1;
    }

    n = wl_word(t, pos[0], w, pos);
    if wl_is(w, n, "routes") == 0 { return 6; }
    if wl_count(t, pos[0], v, pos) == 0 { return 6; }
    cnt = v[0];
    var r: i64 = 0;
    while r < cnt {
        n = wl_word(t, pos[0], w, pos);
        if wl_is(w, n, "route") == 0 { return 7; }
        var f: i64 = 0;
        var top_k: i64 = 0;
        var token_n: i64 = 0;
        var n_ids: i64 = 0;
        while f < 7 {
            if wl_num(t, pos[0], v, pos) == 0 { return 7; }
            if f == 4 { top_k = v[0]; }
            if f == 5 { token_n = v[0]; }
            if f == 6 { n_ids = v[0]; }
            f = f + 1;
        }
        if n_ids != top_k * token_n { return 7; }
        var j: i64 = 0;
        while j < n_ids {
            if wl_num(t, pos[0], v, pos) == 0 { return 8; }
            j = j + 1;
        }
        r = r + 1;
    }
    return 0;
}

fn main() {
    let b: u8[4] = [66, 77, 79, 69];
    let r = wl_parse(b, 211);
    if r < 0 {
        return;
    }
}
