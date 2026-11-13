// JSON-schema-to-GBNF pattern conversion in Goose, from
// common/json-schema-to-grammar.cpp (_pattern_to_rule, _visit_pattern,
// gbnf_escape_length, build_repetition).
//
// Owned decisions:
//   - a pattern must be anchored with '^' and '$', and the anchors are stripped;
//   - '.', '(', '?:', ')', '|', '*', '+', '?', '{n,m}', '[...]' and literals are
//     turned into a GBNF expression, with consecutive literals merged;
//   - a repeated non-literal is interned as "<name>-<n>" (n counts the entry
//     after insertion, so the first is 1), a repeated literal is quoted directly;
//   - '{n,m}' uses build_repetition with an empty separator;
//   - the output is wrapped as "\"" (<rule>) "\"";
//   - an unsupported pattern falls back to the primitive string rule, an invalid
//     one reports the reason and produces no rule.
//
// The surrounding schema walk and the rule map stay native; this module takes the
// map through caller-provided buffers (the caller seeds "space").

import std;
const INT_MAX: i64 = 2147483647;
// state slots
const S_TOK: i64 = 0;
// token count
const S_TX: i64 = 1;
// token text high-water mark
const S_KEYS: i64 = 2;
// rule-map key count
const S_KB: i64 = 3;
// rule-map key bytes used
const S_I: i64 = 4;
// cursor
const S_DEPTH: i64 = 5;
// paren depth
const S_STATUS: i64 = 6;
// 0 ok, 1 unsupported, 2 invalid
const S_UNIQ: i64 = 7;
// unique rule-text id source
const S_SUBS: i64 = 8;
// interned sub-rule count
const S_REASON: i64 = 9;
// reason length
const S_START: i64 = 11;
// pattern body start
const S_END: i64 = 12;
// pattern body end
const S_NAME: i64 = 13;
// chosen rule-name length
const S_RULE: i64 = 14;
// rule-text length

fn is_non_literal(c: u8) -> bool {
    if c == '|' {
        return true;
    }

    if c == '.' {
        return true;
    }

    if c == '(' {
        return true;
    }

    if c == ')' {
        return true;
    }

    if c == '[' {
        return true;
    }

    if c == ']' {
        return true;
    }

    if c == '{' {
        return true;
    }

    if c == '}' {
        return true;
    }

    if c == '*' {
        return true;
    }

    if c == '+' {
        return true;
    }

    if c == '?' {
        return true;
    }

    if c == '^' {
        return true;
    }

    if c == '$' {
        return true;
    }
    return false;
}

fn escaped_in_literals(c: u8) -> bool {
    if c == '^' {
        return true;
    }

    if c == '$' {
        return true;
    }

    if c == '.' {
        return true;
    }

    if c == '[' {
        return true;
    }

    if c == ']' {
        return true;
    }

    if c == '(' {
        return true;
    }

    if c == ')' {
        return true;
    }

    if c == '|' {
        return true;
    }

    if c == '{' {
        return true;
    }

    if c == '}' {
        return true;
    }

    if c == '*' {
        return true;
    }

    if c == '+' {
        return true;
    }

    if c == '?' {
        return true;
    }
    return false;
}

fn hex_val(c: u8) -> i64 {
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

fn is_space_byte(c: u8) -> bool {
    if c == ' ' {
        return true;
    }

    if c == '\t' || c == '\n' || c == '\r' {
        return true;
    }

    if c == (11 as u8) || c == (12 as u8) {
        return true;
    }
    return false;
}

fn escape_len(p: const u8[:], i: i64) -> i64 {
    if i + 1 >= p.len {
        return 0;
    }

    if p[i] != '\\' {
        return 0;
    }
    let nx = p[i + 1];
    var n: i64 = 0;

    if nx == 'x' {
        n = 2;
    }

    if nx == 'u' {
        n = 4;
    }

    if nx == 'U' {
        n = 8;
    }

    if nx == 't' {
        return 2;
    }

    if nx == 'r' {
        return 2;
    }

    if nx == 'n' {
        return 2;
    }

    if nx == '\\' {
        return 2;
    }

    if nx == '"' {
        return 2;
    }

    if nx == '[' {
        return 2;
    }

    if nx == ']' {
        return 2;
    }

    if nx == '-' {
        return 2;
    }

    if n == 0 {
        return 0;
    }

    if i + 2 + n > p.len {
        return 0;
    }
    var k: i64 = i + 2;

    while k < i + 2 + n {
        if hex_val(p[k]) < 0 {
            return 0;
        }
        k = k + 1;
    }
    return 2 + n;
}

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

    if x < 0 {
        p = append_c(dst, p, '-');
        x = 0 - x;
    }
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

fn push_tok(st: i64[:], to: i64[:], tl: i64[:], ti: i64[:], off: i64, len: i64, lit: i64) -> i64 {
    let k = st[S_TOK];

    if k < to.len {
        to[k] = off;
        tl[k] = len;
        ti[k] = lit;
    }
    st[S_TOK] = k + 1;
    return k;
}

fn push_copy(
    st: i64[:],
    tb: u8[:],
    to: i64[:],
    tl: i64[:],
    ti: i64[:],
    s: const u8[:],
    off: i64,
    len: i64,
    lit: i64) -> i64 {
    let at = st[S_TX];
    let end = append_s(tb, at, s, off, len);
    st[S_TX] = end;
    return push_tok(st, to, tl, ti, at, len, lit);
}
// Render one token as a rule expression: literals get quotes, rules stay raw.

fn render_tok(st: i64[:], tb: u8[:], to: const i64[:], tl: const i64[:], ti: const i64[:], idx: i64) -> i64, i64 {
    let at = st[S_TX];
    var end: i64 = at;

    if ti[idx] != 0 {
        end = append_c(tb, at, '"');
        end = append_s(tb, end, tb, to[idx], tl[idx]);
        end = append_c(tb, end, '"');
    } else {
        end = append_s(tb, at, tb, to[idx], tl[idx]);
    }
    st[S_TX] = end;
    return at, end - at;
}
// Join tokens [base, S_TOK) with single spaces, merging consecutive literals.

fn join_seq(st: i64[:], tb: u8[:], to: const i64[:], tl: const i64[:], ti: const i64[:], base: i64) -> i64, i64 {
    let at = st[S_TX];
    var p: i64 = at;
    var have_item: bool = false;
    var have_lit: bool = false;
    var k: i64 = base;

    while k < st[S_TOK] {
        if ti[k] != 0 {
            if !have_lit {
                if have_item {
                    p = append_c(tb, p, ' ');
                }
                p = append_c(tb, p, '"');
                have_item = true;
                have_lit = true;
            }
            p = append_s(tb, p, tb, to[k], tl[k]);
        } else {
            if have_lit {
                p = append_c(tb, p, '"');
                have_lit = false;
            }

            if have_item {
                p = append_c(tb, p, ' ');
            }
            p = append_s(tb, p, tb, to[k], tl[k]);
            have_item = true;
        }
        k = k + 1;
    }

    if have_lit {
        p = append_c(tb, p, '"');
    }
    st[S_TX] = p;
    return at, p - at;
}
// build_repetition with an empty separator.

fn build_rep(st: i64[:], tb: u8[:], item_off: i64, item_len: i64, mn: i64, mx: i64) -> i64, i64 {
    let at = st[S_TX];

    if mx == 0 {
        return at, 0;
    }
    let has_max = mx != INT_MAX;
    var p = append_s(tb, at, tb, item_off, item_len);

    if mn == 0 && mx == 1 {
        p = append_c(tb, p, '?');
    } else

    if mn == 1 && !has_max {
        p = append_c(tb, p, '+');
    } else

    if mn == 0 && !has_max {
        p = append_c(tb, p, '*');
    } else {
        p = append_c(tb, p, '{');
        p = append_dec(tb, p, mn);
        p = append_c(tb, p, ',');

        if has_max {
            p = append_dec(tb, p, mx);
        }
        p = append_c(tb, p, '}');
    }
    st[S_TX] = p;
    return at, p - at;
}

fn sanitize_buf(s: const u8[:], out: u8[:]) -> i64 {
    return sanitize_len(s, s.len, out);
}

fn sanitize_len(s: const u8[:], len: i64, out: u8[:]) -> i64 {
    var i: i64 = 0;
    var n: i64 = 0;
    var in_run: bool = false;

    while i < len {
        let b = s[i];
        var valid: bool = false;

        if b >= 'a' && b <= 'z' {
            valid = true;
        }

        if b >= 'A' && b <= 'Z' {
            valid = true;
        }

        if b >= '0' && b <= '9' {
            valid = true;
        }

        if b == '-' {
            valid = true;
        }

        if valid {
            if n < out.len {
                out[n] = b;
            }
            n = n + 1;
            in_run = false;
        } else {
            if !in_run {
                if n < out.len {
                    out[n] = '-';
                }
                n = n + 1;
                in_run = true;
            }
        }
        i = i + 1;
    }
    return n;
}

fn find_key(st: i64[:], kb: const u8[:], ko: const i64[:], cand: const u8[:], cand_len: i64) -> i64 {
    var i: i64 = 0;

    while i < st[S_KEYS] {
        let a = ko[i];
        var b: i64 = st[S_KB];

        if i + 1 < st[S_KEYS] {
            b = ko[i + 1];
        }

        if b - a == cand_len {
            var same: bool = true;
            var k: i64 = 0;

            while k < cand_len {
                if kb[a + k] != cand[k] {
                    same = false;
                    k = cand_len;
                } else {
                    k = k + 1;
                }
            }

            if same {
                return i;
            }
        }
        i = i + 1;
    }
    return -1;
}
// _add_rule over caller-provided key buffers; the chosen key lands in nb.
// cand_len is the candidate length in nb.

fn add_rule(st: i64[:], kb: u8[:], ko: i64[:], ki: i64[:], nb: u8[:], cand_len: i64, text_id: i64) -> i64 {
    let esc_len = sanitize_len(nb, cand_len, nb);
    var f = find_key(st, kb, ko, nb, esc_len);

    if f < 0 {
        let at = st[S_KB];
        let end = append_s(kb, at, nb, 0, esc_len);
        st[S_KB] = end;
        let k = st[S_KEYS];

        if k < ko.len {
            ko[k] = at;
            ki[k] = text_id;
        }
        st[S_KEYS] = k + 1;
        return esc_len;
    }

    if ki[f] == text_id {
        return esc_len;
    }
    var i: i64 = 0;

    while i >= 0 {
        let total = append_dec(nb, esc_len, i);
        let g = find_key(st, kb, ko, nb, total);

        if g < 0 {
            let at = st[S_KB];
            let end = append_s(kb, at, nb, 0, total);
            st[S_KB] = end;
            let k = st[S_KEYS];

            if k < ko.len {
                ko[k] = at;
                ki[k] = text_id;
            }
            st[S_KEYS] = k + 1;
            return total;
        }

        if ki[g] == text_id {
            return total;
        }
        i = i + 1;
    }
    return esc_len;
}
// std::stoi-like prefix parse; ok=0 when native would throw.

fn parse_num(s: const u8[:], off: i64, len: i64) -> i64, i64 {
    var i: i64 = 0;

    while i < len && is_space_byte(s[off + i]) {
        i = i + 1;
    }
    var neg: bool = false;

    if i < len {
        if s[off + i] == '+' {
            i = i + 1;
        } else

        if s[off + i] == '-' {
            neg = true;
            i = i + 1;
        }
    }
    var any: bool = false;
    var v: i64 = 0;
    var over: bool = false;
    var stop: bool = false;

    while i < len && !stop {
        let c = s[off + i];

        if c < '0' || c > '9' {
            stop = true;
        } else {
            any = true;
            let d = (c - 48) as i64;

            if v > 100000000000 {
                over = true;
            }

            if !over {
                v = v * 10 + d;
            }
            i = i + 1;
        }
    }

    if !any {
        return 0, 0;
    }

    if over {
        return 0, 0;
    }

    if neg {
        if v > 2147483648 {
            return 0, 0;
        }
        return 0 - v, 1;
    }

    if v > 2147483647 {
        return 0, 0;
    }
    return v, 1;
}

fn set_fail(st: i64[:], reason: u8[:], status: i64, msg: const u8[:]) -> i64 {
    let n = append_s(reason, 0, msg, 0, msg.len);
    st[S_REASON] = n;
    st[S_STATUS] = status;
    return 0;
}
// prefix + p[off..off+len], clamped to the longest prefix available.

fn set_fail_at(
    st: i64[:],
    reason: u8[:],
    status: i64,
    prefix: const u8[:],
    p: const u8[:],
    off: i64,
    want: i64) -> i64 {
    var n = append_s(reason, 0, prefix, 0, prefix.len);
    var avail: i64 = p.len - off;

    if want < avail {
        avail = want;
    }

    if avail < 0 {
        avail = 0;
    }
    n = append_s(reason, n, p, off, avail);
    st[S_REASON] = n;
    st[S_STATUS] = status;
    return 0;
}
// The native transform() from _pattern_to_rule. Returns (off, len) of the joined
// expression; failures set st[S_STATUS] and st[S_REASON].
recursive fn transform(
    p: const u8[:],
    name: const u8[:],
    st: i64[:],
    tb: u8[:],
    to: i64[:],
    tl: i64[:],
    ti: i64[:],
    kb: u8[:],
    ko: i64[:],
    ki: i64[:],
    subs: i64[:],
    nb: u8[:],
    reason: u8[:]) -> i64, i64 {
    let base = st[S_TOK];

    while st[S_I] < st[S_END] && st[S_STATUS] == 0 {
        let c = p[st[S_I]];

        if c == '.' {
            let dotname: const u8[:] = "dot";
            var z: i64 = 0;

            while z < dotname.len {
                nb[z] = dotname[z];
                z = z + 1;
            }
            let n = add_rule(st, kb, ko, ki, nb, dotname.len, 1);
            push_copy(st, tb, to, tl, ti, nb, 0, n, 0);
            st[S_I] = st[S_I] + 1;
        } else

        if c == '(' {
            st[S_I] = st[S_I] + 1;

            if st[S_I] < st[S_END] {
                if p[st[S_I]] == '?' {
                    if st[S_I] + 1 < st[S_END] && p[st[S_I] + 1] == ':' {
                        st[S_I] = st[S_I] + 2;
                    } else {
                        set_fail(st, reason, 1, "unsupported group syntax");
                    }
                }
            }

            if st[S_STATUS] == 0 {
                st[S_DEPTH] = st[S_DEPTH] + 1;

                if st[S_DEPTH] > 100 {
                    set_fail(st, reason, 1, "pattern nesting too deep");
                } else {
                    let before = st[S_TOK];
                    let inner_off, inner_len = transform(p, name, st, tb, to, tl, ti, kb, ko, ki, subs, nb, reason);

                    if st[S_STATUS] == 0 {
                        st[S_TOK] = before;
                        let at = st[S_TX];
                        var q = append_c(tb, at, '(');
                        q = append_s(tb, q, tb, inner_off, inner_len);
                        q = append_c(tb, q, ')');
                        st[S_TX] = q;
                        push_tok(st, to, tl, ti, at, q - at, 0);
                    }
                }
            }
        } else

        if c == ')' {
            st[S_I] = st[S_I] + 1;

            if st[S_DEPTH] == 0 {
                set_fail(st, reason, 2, "unbalanced parentheses");
            } else {
                st[S_DEPTH] = st[S_DEPTH] - 1;
                return join_seq(st, tb, to, tl, ti, base);
            }
        } else

        if c == '^' || c == '$' {
            set_fail(st, reason, 1, "anchor inside the pattern");
        } else

        if c == '[' {
            let at = st[S_TX];
            var q = append_c(tb, at, '[');
            st[S_I] = st[S_I] + 1;

            while st[S_I] < st[S_END] && p[st[S_I]] != ']' && st[S_STATUS] == 0 {
                if p[st[S_I]] == '\\' {
                    let e = escape_len(p, st[S_I]);

                    if e == 0 {
                        set_fail_at(st, reason, 1, "unsupported escape in character class: ", p, st[S_I], 2);
                    } else {
                        q = append_s(tb, q, p, st[S_I], e);
                        st[S_I] = st[S_I] + e;
                    }
                } else {
                    q = append_c(tb, q, p[st[S_I]]);
                    st[S_I] = st[S_I] + 1;
                }
            }

            if st[S_STATUS] == 0 {
                if st[S_I] >= st[S_END] {
                    set_fail(st, reason, 2, "unterminated character class");
                } else {
                    q = append_c(tb, q, ']');
                    st[S_I] = st[S_I] + 1;
                    st[S_TX] = q;
                    push_tok(st, to, tl, ti, at, q - at, 0);
                }
            }
        } else

        if c == '|' {
            push_copy(st, tb, to, tl, ti, "|", 0, 1, 0);
            st[S_I] = st[S_I] + 1;
        } else

        if c == '*' || c == '+' || c == '?' {
            if st[S_TOK] == base {
                set_fail(st, reason, 2, "nothing to repeat");
            } else {
                let back = st[S_TOK] - 1;
                let off, len = render_tok(st, tb, to, tl, ti, back);
                let q = append_c(tb, st[S_TX], c);
                st[S_TX] = q;
                to[back] = off;
                tl[back] = len + 1;
                ti[back] = 0;
                st[S_I] = st[S_I] + 1;
            }
        } else

        if c == '{' {
            let inner_at = st[S_TX];
            st[S_I] = st[S_I] + 1;
            var q = inner_at;

            while st[S_I] < st[S_END] && p[st[S_I]] != '}' {
                q = append_c(tb, q, p[st[S_I]]);
                st[S_I] = st[S_I] + 1;
            }

            if st[S_I] >= st[S_END] {
                set_fail(st, reason, 1, "unterminated curly brackets");
            } else {
                let inner_len = q - inner_at;
                st[S_I] = st[S_I] + 1;
                var parts: i64 = 1;
                var k: i64 = 0;

                while k < inner_len {
                    if tb[inner_at + k] == ',' {
                        parts = parts + 1;
                    }
                    k = k + 1;
                }

                if parts != 1 && parts != 2 {
                    set_fail(st, reason, 1, "wrong number of values in curly brackets");
                } else {
                    var mn: i64 = 0;
                    var mx: i64 = INT_MAX;
                    var ok: i64 = 1;

                    if parts == 1 {
                        let v1, o1 = parse_num(tb, inner_at, inner_len);
                        mn = v1;
                        mx = v1;
                        ok = o1;
                    } else {
                        var comma: i64 = inner_at;

                        while tb[comma] != ',' {
                            comma = comma + 1;
                        }
                        let l0 = comma - inner_at;
                        let l1 = inner_at + inner_len - comma - 1;

                        if l0 > 0 {
                            let v0, o0 = parse_num(tb, inner_at, l0);
                            mn = v0;
                            ok = ok * o0;
                        }

                        if l1 > 0 {
                            let v1, o1 = parse_num(tb, comma + 1, l1);
                            mx = v1;
                            ok = ok * o1;
                        }
                    }

                    if ok == 0 {
                        set_fail(st, reason, 1, "invalid number in curly brackets");
                    } else

                    if st[S_TOK] == base {
                        set_fail(st, reason, 2, "nothing to repeat");
                    } else {
                        let back = st[S_TOK] - 1;
                        let sub_off = to[back];
                        let sub_len = tl[back];
                        let was_lit = ti[back];
                        var expr_off: i64 = 0;
                        var expr_len: i64 = 0;

                        if was_lit != 0 {
                            let at = st[S_TX];
                            var r = append_c(tb, at, '"');
                            r = append_s(tb, r, tb, sub_off, sub_len);
                            r = append_c(tb, r, '"');
                            st[S_TX] = r;
                            expr_off = at;
                            expr_len = r - at;
                        } else {
                            var found: i64 = -1;
                            var si: i64 = 0;

                            while si < st[S_SUBS] {
                                let so = subs[si * 4 + 0];
                                let sl = subs[si * 4 + 1];

                                if sl == sub_len {
                                    var same: bool = true;
                                    var z: i64 = 0;

                                    while z < sub_len {
                                        if tb[so + z] != tb[sub_off + z] {
                                            same = false;
                                            z = sub_len;
                                        } else {
                                            z = z + 1;
                                        }
                                    }

                                    if same {
                                        found = si;
                                    }
                                }
                                si = si + 1;
                            }

                            if found >= 0 {
                                expr_off = subs[found * 4 + 2];
                                expr_len = subs[found * 4 + 3];
                            } else {
                                let idx = st[S_SUBS] + 1;
                                var cp: i64 = 0;
                                var z: i64 = 0;

                                while z < name.len {
                                    nb[z] = name[z];
                                    z = z + 1;
                                }
                                cp = append_c(nb, name.len, '-');
                                cp = append_dec(nb, cp, idx);
                                let chosen = add_rule(st, kb, ko, ki, nb, cp, st[S_UNIQ] + 100);
                                st[S_UNIQ] = st[S_UNIQ] + 1;
                                let noff = st[S_TX];
                                let nend = append_s(tb, noff, nb, 0, chosen);
                                st[S_TX] = nend;
                                subs[st[S_SUBS] * 4 + 0] = sub_off;
                                subs[st[S_SUBS] * 4 + 1] = sub_len;
                                subs[st[S_SUBS] * 4 + 2] = noff;
                                subs[st[S_SUBS] * 4 + 3] = chosen;
                                st[S_SUBS] = st[S_SUBS] + 1;
                                expr_off = noff;
                                expr_len = chosen;
                            }
                        }
                        let ro, rl = build_rep(st, tb, expr_off, expr_len, mn, mx);
                        to[back] = ro;
                        tl[back] = rl;
                        ti[back] = 0;
                    }
                }
            }
        } else {
            let lit_at = st[S_TX];
            var q = lit_at;
            var stop: bool = false;

            while st[S_I] < st[S_END] && !stop && st[S_STATUS] == 0 {
                if p[st[S_I]] == '\\' {
                    if st[S_I] == st[S_END] - 1 {
                        set_fail(st, reason, 2, "trailing backslash");
                    } else {
                        let nx = p[st[S_I] + 1];

                        if escaped_in_literals(nx) {
                            st[S_I] = st[S_I] + 1;
                            q = append_c(tb, q, p[st[S_I]]);
                            st[S_I] = st[S_I] + 1;
                        } else {
                            let e = escape_len(p, st[S_I]);

                            if e == 0 {
                                set_fail_at(st, reason, 1, "unsupported escape: ", p, st[S_I], 2);
                            } else {
                                q = append_s(tb, q, p, st[S_I], e);
                                st[S_I] = st[S_I] + e;
                            }
                        }
                    }
                } else

                if p[st[S_I]] == '"' {
                    q = append_c(tb, q, '\\');
                    q = append_c(tb, q, '"');
                    st[S_I] = st[S_I] + 1;
                } else {
                    var take: bool = false;

                    if !is_non_literal(p[st[S_I]]) {
                        if st[S_I] == st[S_END] - 1 {
                            take = true;
                        }

                        if q == lit_at {
                            take = true;
                        }

                        if st[S_I] + 1 < st[S_END] && p[st[S_I] + 1] == '.' {
                            take = true;
                        }

                        if st[S_I] + 1 < st[S_END] && !is_non_literal(p[st[S_I] + 1]) {
                            take = true;
                        }
                    }

                    if take {
                        q = append_c(tb, q, p[st[S_I]]);
                        st[S_I] = st[S_I] + 1;
                    } else {
                        stop = true;
                    }
                }
            }

            if st[S_STATUS] == 0 {
                if q == lit_at {
                    set_fail_at(st, reason, 1, "unsupported character: ", p, st[S_I], 1);
                } else {
                    st[S_TX] = q;
                    push_tok(st, to, tl, ti, lit_at, q - lit_at, 1);
                }
            }
        }
    }

    if st[S_STATUS] != 0 {
        return 0, 0;
    }
    return join_seq(st, tb, to, tl, ti, base);
}
// _visit_pattern: convert, or fall back to the string primitive, or report.

export fn pattern_rule(
    pattern: const u8[:],
    name: const u8[:],
    dotall: i64,
    tb: u8[:],
    to: i64[:],
    tl: i64[:],
    ti: i64[:],
    kb: u8[:],
    ko: i64[:],
    ki: i64[:],
    subs: i64[:],
    st: i64[:],
    nb: u8[:],
    reason: u8[:],
    rule_out: u8[:],
    name_out: u8[:]) -> i64 {
    st[S_TOK] = 0;
    st[S_TX] = 0;
    st[S_I] = 0;
    st[S_DEPTH] = 0;
    st[S_STATUS] = 0;
    st[S_UNIQ] = 0;
    st[S_SUBS] = 0;
    st[S_REASON] = 0;
    st[S_START] = 0;
    st[S_END] = 0;
    let save_keys = st[S_KEYS];
    let save_kb = st[S_KB];

    if pattern.len < 2 || pattern[0] != '^' || pattern[pattern.len - 1] != '$' {
        set_fail(st, reason, 1, "not anchored with '^' and '$'");
    } else {
        st[S_START] = 1;
        st[S_END] = pattern.len - 1;
        st[S_I] = 1;
        let off, len = transform(pattern, name, st, tb, to, tl, ti, kb, ko, ki, subs, nb, reason);

        if st[S_STATUS] == 0 {
            if st[S_DEPTH] != 0 {
                set_fail(st, reason, 2, "unbalanced parentheses");
            } else {
                var p = append_c(rule_out, 0, '"');
                p = append_c(rule_out, p, '\\');
                p = append_c(rule_out, p, '"');
                p = append_c(rule_out, p, '"');
                p = append_c(rule_out, p, ' ');
                p = append_c(rule_out, p, '(');
                p = append_s(rule_out, p, tb, off, len);
                p = append_c(rule_out, p, ')');
                p = append_c(rule_out, p, ' ');
                p = append_c(rule_out, p, '"');
                p = append_c(rule_out, p, '\\');
                p = append_c(rule_out, p, '"');
                p = append_c(rule_out, p, '"');
                var z: i64 = 0;

                while z < name.len {
                    nb[z] = name[z];
                    z = z + 1;
                }
                st[S_RULE] = p;
                let key = add_rule(st, kb, ko, ki, nb, name.len, 3);
                var w: i64 = 0;

                while w < key {
                    if w < name_out.len {
                        name_out[w] = nb[w];
                    }
                    w = w + 1;
                }
                st[S_NAME] = key;
                return 0;
            }
        }
    }
    st[S_KEYS] = save_keys;
    st[S_KB] = save_kb;
    st[S_TOK] = 0;
    st[S_TX] = 0;
    st[S_SUBS] = 0;

    if st[S_STATUS] == 1 {
        let prim: const u8[:] = "string";
        var z: i64 = 0;

        while z < prim.len {
            nb[z] = prim[z];
            z = z + 1;
        }
        // _add_primitive("string") then _add_rule(name, "string")
        add_rule(st, kb, ko, ki, nb, prim.len, 2);
        z = 0;

        while z < prim.len {
            rule_out[z] = prim[z];
            z = z + 1;
        }
        st[S_RULE] = prim.len;
        z = 0;

        while z < name.len {
            nb[z] = name[z];
            z = z + 1;
        }
        let key = add_rule(st, kb, ko, ki, nb, name.len, 2);
        var w: i64 = 0;

        while w < key {
            if w < name_out.len {
                name_out[w] = nb[w];
            }
            w = w + 1;
        }
        st[S_NAME] = key;
        return 1;
    }
    st[S_NAME] = 0;
    return 2;
}
// get_dot's rule text: the dotall variant or the single-line class.

export fn dot_rule(dotall: i64, out: u8[:]) -> i64 {
    let wide: const u8[:] = "[\\U00000000-\\U0010FFFF]";
    let narrow: const u8[:] = "[^\\x0A\\x0D]";

    if dotall != 0 {
        return append_s(out, 0, wide, 0, wide.len);
    }
    return append_s(out, 0, narrow, 0, narrow.len);
}
// KIND_STRING shape selection: 0 pattern, 1 uuid, 2 date-string, 3 time-string,
// 4 date-time-string, 5 length-bounded char run, 6 the plain string primitive.

export fn string_shape(has_pattern: i64, format: i64, min_len: i64, max_len: i64) -> i64 {
    if has_pattern != 0 {
        return 0;
    }

    if format == 1 {
        return 1;
    }

    if format == 2 {
        return 2;
    }

    if format == 3 {
        return 3;
    }

    if format == 4 {
        return 4;
    }

    if min_len > 0 || max_len >= 0 {
        return 5;
    }
    return 6;
}
// KIND_STRING with minLength/maxLength: "\"" + char{min,max} + "\""

export fn length_rule(min_len: i64, max_len: i64, out: u8[:]) -> i64 {
    let item: const u8[:] = "char";
    let at = 0;
    var p = append_c(out, at, '"');
    p = append_c(out, p, '\\');
    p = append_c(out, p, '"');
    p = append_c(out, p, '"');
    p = append_c(out, p, ' ');
    var mx: i64 = max_len;

    if mx < 0 {
        mx = INT_MAX;
    }
    let ro, rl = build_rep_at(out, p, item, 0, item.len, min_len, mx);
    p = ro + rl;
    p = append_c(out, p, ' ');
    p = append_c(out, p, '"');
    p = append_c(out, p, '\\');
    p = append_c(out, p, '"');
    p = append_c(out, p, '"');
    return p;
}
// build_rep over a caller buffer starting at `at` (no state involved)

fn build_rep_at(out: u8[:], at: i64, item: const u8[:], item_off: i64, item_len: i64, mn: i64, mx: i64) -> i64, i64 {
    if mx == 0 {
        return at, 0;
    }
    let has_max = mx != INT_MAX;
    var p = append_s(out, at, item, item_off, item_len);

    if mn == 0 && mx == 1 {
        p = append_c(out, p, '?');
    } else

    if mn == 1 && !has_max {
        p = append_c(out, p, '+');
    } else

    if mn == 0 && !has_max {
        p = append_c(out, p, '*');
    } else {
        p = append_c(out, p, '{');
        p = append_dec(out, p, mn);
        p = append_c(out, p, ',');

        if has_max {
            p = append_dec(out, p, mx);
        }
        p = append_c(out, p, '}');
    }
    return at, p - at;
}
// format_literal: quote a GBNF literal, escaping CR, LF, '"' and '\' only.
// The map also has '-' and ']', but the native regex does not select them.

export fn format_literal(lit: const u8[:], out: u8[:]) -> i64 {
    var p = append_c(out, 0, '"');
    var i: i64 = 0;

    while i < lit.len {
        let c = lit[i];

        if c == (13 as u8) {
            p = append_c(out, p, '\\');
            p = append_c(out, p, 'r');
        } else

        if c == (10 as u8) {
            p = append_c(out, p, '\\');
            p = append_c(out, p, 'n');
        } else

        if c == '"' {
            p = append_c(out, p, '\\');
            p = append_c(out, p, '"');
        } else

        if c == '\\' {
            p = append_c(out, p, '\\');
            p = append_c(out, p, '\\');
        } else {
            p = append_c(out, p, c);
        }
        i = i + 1;
    }
    return append_c(out, p, '"');
}

