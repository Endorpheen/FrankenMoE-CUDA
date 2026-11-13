// PEG parser-tree dump in Goose, from common/peg-parser.cpp
// (common_peg_arena::dump / dump_impl).
//
// Owned decisions:
//   - the textual form of every parser variant;
//   - the visited set: ids are added and never removed, so a shared subtree
//     prints "[cycle]" the second time;
//   - Tag re-dumps its child with a fresh visited set.
//
// Node kinds: 0 eps,1 start,2 end,3 literal,4 sequence,5 choice,6 repetition,
// 7 and,8 not,9 any,10 space,11 chars,12 string,13 until,14 rule,15 ref,
// 16 atomic,17 tag,18 gbnf,19 ac.

import std;

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

fn vis_has(vis: const i64[:], n: i64, id: i64) -> bool {
    var i: i64 = 0;

    while i < n {
        if vis[i] == id {
            return true;
        }
        i = i + 1;
    }
    return false;
}
// str(idx) shortcut

fn sget(strs: const u8[:], sdir: const i64[:], idx: i64) -> const u8[:] {
    let off = sdir[2 * idx + 0];
    let len = sdir[2 * idx + 1];
    return strs[off .. off + len];
}
recursive fn dump_node(
    id: i64,
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    sidx: const i64[:],
    dstart: const i64[:],
    dcount: const i64[:],
    delims: const i64[:],
    mn: const i64[:],
    mx: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    out: u8[:],
    vis: i64[:],
    st: i64[:]) -> i64 {
    if vis_has(vis, st[0], id) {
        let at = st[1];
        let e = append_s(out, at, "[cycle]", 0, 7);
        st[1] = e;
        return e;
    }
    vis[st[0]] = id;
    st[0] = st[0] + 1;
    var p: i64 = st[1];
    let k = kind[id];

    if k == 0 {
        p = append_s(out, p, "Epsilon", 0, 7);
    } else

    if k == 1 {
        p = append_s(out, p, "Start", 0, 5);
    } else

    if k == 2 {
        p = append_s(out, p, "End", 0, 3);
    } else

    if k == 3 {
        p = append_s(out, p, "Literal(", 0, 8);
        let s = sget(strs, sdir, sidx[id]);
        p = append_s(out, p, s, 0, s.len);
        p = append_c(out, p, ')');
    } else

    if k == 4 || k == 5 {
        if k == 4 {
            p = append_s(out, p, "Sequence(", 0, 9);
        } else {
            p = append_s(out, p, "Choice(", 0, 7);
        }
        let base = cfirst[id];
        let n = ccount[id];
        var i: i64 = 0;

        while i < n {
            if i > 0 {
                p = append_s(out, p, ", ", 0, 2);
            }
            st[1] = p;
            dump_node(
                children[base + i],
                kind,
                cfirst,
                ccount,
                children,
                sidx,
                dstart,
                dcount,
                delims,
                mn,
                mx,
                strs,
                sdir,
                out,
                vis,
                st);
            p = st[1];
            i = i + 1;
        }
        p = append_c(out, p, ')');
    } else

    if k == 6 {
        p = append_s(out, p, "Repetition(", 0, 11);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_s(out, p, ", ", 0, 2);
        p = append_dec(out, p, mn[id]);
        p = append_s(out, p, ", ", 0, 2);

        if mx[id] == -1 {
            p = append_s(out, p, "unbounded", 0, 9);
        } else {
            p = append_dec(out, p, mx[id]);
        }
        p = append_c(out, p, ')');
    } else

    if k == 7 {
        p = append_s(out, p, "And(", 0, 4);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_c(out, p, ')');
    } else

    if k == 8 {
        p = append_s(out, p, "Not(", 0, 4);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_c(out, p, ')');
    } else

    if k == 9 {
        p = append_s(out, p, "Any", 0, 3);
    } else

    if k == 10 {
        p = append_s(out, p, "Space", 0, 5);
    } else

    if k == 11 {
        p = append_s(out, p, "CharRepeat(", 0, 11);
        let s = sget(strs, sdir, sidx[id]);
        p = append_s(out, p, s, 0, s.len);
        p = append_s(out, p, ", ", 0, 2);
        p = append_dec(out, p, mn[id]);
        p = append_s(out, p, ", ", 0, 2);

        if mx[id] == -1 {
            p = append_s(out, p, "unbounded", 0, 9);
        } else {
            p = append_dec(out, p, mx[id]);
        }
        p = append_c(out, p, ')');
    } else

    if k == 12 {
        p = append_s(out, p, "String(", 0, 7);
        let s = sget(strs, sdir, sidx[id]);

        if s.len > 0 {
            p = append_c(out, p, s[0]);
        }
        p = append_c(out, p, ')');
    } else

    if k == 13 {
        p = append_s(out, p, "Until(", 0, 6);
        let base = dstart[id];
        let n = dcount[id];
        var i2: i64 = 0;

        while i2 < n {
            if i2 > 0 {
                p = append_s(out, p, " | ", 0, 3);
            }
            let s = sget(strs, sdir, delims[base + i2]);
            p = append_s(out, p, s, 0, s.len);
            i2 = i2 + 1;
        }
        p = append_c(out, p, ')');
    } else

    if k == 14 {
        p = append_s(out, p, "Rule(", 0, 5);
        let s = sget(strs, sdir, sidx[id]);
        p = append_s(out, p, s, 0, s.len);
        p = append_s(out, p, ", ", 0, 2);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_c(out, p, ')');
    } else

    if k == 15 {
        p = append_s(out, p, "Ref(", 0, 4);
        let s = sget(strs, sdir, sidx[id]);
        p = append_s(out, p, s, 0, s.len);
        p = append_c(out, p, ')');
    } else

    if k == 16 {
        p = append_s(out, p, "Atomic(", 0, 7);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_c(out, p, ')');
    } else

    if k == 17 {
        p = append_s(out, p, "Tag(", 0, 4);
        let s = sget(strs, sdir, sidx[id]);
        p = append_s(out, p, s, 0, s.len);
        p = append_s(out, p, ", ", 0, 2);
        let save = st[0];
        st[0] = 0;
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        st[0] = save;
        p = append_c(out, p, ')');
    } else

    if k == 18 {
        p = append_s(out, p, "Gbnf(", 0, 5);
        let s = sget(strs, sdir, sidx[id]);
        p = append_s(out, p, s, 0, s.len);
        p = append_s(out, p, ", ", 0, 2);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_c(out, p, ')');
    } else

    if k == 19 {
        p = append_s(out, p, "Ac(", 0, 3);
        let base = dstart[id];
        let n = dcount[id];
        var i3: i64 = 0;

        while i3 < n {
            if i3 > 0 {
                p = append_s(out, p, " | ", 0, 3);
            }
            let s = sget(strs, sdir, delims[base + i3]);
            p = append_s(out, p, s, 0, s.len);
            i3 = i3 + 1;
        }
        p = append_s(out, p, ", ", 0, 2);
        st[1] = p;
        dump_node(
            children[cfirst[id]],
            kind,
            cfirst,
            ccount,
            children,
            sidx,
            dstart,
            dcount,
            delims,
            mn,
            mx,
            strs,
            sdir,
            out,
            vis,
            st);
        p = st[1];
        p = append_c(out, p, ')');
    } else {
        p = append_s(out, p, "Unknown", 0, 7);
    }
    st[1] = p;
    return p;
}

export fn peg_dump(
    root: i64,
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: const i64[:],
    sidx: const i64[:],
    dstart: const i64[:],
    dcount: const i64[:],
    delims: const i64[:],
    mn: const i64[:],
    mx: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    out: u8[:],
    vis: i64[:],
    st: i64[:]) -> i64 {
    st[0] = 0;
    st[1] = 0;
    dump_node(root, kind, cfirst, ccount, children, sidx, dstart, dcount, delims, mn, mx, strs, sdir, out, vis, st);
    return st[1];
}

