// GBNF rule-text parser in Goose, from src/llama-grammar.cpp
// (parse, parse_rule, parse_alternates, parse_sequence, parse_char, parse_space,
// get_symbol_id, generate_symbol_id).
//
// Owned decisions:
//   - rule names and the symbol table (ids in insertion order);
//   - literals with the native escape table (x/u/U, t/r/n, \\ " [ ] -);
//   - character classes with negation, ranges and alternate chars;
//   - `.`, rule references and nested groups;
//   - repetitions * + ? {n,m} with the native rewrite into generated rules and
//     the 2000-rule guard;
//   - the top-level validation of undefined rules;
//   - error messages and the position suffix.
//
// Tokens (<...> / !<...>) need a vocabulary and are not part of this corpus.

import std;
const E_END: i64 = 0;
const E_ALT: i64 = 1;
const E_REF: i64 = 2;
const E_CHAR: i64 = 3;
const E_NOT: i64 = 4;
const E_RNG: i64 = 5;
const E_ALTCHAR: i64 = 6;
const E_ANY: i64 = 7;
const MAX_REP: i64 = 2000;
const HUGE: i64 = 9223372036854775807;
// state: 0 cursor, 1 nsym, 2 nrules, 3 working cursor, 4 err flag, 5 err len

fn append_s(dst: u8[:], at: i64, s: const u8[:], off: i64, len: i64) -> i64 {
    var k: i64 = 0;

    while k < len {
        if at + k < dst.len {
            dst[at + k] = s[off + k];
        }
        k = k + 1;
    }
    return at + len;
}

fn append_c(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn append_dec(dst: u8[:], at: i64, v: i64) -> i64 {
    var p: i64 = at;
    var x: i64 = v;
    var digits: i64 = 1;
    var y: i64 = x;

    while y >= 10 {
        digits = digits + 1;
        y = y / 10;
    }
    var pos: i64 = p + digits - 1;
    y = x;

    while y > 0 {
        if pos < dst.len {
            dst[pos] = (48 as u8) + ( (y % 10) as u8);
        }
        pos = pos - 1;
        y = y / 10;
    }

    if x == 0 {
        if p < dst.len {
            dst[p] = '0';
        }
    }
    return p + digits;
}

fn is_digit(c: u8) -> bool {
    return c >= '0' && c <= '9';
}

fn is_word(c: u8) -> bool {
    if c >= 'a' && c <= 'z' {
        return true;
    }

    if c >= 'A' && c <= 'Z' {
        return true;
    }

    if c >= '0' && c <= '9' {
        return true;
    }

    if c == '-' {
        return true;
    }
    return false;
}

fn eq_bytes(a: const u8[:], b: const u8[:]) -> bool {
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
// error helpers

fn fail_text(g: const u8[:], err: u8[:], st: i64[:], msg: const u8[:]) -> i64 {
    let n = append_s(err, 0, msg, 0, msg.len);
    st[4] = 1;
    st[5] = n;
    return 0;
}

fn fail_at(g: const u8[:], err: u8[:], st: i64[:], prefix: const u8[:], pos: i64) -> i64 {
    var n = append_s(err, 0, prefix, 0, prefix.len);
    n = append_s(err, n, " at ", 0, 4);
    n = append_s(err, n, g, pos, g.len - pos);
    st[4] = 1;
    st[5] = n;
    return 0;
}

fn parse_space(g: const u8[:], i0: i64, nl_ok: i64) -> i64 {
    var i: i64 = i0;
    var stop: bool = false;

    while i < g.len && !stop {
        let c = g[i];

        if c == ' ' || c == '\t' {
            i = i + 1;
        } else

        if c == '#' {
            var j: i64 = i;

            while j < g.len && g[j] != '\r' && g[j] != '\n' {
                j = j + 1;
            }
            i = j;
        } else

        if nl_ok != 0 && (c == '\r' || c == '\n') {
            i = i + 1;
        } else {
            stop = true;
        }
    }
    return i;
}

fn parse_name(g: const u8[:], i0: i64, err: u8[:], st: i64[:]) -> i64 {
    var i: i64 = i0;

    while i < g.len && is_word(g[i]) {
        i = i + 1;
    }

    if i == i0 {
        fail_at(g, err, st, "expecting name", i0);
    }
    return i;
}

fn parse_hex(g: const u8[:], i0: i64, n: i64, err: u8[:], st: i64[:]) -> i64, i64 {
    var i: i64 = i0;
    var v: i64 = 0;
    var k: i64 = 0;
    var bad: bool = false;

    while k < n && i < g.len && !bad {
        let c = g[i];
        var d: i64 = -1;

        if c >= 'a' && c <= 'f' {
            d = (c - 87) as i64;
        } else

        if c >= 'A' && c <= 'F' {
            d = (c - 55) as i64;
        } else

        if c >= '0' && c <= '9' {
            d = (c - 48) as i64;
        }

        if d < 0 {
            bad = true;
        } else {
            v = v * 16 + d;
            i = i + 1;
            k = k + 1;
        }
    }

    if bad || k < n {
        var pre: const u8[:] = "expecting 2 hex chars";

        if n == 4 {
            pre = "expecting 4 hex chars";
        }

        if n == 8 {
            pre = "expecting 8 hex chars";
        }
        fail_at(g, err, st, pre, i0);
        return 0, i;
    }
    return v, i;
}
// decode one UTF-8 codepoint; returns (cp, next)

fn decode_utf8(g: const u8[:], i0: i64) -> i64, i64 {
    let b0 = g[i0];

    if b0 < 128 {
        return b0, i0 + 1;
    }

    if b0 >= 194 && b0 <= 223 && i0 + 1 < g.len {
        return ( (b0 - 192) as i64) * 64 + ( (g[i0 + 1] - 128) as i64), i0 + 2;
    }

    if b0 >= 224 && b0 <= 239 && i0 + 2 < g.len {
        return ( (b0 - 224) as i64) * 4096 + ( (g[i0 + 1] - 128) as i64) * 64 + ( (g[i0 + 2] - 128) as i64), i0 + 3;
    }

    if b0 >= 240 && b0 <= 244 && i0 + 3 < g.len {
        return ( (b0 - 240) as i64) * 262144 + ( (g[i0 + 1] - 128) as i64) * 4096 + ( (g[i0 + 2] - 128) as i64) * 64 + ( (g[i0 + 3] - 128) as i64), i0 + 4;
    }
    return b0, i0 + 1;
}

fn parse_char(g: const u8[:], i0: i64, err: u8[:], st: i64[:]) -> i64, i64 {
    if i0 >= g.len {
        fail_text(g, err, st, "unexpected end of input");
        return 0, i0;
    }

    if g[i0] == '\\' {
        if i0 + 1 >= g.len {
            fail_text(g, err, st, "unexpected end of input");
            return 0, i0;
        }
        let c = g[i0 + 1];

        if c == 'x' {
            return parse_hex(g, i0 + 2, 2, err, st);
        }

        if c == 'u' {
            return parse_hex(g, i0 + 2, 4, err, st);
        }

        if c == 'U' {
            return parse_hex(g, i0 + 2, 8, err, st);
        }

        if c == 't' {
            return 9, i0 + 2;
        }

        if c == 'r' {
            return 13, i0 + 2;
        }

        if c == 'n' {
            return 10, i0 + 2;
        }

        if c == '\\' || c == '"' || c == '[' || c == ']' || c == '-' {
            return c, i0 + 2;
        }
        fail_at(g, err, st, "unknown escape", i0);
        return 0, i0;
    }
    return decode_utf8(g, i0);
}

fn sym_get(g: const u8[:], name_off: i64, name_len: i64, sbuf: u8[:], soff: i64[:], slen: i64[:], st: i64[:]) -> i64 {
    let name = g[name_off .. name_off + name_len];
    var i: i64 = 0;

    while i < st[1] {
        if slen[i] == name.len {
            if eq_bytes(sbuf[soff[i] .. soff[i] + slen[i]], name) {
                return i;
            }
        }
        i = i + 1;
    }
    let id = st[1];
    let off = st[7];
    let end = append_s(sbuf, off, name, 0, name.len);
    soff[id] = off;
    slen[id] = name.len;
    st[1] = id + 1;
    st[7] = end;
    return id;
}

fn sym_gen(base_off: i64, base_len: i64, g: const u8[:], sbuf: u8[:], soff: i64[:], slen: i64[:], st: i64[:]) -> i64 {
    let id = st[1];
    let off = st[7];
    var end = append_s(sbuf, off, g, base_off, base_len);
    end = append_c(sbuf, end, '_');
    end = append_dec(sbuf, end, id);
    soff[id] = off;
    slen[id] = end - off;
    st[1] = id + 1;
    st[7] = end;
    return id;
}

fn add_rule(
    id: i64,
    start: i64,
    len: i64,
    wtype: const i64[:],
    wval: const i64[:],
    rtype: i64[:],
    rval: i64[:],
    rstart: i64[:],
    rlen: i64[:],
    st: i64[:]) -> i64 {
    if id >= st[2] {
        var k: i64 = st[2];

        while k <= id {
            rstart[k] = 0;
            rlen[k] = 0;
            k = k + 1;
        }
        st[2] = id + 1;
    }
    let at = st[6];
    var i: i64 = 0;

    while i < len {
        rtype[at + i] = wtype[start + i];
        rval[at + i] = wval[start + i];
        i = i + 1;
    }
    rstart[id] = at;
    rlen[id] = len;
    st[6] = at + len;
    return 0;
}
// handle_repetitions: rewrites the previous expression and adds generated rules.

fn handle_reps(
    last_sym_start: i64,
    mn: i64,
    mx: i64,
    rule_name_off: i64,
    rule_name_len: i64,
    g: const u8[:],
    wtype: i64[:],
    wval: i64[:],
    rtype: i64[:],
    rval: i64[:],
    rstart: i64[:],
    rlen: i64[:],
    sbuf: u8[:],
    soff: i64[:],
    slen: i64[:],
    err: u8[:],
    st: i64[:],
    n_prev_rules: i64) -> i64 {
    let pos = st[0];

    if last_sym_start == st[3] {
        fail_at(g, err, st, "expecting preceding item to */+/?/{", pos);
        return n_prev_rules;
    }
    let no_max = mx == HUGE;
    let prev_len = st[3] - last_sym_start;
    // The previous expression is kept in the tail of the rule arena, so a
    // repetition over a long literal does not need a fixed local buffer. The
    // caller must leave room for the emitted elements and the largest repeated
    // expression: rtype.len >= emitted + prev_len.
    let sc: i64 = rtype.len - prev_len;

    if sc < st[6] + prev_len {
        fail_text(g, err, st, "repetition scratch does not fit the rule arena");
        return n_prev_rules;
    }
    var k: i64 = 0;

    while k < prev_len {
        rtype[sc + k] = wtype[last_sym_start + k];
        rval[sc + k] = wval[last_sym_start + k];
        k = k + 1;
    }
    var total: i64 = 1;

    if !no_max && mx > 0 {
        total = mx;
    } else

    if mn > 0 {
        total = mn;
    }

    if n_prev_rules * total > MAX_REP {
        fail_text(g, err, st, "number of rules that are going to be repeated multiplied by the new repetition exceeds sane defaults, please reduce the number of repetitions or rule complexity");
        return n_prev_rules;
    }

    if mn == 0 {
        st[3] = last_sym_start;
    } else {
        var r: i64 = 1;

        while r < mn {
            var z: i64 = 0;

            while z < prev_len {
                wtype[st[3] + z] = rtype[sc + z];
                wval[st[3] + z] = rval[sc + z];
                z = z + 1;
            }
            st[3] = st[3] + prev_len;
            r = r + 1;
        }
    }
    var last_rec: i64 = 0;
    var n_opt: i64 = 0;

    if no_max {
        n_opt = 1;
    } else {
        n_opt = mx - mn;
    }
    var it: i64 = 0;

    while it < n_opt {
        let rec_id = sym_gen(rule_name_off, rule_name_len, g, sbuf, soff, slen, st);
        // build prev + [REF] + ALT + END
        let tmp = st[3];
        var z: i64 = 0;

        while z < prev_len {
            wtype[tmp + z] = rtype[sc + z];
            wval[tmp + z] = rval[sc + z];
            z = z + 1;
        }
        var e: i64 = tmp + prev_len;

        if it > 0 || no_max {
            wtype[e] = E_REF;

            if no_max {
                wval[e] = rec_id;
            } else {
                wval[e] = last_rec;
            }
            e = e + 1;
        }
        wtype[e] = E_ALT;
        wval[e] = 0;
        e = e + 1;
        wtype[e] = E_END;
        wval[e] = 0;
        e = e + 1;
        add_rule(rec_id, tmp, e - tmp, wtype, wval, rtype, rval, rstart, rlen, st);
        last_rec = rec_id;
        it = it + 1;
    }

    if n_opt > 0 {
        wtype[st[3]] = E_REF;
        wval[st[3]] = last_rec;
        st[3] = st[3] + 1;
    }
    return n_prev_rules * total;
}
recursive fn parse_alternates(
    id: i64,
    name_off: i64,
    name_len: i64,
    nested: i64,
    g: const u8[:],
    wtype: i64[:],
    wval: i64[:],
    rtype: i64[:],
    rval: i64[:],
    rstart: i64[:],
    rlen: i64[:],
    sbuf: u8[:],
    soff: i64[:],
    slen: i64[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    let base = st[3];
    parse_sequence(name_off, name_len, nested, g, wtype, wval, rtype, rval, rstart, rlen, sbuf, soff, slen, err, st);

    while st[4] == 0 && st[0] < g.len && g[st[0]] == '|' {
        wtype[st[3]] = E_ALT;
        wval[st[3]] = 0;
        st[3] = st[3] + 1;
        st[0] = parse_space(g, st[0] + 1, 1);
        parse_sequence(
            name_off,
            name_len,
            nested,
            g,
            wtype,
            wval,
            rtype,
            rval,
            rstart,
            rlen,
            sbuf,
            soff,
            slen,
            err,
            st);
    }

    if st[4] == 0 {
        wtype[st[3]] = E_END;
        wval[st[3]] = 0;
        st[3] = st[3] + 1;
        add_rule(id, base, st[3] - base, wtype, wval, rtype, rval, rstart, rlen, st);
    }
    return st[0];
}
recursive fn parse_sequence(
    name_off: i64,
    name_len: i64,
    nested: i64,
    g: const u8[:],
    wtype: i64[:],
    wval: i64[:],
    rtype: i64[:],
    rval: i64[:],
    rstart: i64[:],
    rlen: i64[:],
    sbuf: u8[:],
    soff: i64[:],
    slen: i64[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    var last_sym_start: i64 = st[3];
    var n_prev: i64 = 1;
    var looping: bool = true;

    while looping && st[4] == 0 && st[0] < g.len {
        let c = g[st[0]];

        if c == '"' {
            st[0] = st[0] + 1;
            last_sym_start = st[3];
            n_prev = 1;

            while st[0] < g.len && g[st[0]] != '"' {
                let cp, ni = parse_char(g, st[0], err, st);

                if st[4] != 0 {
                    return st[0];
                }
                wtype[st[3]] = E_CHAR;
                wval[st[3]] = cp;
                st[3] = st[3] + 1;
                st[0] = ni;
            }

            if st[0] >= g.len {
                fail_text(g, err, st, "unexpected end of input");
                return st[0];
            }
            st[0] = parse_space(g, st[0] + 1, nested);
        } else

        if c == '[' {
            st[0] = st[0] + 1;
            var stype: i64 = E_CHAR;

            if st[0] < g.len && g[st[0]] == '^' {
                st[0] = st[0] + 1;
                stype = E_NOT;
            }
            last_sym_start = st[3];
            n_prev = 1;
            var closed: bool = false;

            while st[0] < g.len && !closed && st[4] == 0 {
                if g[st[0]] == ']' {
                    closed = true;
                } else {
                    let cp, ni = parse_char(g, st[0], err, st);

                    if st[4] != 0 {
                        return st[0];
                    }
                    var etype: i64 = stype;

                    if st[3] > last_sym_start {
                        etype = E_ALTCHAR;
                    }
                    wtype[st[3]] = etype;
                    wval[st[3]] = cp;
                    st[3] = st[3] + 1;

                    if ni < g.len && g[ni] == '-' && (ni + 1 >= g.len || g[ni + 1] != ']') {
                        if ni + 1 >= g.len {
                            fail_text(g, err, st, "unexpected end of input");
                            return st[0];
                        }
                        let cp2, ni2 = parse_char(g, ni + 1, err, st);

                        if st[4] != 0 {
                            return st[0];
                        }
                        wtype[st[3]] = E_RNG;
                        wval[st[3]] = cp2;
                        st[3] = st[3] + 1;
                        st[0] = ni2;
                    } else {
                        st[0] = ni;
                    }
                }
            }

            if !closed && st[4] == 0 {
                fail_text(g, err, st, "unexpected end of input");
                return st[0];
            }
            st[0] = parse_space(g, st[0] + 1, nested);
        } else

        if is_word(c) {
            let ne = parse_name(g, st[0], err, st);

            if st[4] != 0 {
                return st[0];
            }
            let id = sym_get(g, st[0], ne - st[0], sbuf, soff, slen, st);
            st[0] = parse_space(g, ne, nested);
            last_sym_start = st[3];
            n_prev = 1;
            wtype[st[3]] = E_REF;
            wval[st[3]] = id;
            st[3] = st[3] + 1;
        } else

        if c == '(' {
            st[0] = parse_space(g, st[0] + 1, 1);
            let n_before = st[1];
            let sub = sym_gen(name_off, name_len, g, sbuf, soff, slen, st);
            let pend = st[3];
            parse_alternates(
                sub,
                name_off,
                name_len,
                1,
                g,
                wtype,
                wval,
                rtype,
                rval,
                rstart,
                rlen,
                sbuf,
                soff,
                slen,
                err,
                st);

            if st[4] != 0 {
                return st[0];
            }
            st[3] = pend;
            var n_after = st[1] - n_before;

            if n_after < 1 {
                n_after = 1;
            }
            n_prev = n_after;
            last_sym_start = st[3];
            wtype[st[3]] = E_REF;
            wval[st[3]] = sub;
            st[3] = st[3] + 1;

            if st[0] >= g.len || g[st[0]] != ')' {
                fail_at(g, err, st, "expecting ')'", st[0]);
                return st[0];
            }
            st[0] = parse_space(g, st[0] + 1, nested);
        } else

        if c == '.' {
            last_sym_start = st[3];
            n_prev = 1;
            wtype[st[3]] = E_ANY;
            wval[st[3]] = 0;
            st[3] = st[3] + 1;
            st[0] = parse_space(g, st[0] + 1, nested);
        } else

        if c == '*' {
            st[0] = parse_space(g, st[0] + 1, nested);
            n_prev = handle_reps(
                last_sym_start,
                0,
                HUGE,
                name_off,
                name_len,
                g,
                wtype,
                wval,
                rtype,
                rval,
                rstart,
                rlen,
                sbuf,
                soff,
                slen,
                err,
                st,
                n_prev);
        } else

        if c == '+' {
            st[0] = parse_space(g, st[0] + 1, nested);
            n_prev = handle_reps(
                last_sym_start,
                1,
                HUGE,
                name_off,
                name_len,
                g,
                wtype,
                wval,
                rtype,
                rval,
                rstart,
                rlen,
                sbuf,
                soff,
                slen,
                err,
                st,
                n_prev);
        } else

        if c == '?' {
            st[0] = parse_space(g, st[0] + 1, nested);
            n_prev = handle_reps(
                last_sym_start,
                0,
                1,
                name_off,
                name_len,
                g,
                wtype,
                wval,
                rtype,
                rval,
                rstart,
                rlen,
                sbuf,
                soff,
                slen,
                err,
                st,
                n_prev);
        } else

        if c == '{' {
            st[0] = parse_space(g, st[0] + 1, nested);

            if st[0] >= g.len || !is_digit(g[st[0]]) {
                fail_at(g, err, st, "expecting an int", st[0]);
                return st[0];
            }
            let i0 = st[0];

            while st[0] < g.len && is_digit(g[st[0]]) {
                st[0] = st[0] + 1;
            }
            var mn: i64 = 0;
            var z: i64 = i0;

            while z < st[0] {
                mn = mn * 10 + ( (g[z] - 48) as i64);
                z = z + 1;
            }
            st[0] = parse_space(g, st[0], nested);
            var mx: i64 = HUGE;

            if st[0] < g.len && g[st[0]] == '}' {
                mx = mn;
                st[0] = parse_space(g, st[0] + 1, nested);
            } else

            if st[0] < g.len && g[st[0]] == ',' {
                st[0] = parse_space(g, st[0] + 1, nested);

                if st[0] < g.len && is_digit(g[st[0]]) {
                    let j0 = st[0];

                    while st[0] < g.len && is_digit(g[st[0]]) {
                        st[0] = st[0] + 1;
                    }
                    var v: i64 = 0;
                    z = j0;

                    while z < st[0] {
                        v = v * 10 + ( (g[z] - 48) as i64);
                        z = z + 1;
                    }
                    mx = v;
                    st[0] = parse_space(g, st[0], nested);
                }

                if st[0] >= g.len || g[st[0]] != '}' {
                    fail_at(g, err, st, "expecting '}'", st[0]);
                    return st[0];
                }
                st[0] = parse_space(g, st[0] + 1, nested);
            } else {
                fail_at(g, err, st, "expecting ','", st[0]);
                return st[0];
            }

            if mn > MAX_REP {
                fail_text(g, err, st, "number of repetitions exceeds sane defaults, please reduce the number of repetitions");
                return st[0];
            }

            if mx != HUGE && mx > MAX_REP {
                mx = HUGE;
            }
            n_prev = handle_reps(
                last_sym_start,
                mn,
                mx,
                name_off,
                name_len,
                g,
                wtype,
                wval,
                rtype,
                rval,
                rstart,
                rlen,
                sbuf,
                soff,
                slen,
                err,
                st,
                n_prev);
        } else {
            looping = false;
        }
    }
    return st[0];
}

fn parse_one_rule(
    g: const u8[:],
    wtype: i64[:],
    wval: i64[:],
    rtype: i64[:],
    rval: i64[:],
    rstart: i64[:],
    rlen: i64[:],
    sbuf: u8[:],
    soff: i64[:],
    slen: i64[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    st[3] = 0;
    let name_end = parse_name(g, st[0], err, st);

    if st[4] != 0 {
        return st[0];
    }
    let name_off = st[0];
    let name_len = name_end - name_off;
    let id = sym_get(g, name_off, name_len, sbuf, soff, slen, st);
    st[0] = parse_space(g, name_end, 0);

    if st[0] + 2 >= g.len || g[st[0]] != ':' || g[st[0] + 1] != ':' || g[st[0] + 2] != '=' {
        fail_at(g, err, st, "expecting ::=", st[0]);
        return st[0];
    }
    st[0] = parse_space(g, st[0] + 3, 1);
    parse_alternates(id, name_off, name_len, 0, g, wtype, wval, rtype, rval, rstart, rlen, sbuf, soff, slen, err, st);

    if st[4] != 0 {
        return st[0];
    }

    if st[0] < g.len && g[st[0]] == '\r' {
        if st[0] + 1 < g.len && g[st[0] + 1] == '\n' {
            st[0] = st[0] + 2;
        } else {
            st[0] = st[0] + 1;
        }
    } else

    if st[0] < g.len && g[st[0]] == '\n' {
        st[0] = st[0] + 1;
    } else

    if st[0] < g.len {
        fail_at(g, err, st, "expecting newline or end", st[0]);
        return st[0];
    }
    st[0] = parse_space(g, st[0], 1);
    return st[0];
}
// Entry: parse the grammar; returns 0 on success, 1 on error (message in err).

export fn parse_grammar(
    g: const u8[:],
    wtype: i64[:],
    wval: i64[:],
    rtype: i64[:],
    rval: i64[:],
    rstart: i64[:],
    rlen: i64[:],
    sbuf: u8[:],
    soff: i64[:],
    slen: i64[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    st[0] = 0;
    st[1] = 0;
    st[2] = 0;
    st[3] = 0;
    st[4] = 0;
    st[5] = 0;
    st[6] = 0;
    st[7] = 0;
    st[0] = parse_space(g, 0, 1);

    while st[0] < g.len && st[4] == 0 {
        parse_one_rule(g, wtype, wval, rtype, rval, rstart, rlen, sbuf, soff, slen, err, st);
    }

    if st[4] == 0 {
        var i: i64 = 0;

        while i < st[1] && st[4] == 0 {
            if rlen[i] == 0 {
                fail_text(g, err, st, "Undefined rule");
            } else {
                var k: i64 = 0;

                while k < rlen[i] && st[4] == 0 {
                    if rtype[rstart[i] + k] == E_REF {
                        let v = rval[rstart[i] + k];

                        if v >= st[2] || rlen[v] == 0 {
                            var pre: const u8[:] = "Undefined rule identifier '";
                            var n = append_s(err, 0, pre, 0, pre.len);
                            n = append_s(err, n, sbuf, soff[v], slen[v]);
                            n = append_c(err, n, '\'');
                            st[4] = 1;
                            st[5] = n;
                        }
                    }
                    k = k + 1;
                }
            }
            i = i + 1;
        }
    }
    return st[4];
}

