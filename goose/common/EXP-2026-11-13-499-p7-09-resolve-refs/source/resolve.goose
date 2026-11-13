// PEG ref resolution in Goose, from common/peg-parser.cpp
// (common_peg_arena::resolve_ref and resolve_refs).
//
// Owned decisions:
//   - every parser is visited in id order; a child slot that holds a ref is
//     replaced by the parser id of the rule with that name;
//   - sequence and choice rewrite every child, the single-child kinds
//     (repetition, and, not, rule, atomic, tag, gbnf, ac, schema) rewrite their
//     one child, and the childless kinds are untouched;
//   - the root is flattened one level the same way;
//   - a ref whose name has no rule is an error naming that rule.

import std;
const K_SEQ: i64 = 4;
const K_CHOICE: i64 = 5;
const K_REP: i64 = 6;
const K_AND: i64 = 7;
const K_NOT: i64 = 8;
const K_RULE: i64 = 14;
const K_REF: i64 = 15;
const K_ATOMIC: i64 = 16;
const K_TAG: i64 = 17;
const K_GBNF: i64 = 18;
const K_AC: i64 = 19;
const K_SCHEMA: i64 = 20;

fn str_eq(strs: const u8[:], sdir: const i64[:], a: i64, b: i64) -> bool {
    let ao = sdir[2 * a + 0];
    let al = sdir[2 * a + 1];
    let bo = sdir[2 * b + 0];
    let bl = sdir[2 * b + 1];

    if al != bl {
        return false;
    }
    var i: i64 = 0;

    while i < al {
        if strs[ao + i] != strs[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}
// rule id for a name, or -1

fn rule_for(strs: const u8[:], sdir: const i64[:], want: i64, rname: const i64[:], rid: const i64[:], rn: i64) -> i64 {
    var i: i64 = 0;

    while i < rn {
        if str_eq(strs, sdir, want, rname[i]) {
            return rid[i];
        }
        i = i + 1;
    }
    return -1;
}
// Resolve one slot in place; returns 0, or 1 when the name has no rule
// (out[1] then holds that name id).

fn resolve_slot(
    kind: const i64[:],
    sidx: const i64[:],
    children: i64[:],
    slot: i64,
    rname: const i64[:],
    rid: const i64[:],
    rn: i64,
    strs: const u8[:],
    sdir: const i64[:],
    out: i64[:]) -> i64 {
    let c = children[slot];

    if kind[c] != K_REF {
        return 0;
    }
    let r = rule_for(strs, sdir, sidx[c], rname, rid, rn);

    if r < 0 {
        out[1] = sidx[c];
        return 1;
    }
    children[slot] = r;
    return 0;
}

export fn resolve_refs(
    kind: const i64[:],
    cfirst: const i64[:],
    ccount: const i64[:],
    children: i64[:],
    sidx: const i64[:],
    rname: const i64[:],
    rid: const i64[:],
    rn: i64,
    root: i64,
    out: i64[:],
    strs: const u8[:],
    sdir: const i64[:]) -> i64 {
    out[0] = root;
    out[1] = -1;
    var i: i64 = 0;
    let n = kind.len;

    while i < n {
        let k = kind[i];

        if k == K_SEQ || k == K_CHOICE {
            var j: i64 = 0;

            while j < ccount[i] {
                let rc = resolve_slot(kind, sidx, children, cfirst[i] + j, rname, rid, rn, strs, sdir, out);

                if rc != 0 {
                    return rc;
                }
                j = j + 1;
            }
        } else

        if k == K_REP || k == K_AND || k == K_NOT || k == K_RULE || k == K_ATOMIC || k == K_TAG || k == K_GBNF || k == K_AC || k == K_SCHEMA {
            if ccount[i] > 0 {
                let rc = resolve_slot(kind, sidx, children, cfirst[i], rname, rid, rn, strs, sdir, out);

                if rc != 0 {
                    return rc;
                }
            }
        }
        i = i + 1;
    }
    // root flattening

    if root >= 0 && kind[root] == K_REF {
        let r = rule_for(strs, sdir, sidx[root], rname, rid, rn);

        if r < 0 {
            out[1] = sidx[root];
            return 1;
        }
        out[0] = r;
    }
    return 0;
}

