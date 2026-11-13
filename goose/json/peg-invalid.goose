// PEG schema matching and invalid-UTF-8 tracking in Goose, from
// common/peg-parser.cpp (schema passthrough; until's invalid runs; their
// propagation through sequence, rule and tag, and into AST nodes).
//
// Owned decisions:
//   - schema is the child's result;
//   - until scans to the first delimiter start (complete or an end-reaching
//     prefix), consuming undecodable bytes as invalid runs and reporting
//     NEED_MORE at a lenient end of input;
//   - sequence concatenates child invalid runs on success/NEED_MORE and drops
//     them on failure;
//   - rule/tag copy the result's invalid runs into the node and return them;
//   - the AST dump is unchanged (invalid runs are not printed).

import std;
const R_FAIL: i64 = 0;
const R_OK: i64 = 1;
const R_NEED: i64 = 2;
const K_LIT: i64 = 3;
const K_SEQ: i64 = 4;
const K_RULE: i64 = 14;
const K_TAG: i64 = 17;
const K_UNTIL: i64 = 13;
const K_SCHEMA: i64 = 20;

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
// state: 0 ast count, 1 result-node count, 2 ast children used,
//        3 dump cursor, 4 invalid run count
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
    strs: const u8[:],
    sdir: const i64[:],
    dstart: const i64[:],
    dcount: const i64[:],
    delims: const i64[:],
    a_rule: i64[:],
    a_tag: i64[:],
    a_start: i64[:],
    a_end: i64[:],
    a_toff: i64[:],
    a_tlen: i64[:],
    a_cs: i64[:],
    a_cc: i64[:],
    a_partial: i64[:],
    a_is: i64[:],
    a_ic: i64[:],
    achild: i64[:],
    nres: i64[:],
    invp: i64[:],
    aninv: i64[:],
    st: i64[:]) -> i64, i64, i64, i64, i64, i64, i64 {
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
                    return R_FAIL, start, start, st[1], 0, st[4], 0;
                }
                return R_NEED, start, pos, st[1], 0, st[4], 0;
            }

            if input[pos] != strs[off + i] {
                return R_FAIL, start, start, st[1], 0, st[4], 0;
            }
            pos = pos + 1;
            i = i + 1;
        }
        return R_OK, start, pos, st[1], 0, st[4], 0;
    }

    if k == K_SEQ {
        let nb = st[1];
        let ib = st[4];
        var pos: i64 = start;
        let cb = cfirst[id];
        let n = ccount[id];
        var i: i64 = 0;

        while i < n {
            let t, s, e, rb, rc, ri, rn = parse_node(
                children[cb + i],
                pos,
                lenient,
                input,
                kind,
                cfirst,
                ccount,
                children,
                sidx,
                strs,
                sdir,
                dstart,
                dcount,
                delims,
                a_rule,
                a_tag,
                a_start,
                a_end,
                a_toff,
                a_tlen,
                a_cs,
                a_cc,
                a_partial,
                a_is,
                a_ic,
                achild,
                nres,
                invp,
                aninv,
                st);

            if t == R_FAIL {
                return R_FAIL, start, e, st[1], 0, st[4], 0;
            }

            if t == R_NEED {
                return R_NEED, start, e, nb, st[1] - nb, ib, st[4] - ib;
            }
            pos = e;
            i = i + 1;
        }
        return R_OK, start, pos, nb, st[1] - nb, ib, st[4] - ib;
    }

    if k == K_UNTIL {
        let ib = st[4];
        var pos: i64 = start;
        var last: i64 = start;
        var stop: bool = false;

        while pos < input.len && !stop {
            let uk, ub = utf8_kind(input, pos);

            if uk == 1 && lenient != 0 {
                return R_NEED, start, last, st[1], 0, ib, st[4] - ib;
            }

            if uk != 0 {
                var step: i64 = ub;

                if step < 1 {
                    step = 1;
                }
                invp[st[4] * 2 + 0] = pos;
                invp[st[4] * 2 + 1] = step;
                st[4] = st[4] + 1;
                pos = pos + step;
                last = pos;
            } else {
                let base = dstart[id];
                let n = dcount[id];
                var hit: bool = false;
                var r: i64 = 0;

                while r < n {
                    let di = delims[base + r];
                    let d_off = sdir[2 * di + 0];
                    let d_len = sdir[2 * di + 1];
                    var z: i64 = 0;
                    var ok: bool = true;

                    while z < d_len && pos + z < input.len {
                        if input[pos + z] != strs[d_off + z] {
                            ok = false;
                            z = d_len;
                        } else {
                            z = z + 1;
                        }
                    }

                    if ok {
                        hit = true;
                    }
                    r = r + 1;
                }

                if hit {
                    return R_OK, start, pos, st[1], 0, ib, st[4] - ib;
                }
                pos = pos + ub;
                last = pos;
            }
        }

        if last == input.len && lenient != 0 {
            return R_NEED, start, last, st[1], 0, ib, st[4] - ib;
        }
        return R_OK, start, last, st[1], 0, ib, st[4] - ib;
    }

    if k == K_RULE || k == K_TAG {
        let t, s, e, rb, rc, ri, rn = parse_node(
            children[cfirst[id]],
            start,
            lenient,
            input,
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            strs,
            sdir,
            dstart,
            dcount,
            delims,
            a_rule,
            a_tag,
            a_start,
            a_end,
            a_toff,
            a_tlen,
            a_cs,
            a_cc,
            a_partial,
            a_is,
            a_ic,
            achild,
            nres,
            invp,
            aninv,
            st);

        if t == R_FAIL {
            return t, s, e, st[1], 0, st[4], 0;
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
        // node invalid runs: copy the result's runs into a stable region
        let nis = st[5];
        var z2: i64 = 0;

        while z2 < rn {
            aninv[st[5] * 2 + 0] = invp[ (ri + z2) * 2 + 0];
            aninv[st[5] * 2 + 1] = invp[ (ri + z2) * 2 + 1];
            st[5] = st[5] + 1;
            z2 = z2 + 1;
        }
        a_is[nid] = nis;
        a_ic[nid] = rn;

        if t == R_NEED {
            a_partial[nid] = 1;
        } else {
            a_partial[nid] = 0;
        }
        st[2] = cb + rc;
        st[0] = nid + 1;
        nres[st[1]] = nid;
        st[1] = st[1] + 1;
        return t, s, e, st[1] - 1, 1, ri, rn;
    }

    if k == K_SCHEMA {
        return parse_node(
            children[cfirst[id]],
            start,
            lenient,
            input,
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            strs,
            sdir,
            dstart,
            dcount,
            delims,
            a_rule,
            a_tag,
            a_start,
            a_end,
            a_toff,
            a_tlen,
            a_cs,
            a_cc,
            a_partial,
            a_is,
            a_ic,
            achild,
            nres,
            invp,
            aninv,
            st);
    }
    return R_FAIL, start, start, st[1], 0, st[4], 0;
}

export fn peg_parse_ast2(
    root: i64,
    input: const u8[:],
    start: i64,
    lenient: i64,
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    sidx: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    dstart: const i64[:],
    dcount: const i64[:],
    delims: const i64[:],
    a_rule: i64[:],
    a_tag: i64[:],
    a_start: i64[:],
    a_end: i64[:],
    a_toff: i64[:],
    a_tlen: i64[:],
    a_cs: i64[:],
    a_cc: i64[:],
    a_partial: i64[:],
    a_is: i64[:],
    a_ic: i64[:],
    achild: i64[:],
    nres: i64[:],
    invp: i64[:],
    aninv: i64[:],
    st: i64[:],
    out: i64[:]) -> i64 {
    st[0] = 0;
    st[1] = 0;
    st[2] = 0;
    st[4] = 0;
    st[5] = 0;
    let t, s, e, rb, rc, ri, rn = parse_node(
        root,
        start,
        lenient,
        input,
        kind,
        cfirst,
        ccount,
        children,
        sidx,
        strs,
        sdir,
        dstart,
        dcount,
        delims,
        a_rule,
        a_tag,
        a_start,
        a_end,
        a_toff,
        a_tlen,
        a_cs,
        a_cc,
        a_partial,
        a_is,
        a_ic,
        achild,
        nres,
        invp,
        aninv,
        st);
    out[0] = t;
    out[1] = s;
    out[2] = e;
    out[3] = rb;
    out[4] = rc;
    out[5] = ri;
    out[6] = rn;
    return t;
}

