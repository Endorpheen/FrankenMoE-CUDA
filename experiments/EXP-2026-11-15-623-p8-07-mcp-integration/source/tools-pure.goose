// P8-07 preparatory slice: local tool definitions, argument validation and
// result formation for the seven tools_io_basic tools.
//
// Native source: upstream/llama.cpp/tools/server/server-tools.cpp (lines cited
// per function below); the JSON is parsed with the accepted parser
// (goose/json/json-parser.goose, called through its export ABI).
//
// Scope: the pure parts only. No file system, no subprocess, no regex engine.
// The content, size, entry list and match list a tool would obtain from
// tools_io are parameters of these functions, so every branch is testable
// without a substrate. What still needs the substrate is listed at the end of
// TOOLS-ANALYSIS.md.
//
// Flat layouts:
//   st[0] status: 0 ok, -1 unparsable params or not an object (native: 400
//   invalid request), -2 a required member is missing (native: params.at throws,
//   400), -3 a tool error (native: 200 with {"error": ...}, message in `msg`);
//   st[1] tool index.
//   Every string a validator keeps is appended to `sbuf` and recorded as
//   (offset, length) pairs from st[20] upward: st[20]/st[21] first string,
//   st[22]/st[23] second, ...
//   Per-tool numbers stay in st[10..19] (documented per case below).

import std;
import jsonparse;

const TOOL_READ_FILE: i64 = 0;
const TOOL_FILE_GLOB: i64 = 1;
const TOOL_GREP: i64 = 2;
const TOOL_EXEC: i64 = 3;
const TOOL_WRITE_FILE: i64 = 4;
const TOOL_EDIT_FILE: i64 = 5;
const TOOL_GET_INFO: i64 = 6;
const TOOL_COUNT: i64 = 7;

// Limits, exactly as the native constants.
const LIMIT_READ_SIZE: i64 = 16 * 1024;              // server-tools.cpp:866
const LIMIT_READ_SIZE_BASE64: i64 = 32 * 1024 * 1024; // :867
const LIMIT_SEARCH_RESULTS: i64 = 100;               // :973
const LIMIT_GREP_RESULTS: i64 = 100;                 // :1090
const LIMIT_EXEC_OUTPUT: i64 = 16 * 1024;            // :1242
const LIMIT_EXEC_TIMEOUT: i64 = 60;                  // :1243
const LIMIT_INFO_OUTPUT: i64 = 4096;                 // :1697
const LIMIT_INFO_TIMEOUT: i64 = 5;                   // :1698

const PJ_K_BOOL: i64 = 1;
const PJ_K_INT: i64 = 2;
const PJ_K_UINT: i64 = 3;
const PJ_K_STRING: i64 = 5;
const PJ_K_ARRAY: i64 = 6;
const PJ_K_OBJECT: i64 = 7;

extern "json_parse" fn json_parse_abi(
    text: const u8[:],
    jk: i64[:], jv: i64[:], jd: f64[:], js: i64[:], jl: i64[:], jf: i64[:],
    jn: i64[:], jh: i64[:],
    e_val: i64[:], e_koff: i64[:], e_klen: i64[:], e_next: i64[:],
    sbuf: u8[:], jkey: i64[:], jkl: i64[:], jchild: i64[:], st: i64[:]) -> i64;

// ── tool table ──────────────────────────────────────────────────────────────

// The names in registration order (server-tools.cpp:1957-1963).
fn tool_name_impl(idx: i64, out: u8[:]) -> i64 {
    if idx == TOOL_READ_FILE {
        return tp_put(out, 0, "read_file");
    }
    if idx == TOOL_FILE_GLOB {
        return tp_put(out, 0, "file_glob_search");
    }
    if idx == TOOL_GREP {
        return tp_put(out, 0, "grep_search");
    }
    if idx == TOOL_EXEC {
        return tp_put(out, 0, "exec_shell_command");
    }
    if idx == TOOL_WRITE_FILE {
        return tp_put(out, 0, "write_file");
    }
    if idx == TOOL_EDIT_FILE {
        return tp_put(out, 0, "edit_file");
    }
    if idx == TOOL_GET_INFO {
        return tp_put(out, 0, "get_info");
    }
    return 0;
}

export fn tool_name(idx: i64, out: u8[:]) -> i64 {
    return tool_name_impl(idx, out);
}

// Dispatch by name; -1 means the native find_tool would throw (HTTP 404).
export fn tool_index(name: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < TOOL_COUNT {
        var buf: u8[32] = [0; 32];
        let n = tool_name_impl(i, buf);

        if tp_eq(name, buf[0 .. n]) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// permission_write / uses_cwd / support_stream flags (server-tools.cpp:869-1963).
export fn tool_flags(idx: i64) -> i64 {
    // bit 0 write permission, bit 1 uses cwd, bit 2 supports streaming
    if idx == TOOL_EXEC {
        return 7;
    }
    if idx == TOOL_READ_FILE || idx == TOOL_FILE_GLOB || idx == TOOL_GREP || idx == TOOL_GET_INFO {
        return 2;
    }
    if idx == TOOL_WRITE_FILE || idx == TOOL_EDIT_FILE {
        return 3;
    }
    return 0;
}

fn tp_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        if at + i < dst.len {
            dst[at + i] = s[i];
        }
        i = i + 1;
    }
    return at + s.len;
}

fn tp_eq(a: const u8[:], b: const u8[:]) -> bool {
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

fn tp_uint(dst: u8[:], at_in: i64, v: i64) -> i64 {
    var at = at_in;
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;

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
    var i = n - 1;

    while i >= 0 {
        if at < dst.len {
            dst[at] = buf[i];
        }
        at = at + 1;
        i = i - 1;
    }
    return at;
}

// ── params access, mirroring server-common.h json_value ─────────────────────

struct PNode {
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

fn pget(p: const PNode, obj: i64, key: const u8[:]) -> i64 {
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

// json_value<T>: a missing or null member is the default, a wrong type warns and
// is also the default (server-common.h:41).
fn pget_int(p: const PNode, obj: i64, key: const u8[:], dflt: i64) -> i64 {
    let n = pget(p, obj, key);

    if n < 0 {
        return dflt;
    }
    if p.jk[n] == PJ_K_INT || p.jk[n] == PJ_K_UINT {
        return p.jv[n];
    }
    return dflt;
}

fn pget_bool(p: const PNode, obj: i64, key: const u8[:], dflt: i64) -> i64 {
    let n = pget(p, obj, key);

    if n < 0 {
        return dflt;
    }
    if p.jk[n] == PJ_K_BOOL {
        return p.jv[n];
    }
    return dflt;
}

// 1 when the member is absent or null (the native params.at would throw).
fn p_missing(p: const PNode, obj: i64, key: const u8[:]) -> i64 {
    if pget(p, obj, key) < 0 {
        return 1;
    }
    return 0;
}

// (offset, length) of a string member, or (-1, 0) when absent.
fn pget_str(p: const PNode, obj: i64, key: const u8[:]) -> i64, i64 {
    let n = pget(p, obj, key);

    if n < 0 || p.jk[n] != PJ_K_STRING {
        return 0 - 1, 0;
    }
    return p.js[n], p.jl[n];
}

fn tp_copy(dst: u8[:], at_in: i64, src: const u8[:], off: i64, len: i64) -> i64 {
    var at = at_in;
    var i: i64 = 0;

    while i < len {
        if at + i < dst.len {
            dst[at + i] = src[off + i];
        }
        i = i + 1;
    }
    return at + len;
}

fn keep_str(st: i64[:], sbuf: u8[:], src: const u8[:], off: i64, len: i64, slot: i64) -> i64 {
    let at = st[18];
    let n = tp_copy(sbuf, at, src, off, len);

    st[18] = n;
    st[20 + 2 * slot] = at;
    st[21 + 2 * slot] = len;
    return 0;
}

fn keep_lit(st: i64[:], sbuf: u8[:], lit: const u8[:], slot: i64) -> i64 {
    let at = st[18];
    let n = tp_put(sbuf, at, lit);

    st[18] = n;
    st[20 + 2 * slot] = at;
    st[21 + 2 * slot] = lit.len;
    return 0;
}

fn set_str(st: i64[:], slot: i64, off: i64, len: i64) -> i64 {
    st[20 + 2 * slot] = off;
    st[21 + 2 * slot] = len;
    return 0;
}

fn set_msg(st: i64[:], msg: u8[:], text: const u8[:]) -> i64 {
    let n = tp_put(msg, 0, text);
    st[19] = n;
    return 0;
}

fn set_msg_prefixed(st: i64[:], msg: u8[:], prefix: const u8[:], arg: const u8[:], off: i64,
                    len: i64, tail: const u8[:]) -> i64 {
    var p = tp_put(msg, 0, prefix);

    p = tp_copy(msg, p, arg, off, len);
    p = tp_put(msg, p, tail);
    st[19] = p;
    return 0;
}

// ── validation ──────────────────────────────────────────────────────────────

// One call: parse the params text and validate them for the tool index.
// Returns st[0] (0 ok, -1 bad params, -2 missing required, -3 tool error).
export fn tool_validate(
    idx: i64,
    params: const u8[:],
    st: i64[:],
    sbuf: u8[:],
    msg: u8[:]) -> i64 {
    var jk: i64[64] = [0; 64];
    var jv: i64[64] = [0; 64];
    var jd: f64[64] = [0.0; 64];
    var js: i64[64] = [0; 64];
    var jl: i64[64] = [0; 64];
    var jf: i64[64] = [0; 64];
    var jn: i64[64] = [0; 64];
    var jh: i64[64] = [0; 64];
    var ev: i64[64] = [0; 64];
    var ek: i64[64] = [0; 64];
    var el: i64[64] = [0; 64];
    var en: i64[64] = [0; 64];
    var sb: u8[8192] = [0; 8192];
    var key: i64[64] = [0; 64];
    var kl: i64[64] = [0; 64];
    var jc: i64[64] = [0; 64];
    var pst: i64[16] = [0; 16];

    st[0] = 0;
    st[1] = idx;
    st[18] = 0;
    st[19] = 0;

    let root = json_parse_abi(params, jk, jv, jd, js, jl, jf, jn, jh,
                             ev, ek, el, en, sb, key, kl, jc, pst);

    if root < 0 || jk[root] != PJ_K_OBJECT {
        st[0] = 0 - 1;
        return st[0];
    }
    let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };

    if idx == TOOL_READ_FILE {
        return tv_read_file(p, root, st, sbuf, msg);
    }
    if idx == TOOL_FILE_GLOB {
        return tv_file_glob(p, root, st, sbuf, msg);
    }
    if idx == TOOL_GREP {
        return tv_grep(p, root, st, sbuf, msg);
    }
    if idx == TOOL_EXEC {
        return tv_exec(p, root, st, sbuf, msg);
    }
    if idx == TOOL_WRITE_FILE {
        return tv_write_file(p, root, st, sbuf, msg);
    }
    if idx == TOOL_EDIT_FILE {
        return tv_edit_file(p, root, st, sbuf, msg);
    }
    if idx == TOOL_GET_INFO {
        return 0;
    }
    st[0] = 0 - 1;
    return st[0];
}

// read_file (server-tools.cpp:898-967): path required; start_line default 1,
// end_line default -1 (unlimited), append_loc false, resp_type is a header and
// never a model argument (it is read here only to model the base64 branch).
fn tv_read_file(p: const PNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path");
        return st[0];
    }
    let po, pl = pget_str(p, root, "path");

    if po < 0 {
        st[0] = 0 - 1;
        set_msg(st, msg, "path must be a string");
        return st[0];
    }
    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    st[10] = pget_int(p, root, "start_line", 1);
    st[11] = pget_int(p, root, "end_line", 0 - 1);
    st[12] = pget_bool(p, root, "append_loc", 0);
    st[13] = 0;
    let ro, rl = pget_str(p, root, "resp_type");

    if ro >= 0 && rl == 6 && tp_eq(p.sbuf[ro .. ro + rl], "base64") {
        st[13] = 1;
    }
    return 0;
}

// file_glob_search (server-tools.cpp:1015-1085): path required; include "**";
// type is file/dir/all; max_depth clamped at 0; limit >= 1 and capped at 100.
fn tv_file_glob(p: const PNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path");
        return st[0];
    }
    let po, pl = pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let io, il = pget_str(p, root, "include");

    if io < 0 {
        keep_lit(st, sbuf, "**", 1);
    } else {
        keep_str(st, sbuf, p.sbuf, io, il, 1);
    }
    let eo, el = pget_str(p, root, "exclude");

    if eo < 0 {
        keep_str(st, sbuf, p.sbuf, 0, 0, 2);
    } else {
        keep_str(st, sbuf, p.sbuf, eo, el, 2);
    }
    let to, tl = pget_str(p, root, "type");
    var kind: i64 = 0; // 0 file, 1 dir, 2 all

    if to >= 0 {
        if tp_eq(p.sbuf[to .. to + tl], "dir") {
            kind = 1;
        } else {
            if tp_eq(p.sbuf[to .. to + tl], "all") {
                kind = 2;
            } else {
                if tp_eq(p.sbuf[to .. to + tl], "file") == false {
                    st[0] = 0 - 3;
                    set_msg_prefixed(st, msg, "invalid type: ", p.sbuf, to, tl,
                                     " (expected \"file\", \"dir\" or \"all\")");
                    return st[0];
                }
            }
        }
        keep_str(st, sbuf, p.sbuf, to, tl, 3);
    } else {
        keep_str(st, sbuf, p.sbuf, 0, 0, 3);
    }
    st[14] = kind;
    let md = pget_int(p, root, "max_depth", 0);

    if md < 0 {
        st[15] = 0;
    } else {
        st[15] = md;
    }
    let lim = pget_int(p, root, "limit", LIMIT_SEARCH_RESULTS);

    if lim < 1 {
        st[0] = 0 - 3;
        var p2 = tp_put(msg, 0, "invalid limit: ");

        p2 = tp_uint(msg, p2, lim);
        p2 = tp_put(msg, p2, " (expected 1 or more)");
        st[19] = p2;
        return st[0];
    }
    if lim > LIMIT_SEARCH_RESULTS {
        st[16] = LIMIT_SEARCH_RESULTS;
    } else {
        st[16] = lim;
    }
    return 0;
}

// grep_search (server-tools.cpp:1130-1231): path and pattern required; include
// "**"; literal/ignore_case false; context_lines >= 0.
fn tv_grep(p: const PNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 || p_missing(p, root, "pattern") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path or pattern");
        return st[0];
    }
    let po, pl = pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let pro, prl = pget_str(p, root, "pattern");

    keep_str(st, sbuf, p.sbuf, pro, prl, 1);
    let io, il = pget_str(p, root, "include");

    if io < 0 {
        keep_lit(st, sbuf, "**", 2);
    } else {
        keep_str(st, sbuf, p.sbuf, io, il, 2);
    }
    let eo, el = pget_str(p, root, "exclude");

    if eo < 0 {
        keep_str(st, sbuf, p.sbuf, 0, 0, 3);
    } else {
        keep_str(st, sbuf, p.sbuf, eo, el, 3);
    }
    st[10] = pget_bool(p, root, "return_line_numbers", 0);
    st[11] = pget_bool(p, root, "literal", 0);
    st[12] = pget_bool(p, root, "ignore_case", 0);
    let c = pget_int(p, root, "context_lines", 0);

    if c < 0 {
        st[13] = 0;
    } else {
        st[13] = c;
    }
    return 0;
}

// exec_shell_command (server-tools.cpp:1273-1319): command required; timeout
// default 10 capped at 60; max_output_size default and cap 16 KiB.
fn tv_exec(p: const PNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "command") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: command");
        return st[0];
    }
    let co, cl = pget_str(p, root, "command");

    keep_str(st, sbuf, p.sbuf, co, cl, 0);
    let t = pget_int(p, root, "timeout", 10);

    if t > LIMIT_EXEC_TIMEOUT {
        st[10] = LIMIT_EXEC_TIMEOUT;
    } else {
        st[10] = t;
    }
    let m = pget_int(p, root, "max_output_size", LIMIT_EXEC_OUTPUT);

    if m > LIMIT_EXEC_OUTPUT {
        st[11] = LIMIT_EXEC_OUTPUT;
    } else {
        st[11] = m;
    }
    return 0;
}

// write_file (server-tools.cpp:1350-1359): path and content required.
fn tv_write_file(p: const PNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 || p_missing(p, root, "content") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path or content");
        return st[0];
    }
    let po, pl = pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let co, cl = pget_str(p, root, "content");

    keep_str(st, sbuf, p.sbuf, co, cl, 1);
    st[10] = cl;
    return 0;
}

// edit_file (server-tools.cpp:1407-1502): path and a non-empty edits array;
// every edit needs a non-empty old_text and a new_text string. The edit pairs
// are copied into sbuf; st[10] is the edit count and st[30 + 4*i ...] holds
// (old off, old len, new off, new len).
fn tv_edit_file(p: const PNode, root: i64, st: i64[:], sbuf: u8[:], msg: u8[:]) -> i64 {
    if p_missing(p, root, "path") == 1 || p_missing(p, root, "edits") == 1 {
        st[0] = 0 - 2;
        set_msg(st, msg, "missing required member: path or edits");
        return st[0];
    }
    let po, pl = pget_str(p, root, "path");

    keep_str(st, sbuf, p.sbuf, po, pl, 0);
    let e = pget(p, root, "edits");

    if e < 0 || p.jk[e] != PJ_K_ARRAY || p.jn[e] == 0 {
        st[0] = 0 - 3;
        set_msg(st, msg, "\"edits\" must be a non-empty array");
        return st[0];
    }
    var at: i64 = st[18];
    var i: i64 = 0;

    while i < p.jn[e] {
        let item = p.jchild[p.jf[e] + i];
        var bad = false;

        if p.jk[item] != PJ_K_OBJECT {
            bad = true;
        }
        let oo, ol = pget_str(p, item, "old_text");

        if bad || oo < 0 {
            st[0] = 0 - 3;
            var m = tp_put(msg, 0, "edits[");

            m = tp_uint(msg, m, i);
            m = tp_put(msg, m, "].old_text must not be empty");
            st[19] = m;
            return st[0];
        }
        if ol == 0 {
            st[0] = 0 - 3;
            var m2 = tp_put(msg, 0, "edits[");

            m2 = tp_uint(msg, m2, i);
            m2 = tp_put(msg, m2, "].old_text must not be empty");
            st[19] = m2;
            return st[0];
        }
        let no, nl = pget_str(p, item, "new_text");
        var no2 = no;
        var nl2 = nl;

        if no < 0 {
            no2 = 0;
            nl2 = 0;
        }
        st[30 + 4 * i + 0] = at;
        st[30 + 4 * i + 1] = ol;
        at = tp_copy(sbuf, at, p.sbuf, oo, ol);
        st[30 + 4 * i + 2] = at;
        st[30 + 4 * i + 3] = nl2;
        at = tp_copy(sbuf, at, p.sbuf, no2, nl2);
        i = i + 1;
    }
    st[10] = p.jn[e];
    st[18] = at;
    return 0;
}

// ── uniqueness and overlap, against the ORIGINAL content ────────────────────
// The fuzzy fallback of the native code (normalize_for_fuzzy_match) is out of
// this slice; only exact matching is implemented, and the analysis says so.
fn count_occurrences(hay: const u8[:], needle: const u8[:]) -> i64 {
    if needle.len == 0 || hay.len < needle.len {
        return 0;
    }
    var n: i64 = 0;
    var i: i64 = 0;

    while i + needle.len <= hay.len {
        var same = true;
        var k: i64 = 0;

        while k < needle.len {
            if hay[i + k] != needle[k] {
                same = false;
                k = needle.len;
            } else {
                k = k + 1;
            }
        }
        if same {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

fn find_at(hay: const u8[:], needle: const u8[:]) -> i64 {
    if needle.len == 0 || hay.len < needle.len {
        return 0 - 1;
    }
    var i: i64 = 0;

    while i + needle.len <= hay.len {
        var same = true;
        var k: i64 = 0;

        while k < needle.len {
            if hay[i + k] != needle[k] {
                same = false;
                k = needle.len;
            } else {
                k = k + 1;
            }
        }
        if same {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// Validate the edits of the last tool_validate call against the file content.
// Returns 0 or -3, with the native message in `msg`. st[11..] receives the
// match positions sorted by index (st[11 + 2*i] index, [12 + 2*i] length).
export fn tool_edit_check(
    st: i64[:],
    sbuf: const u8[:],
    content: const u8[:],
    msg: u8[:]) -> i64 {
    let n = st[10];
    let po = st[20];
    let pl = st[21];
    var order: i64[64] = [0; 64];
    var pos: i64[64] = [0; 64];
    var i: i64 = 0;

    while i < n {
        let oo = st[30 + 4 * i + 0];
        let ol = st[30 + 4 * i + 1];
        let occ = count_occurrences(content, sbuf[oo .. oo + ol]);

        if occ == 0 {
            st[0] = 0 - 3;
            var m = tp_put(msg, 0, "could not find edits[");

            m = tp_uint(msg, m, i);
            m = tp_put(msg, m, "].old_text in ");
            m = tp_copy(msg, m, sbuf, po, pl);
            m = tp_put(msg, m, ", it must match the file's current content exactly");
            st[19] = m;
            return st[0];
        }
        if occ > 1 {
            st[0] = 0 - 3;
            var m2 = tp_put(msg, 0, "found ");

            m2 = tp_uint(msg, m2, occ);
            m2 = tp_put(msg, m2, " occurrences of edits[");
            m2 = tp_uint(msg, m2, i);
            m2 = tp_put(msg, m2, "].old_text in ");
            m2 = tp_copy(msg, m2, sbuf, po, pl);
            m2 = tp_put(msg, m2, ", it must be unique");
            st[19] = m2;
            return st[0];
        }
        order[i] = i;
        pos[i] = find_at(content, sbuf[oo .. oo + ol]);
        i = i + 1;
    }
    // insertion sort by position (n is small)
    i = 1;
    while i < n {
        var j = i;

        while j > 0 && pos[order[j - 1]] > pos[order[j]] {
            let t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
            j = j - 1;
        }
        i = i + 1;
    }
    i = 1;
    while i < n {
        let a = order[i - 1];
        let b = order[i];

        if pos[a] + st[30 + 4 * a + 1] > pos[b] {
            st[0] = 0 - 3;
            var m3 = tp_put(msg, 0, "edits[");

            m3 = tp_uint(msg, m3, a);
            m3 = tp_put(msg, m3, "] and edits[");
            m3 = tp_uint(msg, m3, b);
            m3 = tp_put(msg, m3, "] overlap in ");
            m3 = tp_copy(msg, m3, sbuf, po, pl);
            m3 = tp_put(msg, m3, "; merge them into one edit or target disjoint regions");
            st[19] = m3;
            return st[0];
        }
        i = i + 1;
    }
    i = 0;
    while i < n {
        st[11 + 2 * i] = pos[i];
        st[12 + 2 * i] = st[30 + 4 * i + 1];
        i = i + 1;
    }
    return 0;
}

// ── read_file text, pure over the content bytes ─────────────────────────────

fn tp_esc(dst: u8[:], at_in: i64, s: const u8[:]) -> i64 {
    var at = at_in;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if c == 34 {
            at = tp_put(dst, at, "\\\"");
        } else {
            if c == 92 {
                at = tp_put(dst, at, "\\\\");
            } else {
                if c == 10 {
                    at = tp_put(dst, at, "\\n");
                } else {
                    if c == 13 {
                        at = tp_put(dst, at, "\\r");
                    } else {
                        if c == 9 {
                            at = tp_put(dst, at, "\\t");
                        } else {
                            if c < 32 {
                                at = tp_put(dst, at, "\\u00");
                                at = tp_chr(dst, at, (48 + ((c >> 4) & 15)) as u8);
                                at = tp_chr(dst, at, (48 + (c & 15)) as u8);
                            } else {
                                at = tp_chr(dst, at, c);
                            }
                        }
                    }
                }
            }
        }
        i = i + 1;
    }
    return at;
}

fn tp_chr(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

// server-tools.cpp:920-966: 1-based inclusive range, -1 for "to the end",
// an optional "N→" prefix, and a 16 KiB ceiling with "[output truncated]".
// `start_line` < 1 behaves like 1 (the loop skips nothing earlier).
export fn tool_read_text(
    content: const u8[:],
    start_line: i64,
    end_line: i64,
    append_loc: i64,
    out: u8[:]) -> i64 {
    var p: i64 = 0;
    var lineno: i64 = 0;
    var at: i64 = 0;
    var stopped = false;

    while at < content.len && !stopped {
        // one line, without the newline
        var e = at;

        while e < content.len && content[e] != 10 {
            e = e + 1;
        }
        lineno = lineno + 1;

        if lineno >= start_line {
            if end_line != 0 - 1 && lineno > end_line {
                stopped = true;
            } else {
                var line_at = p;

                if append_loc != 0 {
                    line_at = tp_uint(out, line_at, lineno);
                    line_at = tp_put(out, line_at, "\xe2\x86\x92");
                }
                line_at = tp_copy(out, line_at, content, at, e - at);
                line_at = tp_chr(out, line_at, 10);
                if line_at > LIMIT_READ_SIZE {
                    // the native adds the marker once the ceiling would be crossed
                    p = tp_put(out, p, "[output truncated]");
                    stopped = true;
                } else {
                    p = line_at;
                }
            }
        } else {
            if end_line != 0 - 1 && lineno > end_line {
                stopped = true;
            }
        }
        at = e + 1;
    }
    return p;
}

// server-tools.cpp:908-917, 924-928: the size guards before reading.
// Returns 0 when the read may proceed, 1 when it is refused (message in `msg`).
export fn tool_read_guard(
    file_size: i64,
    end_line: i64,
    as_base64: i64,
    path: const u8[:],
    stat_ok: i64,
    msg: u8[:]) -> i64 {
    var p = 0;

    if stat_ok == 0 {
        p = tp_put(msg, 0, "cannot stat file: ");
        p = tp_copy(msg, p, path, 0, path.len);
        return 1;
    }
    if as_base64 != 0 {
        if file_size > LIMIT_READ_SIZE_BASE64 {
            p = tp_put(msg, 0, "file too large (");
            p = tp_uint(msg, p, file_size);
            p = tp_put(msg, p, " bytes, max ");
            p = tp_uint(msg, p, LIMIT_READ_SIZE_BASE64);
            p = tp_put(msg, p, ")");
            return 1;
        }
        return 0;
    }
    if file_size > LIMIT_READ_SIZE && end_line == 0 - 1 {
        p = tp_put(msg, 0, "file too large (");
        p = tp_uint(msg, p, file_size);
        p = tp_put(msg, p, " bytes, max ");
        p = tp_uint(msg, p, LIMIT_READ_SIZE);
        p = tp_put(msg, p, "). Use start_line/end_line to read a portion.");
        return 1;
    }
    return 0;
}

// server-tools.cpp:1157-1165: the literal-mode escaping of a regex pattern.
export fn tool_literal_regex(pat: const u8[:], out: u8[:]) -> i64 {
    var p: i64 = 0;
    var i: i64 = 0;

    while i < pat.len {
        let c = pat[i];

        if c == 92 || c == 94 || c == 36 || c == 46 || c == 124 || c == 63 || c == 42 ||
           c == 43 || c == 40 || c == 41 || c == 91 || c == 93 || c == 123 || c == 125 {
            p = tp_chr(out, p, 92);
        }
        p = tp_chr(out, p, c);
        i = i + 1;
    }
    return p;
}

// server-tools.cpp:1303-1310: the exec tail.
export fn tool_exec_tail(
    output: const u8[:],
    exit_code: i64,
    timed_out: i64,
    out: u8[:]) -> i64 {
    var p = tp_copy(out, 0, output, 0, output.len);

    p = tp_put(out, p, "\n[exit code: ");
    p = tp_uint(out, p, exit_code);
    p = tp_chr(out, p, 93);
    if timed_out != 0 {
        p = tp_put(out, p, " [exit due to timed out]");
    }
    return p;
}

// ── result JSON, in the native key order ────────────────────────────────────
// nlohmann ordered_json keeps insertion order, so the emitters follow the
// object literals in the source exactly.

export fn tool_json_error(msg: const u8[:], out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"error\":\"");

    p = tp_esc(out, p, msg);
    p = tp_put(out, p, "\"}");
    return p;
}

export fn tool_json_plain(text: const u8[:], out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"plain_text_response\":\"");

    p = tp_esc(out, p, text);
    p = tp_put(out, p, "\"}");
    return p;
}

export fn tool_json_write(path: const u8[:], bytes: i64, out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"result\":\"file written successfully\",\"path\":\"");

    p = tp_esc(out, p, path);
    p = tp_put(out, p, "\",\"bytes\":");
    p = tp_uint(out, p, bytes);
    p = tp_chr(out, p, 125);
    return p;
}

export fn tool_json_edit(path: const u8[:], edits: i64, out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"result\":\"file edited successfully\",\"path\":\"");

    p = tp_esc(out, p, path);
    p = tp_put(out, p, "\",\"edits_applied\":");
    p = tp_uint(out, p, edits);
    p = tp_chr(out, p, 125);
    return p;
}

export fn tool_json_info(os: const u8[:], cwd: const u8[:], out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"os\":\"");

    p = tp_esc(out, p, os);
    p = tp_put(out, p, "\",\"cwd\":\"");
    p = tp_esc(out, p, cwd);
    p = tp_put(out, p, "\"}");
    return p;
}

export fn tool_json_base64(size_bytes: i64, out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "{\"base64\":\"");

    p = tp_put(out, p, "\",\"size_bytes\":");
    p = tp_uint(out, p, size_bytes);
    p = tp_chr(out, p, 125);
    return p;
}

// server-tools.cpp:1060-1075: the glob summary tail.
export fn tool_glob_tail(
    total: i64,
    shown: i64,
    truncated: i64,
    out: u8[:]) -> i64 {
    var p = tp_put(out, 0, "\n---\nTotal matches: ");

    p = tp_uint(out, p, total);
    p = tp_chr(out, p, 10);
    if total > shown {
        p = tp_chr(out, p, 91);
        p = tp_uint(out, p, shown);
        p = tp_put(out, p, " results limit reached (");
        p = tp_uint(out, p, total);
        p = tp_put(out, p, " total matches). Refine the glob pattern to narrow the search.]\n");
    }
    if truncated != 0 {
        p = tp_put(out, p, "[results truncated: time budget or unreadable directory]\n");
    }
    return p;
}
