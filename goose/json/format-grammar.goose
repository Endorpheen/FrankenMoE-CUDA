// JSON-schema document builder in Goose, from common/json-schema.cpp
// (common_chat_schema_builder: build_node dispatch, build_object, build_array,
// build_string, build_integer, build_alternatives, build_ref, get_count,
// get_bound, get_format, resolve_ref, and the "JSON schema error at <path>:" text).
//
// The JSON document itself is decoded by the native parser and handed in as a
// flat tree (kind / integer value / double value / string id / children / keys);
// this module owns the schema reading, the node and field policy, and the
// document $ref map with its cycles.

import std;

import math;
const K_ANY: i64 = 0;
const K_REF: i64 = 1;
const K_ANY_OF: i64 = 2;
const K_ALL_OF: i64 = 3;
const K_CONST: i64 = 4;
const K_ENUM: i64 = 5;
const K_NULL: i64 = 6;
const K_BOOLEAN: i64 = 7;
const K_NUMBER: i64 = 8;
const K_INTEGER: i64 = 9;
const K_STRING: i64 = 10;
const K_ARRAY: i64 = 11;
const K_TUPLE: i64 = 12;
const K_OBJECT: i64 = 13;
const J_NULL: i64 = 0;
const J_BOOL: i64 = 1;
const J_INT: i64 = 2;
const J_DBL: i64 = 3;
const J_STR: i64 = 4;
const J_ARR: i64 = 5;
const J_OBJ: i64 = 6;
const I64_MIN: i64 = -9223372036854775807 - 1;
const I64_MAX: i64 = 9223372036854775807;

fn app(dst: u8[:], at: i64, s: const u8[:], off: i64, len: i64) -> i64 {
    var k: i64 = 0;

    while k < len {
        if at + k < dst.len {
            dst[at + k] = s[off + k];
        }
        k = k + 1;
    }
    return at + len;
}

fn appc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn appd(dst: u8[:], at: i64, v: i64) -> i64 {
    var p: i64 = at;
    var x: i64 = v;

    if x < 0 {
        p = appc(dst, p, '-');
        x = 0 - x;
    }
    var d: i64 = 1;
    var y: i64 = x;

    while y >= 10 {
        d = d + 1;
        y = y / 10;
    }
    var q: i64 = p + d - 1;
    y = x;

    while y > 0 {
        if q < dst.len {
            dst[q] = (48 as u8) + ( (y % 10) as u8);
        }
        q = q - 1;
        y = y / 10;
    }

    if x == 0 {
        if p < dst.len {
            dst[p] = '0';
        }
    }
    return p + d;
}

fn eqb(a: const u8[:], b: const u8[:]) -> bool {
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

fn s_at(strs: const u8[:], sdir: const i64[:], sid: i64) -> const u8[:] {
    let o = sdir[2 * sid + 0];
    let l = sdir[2 * sid + 1];
    return strs[o .. o + l];
}

fn s_eq(strs: const u8[:], sdir: const i64[:], sid: i64, lit: const u8[:]) -> bool {
    return eqb(s_at(strs, sdir, sid), lit);
}
// ---- path building (append-only arena) ------------------------------------

fn p_lit(pb: u8[:], st: i64[:], poff: i64, plen: i64, lit: const u8[:]) -> i64, i64 {
    let at = st[4];
    var p = app(pb, at, pb, poff, plen);
    p = app(pb, p, lit, 0, lit.len);
    st[4] = p;
    return at, p - at;
}

fn p_str(pb: u8[:], st: i64[:], poff: i64, plen: i64, lit: const u8[:], s: const u8[:]) -> i64, i64 {
    let at = st[4];
    var p = app(pb, at, pb, poff, plen);
    p = app(pb, p, lit, 0, lit.len);
    p = app(pb, p, s, 0, s.len);
    st[4] = p;
    return at, p - at;
}

fn p_dec(pb: u8[:], st: i64[:], poff: i64, plen: i64, lit: const u8[:], v: i64) -> i64, i64 {
    let at = st[4];
    var p = app(pb, at, pb, poff, plen);
    p = app(pb, p, lit, 0, lit.len);
    p = appd(pb, p, v);
    st[4] = p;
    return at, p - at;
}
// ---- errors ---------------------------------------------------------------

fn fail(pb: const u8[:], err: u8[:], st: i64[:], poff: i64, plen: i64, msg: const u8[:]) -> i64 {
    var p = app(err, 0, "JSON schema error at ", 0, 21);
    p = app(err, p, pb, poff, plen);
    p = app(err, p, ": ", 0, 2);
    p = app(err, p, msg, 0, msg.len);
    st[1] = 1;
    st[5] = p;
    return -1;
}
// ---- JSON tree access ------------------------------------------------------

fn jkey_is(jkey: const i64[:], key: i64, strs: const u8[:], sdir: const i64[:], lit: const u8[:]) -> bool {
    return s_eq(strs, sdir, jkey[key], lit);
}

fn jf_find(
    jk: const i64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    obj: i64,
    lit: const u8[:]) -> i64 {
    if jk[obj] != J_OBJ {
        return -1;
    }
    let base = jf[obj];
    let n = jn[obj];
    var i: i64 = 0;

    while i < n {
        if jkey_is(jkey, base + i, strs, sdir, lit) {
            return jchild[base + i];
        }
        i = i + 1;
    }
    return -1;
}

fn jhas(
    jk: const i64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    obj: i64,
    lit: const u8[:]) -> bool {
    return jf_find(jk, jf, jn, jkey, jchild, strs, sdir, obj, lit) >= 0;
}
// ---- node creation ---------------------------------------------------------

fn nnew(
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    npf: i64[:],
    npc: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    ntarget: i64[:],
    st: i64[:]) -> i64 {
    let id = st[0];
    st[0] = id + 1;
    nkind[id] = -1;
    ncf[id] = 0;
    ncc[id] = 0;
    npf[id] = 0;
    npc[id] = 0;
    nadd[id] = -1;
    nstr[id] = -1;
    nfmt[id] = 0;
    nminl[id] = 0;
    nmaxl[id] = -1;
    nmin[id] = I64_MIN;
    nmax[id] = I64_MAX;
    nitems[id] = -1;
    nconst[id] = -1;
    nef[id] = 0;
    nec[id] = 0;
    ntarget[id] = -1;
    return id;
}

fn refs_find(rk: const i64[:], rn: i64, sid: i64) -> i64 {
    var i: i64 = 0;

    while i < rn {
        if rk[i] == sid {
            return i;
        }
        i = i + 1;
    }
    return -1;
}
// index of a string in the document string table, by text

fn sid_find(strs: const u8[:], sdir: const i64[:], n: i64, tok: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < n {
        if eqb(s_at(strs, sdir, i), tok) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}
// ---- field readers ---------------------------------------------------------

fn get_count(
    jk: const i64[:],
    jv: const i64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    schema: i64,
    key: const u8[:],
    pb: const u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:],
    poff: i64,
    plen: i64,
    def: i64) -> i64 {
    let v = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, schema, key);

    if v < 0 {
        return def;
    }

    if jk[v] != J_INT || jv[v] < 0 {
        var m: u8[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var p = app(m, 0, key, 0, key.len);
        p = app(m, p, " must be a non-negative integer", 0, 31);
        return fail(pb, err, st, poff, plen, m[0..p]);
    }
    return jv[v];
}

fn get_bound(
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    schema: i64,
    key: const u8[:],
    round_up: i64,
    pb: const u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:],
    poff: i64,
    plen: i64) -> i64 {
    let v = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, schema, key);

    if jk[v] == J_INT {
        return jv[v];
    }

    if jk[v] != J_DBL {
        var m: u8[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var p = app(m, 0, key, 0, key.len);
        p = app(m, p, " must be a number", 0, 17);
        return fail(pb, err, st, poff, plen, m[0..p]);
    }
    let d = jd[v];

    if round_up != 0 {
        return ceil(d) as i64;
    }
    return floor(d) as i64;
}

fn get_format(
    jk: const i64[:],
    jv: const i64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    schema: i64,
    pb: const u8[:],
    err: u8[:],
    st: i64[:],
    poff: i64,
    plen: i64) -> i64 {
    let v = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, schema, "format");

    if v < 0 {
        return 0;
    }

    if jk[v] != J_STR {
        let m: const u8[:] = "format must be a string";
        fail(pb, err, st, poff, plen, m);
        return 0;
    }
    let sid = jv[v];

    if s_eq(strs, sdir, sid, "date") {
        return 2;
    }

    if s_eq(strs, sdir, sid, "time") {
        return 3;
    }

    if s_eq(strs, sdir, sid, "date-time") {
        return 4;
    }

    if s_eq(strs, sdir, sid, "uuid") {
        return 1;
    }
    let s = s_at(strs, sdir, sid);

    if s.len == 5 {
        if s[0] == 'u' && s[1] == 'u' && s[2] == 'i' && s[3] == 'd' && s[4] >= '1' && s[4] <= '5' {
            return 1;
        }
    }
    return 0;
}
// ---- $ref ------------------------------------------------------------------

fn ref_fail_notfound(
    pb: const u8[:],
    err: u8[:],
    st: i64[:],
    poff: i64,
    plen: i64,
    ref: const u8[:],
    sel: const u8[:]) -> i64 {
    var m: u8[1024] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var p = app(m, 0, "cannot resolve $ref ", 0, 20);
    p = app(m, p, ref, 0, ref.len);
    p = app(m, p, ", ", 0, 2);
    p = app(m, p, sel, 0, sel.len);
    p = app(m, p, " not found", 0, 10);
    return fail(pb, err, st, poff, plen, m[0..p]);
}

fn ref_fail_range(
    pb: const u8[:],
    err: u8[:],
    st: i64[:],
    poff: i64,
    plen: i64,
    ref: const u8[:],
    sel: const u8[:]) -> i64 {
    var m: u8[1024] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var p = app(m, 0, "cannot resolve $ref ", 0, 20);
    p = app(m, p, ref, 0, ref.len);
    p = app(m, p, ", ", 0, 2);
    p = app(m, p, sel, 0, sel.len);
    p = app(m, p, " is out of range", 0, 16);
    return fail(pb, err, st, poff, plen, m[0..p]);
}

fn resolve_ref(
    jk: const i64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    ref: const u8[:],
    pb: const u8[:],
    err: u8[:],
    st: i64[:],
    poff: i64,
    plen: i64) -> i64 {
    // tokens of ref[1:] split on '/', the first (empty) token skipped
    var bounds: i64[128] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var tok_n: i64 = 0;
    var start: i64 = 1;
    var i: i64 = 1;

    while i <= ref.len {
        var is_sep: bool = false;

        if i == ref.len {
            is_sep = true;
        } else

        if ref[i] == (47 as u8) {
            is_sep = true;
        }

        if is_sep {
            bounds[tok_n * 2 + 0] = start;
            bounds[tok_n * 2 + 1] = i - start;
            tok_n = tok_n + 1;
            start = i + 1;
        }
        i = i + 1;
    }
    var cur: i64 = -2;
    // -2 = the document root
    var ti: i64 = 1;

    while ti < tok_n {
        let tob = bounds[ti * 2 + 0];
        let tlen = bounds[ti * 2 + 1];
        let tok = ref[tob .. tob + tlen];

        if cur == -2 {
            let sid = sid_find(strs, sdir, nsid, tok);

            if sid < 0 {
                return ref_fail_notfound(pb, err, st, poff, plen, ref, tok);
            }
            let v = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, st[8], tok);

            if v < 0 {
                return ref_fail_notfound(pb, err, st, poff, plen, ref, tok);
            }
            cur = v;
        } else

        if jk[cur] == J_OBJ {
            let v = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, cur, tok);

            if v < 0 {
                return ref_fail_notfound(pb, err, st, poff, plen, ref, tok);
            }
            cur = v;
        } else

        if jk[cur] == J_ARR {
            var idx: i64 = 0;
            var ok: bool = true;
            var z: i64 = 0;

            while z < tlen {
                let c = tok[z];

                if c < '0' || c > '9' {
                    ok = false;
                    z = tlen;
                } else {
                    idx = idx * 10 + ( (c - 48) as i64);
                    z = z + 1;
                }
            }

            if !ok || idx >= jn[cur] {
                return ref_fail_range(pb, err, st, poff, plen, ref, tok);
            }
            cur = jchild[jf[cur] + idx];
        } else {
            return ref_fail_notfound(pb, err, st, poff, plen, ref, tok);
        }
        ti = ti + 1;
    }
    return cur;
}
recursive fn build_node(
    ji: i64,
    poff: i64,
    plen: i64,
    tover: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    if jk[ji] != J_OBJ {
        let m: const u8[:] = "schema must be an object";
        return fail(pb, err, st, poff, plen, m);
    }
    // 1. $ref
    let rvj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "$ref");

    if rvj >= 0 {
        if jk[rvj] != J_STR {
            let m: const u8[:] = "$ref must be a string";
            return fail(pb, err, st, poff, plen, m);
        }
        let refi = jv[rvj];
        let s = s_at(strs, sdir, refi);

        if ! (s.len >= 2 && s[0] == '#' && s[1] == '/') {
            var m: u8[1024] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
            var p = app(m, 0, "unsupported $ref ", 0, 17);
            p = app(m, p, s, 0, s.len);
            p = app(m, p, ", only references into the same document are supported", 0, 54);
            return fail(pb, err, st, poff, plen, m[0..p]);
        }

        if refs_find(rk, st[2], refi) < 0 {
            rk[st[2]] = refi;
            rv[st[2]] = -1;
            st[2] = st[2] + 1;
            let tgt = resolve_ref(jk, jf, jn, jkey, jchild, strs, sdir, nsid, s, pb, err, st, poff, plen);

            if tgt < 0 {
                return -1;
            }
            // ref targets build under the ref text as their path
            let rp_o, rp_l = p_lit(pb, st, 0, 0, s);
            let built = build_node(
                tgt,
                rp_o,
                rp_l,
                -1,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);

            if built < 0 {
                return -1;
            }
            let fi = refs_find(rk, st[2], refi);
            rv[fi] = built;
        }
        let id = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[id] = K_REF;
        nstr[id] = refi;
        pending[st[3]] = id;
        st[3] = st[3] + 1;
        return id;
    }
    // 2. oneOf / anyOf
    var altj: i64 = -1;
    var altk: const u8[:] = "";

    if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "oneOf") {
        altj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "oneOf");
        altk = "/oneOf";
    } else

    if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "anyOf") {
        altj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "anyOf");
        altk = "/anyOf";
    }

    if altj >= 0 {
        let path_o, path_l = p_lit(pb, st, poff, plen, altk);
        return build_alts(
            altj,
            path_o,
            path_l,
            K_ANY_OF,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);
    }
    // 3. type
    var typej: i64 = tover;

    if typej < 0 {
        typej = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "type");
    }

    if typej >= 0 && jk[typej] == J_ARR {
        let n = jn[typej];

        if n == 0 {
            let m: const u8[:] = "type must not be empty";
            return fail(pb, err, st, poff, plen, m);
        }
        let id = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[id] = K_ANY_OF;
        ncf[id] = st[9];
        var i: i64 = 0;

        while i < n {
            let t = jchild[jf[typej] + i];
            let path_o, path_l = p_dec(pb, st, poff, plen, "/type/", i);
            let alt = build_node(
                ji,
                path_o,
                path_l,
                t,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);

            if alt < 0 {
                return -1;
            }
            nchild[st[9]] = alt;
            st[9] = st[9] + 1;
            i = i + 1;
        }
        ncc[id] = st[9] - ncf[id];
        return id;
    }
    // 4. const
    let cj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "const");

    if cj >= 0 {
        let id = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[id] = K_CONST;
        nconst[id] = cj;
        return id;
    }
    // 5. enum
    let ej = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "enum");

    if ej >= 0 {
        if jk[ej] != J_ARR || jn[ej] == 0 {
            let m: const u8[:] = "enum must be a non-empty array";
            return fail(pb, err, st, poff, plen, m);
        }
        let id = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[id] = K_ENUM;
        nef[id] = st[10];
        var i: i64 = 0;
        let n = jn[ej];

        while i < n {
            eval[st[10]] = jchild[jf[ej] + i];
            st[10] = st[10] + 1;
            i = i + 1;
        }
        nec[id] = n;
        return id;
    }
    // 6. type must be a string when present

    if typej >= 0 && jk[typej] != J_STR {
        let m: const u8[:] = "type must be a string or an array of strings";
        return fail(pb, err, st, poff, plen, m);
    }
    var tsid: i64 = -1;

    if typej >= 0 {
        tsid = jv[typej];
    }
    // has_properties
    var hasp: bool = jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "properties");

    if !hasp {
        let ap = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "additionalProperties");

        if ap >= 0 {
            if ! (jk[ap] == J_BOOL && jv[ap] != 0) {
                hasp = true;
            }
        }
    }

    if tsid < 0 {
        if hasp {
            return build_object(
                ji,
                poff,
                plen,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }

        if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "allOf") {
            let aj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "allOf");
            let path_o, path_l = p_lit(pb, st, poff, plen, "/allOf");
            return build_alts(
                aj,
                path_o,
                path_l,
                K_ALL_OF,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }

        if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "items") || jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "prefixItems") {
            return build_array(
                ji,
                poff,
                plen,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }

        if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "pattern") || jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "minLength") || jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "maxLength") {
            return build_string(
                ji,
                poff,
                plen,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }
        let fmt = get_format(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, pb, err, st, poff, plen);

        if st[1] != 0 {
            return -1;
        }

        if fmt != 0 {
            return build_string(
                ji,
                poff,
                plen,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }
        let id = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[id] = K_ANY;
        return id;
    }

    if s_eq(strs, sdir, tsid, "object") {
        if !hasp && jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "allOf") {
            let aj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "allOf");
            let path_o, path_l = p_lit(pb, st, poff, plen, "/allOf");
            return build_alts(
                aj,
                path_o,
                path_l,
                K_ALL_OF,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }
        return build_object(
            ji,
            poff,
            plen,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);
    }

    if s_eq(strs, sdir, tsid, "string") {
        if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "allOf") {
            let aj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "allOf");
            let path_o, path_l = p_lit(pb, st, poff, plen, "/allOf");
            return build_alts(
                aj,
                path_o,
                path_l,
                K_ALL_OF,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);
        }
        return build_string(
            ji,
            poff,
            plen,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);
    }

    if s_eq(strs, sdir, tsid, "array") {
        return build_array(
            ji,
            poff,
            plen,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);
    }

    if s_eq(strs, sdir, tsid, "integer") {
        return build_integer(
            ji,
            poff,
            plen,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);
    }
    let id = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);

    if s_eq(strs, sdir, tsid, "number") {
        nkind[id] = K_NUMBER;
        return id;
    }

    if s_eq(strs, sdir, tsid, "boolean") {
        nkind[id] = K_BOOLEAN;
        return id;
    }

    if s_eq(strs, sdir, tsid, "null") {
        nkind[id] = K_NULL;
        return id;
    }
    var m: u8[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var q = app(m, 0, "unrecognized type ", 0, 18);
    let tn = s_at(strs, sdir, tsid);
    q = app(m, q, tn, 0, tn.len);
    return fail(pb, err, st, poff, plen, m[0..q]);
}
recursive fn build_alts(
    aj: i64,
    poff: i64,
    plen: i64,
    kind: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    if jk[aj] != J_ARR {
        let m: const u8[:] = "must be an array of schemas";
        return fail(pb, err, st, poff, plen, m);
    }
    let n = jn[aj];

    if n == 0 {
        let m: const u8[:] = "must not be empty";
        return fail(pb, err, st, poff, plen, m);
    }
    let id = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);
    nkind[id] = kind;
    var tmp: i64[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var i: i64 = 0;

    while i < n {
        let ch = jchild[jf[aj] + i];
        let path_o, path_l = p_dec(pb, st, poff, plen, "/", i);
        let alt = build_node(
            ch,
            path_o,
            path_l,
            -1,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);

        if alt < 0 {
            return -1;
        }
        tmp[i] = alt;
        i = i + 1;
    }
    ncf[id] = st[9];
    var z: i64 = 0;

    while z < n {
        nchild[st[9]] = tmp[z];
        st[9] = st[9] + 1;
        z = z + 1;
    }
    ncc[id] = n;
    return id;
}
recursive fn build_object(
    ji: i64,
    poff: i64,
    plen: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    let id = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);
    nkind[id] = K_OBJECT;
    let reqj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "required");
    var req_base: i64 = 0;
    var req_n: i64 = 0;

    if reqj >= 0 && jk[reqj] == J_ARR {
        req_base = jf[reqj];
        req_n = jn[reqj];
    }
    let propj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "properties");

    if propj >= 0 {
        if jk[propj] != J_OBJ {
            let m: const u8[:] = "properties must be an object";
            return fail(pb, err, st, poff, plen, m);
        }
        let base = jf[propj];
        let n = jn[propj];
        var tname: i64[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var tnode: i64[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var treq: i64[256] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
        var i: i64 = 0;

        while i < n {
            let nameid = jkey[base + i];
            let child = jchild[base + i];
            let nm = s_at(strs, sdir, nameid);
            let path_o, path_l = p_str(pb, st, poff, plen, "/properties/", nm);
            let pn = build_node(
                child,
                path_o,
                path_l,
                -1,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);

            if pn < 0 {
                return -1;
            }
            var isreq: i64 = 0;
            var r: i64 = 0;

            while r < req_n {
                let rn = jchild[req_base + r];

                if jk[rn] == J_STR {
                    if s_eq(strs, sdir, jv[rn], nm) {
                        isreq = 1;
                        r = req_n;
                    } else {
                        r = r + 1;
                    }
                } else {
                    r = r + 1;
                }
            }
            tname[i] = nameid;
            tnode[i] = pn;
            treq[i] = isreq;
            i = i + 1;
        }
        npf[id] = st[11];
        var z: i64 = 0;

        while z < n {
            pname[st[11]] = tname[z];
            pnode[st[11]] = tnode[z];
            preq[st[11]] = treq[z];
            st[11] = st[11] + 1;
            z = z + 1;
        }
        npc[id] = n;
    }
    let ap = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "additionalProperties");

    if ap >= 0 {
        if jk[ap] == J_BOOL {
            if jv[ap] != 0 {
                let anyid = nnew(
                    nkind,
                    ncf,
                    ncc,
                    npf,
                    npc,
                    nadd,
                    nstr,
                    nfmt,
                    nminl,
                    nmaxl,
                    nmin,
                    nmax,
                    nitems,
                    nconst,
                    nef,
                    nec,
                    ntarget,
                    st);
                nkind[anyid] = K_ANY;
                nadd[id] = anyid;
            }
        } else

        if jk[ap] == J_OBJ {
            let path_o, path_l = p_lit(pb, st, poff, plen, "/additionalProperties");
            let an = build_node(
                ap,
                path_o,
                path_l,
                -1,
                jk,
                jv,
                jd,
                jf,
                jn,
                jkey,
                jchild,
                strs,
                sdir,
                nsid,
                nkind,
                ncf,
                ncc,
                nchild,
                npf,
                npc,
                pname,
                pnode,
                preq,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                eval,
                ntarget,
                rk,
                rv,
                pending,
                pb,
                mb,
                err,
                st);

            if an < 0 {
                return -1;
            }
            nadd[id] = an;
        } else {
            let m: const u8[:] = "additionalProperties must be a boolean or a schema";
            return fail(pb, err, st, poff, plen, m);
        }
    } else

    if !jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "properties") {
        let anyid = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[anyid] = K_ANY;
        nadd[id] = anyid;
    }
    return id;
}
recursive fn build_array(
    ji: i64,
    poff: i64,
    plen: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    let has_items = jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "items");

    if has_items || jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "prefixItems") {
        let key: const u8[:] = if has_items {
            "items"
        } else {
            "prefixItems"
        }
        ;
        let keypath: const u8[:] = if has_items {
            "/items"
        } else {
            "/prefixItems"
        }
        ;
        let keypath_slash: const u8[:] = if has_items {
            "/items/"
        } else {
            "/prefixItems/"
        }
        ;
        let ij = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, key);

        if jk[ij] == J_ARR {
            let tid = nnew(
                nkind,
                ncf,
                ncc,
                npf,
                npc,
                nadd,
                nstr,
                nfmt,
                nminl,
                nmaxl,
                nmin,
                nmax,
                nitems,
                nconst,
                nef,
                nec,
                ntarget,
                st);
            nkind[tid] = K_TUPLE;
            ncf[tid] = st[9];
            let n = jn[ij];
            var i: i64 = 0;

            while i < n {
                let c = jchild[jf[ij] + i];
                let path_o, path_l = p_dec(pb, st, poff, plen, keypath_slash, i);
                let it = build_node(
                    c,
                    path_o,
                    path_l,
                    -1,
                    jk,
                    jv,
                    jd,
                    jf,
                    jn,
                    jkey,
                    jchild,
                    strs,
                    sdir,
                    nsid,
                    nkind,
                    ncf,
                    ncc,
                    nchild,
                    npf,
                    npc,
                    pname,
                    pnode,
                    preq,
                    nadd,
                    nstr,
                    nfmt,
                    nminl,
                    nmaxl,
                    nmin,
                    nmax,
                    nitems,
                    nconst,
                    nef,
                    nec,
                    eval,
                    ntarget,
                    rk,
                    rv,
                    pending,
                    pb,
                    mb,
                    err,
                    st);

                if it < 0 {
                    return -1;
                }
                nchild[st[9]] = it;
                st[9] = st[9] + 1;
                i = i + 1;
            }
            ncc[tid] = st[9] - ncf[tid];
            return tid;
        }
        let id = nnew(
            nkind,
            ncf,
            ncc,
            npf,
            npc,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            ntarget,
            st);
        nkind[id] = K_ARRAY;
        let path_o, path_l = p_lit(pb, st, poff, plen, keypath);
        let it = build_node(
            ij,
            path_o,
            path_l,
            -1,
            jk,
            jv,
            jd,
            jf,
            jn,
            jkey,
            jchild,
            strs,
            sdir,
            nsid,
            nkind,
            ncf,
            ncc,
            nchild,
            npf,
            npc,
            pname,
            pnode,
            preq,
            nadd,
            nstr,
            nfmt,
            nminl,
            nmaxl,
            nmin,
            nmax,
            nitems,
            nconst,
            nef,
            nec,
            eval,
            ntarget,
            rk,
            rv,
            pending,
            pb,
            mb,
            err,
            st);

        if it < 0 {
            return -1;
        }
        nitems[id] = it;
        nminl[id] = get_count(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, "minItems", pb, mb, err, st, poff, plen, 0);

        if st[1] != 0 {
            return -1;
        }
        nmaxl[id] = get_count(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, "maxItems", pb, mb, err, st, poff, plen, -1);

        if st[1] != 0 {
            return -1;
        }
        return id;
    }
    let id = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);
    nkind[id] = K_ARRAY;
    let anyid = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);
    nkind[anyid] = K_ANY;
    nitems[id] = anyid;
    nminl[id] = get_count(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, "minItems", pb, mb, err, st, poff, plen, 0);

    if st[1] != 0 {
        return -1;
    }
    nmaxl[id] = get_count(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, "maxItems", pb, mb, err, st, poff, plen, -1);

    if st[1] != 0 {
        return -1;
    }
    return id;
}
recursive fn build_string(
    ji: i64,
    poff: i64,
    plen: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    let id = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);
    nkind[id] = K_STRING;
    let pj = jf_find(jk, jf, jn, jkey, jchild, strs, sdir, ji, "pattern");

    if pj >= 0 {
        if jk[pj] != J_STR {
            let m: const u8[:] = "pattern must be a string";
            return fail(pb, err, st, poff, plen, m);
        }

        if sdir[2 * jv[pj] + 1] == 0 {
            nstr[id] = -1;
        } else {
            nstr[id] = jv[pj];
        }
    }
    let fmt = get_format(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, pb, err, st, poff, plen);

    if st[1] != 0 {
        return -1;
    }
    nfmt[id] = fmt;
    nminl[id] = get_count(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, "minLength", pb, mb, err, st, poff, plen, 0);

    if st[1] != 0 {
        return -1;
    }
    nmaxl[id] = get_count(jk, jv, jf, jn, jkey, jchild, strs, sdir, ji, "maxLength", pb, mb, err, st, poff, plen, -1);

    if st[1] != 0 {
        return -1;
    }
    return id;
}
recursive fn build_integer(
    ji: i64,
    poff: i64,
    plen: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nsid: i64,
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    let id = nnew(
        nkind,
        ncf,
        ncc,
        npf,
        npc,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        ntarget,
        st);
    nkind[id] = K_INTEGER;

    if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "minimum") {
        nmin[id] = get_bound(jk, jv, jd, jf, jn, jkey, jchild, strs, sdir, ji, "minimum", 1, pb, mb, err, st, poff, plen);

        if st[1] != 0 {
            return -1;
        }
    } else

    if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "exclusiveMinimum") {
        nmin[id] = get_bound(jk, jv, jd, jf, jn, jkey, jchild, strs, sdir, ji, "exclusiveMinimum", 0, pb, mb, err, st, poff, plen) + 1;

        if st[1] != 0 {
            return -1;
        }
    }

    if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "maximum") {
        nmax[id] = get_bound(jk, jv, jd, jf, jn, jkey, jchild, strs, sdir, ji, "maximum", 0, pb, mb, err, st, poff, plen);

        if st[1] != 0 {
            return -1;
        }
    } else

    if jhas(jk, jf, jn, jkey, jchild, strs, sdir, ji, "exclusiveMaximum") {
        nmax[id] = get_bound(jk, jv, jd, jf, jn, jkey, jchild, strs, sdir, ji, "exclusiveMaximum", 1, pb, mb, err, st, poff, plen) - 1;

        if st[1] != 0 {
            return -1;
        }
    }
    return id;
}
// Document entry: build root, then point every pending ref at its map target.

export fn schema_build(
    root: i64,
    ndocsid: i64,
    jk: const i64[:],
    jv: const i64[:],
    jd: const f64[:],
    jf: const i64[:],
    jn: const i64[:],
    jkey: const i64[:],
    jchild: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    nkind: i64[:],
    ncf: i64[:],
    ncc: i64[:],
    nchild: i64[:],
    npf: i64[:],
    npc: i64[:],
    pname: i64[:],
    pnode: i64[:],
    preq: i64[:],
    nadd: i64[:],
    nstr: i64[:],
    nfmt: i64[:],
    nminl: i64[:],
    nmaxl: i64[:],
    nmin: i64[:],
    nmax: i64[:],
    nitems: i64[:],
    nconst: i64[:],
    nef: i64[:],
    nec: i64[:],
    eval: i64[:],
    ntarget: i64[:],
    rk: i64[:],
    rv: i64[:],
    pending: i64[:],
    pb: u8[:],
    mb: u8[:],
    err: u8[:],
    st: i64[:]) -> i64 {
    st[0] = 0;
    st[1] = 0;
    st[2] = 0;
    st[3] = 0;
    st[4] = 0;
    st[5] = 0;
    st[6] = 0;
    st[8] = root;
    st[9] = 0;
    st[10] = 0;
    st[11] = 0;
    let ro_o, ro_l = p_lit(pb, st, 0, 0, "#");
    let r = build_node(
        root,
        ro_o,
        ro_l,
        -1,
        jk,
        jv,
        jd,
        jf,
        jn,
        jkey,
        jchild,
        strs,
        sdir,
        ndocsid,
        nkind,
        ncf,
        ncc,
        nchild,
        npf,
        npc,
        pname,
        pnode,
        preq,
        nadd,
        nstr,
        nfmt,
        nminl,
        nmaxl,
        nmin,
        nmax,
        nitems,
        nconst,
        nef,
        nec,
        eval,
        ntarget,
        rk,
        rv,
        pending,
        pb,
        mb,
        err,
        st);

    if r < 0 {
        return -1;
    }
    var i: i64 = 0;

    while i < st[3] {
        let nid = pending[i];
        let sid = nstr[nid];
        let fi = refs_find(rk, st[2], sid);

        if fi >= 0 {
            ntarget[nid] = rv[fi];
        }
        i = i + 1;
    }
    return r;
}
// ---- converter output ------------------------------------------------------
// format_grammar: every rule as "name ::= text\n", in std::map (byte-wise) key
// order.

export fn grammar_text(
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    n: i64,
    buf: u8[:],
    out: u8[:]) -> i64 {
    // insertion sort the entry indices by key bytes
    var idx: i64[4096] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var z: i64 = 0;

    while z < n {
        idx[z] = z;
        z = z + 1;
    }
    var i: i64 = 1;

    while i < n {
        let cur = idx[i];
        var j: i64 = i;
        var stop: bool = false;

        while j > 0 && !stop {
            let prev = idx[j - 1];
            let pa = koff[prev];
            let la = klen[prev];
            let pb2 = koff[cur];
            let lb = klen[cur];
            // less(cur, prev) ?
            var less: bool = false;
            var decided: bool = false;
            var k: i64 = 0;

            while k < la && k < lb && !decided {
                if buf[pb2 + k] < buf[pa + k] {
                    less = true;
                    decided = true;
                } else

                if buf[pb2 + k] > buf[pa + k] {
                    decided = true;
                } else {
                    k = k + 1;
                }
            }

            if !decided && lb < la {
                less = true;
            }

            if less {
                idx[j] = prev;
                j = j - 1;
            } else {
                stop = true;
            }
        }
        idx[j] = cur;
        i = i + 1;
    }
    var p: i64 = 0;
    z = 0;

    while z < n {
        let e = idx[z];
        p = app(out, p, buf, koff[e], klen[e]);
        p = app(out, p, " ::= ", 0, 5);
        p = app(out, p, buf, toff[e], tlen[e]);
        p = appc(out, p, (10 as u8));
        z = z + 1;
    }
    return p;
}
// check_errors: the thrown text for a conversion that recorded errors

export fn conversion_failed(err: const u8[:], errlen: i64, out: u8[:]) -> i64 {
    var p = app(out, 0, "JSON schema conversion failed:\n", 0, 31);
    p = app(out, p, err, 0, errlen);
    return p;
}
// check_errors: the warning line for a conversion that only warned

export fn conversion_warning(buf: const u8[:], off: const i64[:], len: const i64[:], n: i64, out: u8[:]) -> i64 {
    var p = app(out, 0, "WARNING: JSON schema conversion was incomplete: ", 0, 48);
    var i: i64 = 0;

    while i < n {
        if i > 0 {
            p = app(out, p, "; ", 0, 2);
        }
        p = app(out, p, buf, off[i], len[i]);
        i = i + 1;
    }
    p = appc(out, p, (10 as u8));
    return p;
}


// ---- json-schema-to-grammar converter block (FG573) ------------------------
// Appended to the canonical format-grammar.goose. Helpers that already exist in
// the canon are reused (app/appc/appd/eqb/s_at, grammar_text); this block adds
// only the converter, its private helpers and one export.
// Kind constants are FG573_* so they cannot collide with the canon K_* schema
// tree kinds; the caller maps its tree into the documented meta/props/vals
// contract of this converter.
// grammar_text's slice params are mutable only because Goose cannot specialize
// an export fn that mixes const and mutable slice parameters.

const FG573_K_ANY: i64 = 0;
const FG573_K_REF: i64 = 1;
const FG573_K_ANY_OF: i64 = 2;
const FG573_K_ALLOF: i64 = 3;
const FG573_K_CONST: i64 = 4;
const FG573_K_ENUM: i64 = 5;
const FG573_K_NULL: i64 = 6;
const FG573_K_BOOLEAN: i64 = 7;
const FG573_K_NUMBER: i64 = 8;
const FG573_K_INTEGER: i64 = 9;
const FG573_K_STRING: i64 = 10;
const FG573_K_ARRAY: i64 = 11;
const FG573_K_OBJECT: i64 = 13;
const FG573_K_EMPTY: i64 = 14;
const FG573_K_STRING_LEN: i64 = 15;
const FG573_K_INTEGER_BOUNDS: i64 = 16;
const FG573_K_TYPE_ARRAY: i64 = 17;
const FG573_K_PATTERN: i64 = 18;
const FG573_K_TUPLE: i64 = 19;
const FG573_K_FORMAT: i64 = 20;
const MS: i64 = 11;
const PS: i64 = 4;
const INT_MAX: i64 = 2147483647;

const P_BOOLEAN: i64 = 0;
const P_DECIMAL: i64 = 1;
const P_INTEGRAL: i64 = 2;
const P_NUMBER: i64 = 3;
const P_INTEGER: i64 = 4;
const P_VALUE: i64 = 5;
const P_OBJECT: i64 = 6;
const P_ARRAY: i64 = 7;
const P_CHAR: i64 = 8;
const P_STRING: i64 = 9;
const P_NULL: i64 = 10;
const P_UUID: i64 = 11;
const P_DATE: i64 = 12;
const P_TIME: i64 = 13;
const P_DATETIME: i64 = 14;
const P_DATE_STRING: i64 = 15;
const P_TIME_STRING: i64 = 16;
const P_DATETIME_STRING: i64 = 17;

// state: 0 entries, 1 bump offset, 2 error count, 3 first error off, 4 len
const ST_N: i64 = 0;
const ST_B: i64 = 1;
const ST_E: i64 = 2;
const ST_EO: i64 = 3;
const ST_EL: i64 = 4;
const ST_A: i64 = 5;      // bounds decimal arena cursor in sbuf
const ST_POS: i64 = 6;    // pattern cursor
const ST_PCUR: i64 = 7;   // pattern item-region cursor
const ST_PSTAT: i64 = 8;  // pattern status: 0 ok, 1 unsupported, 2 invalid
const ST_SUB: i64 = 9;    // pattern sub-rule count
const ST_R: i64 = 10;     // $ref resolving stack size
const ST_RN: i64 = 11;    // $ref resolving stack index
const ST_MC: i64 = 12;    // allOf merged property count
const ST_RSN: i64 = 13;   // pattern reason code

// Same name -> same bytes comparison over the rule map buffer.
fn slice_eq(buf: const u8[:], ao: i64, al: i64, bo: i64, bl: i64) -> bool {
    if al != bl {
        return false;
    }
    var i: i64 = 0;

    while i < al {
        if buf[ao + i] != buf[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn find_entry(koff: const i64[:], klen: const i64[:], buf: const u8[:], key: const u8[:], n: i64) -> i64 {
    var i: i64 = 0;

    while i < n {
        if klen[i] == key.len {
            var j: i64 = 0;
            var same: bool = true;

            while j < key.len && same {
                if buf[koff[i] + j] != key[j] {
                    same = false;
                }
                j = j + 1;
            }

            if same {
                return i;
            }
        }
        i = i + 1;
    }
    return -1;
}

// _add_rule: sanitize the key, reuse an identical text, otherwise suffix.
fn add_rule(
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    name: const u8[:],
    text_off: i64,
    text_len: i64) -> i64, i64 {
    let base = st[ST_B];
    var esc_len: i64 = sanitize_into(name, buf, base);
    st[ST_B] = base + esc_len;
    let esc = buf[base .. base + esc_len];
    let found = find_entry(koff, klen, buf, esc, st[ST_N]);

    if found >= 0 && slice_eq(buf, toff[found], tlen[found], text_off, text_len) {
        return koff[found], klen[found];
    }
    var cand_at = base;
    var cand_len = esc_len;

    if found >= 0 {
        var i: i64 = 0;
        var placed: bool = false;

        while !placed {
            let at = st[ST_B];
            var p = app(buf, at, esc, 0, esc.len);
            p = appd(buf, p, i);
            st[ST_B] = p;
            let cand = buf[at .. p];
            let f = find_entry(koff, klen, buf, cand, st[ST_N]);

            if f < 0 {
                cand_at = at;
                cand_len = p - at;
                placed = true;
            } else {
                if slice_eq(buf, toff[f], tlen[f], text_off, text_len) {
                    return koff[f], klen[f];
                }
                i = i + 1;
            }
        }
    }
    let idx = st[ST_N];
    koff[idx] = cand_at;
    klen[idx] = cand_len;
    toff[idx] = text_off;
    tlen[idx] = text_len;
    st[ST_N] = idx + 1;
    return cand_at, cand_len;
}

// _add_rule key sanitation: every maximal run outside [a-zA-Z0-9-] -> '-'.
fn sanitize_into(s: const u8[:], dst: u8[:], at: i64) -> i64 {
    var i: i64 = 0;
    var n: i64 = 0;
    var run: bool = false;

    while i < s.len {
        let b = s[i];
        var ok: bool = false;

        if b >= 'a' && b <= 'z' {
            ok = true;
        }

        if b >= 'A' && b <= 'Z' {
            ok = true;
        }

        if b >= '0' && b <= '9' {
            ok = true;
        }

        if b == '-' {
            ok = true;
        }

        if ok {
            if at + n < dst.len {
                dst[at + n] = b;
            }
            n = n + 1;
            run = false;
        } else {
            if !run {
                if at + n < dst.len {
                    dst[at + n] = '-';
                }
                n = n + 1;
                run = true;
            }
        }
        i = i + 1;
    }
    return n;
}

fn is_reserved(name: const u8[:]) -> bool {
    let names: const u8[:][> ..] = [
        "root",
        "boolean",
        "decimal-part",
        "integral-part",
        "number",
        "integer",
        "value",
        "object",
        "array",
        "uuid",
        "char",
        "string",
        "null",
        "date",
        "time",
        "date-time",
        "date-string",
        "time-string",
        "date-time-string",
    ];
    for & n in names {
        if eqb(name, n) {
            return true;
        }
    }
    return false;
}

// visit's rule_name: reserved -> name + '-', empty -> root, else name.
fn make_rule_name(name: const u8[:], buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p: i64 = at;

    if name.len == 0 {
        p = app(buf, p, "root", 0, 4);
    } else {
        p = app(buf, p, name, 0, name.len);

        if is_reserved(name) {
            p = appc(buf, p, '-');
        }
    }
    st[ST_B] = p;
    return at, p - at;
}

// visit's sub_name: name + '-' when the name is non-empty.
fn make_sub_name(name: const u8[:], buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = app(buf, at, name, 0, name.len);

    if name.len != 0 {
        p = appc(buf, p, '-');
    }
    st[ST_B] = p;
    return at, p - at;
}

// name + ('-' when non-empty) + suffix, used for item and -rest names.
fn join_name(name: const u8[:], suffix: const u8[:], buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = app(buf, at, name, 0, name.len);

    if name.len != 0 {
        p = appc(buf, p, '-');
    }
    p = app(buf, p, suffix, 0, suffix.len);
    st[ST_B] = p;
    return at, p - at;
}

fn tuple_name(name: const u8[:], i: i64, buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = app(buf, at, name, 0, name.len);

    if name.len != 0 {
        p = appc(buf, p, '-');
    }
    p = app(buf, p, "tuple-", 0, 6);
    p = appd(buf, p, i);
    st[ST_B] = p;
    return at, p - at;
}

fn is_root_name(n: const u8[:]) -> bool {
    return n.len == 4 && n[0] == 'r' && n[1] == 'o' && n[2] == 'o' && n[3] == 't';
}

fn is_uuid_format(fmt: const u8[:]) -> bool {
    if eqb(fmt, "uuid") {
        return true;
    }

    if fmt.len == 5 && fmt[0] == 'u' && fmt[1] == 'u' && fmt[2] == 'i' && fmt[3] == 'd' {
        return fmt[4] >= '1' && fmt[4] <= '5';
    }
    return false;
}

// format_literal: CR, LF, '"' and '\' only.
fn format_literal(dump: const u8[:], buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = appc(buf, at, '"');
    var i: i64 = 0;

    while i < dump.len {
        let c = dump[i];

        if c == (13 as u8) {
            p = appc(buf, p, '\\');
            p = appc(buf, p, 'r');
        } else

        if c == (10 as u8) {
            p = appc(buf, p, '\\');
            p = appc(buf, p, 'n');
        } else

        if c == '"' {
            p = appc(buf, p, '\\');
            p = appc(buf, p, '"');
        } else

        if c == '\\' {
            p = appc(buf, p, '\\');
            p = appc(buf, p, '\\');
        } else {
            p = appc(buf, p, c);
        }
        i = i + 1;
    }
    p = appc(buf, p, '"');
    st[ST_B] = p;
    return at, p - at;
}

// The native PRIMITIVE_RULES contents. Kept as native data, as the other
// dispatch slices do; only the graph and the root override live in code.
fn primitive_name(which: i64) -> const u8[:] {
    if which == P_BOOLEAN {
        return "boolean";
    }

    if which == P_DECIMAL {
        return "decimal-part";
    }

    if which == P_INTEGRAL {
        return "integral-part";
    }

    if which == P_NUMBER {
        return "number";
    }

    if which == P_INTEGER {
        return "integer";
    }

    if which == P_VALUE {
        return "value";
    }

    if which == P_OBJECT {
        return "object";
    }

    if which == P_ARRAY {
        return "array";
    }

    if which == P_CHAR {
        return "char";
    }

    if which == P_STRING {
        return "string";
    }

    if which == P_UUID {
        return "uuid";
    }

    if which == P_DATE {
        return "date";
    }

    if which == P_TIME {
        return "time";
    }

    if which == P_DATETIME {
        return "date-time";
    }

    if which == P_DATE_STRING {
        return "date-string";
    }

    if which == P_TIME_STRING {
        return "time-string";
    }

    if which == P_DATETIME_STRING {
        return "date-time-string";
    }
    return "null";
}

fn primitive_text(which: i64) -> const u8[:] {
    if which == P_BOOLEAN {
        return "(\"true\" | \"false\")";
    }

    if which == P_DECIMAL {
        return "[0-9]{1,16}";
    }

    if which == P_INTEGRAL {
        return "[0] | [1-9] [0-9]{0,15}";
    }

    if which == P_NUMBER {
        return "(\"-\"? integral-part) (\".\" decimal-part)? ([eE] [-+]? integral-part)?";
    }

    if which == P_INTEGER {
        return "(\"-\"? integral-part)";
    }

    if which == P_VALUE {
        return "object | array | string | number | boolean | null";
    }

    if which == P_OBJECT {
        return "\"{\" space ( string \":\" space value (\",\" space string \":\" space value)* )? space \"}\"";
    }

    if which == P_ARRAY {
        return "\"[\" space ( value (\",\" space value)* )? space \"]\"";
    }

    if which == P_CHAR {
        return "[^\"\\\\\\x7F\\x00-\\x1F] | [\\\\] ([\"\\\\bfnrt] | \"u\" [0-9a-fA-F]{4})";
    }

    if which == P_STRING {
        return "\"\\\"\" char* \"\\\"\"";
    }

    if which == P_UUID {
        return "\"\\\"\" [0-9a-fA-F]{8} \"-\" [0-9a-fA-F]{4} \"-\" [0-9a-fA-F]{4} \"-\" [0-9a-fA-F]{4} \"-\" [0-9a-fA-F]{12} \"\\\"\"";
    }

    if which == P_DATE {
        return "[0-9]{4} \"-\" ( \"0\" [1-9] | \"1\" [0-2] ) \"-\" ( \"0\" [1-9] | [1-2] [0-9] | \"3\" [0-1] )";
    }

    if which == P_TIME {
        return "([01] [0-9] | \"2\" [0-3]) \":\" [0-5] [0-9] \":\" [0-5] [0-9] ( \".\" [0-9]{3} )? ( \"Z\" | ( \"+\" | \"-\" ) ( [01] [0-9] | \"2\" [0-3] ) \":\" [0-5] [0-9] )";
    }

    if which == P_DATETIME {
        return "date \"T\" time";
    }

    if which == P_DATE_STRING {
        return "\"\\\"\" date \"\\\"\"";
    }

    if which == P_TIME_STRING {
        return "\"\\\"\" time \"\\\"\"";
    }

    if which == P_DATETIME_STRING {
        return "\"\\\"\" date-time \"\\\"\"";
    }
    return "\"null\"";
}

fn primitive_dep_count(which: i64) -> i64 {
    if which == P_NUMBER || which == P_OBJECT || which == P_DATETIME {
        return 2;
    }

    if which == P_INTEGER || which == P_ARRAY || which == P_STRING ||
        which == P_DATE_STRING || which == P_TIME_STRING || which == P_DATETIME_STRING {
        return 1;
    }

    if which == P_VALUE {
        return 6;
    }
    return 0;
}

fn primitive_dep(which: i64, i: i64) -> i64 {
    if which == P_NUMBER {
        if i == 0 {
            return P_INTEGRAL;
        }
        return P_DECIMAL;
    }

    if which == P_INTEGER {
        return P_INTEGRAL;
    }

    if which == P_OBJECT {
        if i == 0 {
            return P_STRING;
        }
        return P_VALUE;
    }

    if which == P_ARRAY || which == P_STRING {
        if which == P_ARRAY {
            return P_VALUE;
        }
        return P_CHAR;
    }

    if which == P_DATETIME {
        if i == 0 {
            return P_DATE;
        }
        return P_TIME;
    }

    if which == P_DATE_STRING {
        return P_DATE;
    }

    if which == P_TIME_STRING {
        return P_TIME;
    }

    if which == P_DATETIME_STRING {
        return P_DATETIME;
    }

    if which == P_VALUE {
        if i == 0 {
            return P_OBJECT;
        }

        if i == 1 {
            return P_ARRAY;
        }

        if i == 2 {
            return P_STRING;
        }

        if i == 3 {
            return P_NUMBER;
        }

        if i == 4 {
            return P_BOOLEAN;
        }
        return P_NULL;
    }
    return P_NULL;
}

// _add_primitive: the rule, then its dependencies, each only when absent.
// `override` is empty for a dependency and holds the visit rule name otherwise.
recursive fn ensure_primitive(
    which: i64,
    override: const u8[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:]) -> i64, i64 {
    var key: const u8[:] = primitive_name(which);

    if override.len != 0 {
        key = override;
    }
    let text = primitive_text(which);
    let at = st[ST_B];
    let end = app(buf, at, text, 0, text.len);
    let tl = end - at;
    st[ST_B] = end;
    let ko, kl = add_rule(koff, klen, toff, tlen, buf, st, key, at, tl);
    let nd = primitive_dep_count(which);
    var i: i64 = 0;

    while i < nd {
        let dep = primitive_dep(which, i);
        let dn = primitive_name(dep);
        let exists = find_entry(koff, klen, buf, dn, st[ST_N]);

        if exists < 0 {
            let empty: const u8[:] = "";
            let d_off, d_len = ensure_primitive(dep, empty, koff, klen, toff, tlen, buf, st);
        }
        i = i + 1;
    }
    return ko, kl;
}

// build_repetition, including the separator form and its recursion.
recursive fn rep_impl(
    item: const u8[:],
    sep: const u8[:],
    mn: i64,
    mx: i64,
    buf: u8[:],
    st: i64[:]) -> i64, i64 {
    let has_max = mx != INT_MAX;

    if mx == 0 {
        return st[ST_B], 0;
    }

    if mn == 0 && mx == 1 {
        let at = st[ST_B];
        var p = app(buf, at, item, 0, item.len);
        p = appc(buf, p, '?');
        st[ST_B] = p;
        return at, p - at;
    }

    if sep.len == 0 {
        let at = st[ST_B];
        var p = app(buf, at, item, 0, item.len);

        if mn == 1 && !has_max {
            p = appc(buf, p, '+');
        } else

        if mn == 0 && !has_max {
            p = appc(buf, p, '*');
        } else {
            p = appc(buf, p, '{');
            p = appd(buf, p, mn);
            p = appc(buf, p, ',');
            if has_max {
                p = appd(buf, p, mx);
            }
            p = appc(buf, p, '}');
        }
        st[ST_B] = p;
        return at, p - at;
    }
    // inner item: "(" + separator + " " + item + ")"
    let inner_at = st[ST_B];
    var ip = appc(buf, inner_at, '(');
    ip = app(buf, ip, sep, 0, sep.len);
    ip = appc(buf, ip, ' ');
    ip = app(buf, ip, item, 0, item.len);
    ip = appc(buf, ip, ')');
    st[ST_B] = ip;
    let inner = buf[inner_at .. ip];
    var imn: i64 = mn;

    if mn != 0 {
        imn = mn - 1;
    }
    var imx: i64 = mx;

    if has_max {
        imx = mx - 1;
    }
    let empty: const u8[:] = "";
    let ro, rl = rep_impl(inner, empty, imn, imx, buf, st);
    let at = st[ST_B];
    var p = app(buf, at, item, 0, item.len);
    p = appc(buf, p, ' ');
    p = app(buf, p, buf, ro, rl);
    st[ST_B] = p;

    if mn == 0 {
        let wrap_at = st[ST_B];
        var w = appc(buf, wrap_at, '(');
        w = app(buf, w, buf, at, p - at);
        w = appc(buf, w, ')');
        w = appc(buf, w, '?');
        st[ST_B] = w;
        return wrap_at, w - wrap_at;
    }
    return at, p - at;
}

// Object optional chain: the native get_recursive_refs, built backwards.
recursive fn opt_chain(
    opt: const i64[:],
    from: i64,
    n: i64,
    first_optional: i64,
    star: i64,
    name: const u8[:],
    props: const i64[:],
    kvoff: const i64[:],
    kvlen: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:]) -> i64, i64 {
    let k = opt[from];
    let kv = buf[kvoff[k] .. kvoff[k] + kvlen[k]];
    // allocate the recursive rest first, so the final text sits after it
    var have_rest: bool = false;
    var rest_off: i64 = 0;
    var rest_len: i64 = 0;

    if from + 1 < n && k != star {
        let sid = props[4 * k + 1];
        let pname = s_at(strs, sdir, sid);
        let rn_off, rn_len = join_name(name, pname, buf, st);
        let rest_at = st[ST_B];
        let suffix: const u8[:] = "-rest";
        var rp = app(buf, rest_at, buf, rn_off, rn_len);
        rp = app(buf, rp, suffix, 0, suffix.len);
        st[ST_B] = rp;
        let rest_name = buf[rest_at .. rp];
        let ro, rl = opt_chain(opt, from + 1, n, 1, star, name, props, kvoff, kvlen, strs, sdir, koff, klen, toff, tlen, buf, st);
        let ko, kl = add_rule(koff, klen, toff, tlen, buf, st, rest_name, ro, rl);
        rest_off = ko;
        rest_len = kl;
        have_rest = true;
    }
    let at = st[ST_B];
    var p: i64 = at;

    if k == star {
        // native "*": kv ( "," space kv )*, or ( "," space kv )* when it is
        // the first optional branch
        if first_optional != 0 {
            p = app(buf, p, "( \",\" space ", 0, 12);
            p = app(buf, p, kv, 0, kv.len);
            p = app(buf, p, " )*", 0, 3);
        } else {
            p = app(buf, p, kv, 0, kv.len);
            p = app(buf, p, " ( \",\" space ", 0, 13);
            p = app(buf, p, kv, 0, kv.len);
            p = app(buf, p, " )*", 0, 3);
        }
    } else if first_optional != 0 {
        p = app(buf, p, "( \",\" space ", 0, 12);
        p = app(buf, p, kv, 0, kv.len);
        p = app(buf, p, " )?", 0, 3);
    } else {
        p = app(buf, p, kv, 0, kv.len);
    }

    if have_rest {
        p = appc(buf, p, ' ');
        p = app(buf, p, buf, rest_off, rest_len);
    }
    st[ST_B] = p;
    return at, p - at;
}

// ---- _not_strings: a JSON string that is none of the property names --------

fn ns_prefix_ok(
    props: const i64[:],
    pf: i64,
    pc: i64,
    strs: const u8[:],
    sdir: const i64[:],
    sbuf: const u8[:],
    pre_len: i64,
    j: i64) -> bool {
    let sid = props[(pf + j) * PS + 1];
    let name = s_at(strs, sdir, sid);

    if name.len < pre_len {
        return false;
    }
    var i: i64 = 0;

    while i < pre_len {
        if name[i] != sbuf[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn ns_char_exists(
    props: const i64[:],
    pf: i64,
    pc: i64,
    strs: const u8[:],
    sdir: const i64[:],
    sbuf: const u8[:],
    pre_len: i64,
    c: u8) -> bool {
    var j: i64 = 0;

    while j < pc {
        if ns_prefix_ok(props, pf, pc, strs, sdir, sbuf, pre_len, j) {
            let name = s_at(strs, sdir, props[(pf + j) * PS + 1]);

            if name.len > pre_len && name[pre_len] == c {
                return true;
            }
        }
        j = j + 1;
    }
    return false;
}

fn ns_has_more(
    props: const i64[:],
    pf: i64,
    pc: i64,
    strs: const u8[:],
    sdir: const i64[:],
    sbuf: const u8[:],
    pre_len: i64,
    c: u8) -> bool {
    var j: i64 = 0;

    while j < pc {
        if ns_prefix_ok(props, pf, pc, strs, sdir, sbuf, pre_len, j) {
            let name = s_at(strs, sdir, props[(pf + j) * PS + 1]);

            if name.len > pre_len + 1 && name[pre_len] == c {
                return true;
            }
        }
        j = j + 1;
    }
    return false;
}

fn ns_is_end(
    props: const i64[:],
    pf: i64,
    pc: i64,
    strs: const u8[:],
    sdir: const i64[:],
    sbuf: const u8[:],
    pre_len: i64) -> bool {
    var j: i64 = 0;

    while j < pc {
        if ns_prefix_ok(props, pf, pc, strs, sdir, sbuf, pre_len, j) {
            let name = s_at(strs, sdir, props[(pf + j) * PS + 1]);

            if name.len == pre_len {
                return true;
            }
        }
        j = j + 1;
    }
    return false;
}

recursive fn ns_node(
    props: const i64[:],
    pf: i64,
    pc: i64,
    strs: const u8[:],
    sdir: const i64[:],
    char_rule: const u8[:],
    buf: u8[:],
    st: i64[:],
    pwork: i64[:],
    sbuf: u8[:],
    pre_len: i64) -> i64, i64 {
    let base = st[ST_PCUR];
    let rb = 1024 + pre_len * 512;
    st[ST_PCUR] = base + 512;
    var nchild: i64 = 0;
    var c: i64 = 0;

    while c < 256 {
        let ch = c as u8;

        if ns_char_exists(props, pf, pc, strs, sdir, sbuf, pre_len, ch) {
            sbuf[pre_len] = ch;
            let more = ns_has_more(props, pf, pc, strs, sdir, sbuf, pre_len, ch);
            let end = ns_is_end(props, pf, pc, strs, sdir, sbuf, pre_len + 1);
            var co: i64 = 0;
            var cl: i64 = 0;

            if more {
                co, cl = ns_node(props, pf, pc, strs, sdir, char_rule, buf, st, pwork, sbuf, pre_len + 1);
            } else if end {
                let at = st[ST_B];
                var p = app(buf, at, char_rule, 0, char_rule.len);
                p = appc(buf, p, '+');
                st[ST_B] = p;
                co = at;
                cl = p - at;
            }
            sbuf[rb + nchild] = ch;

            if more {
                sbuf[rb + 256 + nchild] = 1;
            } else {
                sbuf[rb + 256 + nchild] = 0;
            }
            pwork[base + 2 * nchild] = co;
            pwork[base + 2 * nchild + 1] = cl;
            nchild = nchild + 1;
        }
        c = c + 1;
    }
    let at = st[ST_B];
    var p: i64 = at;
    var i: i64 = 0;

    while i < nchild {
        if i > 0 {
            p = app(buf, p, " | ", 0, 3);
        }
        p = appc(buf, p, '[');
        p = appc(buf, p, sbuf[rb + i]);
        p = appc(buf, p, ']');

        if sbuf[rb + 256 + i] != 0 {
            p = app(buf, p, " (", 0, 2);
            p = app(buf, p, buf, pwork[base + 2 * i], pwork[base + 2 * i + 1]);
            p = appc(buf, p, ')');
        } else {
            p = appc(buf, p, ' ');
            p = app(buf, p, char_rule, 0, char_rule.len);
            p = appc(buf, p, '+');
        }
        i = i + 1;
    }

    if nchild > 0 {
        if i > 0 {
            p = app(buf, p, " | ", 0, 3);
        }
        p = app(buf, p, "[^\"", 0, 3);
        var z: i64 = 0;

        while z < nchild {
            p = appc(buf, p, sbuf[rb + z]);
            z = z + 1;
        }
        p = app(buf, p, "] ", 0, 2);
        p = app(buf, p, char_rule, 0, char_rule.len);
        p = appc(buf, p, '*');
    }
    st[ST_B] = p;
    st[ST_PCUR] = base;
    return at, p - at;
}

fn not_strings(
    props: const i64[:],
    pf: i64,
    pc: i64,
    strs: const u8[:],
    sdir: const i64[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    pwork: i64[:],
    sbuf: u8[:]) -> i64, i64 {
    let ch_off, ch_len = ensure_primitive(P_CHAR, "char", koff, klen, toff, tlen, buf, st);
    let char_rule = buf[ch_off .. ch_off + ch_len];
    let no, nl = ns_node(props, pf, pc, strs, sdir, char_rule, buf, st, pwork, sbuf, 0);
    let at = st[ST_B];
    var p = app(buf, at, "[\"] ( ", 0, 6);
    p = app(buf, p, buf, no, nl);
    p = app(buf, p, " )", 0, 2);

    if !ns_is_end(props, pf, pc, strs, sdir, sbuf, 0) {
        p = appc(buf, p, '?');
    }
    p = app(buf, p, " [\"]", 0, 4);
    st[ST_B] = p;
    return at, p - at;
}


recursive fn object_rule(
    si: i64,
    name: const u8[:],
    meta: const i64[:],
    props: const i64[:],
    vals: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    opt: i64[:],
    kvoff: i64[:],
    kvlen: i64[:],
    choff: i64[:],
    chlen: i64[:],
    sbuf: u8[:],
    pwork: i64[:],
    mprops: i64[:],
    rref: i64[:]) -> i64, i64 {
    return object_from(
        meta[si * MS + 5],
        meta[si * MS + 6],
        meta[si * MS + 10],
        meta[si * MS + 7],
        meta,
        name,
        props,
        vals,
        strs,
        sdir,
        koff,
        klen,
        toff,
        tlen,
        buf,
        st,
        opt,
        kvoff,
        kvlen,
        choff,
        chlen,
        sbuf,
        pwork,
        mprops,
        rref);
}

recursive fn object_from(
    pf: i64,
    pc: i64,
    addk: i64,
    add_node: i64,
    meta: const i64[:],
    name: const u8[:],
    props: const i64[:],
    vals: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    opt: i64[:],
    kvoff: i64[:],
    kvlen: i64[:],
    choff: i64[:],
    chlen: i64[:],
    sbuf: u8[:],
    pwork: i64[:],
    mprops: i64[:],
    rref: i64[:]) -> i64, i64 {
    var j: i64 = 0;

    while j < pc {
        let pp = pf + j;
        let child = props[pp * PS + 0];
        let nsid = props[pp * PS + 1];
        let dsid = props[pp * PS + 2];
        let pname = s_at(strs, sdir, nsid);
        let cn_off, cn_len = join_name(name, pname, buf, st);
        let child_name = buf[cn_off .. cn_off + cn_len];
        let ck_off, ck_len = visit(
            child,
            child_name,
            meta,
            props,
            vals,
            strs,
            sdir,
            koff,
            klen,
            toff,
            tlen,
            buf,
            st,
            opt,
            kvoff,
            kvlen,
            choff,
            chlen,
            sbuf,
            pwork,
            mprops,
            rref);
        let kn_off, kn_len = join_name(name, pname, buf, st);
        let kv_suffix: const u8[:] = "-kv";
        let kvn_at = st[ST_B];
        var kp = app(buf, kvn_at, buf, kn_off, kn_len);
        kp = app(buf, kp, kv_suffix, 0, kv_suffix.len);
        st[ST_B] = kp;
        let kv_name = buf[kvn_at .. kp];
        let dump = s_at(strs, sdir, dsid);
        let lit_off, lit_len = format_literal(dump, buf, st);
        let txt_at = lit_off;
        var tp = lit_off + lit_len;
        let sep: const u8[:] = " space \":\" space ";
        tp = app(buf, tp, sep, 0, sep.len);
        tp = app(buf, tp, buf, ck_off, ck_len);
        st[ST_B] = tp;
        let ko, kl = add_rule(koff, klen, toff, tlen, buf, st, kv_name, txt_at, tp - txt_at);
        kvoff[j] = ko;
        kvlen[j] = kl;
        j = j + 1;
    }
    // additionalProperties true/schema: key rule, value rule, kv rule and the
    // "*" optional branch that native appends after the named optionals.
    var star: i64 = -1;

    if addk == 1 || addk == 3 {
        let sn_off, sn_len = join_name(name, "additional", buf, st);
        let sub_name = buf[sn_off .. sn_off + sn_len];
        var vo: i64 = 0;
        var vl: i64 = 0;

        if addk == 3 {
            let vn_off, vn_len = join_name(sub_name, "value", buf, st);
            let vname = buf[vn_off .. vn_off + vn_len];
            let vk_off, vk_len = visit(
                add_node,
                vname,
                meta,
                props,
                vals,
                strs,
                sdir,
                koff,
                klen,
                toff,
                tlen,
                buf,
                st,
                opt,
                kvoff,
                kvlen,
                choff,
                chlen,
                sbuf,
                pwork,
                mprops,
                rref);
            vo = vk_off;
            vl = vk_len;
        } else {
            let vk_off, vk_len = ensure_primitive(P_VALUE, "value", koff, klen, toff, tlen, buf, st);
            vo = vk_off;
            vl = vk_len;
        }
        var key_off: i64 = 0;
        var key_len: i64 = 0;

        if pc == 0 {
            let kk_off, kk_len = ensure_primitive(P_STRING, "string", koff, klen, toff, tlen, buf, st);
            key_off = kk_off;
            key_len = kk_len;
        } else {
            let kn_off, kn_len = join_name(sub_name, "k", buf, st);
            let key_name = buf[kn_off .. kn_off + kn_len];
            let tx_off, tx_len = not_strings(props, pf, pc, strs, sdir, koff, klen, toff, tlen, buf, st, pwork, sbuf);
            let kk_off, kk_len = add_rule(koff, klen, toff, tlen, buf, st, key_name, tx_off, tx_len);
            key_off = kk_off;
            key_len = kk_len;
        }
        let kvn_off, kvn_len = join_name(sub_name, "kv", buf, st);
        let kv_name = buf[kvn_off .. kvn_off + kvn_len];
        let txt_at = st[ST_B];
        var tp = app(buf, txt_at, buf, key_off, key_len);
        let asep: const u8[:] = " \":\" space ";
        tp = app(buf, tp, asep, 0, asep.len);
        tp = app(buf, tp, buf, vo, vl);
        st[ST_B] = tp;
        let ko, kl = add_rule(koff, klen, toff, tlen, buf, st, kv_name, txt_at, tp - txt_at);
        star = pc;
        kvoff[pc] = ko;
        kvlen[pc] = kl;
    }
    // required in property order, then one alternative per optional property
    var pre_at = st[ST_B];
    var p = app(buf, pre_at, "\"{\" space ", 0, 10);
    var have_req: bool = false;
    j = 0;

    while j < pc {
        if props[(pf + j) * PS + 3] != 0 {
            if have_req {
                p = app(buf, p, " \",\" space ", 0, 11);
            }
            p = app(buf, p, buf, kvoff[j], kvlen[j]);
            have_req = true;
        }
        j = j + 1;
    }
    let pre_len = p - pre_at;
    st[ST_B] = p;
    var oc: i64 = 0;
    j = 0;

    while j < pc {
        if props[(pf + j) * PS + 3] == 0 {
            opt[oc] = j;
            oc = oc + 1;
        }
        j = j + 1;
    }

    if star >= 0 {
        opt[oc] = star;
        oc = oc + 1;
    }
    // every optional branch first, then the body, so the final text sits after
    // every rule and scratch slice it refers to
    var i: i64 = 0;

    while i < oc {
        let co, cl = opt_chain(opt, i, oc, 0, star, name, props, kvoff, kvlen, strs, sdir, koff, klen, toff, tlen, buf, st);
        choff[i] = co;
        chlen[i] = cl;
        i = i + 1;
    }
    let body_at = st[ST_B];
    p = app(buf, body_at, buf, pre_at, pre_len);

    if oc > 0 {
        p = app(buf, p, " (", 0, 2);

        if have_req {
            p = app(buf, p, " \",\" space ( ", 0, 13);
        }
        i = 0;

        while i < oc {
            if i > 0 {
                p = app(buf, p, " | ", 0, 3);
            }
            p = app(buf, p, buf, choff[i], chlen[i]);
            i = i + 1;
        }

        if have_req {
            p = app(buf, p, " )", 0, 2);
        }
        p = app(buf, p, " )?", 0, 3);
    }
    p = app(buf, p, " space \"}\"", 0, 10);
    st[ST_B] = p;
    return body_at, p - body_at;
}

// ---- $ref, allOf and integer bounds helpers -------------------------------

// _resolve_ref name: "ref" + the fragment after the first '#', with every
// maximal non-alphanumeric run collapsed to '-'.
fn ref_name_text(ref: const u8[:], buf: u8[:], st: i64[:]) -> i64, i64 {
    var cut: i64 = 0;
    var i: i64 = 0;

    while i < ref.len {
        if ref[i] == '#' {
            cut = i + 1;
            i = ref.len;
        } else {
            i = i + 1;
        }
    }
    let at = st[ST_B];
    var p = app(buf, at, "ref", 0, 3);
    let n = sanitize_into(ref[cut .. ref.len], buf, p);
    p = p + n;
    st[ST_B] = p;
    return at, p - at;
}

// allOf property merge: refs recurse, top-level anyOf alternatives are added
// as optional, nested anyOf is ignored, properties are copied in order.
recursive fn allof_collect(
    node: i64,
    is_req: i64,
    top: i64,
    meta: const i64[:],
    props: const i64[:],
    vals: const i64[:],
    mprops: i64[:],
    st: i64[:]) {
    let k = meta[node * MS + 0];

    if k == FG573_K_REF {
        let t = meta[node * MS + 3];

        if t >= 0 {
            allof_collect(t, is_req, 0, meta, props, vals, mprops, st);
        }
        return;
    }

    if k == FG573_K_ANY_OF {
        if top != 0 {
            let vf = meta[node * MS + 3];
            let vn = meta[node * MS + 4];
            var i: i64 = 0;

            while i < vn {
                allof_collect(vals[vf + i], 0, 0, meta, props, vals, mprops, st);
                i = i + 1;
            }
        }
        return;
    }

    if k == FG573_K_OBJECT {
        let pf = meta[node * MS + 5];
        let pc = meta[node * MS + 6];
        var j: i64 = 0;

        while j < pc {
            let mc = st[ST_MC];

            if 4 * mc + 3 < mprops.len {
                mprops[4 * mc + 0] = props[(pf + j) * PS + 0];
                mprops[4 * mc + 1] = props[(pf + j) * PS + 1];
                mprops[4 * mc + 2] = props[(pf + j) * PS + 2];
                mprops[4 * mc + 3] = is_req;
            }
            st[ST_MC] = mc + 1;
            j = j + 1;
        }
    }
}

// required is a name set in native, so one required occurrence makes every
// property with that name required.
fn allof_fix_required(mprops: i64[:], mc: i64) {
    var i: i64 = 0;

    while i < mc {
        if mprops[4 * i + 3] != 0 {
            let sid = mprops[4 * i + 1];
            var j: i64 = 0;

            while j < mc {
                if mprops[4 * j + 1] == sid {
                    mprops[4 * j + 3] = 1;
                }
                j = j + 1;
            }
        }
        i = i + 1;
    }
}

// allOf enum values: distinct candidates first, then those whose occurrence
// count equals the number of top-level components.
recursive fn allof_enum_sids(
    node: i64,
    top: i64,
    meta: const i64[:],
    vals: const i64[:],
    out: i64[:],
    count: i64,
    st: i64[:]) -> i64 {
    let k = meta[node * MS + 0];

    if k == FG573_K_REF {
        let t = meta[node * MS + 3];

        if t >= 0 {
            return allof_enum_sids(t, 0, meta, vals, out, count, st);
        }
        return count;
    }

    if k == FG573_K_ANY_OF {
        var n = count;

        if top != 0 {
            let vf = meta[node * MS + 3];
            let vn = meta[node * MS + 4];
            var i: i64 = 0;

            while i < vn {
                n = allof_enum_sids(vals[vf + i], 0, meta, vals, out, n, st);
                i = i + 1;
            }
        }
        return n;
    }

    if k == FG573_K_ENUM {
        let vf = meta[node * MS + 3];
        let vn = meta[node * MS + 4];
        var n = count;
        var i: i64 = 0;

        while i < vn {
            let sid = vals[vf + i];
            var seen: bool = false;
            var j: i64 = 0;

            while j < n {
                if out[j] == sid {
                    seen = true;
                }
                j = j + 1;
            }

            if !seen && n < out.len {
                out[n] = sid;
                n = n + 1;
            }
            i = i + 1;
        }
        return n;
    }
    return count;
}

recursive fn allof_enum_count(
    node: i64,
    top: i64,
    sid: i64,
    meta: const i64[:],
    vals: const i64[:]) -> i64 {
    let k = meta[node * MS + 0];

    if k == FG573_K_REF {
        let t = meta[node * MS + 3];

        if t >= 0 {
            return allof_enum_count(t, 0, sid, meta, vals);
        }
        return 0;
    }

    if k == FG573_K_ANY_OF {
        var n: i64 = 0;

        if top != 0 {
            let vf = meta[node * MS + 3];
            let vn = meta[node * MS + 4];
            var i: i64 = 0;

            while i < vn {
                n = n + allof_enum_count(vals[vf + i], 0, sid, meta, vals);
                i = i + 1;
            }
        }
        return n;
    }

    if k == FG573_K_ENUM {
        let vf = meta[node * MS + 3];
        let vn = meta[node * MS + 4];
        var n: i64 = 0;
        var i: i64 = 0;

        while i < vn {
            if vals[vf + i] == sid {
                n = n + 1;
            }
            i = i + 1;
        }
        return n;
    }
    return 0;
}

fn dump_less(strs: const u8[:], sdir: const i64[:], a: i64, b: i64) -> bool {
    let ao = sdir[2 * a + 0];
    let al = sdir[2 * a + 1];
    let bo = sdir[2 * b + 0];
    let bl = sdir[2 * b + 1];
    var i: i64 = 0;

    while i < al && i < bl {
        if strs[ao + i] < strs[bo + i] {
            return true;
        }

        if strs[ao + i] > strs[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return al < bl;
}

// Returns the sorted kept list offset in pwork and its length.
fn allof_enum(
    vals: const i64[:],
    vf: i64,
    vn: i64,
    meta: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    pwork: i64[:],
    st: i64[:]) -> i64, i64 {
    let base = st[ST_PCUR];
    st[ST_PCUR] = base + vn + 1;
    var count: i64 = 0;
    var i: i64 = 0;

    while i < vn {
        count = allof_enum_sids(vals[vf + i], 1, meta, vals, pwork[base .. pwork.len], count, st);
        i = i + 1;
    }
    var kept: i64 = 0;
    i = 0;

    while i < count {
        let sid = pwork[base + i];
        var total: i64 = 0;
        var j: i64 = 0;

        while j < vn {
            total = total + allof_enum_count(vals[vf + j], 1, sid, meta, vals);
            j = j + 1;
        }

        if total == vn {
            pwork[base + kept] = sid;
            kept = kept + 1;
        }
        i = i + 1;
    }
    // insertion sort by dump bytes, as std::map does in native
    i = 1;

    while i < kept {
        let cur = pwork[base + i];
        var j: i64 = i;
        var stop: bool = false;

        while j > 0 && !stop {
            let prev = pwork[base + j - 1];

            if dump_less(strs, sdir, cur, prev) {
                pwork[base + j] = prev;
                j = j - 1;
            } else {
                stop = true;
            }
        }
        pwork[base + j] = cur;
        i = i + 1;
    }
    return base, kept;
}

// ---- build_min_max_int port ------------------------------------------------

fn b_dec(buf: u8[:], ast: i64[:], v: i64) -> i64, i64 {
    let at = ast[0];
    let end = appd(buf, at, v);
    ast[0] = end;
    return at, end - at;
}

fn b_rep(buf: u8[:], ast: i64[:], c: u8, n: i64) -> i64, i64 {
    let at = ast[0];
    var p: i64 = at;
    var k: i64 = 0;

    while k < n {
        p = appc(buf, p, c);
        k = k + 1;
    }
    ast[0] = p;
    return at, p - at;
}

fn b_digit_range(out: u8[:], p: i64, from: u8, to: u8) -> i64 {
    var q = appc(out, p, '[');

    if from == to {
        q = appc(out, q, from);
    } else {
        q = appc(out, q, from);
        q = appc(out, q, '-');
        q = appc(out, q, to);
    }
    return appc(out, q, ']');
}

fn b_more_digits(out: u8[:], p: i64, mn: i64, mx: i64) -> i64 {
    var q = app(out, p, "[0-9]", 0, 5);

    if mn == mx && mn == 1 {
        return q;
    }
    q = appc(out, q, '{');
    q = appd(out, q, mn);

    if mx != mn {
        q = appc(out, q, ',');

        if mx != INT_MAX {
            q = appd(out, q, mx);
        }
    }
    return appc(out, q, '}');
}

recursive fn b_uniform(
    out: u8[:],
    p: i64,
    from: const u8[:],
    to: const u8[:],
    sbuf: u8[:],
    ast: i64[:]) -> i64 {
    var i: i64 = 0;

    while i < from.len && i < to.len && from[i] == to[i] {
        i = i + 1;
    }
    var q: i64 = p;

    if i > 0 {
        q = appc(out, q, '"');
        q = app(out, q, from, 0, i);
        q = appc(out, q, '"');
    }

    if i < from.len && i < to.len {
        if i > 0 {
            q = appc(out, q, ' ');
        }
        let sub_len = from.len - i - 1;

        if sub_len > 0 {
            let from_sub = from[i + 1 .. from.len];
            let to_sub = to[i + 1 .. to.len];
            let zo, zl = b_rep(sbuf, ast, '0', sub_len);
            let no, nl = b_rep(sbuf, ast, '9', sub_len);
            let zeros = sbuf[zo .. zo + zl];
            let nines = sbuf[no .. no + nl];
            var to_reached: bool = false;
            q = appc(out, q, '(');

            if eqb(from_sub, zeros) {
                q = b_digit_range(out, q, from[i], to[i] - 1);
                q = appc(out, q, ' ');
                q = b_more_digits(out, q, sub_len, sub_len);
            } else {
                q = appc(out, q, '[');
                q = appc(out, q, from[i]);
                q = appc(out, q, ']');
                q = appc(out, q, ' ');
                q = appc(out, q, '(');
                q = b_uniform(out, q, from_sub, nines, sbuf, ast);
                q = appc(out, q, ')');

                if from[i] < to[i] - 1 {
                    q = app(out, q, " | ", 0, 3);

                    if eqb(to_sub, nines) {
                        q = b_digit_range(out, q, from[i] + 1, to[i]);
                        to_reached = true;
                    } else {
                        q = b_digit_range(out, q, from[i] + 1, to[i] - 1);
                    }
                    q = appc(out, q, ' ');
                    q = b_more_digits(out, q, sub_len, sub_len);
                }
            }

            if !to_reached {
                q = app(out, q, " | ", 0, 3);
                q = b_digit_range(out, q, to[i], to[i]);
                q = appc(out, q, ' ');
                q = b_uniform(out, q, zeros, to_sub, sbuf, ast);
            }
            q = appc(out, q, ')');
        } else {
            q = appc(out, q, '[');
            q = appc(out, q, from[i]);
            q = appc(out, q, '-');
            q = appc(out, q, to[i]);
            q = appc(out, q, ']');
        }
    }
    return q;
}

fn b_parse_digits(s: const u8[:]) -> i64 {
    var v: i64 = 0;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if c >= '0' && c <= '9' {
            v = v * 10 + ((c - 48) as i64);
        }
        i = i + 1;
    }
    return v;
}

recursive fn build_mm_at(
    out: u8[:],
    p: i64,
    min_v: i64,
    max_v: i64,
    decimals_left: i64,
    top_level: i64,
    sbuf: u8[:],
    ast: i64[:]) -> i64 {
    let has_min = min_v != I64_MIN;
    let has_max = max_v != I64_MAX;

    if !has_min && !has_max {
        return -1;
    }
    var q: i64 = p;

    if has_min && has_max {
        var lo: i64 = min_v;
        let hi: i64 = max_v;

        if lo < 0 && hi < 0 {
            q = app(out, q, "\"-\" (", 0, 5);
            q = build_mm_at(out, q, 0 - hi, 0 - lo, decimals_left, 1, sbuf, ast);
            return appc(out, q, ')');
        }

        if lo < 0 {
            q = app(out, q, "\"-\" (", 0, 5);
            q = build_mm_at(out, q, 0, 0 - lo, decimals_left, 1, sbuf, ast);
            q = app(out, q, ") | ", 0, 4);
            lo = 0;
        }
        let mo0, ml0 = b_dec(sbuf, ast, lo);
        var min_s_off: i64 = mo0;
        var min_s_len: i64 = ml0;
        let xo0, xl0 = b_dec(sbuf, ast, hi);
        let max_s_off: i64 = xo0;
        let max_s_len: i64 = xl0;
        var digits: i64 = min_s_len;
        let max_digits: i64 = max_s_len;

        while digits < max_digits {
            let nz, nl = b_rep(sbuf, ast, '9', digits);
            q = b_uniform(out, q, sbuf[min_s_off .. min_s_off + min_s_len], sbuf[nz .. nz + nl], sbuf, ast);
            let oo, ol = b_rep(sbuf, ast, '0', digits);
            let one_at = ast[0];
            var op = appc(sbuf, one_at, '1');
            op = app(sbuf, op, sbuf, oo, ol);
            ast[0] = op;
            min_s_off = one_at;
            min_s_len = op - one_at;
            q = app(out, q, " | ", 0, 3);
            digits = digits + 1;
        }
        q = b_uniform(
            out,
            q,
            sbuf[min_s_off .. min_s_off + min_s_len],
            sbuf[max_s_off .. max_s_off + max_s_len],
            sbuf,
            ast);
        return q;
    }
    var less: i64 = decimals_left - 1;

    if less < 1 {
        less = 1;
    }

    if has_min {
        let lo: i64 = min_v;

        if lo < 0 {
            q = app(out, q, "\"-\" (", 0, 5);
            q = build_mm_at(out, q, I64_MIN, 0 - lo, decimals_left, 0, sbuf, ast);
            q = app(out, q, ") | [0] | [1-9] ", 0, 16);
            return b_more_digits(out, q, 0, decimals_left - 1);
        }

        if lo == 0 {
            if top_level != 0 {
                q = app(out, q, "[0] | [1-9] ", 0, 12);
                return b_more_digits(out, q, 0, less);
            }
            return b_more_digits(out, q, 1, decimals_left);
        }

        if lo <= 9 {
            let c = (48 as u8) + (lo as u8);
            var start: u8 = (48 as u8);

            if top_level != 0 {
                start = '1';
            }

            if c > start {
                q = b_digit_range(out, q, start, c - 1);
                q = appc(out, q, ' ');
                q = b_more_digits(out, q, 1, less);
                q = app(out, q, " | ", 0, 3);
            }
            q = b_digit_range(out, q, c, '9');
            q = appc(out, q, ' ');
            return b_more_digits(out, q, 0, less);
        }
        let mo, ml = b_dec(sbuf, ast, lo);
        let ms = sbuf[mo .. mo + ml];
        let c = ms[0];

        if c > '1' {
            var start: u8 = (48 as u8);

            if top_level != 0 {
                start = '1';
            }
            q = b_digit_range(out, q, start, c - 1);
            q = appc(out, q, ' ');
            q = b_more_digits(out, q, ml, less);
            q = app(out, q, " | ", 0, 3);
        }
        q = b_digit_range(out, q, c, c);
        q = app(out, q, " (", 0, 2);
        let rest = b_parse_digits(ms[1 .. ml]);
        q = build_mm_at(out, q, rest, I64_MAX, less, 0, sbuf, ast);
        q = appc(out, q, ')');

        if c < '9' {
            q = app(out, q, " | ", 0, 3);
            q = b_digit_range(out, q, c + 1, '9');
            q = appc(out, q, ' ');
            q = b_more_digits(out, q, ml - 1, less);
        }
        return q;
    }

    if has_max {
        if max_v >= 0 {
            if top_level != 0 {
                q = app(out, q, "\"-\" [1-9] ", 0, 10);
                q = b_more_digits(out, q, 0, less);
                q = app(out, q, " | ", 0, 3);
            }
            return build_mm_at(out, q, 0, max_v, decimals_left, 1, sbuf, ast);
        }
        q = app(out, q, "\"-\" (", 0, 5);
        q = build_mm_at(out, q, 0 - max_v, I64_MAX, decimals_left, 0, sbuf, ast);
        return appc(out, q, ')');
    }
    return q;
}


// ---- _pattern_to_rule port -------------------------------------------------

fn pat_non_literal(c: u8) -> bool {
    if c == '|' || c == '.' || c == '(' || c == ')' || c == '[' || c == ']' {
        return true;
    }

    if c == '{' || c == '}' || c == '*' || c == '+' || c == '?' || c == '^' || c == '$' {
        return true;
    }
    return false;
}

fn pat_escaped_special(c: u8) -> bool {
    if c == '^' || c == '$' || c == '.' || c == '[' || c == ']' {
        return true;
    }

    if c == '(' || c == ')' || c == '|' || c == '{' || c == '}' {
        return true;
    }

    if c == '*' || c == '+' || c == '?' {
        return true;
    }
    return false;
}

// gbnf_escape_length: 0 when the escape is not one GBNF keeps.
fn pat_escape_length(pat: const u8[:], pos: i64) -> i64 {
    if pos + 1 >= pat.len || pat[pos] != '\\' {
        return 0;
    }
    var n_hex: i64 = 0;
    let e = pat[pos + 1];

    if e == 'x' {
        n_hex = 2;
    } else

    if e == 'u' {
        n_hex = 4;
    } else

    if e == 'U' {
        n_hex = 8;
    } else

    if e == 't' || e == 'r' || e == 'n' || e == '\\' || e == '"' || e == '[' || e == ']' {
        return 2;
    } else {
        return 0;
    }

    if pos + 2 + n_hex > pat.len {
        return 0;
    }
    var i: i64 = 0;

    while i < n_hex {
        let h = pat[pos + 2 + i];

        if !((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F')) {
            return 0;
        }
        i = i + 1;
    }
    return 2 + n_hex;
}

fn pat_add(
    offs: i64[:],
    lens: i64[:],
    kinds: i64[:],
    count: i64,
    off: i64,
    len: i64,
    kind: i64,
    st: i64[:]) -> i64 {
    if count < offs.len {
        offs[count] = off;
        lens[count] = len;
        kinds[count] = kind;
        return count + 1;
    }
    st[ST_PSTAT] = 1;
    st[ST_RSN] = 8;
    return count;
}

// Quote a literal run as GBNF text. The run is already GBNF-escaped.
fn pat_quote(raw: const u8[:], buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = appc(buf, at, '"');
    p = app(buf, p, raw, 0, raw.len);
    p = appc(buf, p, '"');
    st[ST_B] = p;
    return at, p - at;
}

fn pat_cat(text: const u8[:], c: u8, buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = app(buf, at, text, 0, text.len);
    p = appc(buf, p, c);
    st[ST_B] = p;
    return at, p - at;
}

// join_seq: consecutive literal items merge into one quoted literal; items are
// separated by one space.
fn pat_join(
    offs: const i64[:],
    lens: const i64[:],
    kinds: const i64[:],
    count: i64,
    buf: u8[:],
    st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p: i64 = at;
    var i: i64 = 0;

    while i < count {
        if i > 0 {
            p = appc(buf, p, ' ');
        }

        if kinds[i] == 0 {
            let run_off = offs[i];
            var run_len = lens[i];
            var j = i + 1;

            while j < count && kinds[j] == 0 {
                run_len = run_len + lens[j];
                j = j + 1;
            }
            p = appc(buf, p, '"');
            p = app(buf, p, buf, run_off, run_len);
            p = appc(buf, p, '"');
            i = j;
        } else {
            p = app(buf, p, buf, offs[i], lens[i]);
            i = i + 1;
        }
    }
    st[ST_B] = p;
    return at, p - at;
}

fn pat_num(s: const u8[:]) -> i64, bool {
    if s.len == 0 {
        return 0, false;
    }
    var v: i64 = 0;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if c < '0' || c > '9' {
            return 0, false;
        }
        v = v * 10 + ((c - 48) as i64);
        i = i + 1;
    }
    return v, true;
}

// {m}, {m,} or {m,n}; advances ST_POS past '}'.
fn pat_brace(pat: const u8[:], pos: i64, st: i64[:]) -> i64, i64 {
    var j: i64 = pos;

    while j < pat.len && pat[j] != '}' {
        j = j + 1;
    }

    if j >= pat.len {
        st[ST_PSTAT] = 1;
        st[ST_RSN] = 5;
        return 0, 0;
    }
    let inner = pat[pos .. j];
    st[ST_POS] = j + 1;
    var comma: i64 = -1;
    var commas: i64 = 0;
    var k: i64 = 0;

    while k < inner.len {
        if inner[k] == ',' {
            if commas == 0 {
                comma = k;
            }
            commas = commas + 1;
        }
        k = k + 1;
    }

    if commas > 1 {
        st[ST_PSTAT] = 1;
        st[ST_RSN] = 6;
        return 0, 0;
    }

    if commas == 0 {
        let v, ok = pat_num(inner);

        if !ok {
            st[ST_PSTAT] = 1;
            st[ST_RSN] = 7;
            return 0, 0;
        }
        return v, v;
    }
    var mn: i64 = 0;
    var mx: i64 = INT_MAX;

    if comma > 0 {
        let v, ok = pat_num(inner[0 .. comma]);

        if !ok {
            st[ST_PSTAT] = 1;
            st[ST_RSN] = 7;
            return 0, 0;
        }
        mn = v;
    }

    if comma + 1 < inner.len {
        let v, ok = pat_num(inner[comma + 1 .. inner.len]);

        if !ok {
            st[ST_PSTAT] = 1;
            st[ST_RSN] = 7;
            return 0, 0;
        }
        mx = v;
    }
    return mn, mx;
}

fn pat_sub_name(name: const u8[:], n: i64, buf: u8[:], st: i64[:]) -> i64, i64 {
    let at = st[ST_B];
    var p = app(buf, at, name, 0, name.len);
    p = appc(buf, p, '-');
    p = appd(buf, p, n);
    st[ST_B] = p;
    return at, p - at;
}

recursive fn pat_transform(
    pat: const u8[:],
    in_group: i64,
    depth: i64,
    name: const u8[:],
    dotall: i64,
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    pwork: i64[:],
    suboff: i64[:],
    sublen: i64[:],
    subkeyoff: i64[:],
    subkeylen: i64[:],
    base: i64,
    cap: i64) -> i64 {
    if base + 3 * cap > pwork.len {
        st[ST_PSTAT] = 1;
        st[ST_RSN] = 8;
        return 0;
    }
    let offs = pwork[base .. base + cap];
    let lens = pwork[base + cap .. base + 2 * cap];
    let kinds = pwork[base + 2 * cap .. base + 3 * cap];
    var count: i64 = 0;
    var have_lit: bool = false;
    var lit_at: i64 = 0;
    var lit_len: i64 = 0;

    while st[ST_POS] < pat.len && st[ST_PSTAT] == 0 {
        let pos = st[ST_POS];
        let c = pat[pos];

        if c == '(' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }
            st[ST_POS] = pos + 1;

            if st[ST_POS] < pat.len && pat[st[ST_POS]] == '?' {
                if st[ST_POS] + 1 < pat.len && pat[st[ST_POS] + 1] == ':' {
                    st[ST_POS] = st[ST_POS] + 2;
                } else {
                    st[ST_PSTAT] = 1;
                    st[ST_RSN] = 2;
                }
            }

            if st[ST_PSTAT] == 0 {
                let nd = depth + 1;

                if nd > 100 {
                    st[ST_PSTAT] = 1;
                    st[ST_RSN] = 3;
                } else {
                    let child = pat_transform(
                        pat,
                        1,
                        nd,
                        name,
                        dotall,
                        koff,
                        klen,
                        toff,
                        tlen,
                        buf,
                        st,
                        pwork,
                        suboff,
                        sublen,
                        subkeyoff,
                        subkeylen,
                        base + 3 * cap,
                        cap);

                    if st[ST_PSTAT] == 0 {
                        let io, il = pat_join(
                            pwork[base + 3 * cap .. base + 3 * cap + cap],
                            pwork[base + 3 * cap + cap .. base + 3 * cap + 2 * cap],
                            pwork[base + 3 * cap + 2 * cap .. base + 3 * cap + 3 * cap],
                            child,
                            buf,
                            st);
                        let at = st[ST_B];
                        var p = appc(buf, at, '(');
                        p = app(buf, p, buf, io, il);
                        p = appc(buf, p, ')');
                        st[ST_B] = p;
                        count = pat_add(offs, lens, kinds, count, at, p - at, 1, st);
                    }
                }
            }
        } else

        if c == ')' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }
            st[ST_POS] = pos + 1;

            if in_group == 0 {
                st[ST_PSTAT] = 2;
                st[ST_RSN] = 1;
                return count;
            }
            return count;
        } else

        if c == '^' || c == '$' {
            st[ST_PSTAT] = 1;
            st[ST_RSN] = 4;
        } else

        if c == '[' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }
            let start = pos;
            var q = pos + 1;

            while q < pat.len && pat[q] != ']' && st[ST_PSTAT] == 0 {
                if pat[q] == '\\' {
                    let el = pat_escape_length(pat, q);

                    if el == 0 {
                        st[ST_PSTAT] = 1;
                        st[ST_RSN] = 5;
                    } else {
                        q = q + el;
                    }
                } else {
                    q = q + 1;
                }
            }

            if st[ST_PSTAT] == 0 {
                if q >= pat.len {
                    st[ST_PSTAT] = 2;
                    st[ST_RSN] = 2;
                } else {
                    let at = st[ST_B];
                    let n = q + 1 - start;
                    var p = app(buf, at, pat, start, n);
                    st[ST_B] = p;
                    st[ST_POS] = q + 1;
                    count = pat_add(offs, lens, kinds, count, at, n, 1, st);
                }
            }
        } else

        if c == '|' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }
            let at = st[ST_B];
            let p = appc(buf, at, '|');
            st[ST_B] = p;
            st[ST_POS] = pos + 1;
            count = pat_add(offs, lens, kinds, count, at, 1, 1, st);
        } else

        if c == '*' || c == '+' || c == '?' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }

            if count == 0 {
                st[ST_PSTAT] = 2;
                st[ST_RSN] = 3;
            } else {
                let li = count - 1;
                let o = offs[li];
                let l = lens[li];

                if kinds[li] == 0 {
                    let qo, ql = pat_quote(buf[o .. o + l], buf, st);
                    let no, nl = pat_cat(buf[qo .. qo + ql], c, buf, st);
                    offs[li] = no;
                    lens[li] = nl;
                    kinds[li] = 1;
                } else {
                    let no, nl = pat_cat(buf[o .. o + l], c, buf, st);
                    offs[li] = no;
                    lens[li] = nl;
                }
                st[ST_POS] = pos + 1;
            }
        } else

        if c == '{' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }
            st[ST_POS] = pos + 1;
            let mn, mx = pat_brace(pat, st[ST_POS], st);

            if st[ST_PSTAT] == 0 {
                if count == 0 {
                    st[ST_PSTAT] = 2;
                    st[ST_RSN] = 3;
                } else {
                    let li = count - 1;
                    let o = offs[li];
                    let l = lens[li];

                    if kinds[li] == 0 {
                        let qo, ql = pat_quote(buf[o .. o + l], buf, st);
                        let ro, rl = rep_impl(buf[qo .. qo + ql], "", mn, mx, buf, st);
                        offs[li] = ro;
                        lens[li] = rl;
                        kinds[li] = 1;
                    } else {
                        var found: i64 = -1;
                        var j: i64 = 0;

                        while j < st[ST_SUB] {
                            if slice_eq(buf, suboff[j], sublen[j], o, l) {
                                found = j;
                            }
                            j = j + 1;
                        }

                        if found < 0 {
                            found = st[ST_SUB];
                            suboff[found] = o;
                            sublen[found] = l;
                            let no, nl = pat_sub_name(name, found + 1, buf, st);
                            let ko, kl = add_rule(koff, klen, toff, tlen, buf, st, buf[no .. no + nl], o, l);
                            subkeyoff[found] = ko;
                            subkeylen[found] = kl;
                            st[ST_SUB] = found + 1;
                        }
                        let sk = buf[subkeyoff[found] .. subkeyoff[found] + subkeylen[found]];
                        let ro, rl = rep_impl(sk, "", mn, mx, buf, st);
                        offs[li] = ro;
                        lens[li] = rl;
                        kinds[li] = 1;
                    }
                }
            }
        } else

        if c == '.' {
            if have_lit {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }
            let dot_text: const u8[:] = "[^\\x0A\\x0D]";
            let at = st[ST_B];
            let end = app(buf, at, dot_text, 0, dot_text.len);
            let n = end - at;
            st[ST_B] = end;
            let ko, kl = add_rule(koff, klen, toff, tlen, buf, st, "dot", at, n);
            st[ST_POS] = pos + 1;
            count = pat_add(offs, lens, kinds, count, ko, kl, 1, st);
        } else {
            // literal run
            if !have_lit {
                lit_at = st[ST_B];
                lit_len = 0;
                have_lit = true;
            }
            var consumed: bool = false;

            while st[ST_POS] < pat.len && st[ST_PSTAT] == 0 {
                let q = st[ST_POS];

                if pat[q] == '\\' {
                    if q == pat.len - 1 {
                        st[ST_PSTAT] = 2;
                        st[ST_RSN] = 4;
                    } else {
                        let nx = pat[q + 1];

                        if pat_escaped_special(nx) {
                            if lit_at + lit_len < buf.len {
                                buf[lit_at + lit_len] = nx;
                            }
                            lit_len = lit_len + 1;
                            st[ST_B] = lit_at + lit_len;
                            st[ST_POS] = q + 2;
                            consumed = true;
                        } else {
                            let el = pat_escape_length(pat, q);

                            if el == 0 {
                                st[ST_PSTAT] = 1;
                                st[ST_RSN] = 6;
                            } else {
                                var z: i64 = 0;

                                while z < el {
                                    if lit_at + lit_len < buf.len {
                                        buf[lit_at + lit_len] = pat[q + z];
                                    }
                                    lit_len = lit_len + 1;
                                    z = z + 1;
                                }
                                st[ST_B] = lit_at + lit_len;
                                st[ST_POS] = q + el;
                                consumed = true;
                            }
                        }
                    }
                } else

                if pat[q] == '"' {
                    if lit_at + lit_len < buf.len {
                        buf[lit_at + lit_len] = '\\';
                    }
                    lit_len = lit_len + 1;

                    if lit_at + lit_len < buf.len {
                        buf[lit_at + lit_len] = '"';
                    }
                    lit_len = lit_len + 1;
                    st[ST_B] = lit_at + lit_len;
                    st[ST_POS] = q + 1;
                    consumed = true;
                } else

                if !pat_non_literal(pat[q]) &&
                    (q == pat.len - 1 || lit_len == 0 || pat[q + 1] == '.' || !pat_non_literal(pat[q + 1])) {
                    if lit_at + lit_len < buf.len {
                        buf[lit_at + lit_len] = pat[q];
                    }
                    lit_len = lit_len + 1;
                    st[ST_B] = lit_at + lit_len;
                    st[ST_POS] = q + 1;
                    consumed = true;
                } else {
                    st[ST_POS] = q;
                    break;
                }
            }

            if have_lit && st[ST_PSTAT] == 0 {
                count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
                have_lit = false;
            }

            if !consumed && st[ST_PSTAT] == 0 {
                st[ST_PSTAT] = 1;
                st[ST_RSN] = 7;
            }
        }
    }

    if have_lit && st[ST_PSTAT] == 0 {
        count = pat_add(offs, lens, kinds, count, lit_at, lit_len, 0, st);
    }

    if st[ST_PSTAT] == 0 && in_group != 0 {
        st[ST_PSTAT] = 2;
        st[ST_RSN] = 1;
    }
    return count;
}

fn pat_reason(code: i64) -> const u8[:] {
    if code == 1 {
        return "unbalanced parentheses";
    }

    if code == 2 {
        return "unterminated character class";
    }

    if code == 3 {
        return "nothing to repeat";
    }

    if code == 4 {
        return "trailing backslash";
    }

    if code == 5 {
        return "unterminated curly brackets";
    }

    if code == 6 {
        return "wrong number of values in curly brackets";
    }
    return "invalid number in curly brackets";
}

fn record_pattern_error(buf: u8[:], st: i64[:], pat: const u8[:]) {
    if st[ST_E] != 0 {
        return;
    }
    let at = st[ST_B];
    var p = app(buf, at, "Invalid pattern ", 0, 16);
    p = app(buf, p, pat, 0, pat.len);
    p = app(buf, p, ": ", 0, 2);
    let reason = pat_reason(st[ST_RSN]);
    p = app(buf, p, reason, 0, reason.len);
    st[ST_B] = p;
    st[ST_EO] = at;
    st[ST_EL] = p - at;
    st[ST_E] = 1;
}

// _visit_pattern: convert, otherwise fall back to the string primitive, or
// record the native invalid-pattern error. The partial rule map is rolled back
// exactly as the native copy of _rules is.
fn pattern_apply(
    pat: const u8[:],
    name: const u8[:],
    dotall: i64,
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    pwork: i64[:],
    suboff: i64[:],
    sublen: i64[:],
    subkeyoff: i64[:],
    subkeylen: i64[:]) -> i64, i64 {
    let n0 = st[ST_N];
    let b0 = st[ST_B];
    let p0 = st[ST_PCUR];
    var status: i64 = 0;
    var pbase: i64 = st[ST_PCUR];

    if pat.len < 2 || pat[0] != '^' || pat[pat.len - 1] != '$' {
        status = 1;
        st[ST_RSN] = 1;
    }
    var count: i64 = 0;

    if status == 0 {
        let sub = pat[1 .. pat.len - 1];
        let cap = sub.len + 8;
        pbase = st[ST_PCUR];
        st[ST_PCUR] = pbase + 3 * cap;
        st[ST_POS] = 0;
        st[ST_PSTAT] = 0;
        st[ST_SUB] = 0;
        st[ST_RSN] = 0;
        count = pat_transform(
            sub,
            0,
            0,
            name,
            dotall,
            koff,
            klen,
            toff,
            tlen,
            buf,
            st,
            pwork,
            suboff,
            sublen,
            subkeyoff,
            subkeylen,
            pbase,
            cap);
        status = st[ST_PSTAT];
    }

    if status == 1 {
        st[ST_N] = n0;
        st[ST_B] = b0;
        st[ST_PCUR] = p0;
        let so, sl = ensure_primitive(P_STRING, "string", koff, klen, toff, tlen, buf, st);
        return add_rule(koff, klen, toff, tlen, buf, st, name, so, sl);
    }

    if status == 2 {
        st[ST_N] = n0;
        st[ST_B] = b0;
        st[ST_PCUR] = p0;
        record_pattern_error(buf, st, pat);
        return st[ST_B], 0;
    }
    let cap = pat.len - 2 + 8;
    let io, il = pat_join(
        pwork[pbase .. pbase + cap],
        pwork[pbase + cap .. pbase + 2 * cap],
        pwork[pbase + 2 * cap .. pbase + 3 * cap],
        count,
        buf,
        st);
    let at = st[ST_B];
    var p = app(buf, at, "\"\\\"\" (", 0, 6);
    p = app(buf, p, buf, io, il);
    p = app(buf, p, ") \"\\\"\"", 0, 6);
    st[ST_B] = p;
    return add_rule(koff, klen, toff, tlen, buf, st, name, at, p - at);
}

recursive fn visit(
    si: i64,
    name: const u8[:],
    meta: const i64[:],
    props: const i64[:],
    vals: const i64[:],
    strs: const u8[:],
    sdir: const i64[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    opt: i64[:],
    kvoff: i64[:],
    kvlen: i64[:],
    choff: i64[:],
    chlen: i64[:],
    sbuf: u8[:],
    pwork: i64[:],
    mprops: i64[:],
    rref: i64[:]) -> i64, i64 {
    let rn_off, rn_len = make_rule_name(name, buf, st);
    let rn = buf[rn_off .. rn_off + rn_len];
    let sn_off, sn_len = make_sub_name(name, buf, st);
    let k = meta[si * MS + 0];
    let unsupported = meta[si * MS + 1];

    if unsupported != 0 {
        let dump = s_at(strs, sdir, meta[si * MS + 2]);
        record_error(buf, st, dump);
        return add_rule(koff, klen, toff, tlen, buf, st, rn, st[ST_B], 0);
    }

    if k == FG573_K_REF {
        let target = meta[si * MS + 3];
        let refsid = meta[si * MS + 4];
        let ref = s_at(strs, sdir, refsid);
        let ro, rl = ref_name_text(ref, buf, st);
        let ref_name = buf[ro .. ro + rl];
        let exists = find_entry(koff, klen, buf, ref_name, st[ST_N]) >= 0;
        var resolving: bool = false;
        var ri: i64 = 0;

        while ri < st[ST_R] && !resolving {
            if rref[ri] == refsid {
                resolving = true;
            }
            ri = ri + 1;
        }

        var alias_off = ro;
        var alias_len = rl;

        if !exists && !resolving {
            if target < 0 {
                let dump = s_at(strs, sdir, meta[si * MS + 2]);
                record_error(buf, st, dump);
                return add_rule(koff, klen, toff, tlen, buf, st, rn, st[ST_B], 0);
            }
            rref[st[ST_R]] = refsid;
            st[ST_R] = st[ST_R] + 1;
            let tk_off, tk_len = visit(
                target,
                ref_name,
                meta,
                props,
                vals,
                strs,
                sdir,
                koff,
                klen,
                toff,
                tlen,
                buf,
                st,
                opt,
                kvoff,
                kvlen,
                choff,
                chlen,
                sbuf,
                pwork,
                mprops,
                rref);
            st[ST_R] = st[ST_R] - 1;
            // _resolve_ref returns the name visit returned: a primitive target
            // shares its type rule (e.g. "string"), not the ref name.
            alias_off = tk_off;
            alias_len = tk_len;
        }
        // the alias keeps the native text: rule_name ::= returned name
        return add_rule(koff, klen, toff, tlen, buf, st, rn, alias_off, alias_len);
    }

    if k == FG573_K_ANY_OF || k == FG573_K_TYPE_ARRAY {
        let vf = meta[si * MS + 3];
        let vn = meta[si * MS + 4];
        let base = st[ST_PCUR];

        if base + 2 * vn > pwork.len {
            let dump = s_at(strs, sdir, meta[si * MS + 2]);
            record_error(buf, st, dump);
            return add_rule(koff, klen, toff, tlen, buf, st, rn, st[ST_B], 0);
        }
        st[ST_PCUR] = base + 2 * vn;
        var i: i64 = 0;

        while i < vn {
            let alt_at = st[ST_B];
            var ap = app(buf, alt_at, name, 0, name.len);

            if name.len == 0 {
                ap = app(buf, ap, "alternative-", 0, 12);
            } else {
                ap = appc(buf, ap, '-');
            }
            ap = appd(buf, ap, i);
            st[ST_B] = ap;
            let alt_name = buf[alt_at .. ap];
            let ko, kl = visit(
                vals[vf + i],
                alt_name,
                meta,
                props,
                vals,
                strs,
                sdir,
                koff,
                klen,
                toff,
                tlen,
                buf,
                st,
                opt,
                kvoff,
                kvlen,
                choff,
                chlen,
                sbuf,
                pwork,
                mprops,
                rref);
            pwork[base + i] = ko;
            pwork[base + vn + i] = kl;
            i = i + 1;
        }
        let at = st[ST_B];
        var p: i64 = at;
        i = 0;

        while i < vn {
            if i > 0 {
                p = app(buf, p, " | ", 0, 3);
            }
            p = app(buf, p, buf, pwork[base + i], pwork[base + vn + i]);
            i = i + 1;
        }
        st[ST_B] = p;
        st[ST_PCUR] = base;
        return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
    }

    if k == FG573_K_ALLOF {
        let vf = meta[si * MS + 3];
        let vn = meta[si * MS + 4];
        let eo, ec = allof_enum(vals, vf, vn, meta, strs, sdir, pwork, st);

        if ec > 0 {
            let at = st[ST_B];
            var p = appc(buf, at, '(');
            st[ST_B] = p;
            var i: i64 = 0;

            while i < ec {
                if i > 0 {
                    p = app(buf, p, " | ", 0, 3);
                    st[ST_B] = p;
                }
                let dump = s_at(strs, sdir, pwork[eo + i]);
                let lo, ll = format_literal(dump, buf, st);
                p = lo + ll;
                st[ST_B] = p;
                i = i + 1;
            }
            p = appc(buf, p, ')');
            st[ST_B] = p;
            return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
        }
        st[ST_MC] = 0;
        var i2: i64 = 0;

        while i2 < vn {
            allof_collect(vals[vf + i2], 1, 1, meta, props, vals, mprops, st);
            i2 = i2 + 1;
        }
        allof_fix_required(mprops, st[ST_MC]);
        let o, l = object_from(
            0,
            st[ST_MC],
            0,
            -1,
            meta,
            name,
            mprops,
            vals,
            strs,
            sdir,
            koff,
            klen,
            toff,
            tlen,
            buf,
            st,
            opt,
            kvoff,
            kvlen,
            choff,
            chlen,
            sbuf,
            pwork,
            mprops,
            rref);
        return add_rule(koff, klen, toff, tlen, buf, st, rn, o, l);
    }

    if k == FG573_K_PATTERN {
        let psid = meta[si * MS + 3];
        let pattern = s_at(strs, sdir, psid);
        return pattern_apply(
            pattern,
            rn,
            0,
            koff,
            klen,
            toff,
            tlen,
            buf,
            st,
            pwork,
            kvoff,
            kvlen,
            choff,
            chlen);
    }

    if k == FG573_K_STRING_LEN {
        let ch_off, ch_len = ensure_primitive(P_CHAR, "char", koff, klen, toff, tlen, buf, st);
        let ch = buf[ch_off .. ch_off + ch_len];
        let mn = meta[si * MS + 8];
        let mx = meta[si * MS + 9];
        let ro, rl = rep_impl(ch, "", mn, mx, buf, st);
        let at = st[ST_B];
        var p = app(buf, at, "\"\\\"\" ", 0, 5);
        p = app(buf, p, buf, ro, rl);
        p = app(buf, p, " \"\\\"\"", 0, 5);
        st[ST_B] = p;
        return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
    }

    if k == FG573_K_INTEGER_BOUNDS {
        let mn = meta[si * MS + 8];
        let mx = meta[si * MS + 9];
        st[ST_A] = 0;
        let ast = st[ST_A .. ST_A + 1];
        let at = st[ST_B];
        var p = appc(buf, at, '(');
        p = build_mm_at(buf, p, mn, mx, 16, 1, sbuf, ast);

        if p < 0 {
            let dump = s_at(strs, sdir, meta[si * MS + 2]);
            record_error(buf, st, dump);
            return add_rule(koff, klen, toff, tlen, buf, st, rn, st[ST_B], 0);
        }
        p = appc(buf, p, ')');
        st[ST_B] = p;
        return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
    }

    if k == FG573_K_CONST {
        let dump = s_at(strs, sdir, meta[si * MS + 2]);
        let o, l = format_literal(dump, buf, st);
        return add_rule(koff, klen, toff, tlen, buf, st, rn, o, l);
    }

    if k == FG573_K_ENUM {
        let vf = meta[si * MS + 3];
        let vn = meta[si * MS + 4];
        let at = st[ST_B];
        var p = appc(buf, at, '(');
        st[ST_B] = p;
        var i: i64 = 0;

        while i < vn {
            if i > 0 {
                p = app(buf, p, " | ", 0, 3);
                st[ST_B] = p;
            }
            let dump = s_at(strs, sdir, vals[vf + i]);
            let o, l = format_literal(dump, buf, st);
            p = o + l;
            st[ST_B] = p;
            i = i + 1;
        }
        p = appc(buf, p, ')');
        st[ST_B] = p;
        return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
    }

    if k == FG573_K_OBJECT {
        let pc = meta[si * MS + 6];
        let addk = meta[si * MS + 10];
        let body = object_branch(pc, addk);

        if body == 0 {
            let obj: const u8[:] = "object";
            let ko, kl = ensure_primitive(P_OBJECT, obj, koff, klen, toff, tlen, buf, st);
            return add_rule(koff, klen, toff, tlen, buf, st, rn, ko, kl);
        }

        if body == 2 {
            let dump = s_at(strs, sdir, meta[si * MS + 2]);
            record_error(buf, st, dump);
            return add_rule(koff, klen, toff, tlen, buf, st, rn, st[ST_B], 0);
        }
        let o, l = object_rule(si, name, meta, props, vals, strs, sdir, koff, klen, toff, tlen, buf, st, opt, kvoff, kvlen, choff, chlen, sbuf, pwork, mprops, rref);
        return add_rule(koff, klen, toff, tlen, buf, st, rn, o, l);
    }

    if k == FG573_K_ARRAY {
        let items = meta[si * MS + 7];

        if items >= 0 {
            let iname = "item";
            let cn_off, cn_len = join_name(name, iname, buf, st);
            let child_name = buf[cn_off .. cn_off + cn_len];
            let ik_off, ik_len = visit(
                items,
                child_name,
                meta,
                props,
                vals,
                strs,
                sdir,
                koff,
                klen,
                toff,
                tlen,
                buf,
                st,
                opt,
                kvoff,
                kvlen,
                choff,
                chlen,
                sbuf,
                pwork,
                mprops,
                rref);
            let item = buf[ik_off .. ik_off + ik_len];
            let sep: const u8[:] = "\",\" space";
            let mn = meta[si * MS + 8];
            let mx = meta[si * MS + 9];
            let ro, rl = rep_impl(item, sep, mn, mx, buf, st);
            let at = st[ST_B];
            var p = app(buf, at, "\"[\" space ", 0, 10);
            p = app(buf, p, buf, ro, rl);
            p = app(buf, p, " space \"]\"", 0, 10);
            st[ST_B] = p;
            return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
        }
        return primitive_branch(P_ARRAY, rn, koff, klen, toff, tlen, buf, st);
    }

    if k == FG573_K_TUPLE {
        let vf = meta[si * MS + 3];
        let vn = meta[si * MS + 4];
        let base = st[ST_PCUR];

        if base + 2 * vn > pwork.len {
            let dump = s_at(strs, sdir, meta[si * MS + 2]);
            record_error(buf, st, dump);
            return add_rule(koff, klen, toff, tlen, buf, st, rn, st[ST_B], 0);
        }
        st[ST_PCUR] = base + 2 * vn;
        var i: i64 = 0;

        while i < vn {
            let tn_off, tn_len = tuple_name(name, i, buf, st);
            let tname = buf[tn_off .. tn_off + tn_len];
            let ko, kl = visit(
                vals[vf + i],
                tname,
                meta,
                props,
                vals,
                strs,
                sdir,
                koff,
                klen,
                toff,
                tlen,
                buf,
                st,
                opt,
                kvoff,
                kvlen,
                choff,
                chlen,
                sbuf,
                pwork,
                mprops,
                rref);
            pwork[base + i] = ko;
            pwork[base + vn + i] = kl;
            i = i + 1;
        }
        let at = st[ST_B];
        var p = app(buf, at, "\"[\" space ", 0, 10);
        i = 0;

        while i < vn {
            if i > 0 {
                p = app(buf, p, " \",\" space ", 0, 11);
            }
            p = app(buf, p, buf, pwork[base + i], pwork[base + vn + i]);
            i = i + 1;
        }
        p = app(buf, p, " space \"]\"", 0, 10);
        st[ST_B] = p;
        st[ST_PCUR] = base;
        return add_rule(koff, klen, toff, tlen, buf, st, rn, at, p - at);
    }

    if k == FG573_K_FORMAT {
        let fmt = s_at(strs, sdir, meta[si * MS + 3]);

        if is_uuid_format(fmt) {
            var key_name: const u8[:] = fmt;

            if is_root_name(rn) {
                key_name = "root";
            }
            return ensure_primitive(P_UUID, key_name, koff, klen, toff, tlen, buf, st);
        }
        var which: i64 = P_DATE_STRING;
        var prim: const u8[:] = "date-string";

        if eqb(fmt, "time") {
            which = P_TIME_STRING;
            prim = "time-string";
        }

        if eqb(fmt, "date-time") {
            which = P_DATETIME_STRING;
            prim = "date-time-string";
        }
        let ko, kl = ensure_primitive(which, prim, koff, klen, toff, tlen, buf, st);
        return add_rule(koff, klen, toff, tlen, buf, st, rn, ko, kl);
    }

    if k == FG573_K_STRING {
        return primitive_branch(P_STRING, rn, koff, klen, toff, tlen, buf, st);
    }

    if k == FG573_K_INTEGER {
        return primitive_branch(P_INTEGER, rn, koff, klen, toff, tlen, buf, st);
    }

    if k == FG573_K_NUMBER {
        return primitive_branch(P_NUMBER, rn, koff, klen, toff, tlen, buf, st);
    }

    if k == FG573_K_BOOLEAN {
        return primitive_branch(P_BOOLEAN, rn, koff, klen, toff, tlen, buf, st);
    }

    if k == FG573_K_NULL {
        return primitive_branch(P_NULL, rn, koff, klen, toff, tlen, buf, st);
    }

    if k == FG573_K_EMPTY {
        let obj: const u8[:] = "object";
        let ko, kl = ensure_primitive(P_OBJECT, obj, koff, klen, toff, tlen, buf, st);
        return add_rule(koff, klen, toff, tlen, buf, st, rn, ko, kl);
    }
    // FG573_K_ANY and any other no-type schema: the value primitive alias.
    let val: const u8[:] = "value";
    let ko, kl = ensure_primitive(P_VALUE, val, koff, klen, toff, tlen, buf, st);
    return add_rule(koff, klen, toff, tlen, buf, st, rn, ko, kl);
}

// Object branch decision: 0 object primitive, 1 normal body, 2 unsupported
// additionalProperties.
// visit's primitive tail: a root primitive takes the "root" name, every other
// visit shares the primitive's own rule name (native line 1084).
fn primitive_branch(
    which: i64,
    rn: const u8[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:]) -> i64, i64 {
    if rn.len == 4 && rn[0] == 'r' && rn[1] == 'o' && rn[2] == 'o' && rn[3] == 't' {
        return ensure_primitive(which, "root", koff, klen, toff, tlen, buf, st);
    }
    let empty: const u8[:] = "";
    return ensure_primitive(which, empty, koff, klen, toff, tlen, buf, st);
}

fn object_branch(pc: i64, addk: i64) -> i64 {
    if pc == 0 {
        // true and absent fall through to the object primitive; false builds
        // the empty object; a schema builds the additional rule.
        if addk == 2 || addk == 3 {
            return 1;
        }
        return 0;
    }
    return 1;
}

// check_errors: the native text, first error only in this draft.
fn record_error(buf: u8[:], st: i64[:], dump: const u8[:]) {
    if st[ST_E] != 0 {
        return;
    }
    let at = st[ST_B];
    var p = app(buf, at, "Unrecognized schema: ", 0, 21);
    p = app(buf, p, dump, 0, dump.len);
    st[ST_B] = p;
    st[ST_EO] = at;
    st[ST_EL] = p - at;
    st[ST_E] = 1;
}

// format_grammar: "name ::= text\n" in std::map byte order.
// Entry: space rule, root visit, then the sorted rule text.
export fn json_schema_convert(
    root: i64,
    meta: i64[:],
    props: i64[:],
    vals: i64[:],
    strs: u8[:],
    sdir: i64[:],
    koff: i64[:],
    klen: i64[:],
    toff: i64[:],
    tlen: i64[:],
    buf: u8[:],
    st: i64[:],
    opt: i64[:],
    kvoff: i64[:],
    kvlen: i64[:],
    choff: i64[:],
    chlen: i64[:],
    sbuf: u8[:],
    pwork: i64[:],
    mprops: i64[:],
    rref: i64[:],
    out: u8[:]) -> i64 {
    st[ST_N] = 0;
    st[ST_B] = 0;
    st[ST_E] = 0;
    st[ST_EO] = 0;
    st[ST_EL] = 0;
    let space: const u8[:] = "| \" \" | \"\\n\"{1,2} [ \\t]{0,20}";
    let at = st[ST_B];
    let end = app(buf, at, space, 0, space.len);
    let sl = end - at;
    st[ST_B] = end;
    let sk_off, sk_len = add_rule(koff, klen, toff, tlen, buf, st, "space", at, sl);
    let empty: const u8[:] = "";
    let rk_off, rk_len = visit(
        root,
        empty,
        meta,
        props,
        vals,
        strs,
        sdir,
        koff,
        klen,
        toff,
        tlen,
        buf,
        st,
        opt,
        kvoff,
        kvlen,
        choff,
        chlen,
        sbuf,
        pwork,
        mprops,
        rref);

    if st[ST_E] != 0 {
        return -1;
    }
    return grammar_text(koff, klen, toff, tlen, st[ST_N], buf, out);
}
