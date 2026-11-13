// PEG matching engine in Goose, first slice: from common/peg-parser.cpp
// parser_executor for literal, sequence, choice, repetition, any and space.
//
// Owned decisions:
//   - literal byte matching, with FAIL at end of input or NEED_MORE in lenient
//     mode (the already matched prefix stays consumed);
//   - sequence: first failure wins and carries its end; NEED_MORE propagates;
//   - choice: the first non-failing option is returned unchanged;
//   - repetition: stop before a child at end of input, stop on an empty match,
//     propagate NEED_MORE, enforce the minimum (NEED_MORE only when lenient and
//     the input is exhausted);
//   - any: one UTF-8 codepoint with the native INCOMPLETE/INVALID split;
//   - space: C-locale whitespace bytes.
//
// AST nodes are produced by the rule/tag/schema variants, which are outside this
// slice; these variants return an empty node list.

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

fn is_space_byte(c: u8) -> bool {
    if c == ' ' || c == '\t' || c == '\n' || c == '\r' {
        return true;
    }

    if c == (11 as u8) || c == (12 as u8) {
        return true;
    }
    return false;
}
// returns (kind, bytes): 0 success, 1 incomplete, 2 invalid

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
            return 1, 0;
        }

        if (input[off + 1] & 192) != 128 {
            return 2, 0;
        }
        return 0, 2;
    }

    if b0 < 240 {
        var i: i64 = 1;

        while i < 3 {
            if off + i >= input.len {
                return 1, 0;
            }

            if (input[off + i] & 192) != 128 {
                return 2, 0;
            }
            i = i + 1;
        }
        return 0, 3;
    }

    if b0 < 248 {
        var i: i64 = 1;

        while i < 4 {
            if off + i >= input.len {
                return 1, 0;
            }

            if (input[off + i] & 192) != 128 {
                return 2, 0;
            }
            i = i + 1;
        }
        return 0, 4;
    }
    return 2, 1;
}
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
    sdir: const i64[:]) -> i64, i64, i64 {
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
                    return R_FAIL, start, start;
                }
                return R_NEED, start, pos;
            }

            if input[pos] != strs[off + i] {
                return R_FAIL, start, start;
            }
            pos = pos + 1;
            i = i + 1;
        }
        return R_OK, start, pos;
    }

    if k == K_SEQ {
        var pos: i64 = start;
        let base = cfirst[id];
        let n = ccount[id];
        var i: i64 = 0;

        while i < n {
            let t, s, e = parse_node(
                children[base + i],
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
                sdir);

            if t == R_FAIL {
                return R_FAIL, start, e;
            }

            if t == R_NEED {
                return R_NEED, start, e;
            }
            pos = e;
            i = i + 1;
        }
        return R_OK, start, pos;
    }

    if k == K_CHOICE {
        let base = cfirst[id];
        let n = ccount[id];
        var i: i64 = 0;

        while i < n {
            let t, s, e = parse_node(
                children[base + i],
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
                sdir);

            if t != R_FAIL {
                return t, s, e;
            }
            i = i + 1;
        }
        return R_FAIL, start, start;
    }

    if k == K_REP {
        let child = children[cfirst[id]];
        let mxv = mx[id];
        let mnv = mn[id];
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
                    let t, s, e = parse_node(
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
                        sdir);

                    if t == R_OK {
                        if e == pos {
                            looping = false;
                        } else {
                            pos = e;
                            count = count + 1;
                        }
                    } else

                    if t == R_NEED {
                        return R_NEED, start, e;
                    } else {
                        looping = false;
                    }
                }
            }
        }

        if mnv > 0 && count < mnv {
            if pos >= input.len && lenient != 0 {
                return R_NEED, start, pos;
            }
            return R_FAIL, start, pos;
        }
        return R_OK, start, pos;
    }

    if k == K_ANY {
        let uk, ub = utf8_kind(input, start);

        if uk == 1 {
            if lenient == 0 {
                return R_FAIL, start, start;
            }
            return R_NEED, start, start;
        }

        if uk == 2 {
            return R_FAIL, start, start;
        }
        return R_OK, start, start + ub;
    }

    if k == K_SPACE {
        var pos: i64 = start;

        while pos < input.len && is_space_byte(input[pos]) {
            pos = pos + 1;
        }
        return R_OK, start, pos;
    }
    return R_FAIL, start, start;
}

export fn peg_parse(
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
    out: i64[:]) -> i64 {
    let t, s, e = parse_node(root, start, lenient, input, kind, cfirst, ccount, children, sidx, mn, mx, strs, sdir);
    out[0] = t;
    out[1] = s;
    out[2] = e;
    return t;
}

