// P9-06 S4: common_chat_parser_params construction and the serialized PEG
// arena load.
//
// Native sources:
//   runtime/core/src/engine/chat_parse.cpp:8-14  build_parse_params
//   common/chat.h:288-297                        common_chat_parser_params
//   common/peg-parser.cpp:1890-1900              common_peg_arena::to_json
//   common/peg-parser.cpp:2071-2105              common_peg_arena::from_json
//   common/peg-parser.cpp:2107-2112              dump/load (JSON, not the text dump)
//   common/peg-parser.cpp:~1810-1888             serialize_parser_variant
//   common/peg-parser.cpp:~1560-1810             deserialize_parser_variant
//
// The serialized arena is JSON:
//   {"parsers":[<variant>, ...], "rules":{<name>: <id>}, "root": <id>}
// with the variant objects keyed by "type" ("literal", "sequence", "choice",
// "repetition", "until", "rule", "tag", ...). `load` is from_json(parse(data));
// the text form is `dump`, a different function (the port has it as peg_dump).
//
// The port had no inverse of the JSON serialization, so this slice adds a
// minimal one: ids are the array indices, and only the fields the accepted
// peg_dump (goose/json/peg-dump.goose) reads are filled — kind, child range and
// child ids, one string per node (literal/name/pattern/delimiter/grammar) via a
// (offset, length) table, the delimiter list of Until/Ac, and min/max counts.
// The rules table is validated but not stored: Ref resolution belongs to the
// parser, which is out of this slice.
//
// Status: 0 ok; -1 the text is not JSON; -2 "parsers"/"rules"/"root" missing or
// not of the right type; -3 a variant has an unknown type or a missing field;
// -4 a child or the root references an id outside the parsers array; -5 a pool
// or array is too small. The native messages are written to `err`.

import std;
import jsonparse;

// Node kinds of common_peg_parser_variant, as in peg-dump.goose.
const PK_EPS: i64 = 0;
const PK_START: i64 = 1;
const PK_END: i64 = 2;
const PK_LIT: i64 = 3;
const PK_SEQ: i64 = 4;
const PK_CHOICE: i64 = 5;
const PK_REP: i64 = 6;
const PK_AND: i64 = 7;
const PK_NOT: i64 = 8;
const PK_ANY: i64 = 9;
const PK_SPACE: i64 = 10;
const PK_CHARS: i64 = 11;
const PK_STRING: i64 = 12;
const PK_UNTIL: i64 = 13;
const PK_RULE: i64 = 14;
const PK_REF: i64 = 15;
const PK_ATOMIC: i64 = 16;
const PK_TAG: i64 = 17;
const PK_GBNF: i64 = 18;
const PK_AC: i64 = 19;

// common_reasoning_format: NONE 0, AUTO 1 (common.h:419-426).
const PQ_REASONING_AUTO: i64 = 1;

const JK_BOOL: i64 = 1;
const JK_INT: i64 = 2;
const JK_UINT: i64 = 3;
const JK_STRING: i64 = 5;
const JK_ARRAY: i64 = 6;
const JK_OBJECT: i64 = 7;

extern "json_parse" fn json_parse_abi(
    text: const u8[:],
    jk: i64[:], jv: i64[:], jd: f64[:], js: i64[:], jl: i64[:], jf: i64[:],
    jn: i64[:], jh: i64[:],
    e_val: i64[:], e_koff: i64[:], e_klen: i64[:], e_next: i64[:],
    sbuf: u8[:], jkey: i64[:], jkl: i64[:], jchild: i64[:], st: i64[:]) -> i64;

// ── tree access ─────────────────────────────────────────────────────────────

struct PJ {
    jk: i64[:],
    jv: i64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    sbuf: const u8[:],
}

fn pj_get(p: const PJ, obj: i64, key: const u8[:]) -> i64 {
    let base = p.jf[obj];
    let n = p.jn[obj];
    var k: i64 = 0;

    while k < n {
        let ko = p.jkey[base + k];
        let kl = p.jkl[base + k];

        if kl == key.len {
            var same = true;
            var i: i64 = 0;

            while i < kl {
                if p.sbuf[ko + i] != key[i] {
                    same = false;
                    i = kl;
                } else {
                    i = i + 1;
                }
            }
            if same {
                return p.jchild[base + k];
            }
        }
        k = k + 1;
    }
    return 0 - 1;
}

fn pj_int(p: const PJ, obj: i64, key: const u8[:], dflt: i64) -> i64 {
    let n = pj_get(p, obj, key);

    if n < 0 {
        return dflt;
    }
    if p.jk[n] == JK_INT || p.jk[n] == JK_UINT {
        return p.jv[n];
    }
    return dflt;
}

fn pj_str(p: const PJ, obj: i64, key: const u8[:]) -> i64, i64 {
    let n = pj_get(p, obj, key);

    if n < 0 || p.jk[n] != JK_STRING {
        return 0 - 1, 0;
    }
    return p.js[n], p.jl[n];
}

fn pq_eq(a: const u8[:], b: const u8[:]) -> bool {
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

// ── string pool ─────────────────────────────────────────────────────────────

fn pq_put_str(strs: u8[:], sdir: i64[:], st: i64[:], s: const u8[:]) -> i64 {
    let sid = st[1];
    let at = st[0];
    var i: i64 = 0;

    while i < s.len {
        if at + i < strs.len {
            strs[at + i] = s[i];
        }
        i = i + 1;
    }
    if 2 * sid + 1 < sdir.len {
        sdir[2 * sid] = at;
        sdir[2 * sid + 1] = s.len;
    }
    st[0] = at + s.len;
    st[1] = sid + 1;
    return sid;
}

fn pq_err(err: u8[:], out: i64[:], s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len && i < err.len {
        err[i] = s[i];
        i = i + 1;
    }
    out[3] = i;
    return 0 - 3;
}

fn pq_err_c(err: u8[:], out: i64[:], code: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len && i < err.len {
        err[i] = s[i];
        i = i + 1;
    }
    out[3] = i;
    return code;
}

fn pq_err2(err: u8[:], out: i64[:], a: const u8[:], b: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < a.len && i < err.len {
        err[i] = a[i];
        i = i + 1;
    }
    var k: i64 = 0;

    while k < b.len && i + k < err.len {
        err[i + k] = b[k];
        k = k + 1;
    }
    out[3] = i + k;
    return 0 - 3;
}

fn pq_err_id(err: u8[:], out: i64[:], a: const u8[:], id: i64) -> i64 {
    var i: i64 = 0;

    while i < a.len && i < err.len {
        err[i] = a[i];
        i = i + 1;
    }
    // decimal id
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = id;

    if x < 0 {
        x = 0;
    }
    if x == 0 {
        buf[0] = 48;
        n = 1;
    }
    while x > 0 {
        buf[n] = (48 + (x - (x / 10) * 10)) as u8;
        x = x / 10;
        n = n + 1;
    }
    var j = n - 1;

    while j >= 0 && i < err.len {
        err[i] = buf[j];
        i = i + 1;
        j = j - 1;
    }
    out[3] = i;
    return 0 - 4;
}

// ── the load ────────────────────────────────────────────────────────────────

fn arena_load_impl(
    json: const u8[:],
    kind: i64[:],
    cfirst: i64[:],
    ccount: i64[:],
    children: i64[:],
    sidx: i64[:],
    dstart: i64[:],
    dcount: i64[:],
    delims: i64[:],
    mn: i64[:],
    mx: i64[:],
    strs: u8[:],
    sdir: i64[:],
    out: i64[:],
    err: u8[:]) -> i64 {
    var jk: i64[256] = [0; 256];
    var jv: i64[256] = [0; 256];
    var jd: f64[256] = [0.0; 256];
    var js: i64[256] = [0; 256];
    var jl: i64[256] = [0; 256];
    var jf: i64[256] = [0; 256];
    var jn: i64[256] = [0; 256];
    var jh: i64[256] = [0; 256];
    var ev: i64[512] = [0; 512];
    var ek: i64[512] = [0; 512];
    var el: i64[512] = [0; 512];
    var en: i64[512] = [0; 512];
    var sb: u8[16384] = [0; 16384];
    var key: i64[256] = [0; 256];
    var kl: i64[256] = [0; 256];
    var jc: i64[512] = [0; 512];
    var pst: i64[16] = [0; 16];

    out[0] = 0 - 1;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;

    // pool cursors: st[0] bytes, st[1] ids, st[2] delimiter slots
    var wst: i64[8] = [0; 8];

    let root = json_parse_abi(json, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);

    if root < 0 || jk[root] != JK_OBJECT {
        return pq_err_c(err, out, 0 - 1, "JSON missing or invalid arena object");
    }
    let p = PJ { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                 jkey: key, jkl: kl, jchild: jc, sbuf: sb };

    let parsers = pj_get(p, root, "parsers");

    if parsers < 0 || jk[parsers] != JK_ARRAY {
        return pq_err_c(err, out, 0 - 2, "JSON missing or invalid 'parsers' array");
    }
    let rules = pj_get(p, root, "rules");

    if rules < 0 || jk[rules] != JK_OBJECT {
        return pq_err_c(err, out, 0 - 2, "JSON missing or invalid 'rules' object");
    }
    let rootn = pj_get(p, root, "root");

    if rootn < 0 || (jk[rootn] != JK_INT && jk[rootn] != JK_UINT) {
        return pq_err_c(err, out, 0 - 2, "JSON missing 'root' field");
    }
    let n = jn[parsers];

    if n > kind.len || n > cfirst.len || n > ccount.len || n > sidx.len ||
       n > dstart.len || n > dcount.len || n > mn.len || n > mx.len {
        return pq_err_c(err, out, 0 - 5, "arena arrays too small");
    }
    var i: i64 = 0;
    var cur: i64 = 0; // children cursor

    while i < n {
        let node = p.jchild[jf[parsers] + i];
        let to, tl = pj_str(p, node, "type");

        if to < 0 {
            return pq_err_c(err, out, 0 - 3, "Parser variant JSON missing or invalid 'type' field");
        }
        let t = p.sbuf[to .. to + tl];
        var k: i64 = 0 - 1;

        if pq_eq(t, "epsilon") {
            k = PK_EPS;
        } else {
            if pq_eq(t, "start") {
                k = PK_START;
            } else {
                if pq_eq(t, "end") {
                    k = PK_END;
                } else {
                    if pq_eq(t, "literal") {
                        k = PK_LIT;
                    } else {
                        if pq_eq(t, "sequence") {
                            k = PK_SEQ;
                        } else {
                            if pq_eq(t, "choice") {
                                k = PK_CHOICE;
                            } else {
                                if pq_eq(t, "repetition") {
                                    k = PK_REP;
                                } else {
                                    if pq_eq(t, "and") {
                                        k = PK_AND;
                                    } else {
                                        if pq_eq(t, "not") {
                                            k = PK_NOT;
                                        } else {
                                            if pq_eq(t, "any") {
                                                k = PK_ANY;
                                            } else {
                                                if pq_eq(t, "space") {
                                                    k = PK_SPACE;
                                                } else {
                                                    if pq_eq(t, "chars") {
                                                        k = PK_CHARS;
                                                    } else {
                                                        if pq_eq(t, "string") {
                                                            k = PK_STRING;
                                                        } else {
                                                            if pq_eq(t, "until") {
                                                                k = PK_UNTIL;
                                                            } else {
                                                                if pq_eq(t, "rule") {
                                                                    k = PK_RULE;
                                                                } else {
                                                                    if pq_eq(t, "ref") {
                                                                        k = PK_REF;
                                                                    } else {
                                                                        if pq_eq(t, "atomic") {
                                                                            k = PK_ATOMIC;
                                                                        } else {
                                                                            if pq_eq(t, "tag") {
                                                                                k = PK_TAG;
                                                                            } else {
                                                                                if pq_eq(t, "gbnf") {
                                                                                    k = PK_GBNF;
                                                                                } else {
                                                                                    if pq_eq(t, "ac") {
                                                                                        k = PK_AC;
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if k < 0 {
            return pq_err2(err, out, "Unknown parser type: ", t);
        }
        kind[i] = k;
        cfirst[i] = cur;
        ccount[i] = 0;
        sidx[i] = 0 - 1;
        dstart[i] = 0;
        dcount[i] = 0;
        mn[i] = 0;
        mx[i] = 0 - 1;

        // one string per node where the type has one
        if k == PK_LIT || k == PK_RULE || k == PK_REF || k == PK_TAG || k == PK_GBNF ||
           k == PK_CHARS {
            var so: i64 = 0 - 1;
            var sl: i64 = 0;

            if k == PK_LIT {
                so, sl = pj_str(p, node, "literal");
                if so < 0 {
                    return pq_err(err, out, "literal parser missing or invalid 'literal' field");
                }
            } else {
                if k == PK_CHARS {
                    so, sl = pj_str(p, node, "pattern");
                } else {
                    if k == PK_GBNF {
                        so, sl = pj_str(p, node, "grammar");
                    } else {
                        so, sl = pj_str(p, node, "name");
                        if k == PK_TAG {
                            so, sl = pj_str(p, node, "tag");
                        }
                    }
                }
                if so < 0 {
                    return pq_err(err, out, "parser missing a required string field");
                }
            }
            sidx[i] = pq_put_str(strs, sdir, wst, p.sbuf[so .. so + sl]);
        }
        if k == PK_STRING {
            let so, sl = pj_str(p, node, "delimiter");

            if so < 0 || sl != 1 {
                return pq_err(err, out, "string parser missing or invalid 'delimiter' field");
            }
            sidx[i] = pq_put_str(strs, sdir, wst, p.sbuf[so .. so + sl]);
        }
        if k == PK_REP || k == PK_CHARS {
            mn[i] = pj_int(p, node, "min_count", 0);
            mx[i] = pj_int(p, node, "max_count", 0 - 1);
        }
        // children
        if k == PK_SEQ || k == PK_CHOICE {
            let ca = pj_get(p, node, "children");

            if ca < 0 || jk[ca] != JK_ARRAY {
                if k == PK_SEQ {
                    return pq_err(err, out, "sequence parser missing or invalid 'children' field");
                }
                return pq_err(err, out, "choice parser missing or invalid 'children' field");
            }
            var c: i64 = 0;

            while c < jn[ca] {
                if cur < children.len {
                    children[cur] = jv[p.jchild[jf[ca] + c]];
                }
                cur = cur + 1;
                c = c + 1;
            }
            ccount[i] = jn[ca];
        } else {
            if k == PK_REP || k == PK_AND || k == PK_NOT || k == PK_ATOMIC || k == PK_TAG ||
               k == PK_GBNF || k == PK_AC || k == PK_RULE {
                let ch = pj_get(p, node, "child");

                if ch < 0 || (jk[ch] != JK_INT && jk[ch] != JK_UINT) {
                    return pq_err(err, out, "parser missing required child field");
                }
                if cur < children.len {
                    children[cur] = jv[ch];
                }
                cur = cur + 1;
                ccount[i] = 1;
            }
        }
        // until / ac delimiter list
        if k == PK_UNTIL || k == PK_AC {
            let da = pj_get(p, node, "delimiters");

            if da < 0 || jk[da] != JK_ARRAY {
                if k == PK_UNTIL {
                    // an empty delimiter list is legal (rest()); a missing field is not
                    if da < 0 {
                        return pq_err(err, out, "until parser missing 'delimiters' field");
                    }
                } else {
                    return pq_err(err, out, "ac parser requires 'child' and a non-empty 'delimiters' array");
                }
            }
            if da >= 0 && jk[da] == JK_ARRAY {
                dstart[i] = wst[2];
                var c: i64 = 0;

                while c < jn[da] {
                    let dn = p.jchild[jf[da] + c];
                    var so: i64 = 0 - 1;
                    var sl: i64 = 0;

                    if jk[dn] == JK_STRING {
                        so = js[dn];
                        sl = jl[dn];
                    }
                    if so < 0 {
                        return pq_err(err, out, "until parser delimiter is not a string");
                    }
                    let sid = pq_put_str(strs, sdir, wst, p.sbuf[so .. so + sl]);

                    if wst[2] < delims.len {
                        delims[wst[2]] = sid;
                    }
                    wst[2] = wst[2] + 1;
                    c = c + 1;
                }
                dcount[i] = jn[da];
            }
        }
        i = i + 1;
    }

    // child ids must be inside the arena (native would trap on get())
    i = 0;
    while i < n {
        var c: i64 = 0;

        while c < ccount[i] {
            let id = children[cfirst[i] + c];

            if id < 0 || id >= n {
                return pq_err_id(err, out, "Child references invalid parser ID: ", id);
            }
            c = c + 1;
        }
        i = i + 1;
    }
    let r = jv[rootn];

    if r < 0 || r >= n {
        return pq_err_id(err, out, "Root references invalid parser ID: ", r);
    }
    out[0] = r;
    out[1] = n;
    out[2] = cur;
    return 0;
}

// Public wrapper: the loader is private because an exported function with a
// const-slice parameter cannot be instantiated for an internal call here.
export fn arena_load(
    json: const u8[:],
    kind: i64[:],
    cfirst: i64[:],
    ccount: i64[:],
    children: i64[:],
    sidx: i64[:],
    dstart: i64[:],
    dcount: i64[:],
    delims: i64[:],
    mn: i64[:],
    mx: i64[:],
    strs: u8[:],
    sdir: i64[:],
    out: i64[:],
    err: u8[:]) -> i64 {
    return arena_load_impl(json, kind, cfirst, ccount, children, sidx, dstart, dcount,
                           delims, mn, mx, strs, sdir, out, err);
}

// ── build_parse_params ──────────────────────────────────────────────────────

// chat_parse.cpp:8-14: copy format and generation prompt, load the arena, force
// reasoning_format = AUTO. out[0] format, out[1] prompt tokens/bytes, out[2]
// AUTO, out[3] arena root, out[4] node count, out[5] child slots used.
export fn pq_build_params(
    format: i64,
    gen_prompt_len: i64,
    json: const u8[:],
    kind: i64[:],
    cfirst: i64[:],
    ccount: i64[:],
    children: i64[:],
    sidx: i64[:],
    dstart: i64[:],
    dcount: i64[:],
    delims: i64[:],
    mn: i64[:],
    mx: i64[:],
    strs: u8[:],
    sdir: i64[:],
    out: i64[:],
    err: u8[:]) -> i64 {
    var arena_out: i64[8] = [0; 8];
    let st = arena_load_impl(json, kind, cfirst, ccount, children, sidx, dstart, dcount,
                             delims, mn, mx, strs, sdir, arena_out, err);
    var i: i64 = 0;

    while i < 8 {
        out[8 + i] = arena_out[i];
        i = i + 1;
    }
    // The native constructor copies format and generation prompt before the
    // load throws, so those two survive a bad arena; AUTO is set only after it
    // succeeds (chat_parse.cpp:10-13).
    out[0] = format;
    out[1] = gen_prompt_len;
    if st != 0 {
        out[2] = 0;
        return st;
    }
    out[2] = PQ_REASONING_AUTO;
    out[3] = arena_out[0];
    out[4] = arena_out[1];
    out[5] = arena_out[2];
    return 0;
}

// warn_parse_failed_once (chat_parse.cpp:16-21): std::call_once on a static
// flag. st[0] is the flag: the first call returns 1 (print), the rest 0.
export fn pq_warn_once(st: i64[:]) -> i64 {
    if st[0] != 0 {
        return 0;
    }
    st[0] = 1;
    return 1;
}
