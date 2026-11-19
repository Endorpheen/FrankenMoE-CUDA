// The deterministic-workload format and replay guards, ported from
// runtime/core/src/moe/deterministic_workload.cpp. Every part here is verified against the compiled
// class by a white-box include of that file - no ggml, no llama, no model:
//   reading  9 crafted files, every failure mode       (EXP-2026-11-19-1261)
//   writing  byte-exact against save(), 4 workloads    (EXP-2026-11-19-1263)
//   guards   token/finish, 1000 cases                  (EXP-2026-11-19-1264)
//   routing  replay_topk and the cursor, 1000 cases    (EXP-2026-11-19-1265)
// The native error strings are presentation and are not ported; callers get rule indices.

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

fn wl_put(out: u8[:], at: i64, c: i64) -> i64 {
    if at < out.len {
        out[at] = (c as u8);
    }
    return at + 1;
}

fn wl_put_bytes(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var k: i64 = 0;
    var p: i64 = at;
    while k < s.len {
        p = wl_put(out, p, s[k] as i64);
        k = k + 1;
    }
    return p;
}

// Iterative on purpose: a recursive form needs a declaration this compiler does not accept here,
// and the digits are cheapest written into a scratch buffer and copied back.
fn wl_put_int(out: u8[:], at: i64, v: i64) -> i64 {
    var p: i64 = at;
    var n: i64 = v;
    if n < 0 {
        p = wl_put(out, p, 45);
        n = 0 - n;
    }
    var digits: u8[24] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var used: i64 = 0;
    if n == 0 {
        digits[0] = 48;
        used = 1;
    } else {
        while n > 0 {
            digits[used] = (48 + n - 10 * (n / 10)) as u8;
            n = n / 10;
            used = used + 1;
        }
    }
    var k: i64 = used;
    while k > 0 {
        k = k - 1;
        p = wl_put(out, p, digits[k] as i64);
    }
    return p;
}

fn wl_save(arch: const u8[:], n_layer: i64, top_k: i64, prompt: const i64[:], n_prompt: i64,
           output_tokens: const i64[:], n_output: i64, routes: const i64[:], n_routes: i64,
           out: u8[:], marker: i64) -> i64 {
    if marker != 213 {
        return -1;
    }
    var p: i64 = 0;
    p = wl_put_bytes(out, p, "BMOE_DETERMINISTIC_WORKLOAD 1\n");
    p = wl_put_bytes(out, p, "arch ");
    p = wl_put_bytes(out, p, arch);
    p = wl_put_bytes(out, p, "\nlayers ");
    p = wl_put_int(out, p, n_layer);
    p = wl_put_bytes(out, p, "\ntop_k ");
    p = wl_put_int(out, p, top_k);
    p = wl_put_bytes(out, p, "\nprompt ");
    p = wl_put_int(out, p, n_prompt);
    var i: i64 = 0;
    while i < n_prompt {
        p = wl_put(out, p, 32);
        p = wl_put_int(out, p, prompt[i]);
        i = i + 1;
    }
    p = wl_put_bytes(out, p, "\noutput ");
    p = wl_put_int(out, p, n_output);
    i = 0;
    while i < n_output {
        p = wl_put(out, p, 32);
        p = wl_put_int(out, p, output_tokens[i]);
        i = i + 1;
    }
    p = wl_put_bytes(out, p, "\nroutes ");
    p = wl_put_int(out, p, n_routes);
    p = wl_put_bytes(out, p, "\n");
    var at: i64 = 0;
    var r: i64 = 0;
    while r < n_routes {
        p = wl_put_bytes(out, p, "route");
        var f: i64 = 0;
        while f < 7 {
            p = wl_put(out, p, 32);
            p = wl_put_int(out, p, routes[at + f]);
            f = f + 1;
        }
        at = at + 7;
        let n_ids = routes[at - 1];
        var j: i64 = 0;
        while j < n_ids {
            p = wl_put(out, p, 32);
            p = wl_put_int(out, p, routes[at + j]);
            j = j + 1;
        }
        at = at + n_ids;
        p = wl_put(out, p, 10);
        r = r + 1;
    }
    return p;
}

fn wl_replay_token(index: i64, recorded: i64, marker: i64) -> i64 {
    if marker != 214 {
        return -1;
    }
    if index >= recorded {
        return 1;
    }
    return 0;
}

fn wl_replay_finish(route_cursor: i64, routes: i64, emitted: i64, recorded: i64, marker: i64) -> i64 {
    if marker != 215 {
        return -1;
    }
    if route_cursor != routes {
        return 2;
    }
    if emitted != recorded {
        return 3;
    }
    return 0;
}

fn wl_replay_topk(cursor: i64, routes_n: i64,
                  e_phase: i64, e_base: i64, e_batch: i64, e_layer: i64, e_topk: i64, e_tokn: i64,
                  a_phase: i64, a_base: i64, a_batch: i64, a_layer: i64, a_topk: i64, a_tokn: i64,
                  out: i64[:], marker: i64) -> i64 {
    if marker != 216 {
        return -1;
    }
    if cursor >= routes_n {
        out[0] = cursor;
        return 1;
    }
    out[0] = cursor + 1;
    if e_phase != a_phase || e_base != a_base || e_batch != a_batch ||
       e_layer != a_layer || e_topk != a_topk || e_tokn != a_tokn {
        return 2;
    }
    return 0;
}
