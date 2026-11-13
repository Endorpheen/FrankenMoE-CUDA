// PEG matching with AST accumulation in Goose, from common/peg-parser.cpp
// (rule, tag, atomic; sequence/choice/repetition node propagation and the
// common_peg_ast_arena node/dump format).
//
// Owned decisions:
//   - rule/tag wrap a non-failing child into one node (rule or tag name, start,
//     end, text, child nodes, partial = NEED_MORE) and return exactly that node;
//   - sequence and repetition concatenate child nodes in order; choice returns
//     the chosen option's nodes; atomic drops nodes on NEED_MORE;
//   - node ids are creation order, and dump prints every node with its subtree,
//     so nested nodes repeat at the top level.
//
// Schema/ref matching and invalid-UTF-8 tracking are outside this slice.

import std;
const R_FAIL: i64 = 0;
const R_OK: i64 = 1;
const R_NEED: i64 = 2;
const K_LIT: i64 = 3;
const K_SEQ: i64 = 4;
const K_CHOICE: i64 = 5;
const K_REP: i64 = 6;
const K_ANY: i64 = 9;
const K_SPACE: i64 = 10;
const K_RULE: i64 = 14;
const K_ATOMIC: i64 = 16;
const K_TAG: i64 = 17;
const K_GBNF: i64 = 18;
const K_AC: i64 = 19;

fn is_space_byte(c: u8) -> bool {
    if c == ' ' || c == '\t' || c == '\n' || c == '\r' {
        return true;
    }

    if c == (11 as u8) || c == (12 as u8) {
        return true;
    }
    return false;
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

fn utf8_kind(input: const u8[:], off: i64) -> i64, i64 {
    if off >= input.len {
        return 1, 0;
    }
    let b0 = input[off];

    if b0 < 128 {
        return 0, 1;
    }

    if b0 < 192 {
        return 2, 1;
    }

    if b0 < 224 {
        if off + 1 >= input.len {
            return 1, 1;
        }

        if (input[off + 1] & 192) != 128 {
            return 2, 1;
        }
        return 0, 2;
    }

    if b0 < 240 {
        var i: i64 = 1;

        while i < 3 {
            if off + i >= input.len {
                return 1, i;
            }

            if (input[off + i] & 192) != 128 {
                return 2, i;
            }
            i = i + 1;
        }
        return 0, 3;
    }

    if b0 < 248 {
        var i: i64 = 1;

        while i < 4 {
            if off + i >= input.len {
                return 1, i;
            }

            if (input[off + i] & 192) != 128 {
                return 2, i;
            }
            i = i + 1;
        }
        return 0, 4;
    }
    return 2, 1;
}
// state: 0 ast count, 1 result-node count, 2 ast children used
recursive fn parse_node(
    id: i64,
    start: i64,
    lenient: i64,
    input: const u8[:],
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    sidx: const i64[:],
    mn: const i64[:],
    mx: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    a_rule: i64[:],
    a_tag: i64[:],
    a_start: i64[:],
    a_end: i64[:],
    a_toff: i64[:],
    a_tlen: i64[:],
    a_cs: i64[:],
    a_cc: i64[:],
    a_partial: i64[:],
    achild: i64[:],
    nres: i64[:],
    st: i64[:]) -> i64, i64, i64, i64, i64 {
    let k = kind[id];

    if k == K_LIT {
        let si = sidx[id];
        let off = sdir[2 * si + 0];
        let lit_len = sdir[2 * si + 1];
        var pos: i64 = start;
        var i: i64 = 0;

        while i < lit_len {
            if pos >= input.len {
                if lenient == 0 {
                    return R_FAIL, start, start, st[1], 0;
                }
                return R_NEED, start, pos, st[1], 0;
            }

            if input[pos] != strs[off + i] {
                return R_FAIL, start, start, st[1], 0;
            }
            pos = pos + 1;
            i = i + 1;
        }
        return R_OK, start, pos, st[1], 0;
    }

    if k == K_SEQ {
        let base = st[1];
        var pos: i64 = start;
        let cb = cfirst[id];
        let n = ccount[id];
        var i: i64 = 0;

        while i < n {
            let t, s, e, rb, rc = parse_node(
                children[cb + i],
                pos,
                lenient,
                input,
                kind,
                cfirst,
                ccount,
                children,
                sidx,
                mn,
                mx,
                strs,
                sdir,
                a_rule,
                a_tag,
                a_start,
                a_end,
                a_toff,
                a_tlen,
                a_cs,
                a_cc,
                a_partial,
                achild,
                nres,
                st);

            if t == R_FAIL {
                return R_FAIL, start, e, st[1], 0;
            }

            if t == R_NEED {
                return R_NEED, start, e, base, st[1] - base;
            }
            pos = e;
            i = i + 1;
        }
        return R_OK, start, pos, base, st[1] - base;
    }

    if k == K_CHOICE {
        let cb = cfirst[id];
        let n = ccount[id];
        var i: i64 = 0;

        while i < n {
            let t, s, e, rb, rc = parse_node(
                children[cb + i],
                start,
                lenient,
                input,
                kind,
                cfirst,
                ccount,
                children,
                sidx,
                mn,
                mx,
                strs,
                sdir,
                a_rule,
                a_tag,
                a_start,
                a_end,
                a_toff,
                a_tlen,
                a_cs,
                a_cc,
                a_partial,
                achild,
                nres,
                st);

            if t != R_FAIL {
                return t, s, e, rb, rc;
            }
            i = i + 1;
        }
        return R_FAIL, start, start, st[1], 0;
    }

    if k == K_REP {
        let child = children[cfirst[id]];
        let mxv = mx[id];
        let mnv = mn[id];
        let base = st[1];
        var pos: i64 = start;
        var count: i64 = 0;
        var looping: bool = true;

        while looping {
            if mxv != -1 && count >= mxv {
                looping = false;
            } else {
                if pos >= input.len {
                    looping = false;
                } else {
                    let t, s, e, rb, rc = parse_node(
                        child,
                        pos,
                        lenient,
                        input,
                        kind,
                        cfirst,
                        ccount,
                        children,
                        sidx,
                        mn,
                        mx,
                        strs,
                        sdir,
                        a_rule,
                        a_tag,
                        a_start,
                        a_end,
                        a_toff,
                        a_tlen,
                        a_cs,
                        a_cc,
                        a_partial,
                        achild,
                        nres,
                        st);

                    if t == R_OK {
                        if e == pos {
                            looping = false;
                        } else {
                            pos = e;
                            count = count + 1;
                        }
                    } else

                    if t == R_NEED {
                        return R_NEED, start, e, base, st[1] - base;
                    } else {
                        looping = false;
                    }
                }
            }
        }

        if mnv > 0 && count < mnv {
            if pos >= input.len && lenient != 0 {
                return R_NEED, start, pos, base, st[1] - base;
            }
            return R_FAIL, start, pos, st[1], 0;
        }
        return R_OK, start, pos, base, st[1] - base;
    }

    if k == K_ANY {
        let uk, ub = utf8_kind(input, start);

        if uk == 1 {
            if lenient == 0 {
                return R_FAIL, start, start, st[1], 0;
            }
            return R_NEED, start, start, st[1], 0;
        }

        if uk == 2 {
            return R_FAIL, start, start, st[1], 0;
        }
        return R_OK, start, start + ub, st[1], 0;
    }

    if k == K_SPACE {
        var pos: i64 = start;

        while pos < input.len && is_space_byte(input[pos]) {
            pos = pos + 1;
        }
        return R_OK, start, pos, st[1], 0;
    }

    if k == K_ATOMIC || k == K_GBNF || k == K_AC {
        let t, s, e, rb, rc = parse_node(
            children[cfirst[id]],
            start,
            lenient,
            input,
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            mn,
            mx,
            strs,
            sdir,
            a_rule,
            a_tag,
            a_start,
            a_end,
            a_toff,
            a_tlen,
            a_cs,
            a_cc,
            a_partial,
            achild,
            nres,
            st);

        if k == K_ATOMIC && t == R_NEED {
            return t, s, e, st[1], 0;
        }
        return t, s, e, rb, rc;
    }

    if k == K_RULE || k == K_TAG {
        let t, s, e, rb, rc = parse_node(
            children[cfirst[id]],
            start,
            lenient,
            input,
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            mn,
            mx,
            strs,
            sdir,
            a_rule,
            a_tag,
            a_start,
            a_end,
            a_toff,
            a_tlen,
            a_cs,
            a_cc,
            a_partial,
            achild,
            nres,
            st);

        if t == R_FAIL {
            return t, s, e, st[1], 0;
        }
        let nid = st[0];
        let si = sidx[id];

        if k == K_RULE {
            a_rule[nid] = si;
            a_tag[nid] = -1;
        } else {
            a_rule[nid] = -1;
            a_tag[nid] = si;
        }
        a_start[nid] = s;
        a_end[nid] = e;

        if s < input.len {
            a_toff[nid] = s;
            a_tlen[nid] = e - s;
        } else {
            a_toff[nid] = 0;
            a_tlen[nid] = 0;
        }
        let cb = st[2];
        var z: i64 = 0;

        while z < rc {
            achild[cb + z] = nres[rb + z];
            z = z + 1;
        }
        a_cs[nid] = cb;
        a_cc[nid] = rc;

        if t == R_NEED {
            a_partial[nid] = 1;
        } else {
            a_partial[nid] = 0;
        }
        st[2] = cb + rc;
        st[0] = nid + 1;
        nres[st[1]] = nid;
        st[1] = st[1] + 1;
        return t, s, e, st[1] - 1, 1;
    }
    return R_FAIL, start, start, st[1], 0;
}

export fn peg_parse_ast(
    root: i64,
    input: const u8[:],
    start: i64,
    lenient: i64,
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    sidx: const i64[:],
    mn: const i64[:],
    mx: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    a_rule: i64[:],
    a_tag: i64[:],
    a_start: i64[:],
    a_end: i64[:],
    a_toff: i64[:],
    a_tlen: i64[:],
    a_cs: i64[:],
    a_cc: i64[:],
    a_partial: i64[:],
    achild: i64[:],
    nres: i64[:],
    st: i64[:],
    out: i64[:]) -> i64 {
    st[0] = 0;
    st[1] = 0;
    st[2] = 0;
    let t, s, e, rb, rc = parse_node(
        root,
        start,
        lenient,
        input,
        kind,
        cfirst,
        ccount,
        children,
        sidx,
        mn,
        mx,
        strs,
        sdir,
        a_rule,
        a_tag,
        a_start,
        a_end,
        a_toff,
        a_tlen,
        a_cs,
        a_cc,
        a_partial,
        achild,
        nres,
        st);
    out[0] = t;
    out[1] = s;
    out[2] = e;
    out[3] = rb;
    out[4] = rc;
    return t;
}
recursive fn dump_node(
    id: i64,
    indent: i64,
    input: const u8[:],
    strs: const u8[:],
    sdir: const i64[:],
    a_rule: const i64[:],
    a_tag: const i64[:],
    a_start: const i64[:],
    a_end: const i64[:],
    a_toff: const i64[:],
    a_tlen: const i64[:],
    a_cs: const i64[:],
    a_cc: const i64[:],
    achild: const i64[:],
    out: u8[:],
    st: i64[:]) -> i64 {
    var p: i64 = st[3];
    var z: i64 = 0;
    var two: i64 = 0;

    while two < indent * 2 {
        p = append_c(out, p, ' ');
        two = two + 1;
    }
    p = append_s(out, p, "NODE ", 0, 5);
    p = append_dec(out, p, id);

    if a_rule[id] >= 0 {
        let si = a_rule[id];
        let ro = sdir[2 * si + 0];
        let rl = sdir[2 * si + 1];
        p = append_s(out, p, " (rule ", 0, 7);
        p = append_s(out, p, strs, ro, rl);
        p = append_c(out, p, ')');
    }

    if a_tag[id] >= 0 {
        let si = a_tag[id];
        let ro = sdir[2 * si + 0];
        let rl = sdir[2 * si + 1];
        p = append_s(out, p, " (tag ", 0, 6);
        p = append_s(out, p, strs, ro, rl);
        p = append_c(out, p, ')');
    }
    p = append_s(out, p, " ['", 0, 3);
    p = append_s(out, p, input, a_toff[id], a_tlen[id]);
    p = append_s(out, p, "']", 0, 2);
    p = append_c(out, p, (10 as u8));
    st[3] = p;
    let cb = a_cs[id];
    let n = a_cc[id];
    z = 0;

    while z < n {
        dump_node(
            achild[cb + z],
            indent + 1,
            input,
            strs,
            sdir,
            a_rule,
            a_tag,
            a_start,
            a_end,
            a_toff,
            a_tlen,
            a_cs,
            a_cc,
            achild,
            out,
            st);
        z = z + 1;
    }
    return st[3];
}

export fn ast_dump(
    input: const u8[:],
    strs: const u8[:],
    sdir: const i64[:],
    a_rule: const i64[:],
    a_tag: const i64[:],
    a_start: const i64[:],
    a_end: const i64[:],
    a_toff: const i64[:],
    a_tlen: const i64[:],
    a_cs: const i64[:],
    a_cc: const i64[:],
    a_partial: const i64[:],
    achild: const i64[:],
    an: i64,
    out: u8[:],
    st: i64[:]) -> i64 {
    st[3] = 0;
    var id: i64 = 0;

    while id < an {
        dump_node(id, 0, input, strs, sdir, a_rule, a_tag, a_start, a_end, a_toff, a_tlen, a_cs, a_cc, achild, out, st);
        id = id + 1;
    }
    return st[3];
}

