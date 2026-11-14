// Decision core (P8-07 S4) in Goose, from tools/server/server-decision.cpp.
//
// Covered native anchors: decision_question_type_name (10), decision_is_payload
// (216), parse_questions (220), parse_state (359), decision_replace_text (420),
// decision_sort_keys (444), decision_kev_render (467), escape_special_tokens
// (499), decision_kev_text (526), decision_d1omni_escape (531), the
// Python-compatible string/float/space helpers (556-722), h2o_cut_state (725),
// h2o_quote (748), prepare_state (774), render_h2o (782), the D1-omni markers
// (827-836), n_variants/n_outputs (838-846), the confidence functions
// (1442-1494), format_answer (1496), server_decision_group_tasks (1588).
//
// Left to S5, not covered here: init() from the model metadata (36), the
// image/audio loaders (338, 349, they need mtmd), the jinja render (984), the
// fill_task family and d1_labels (they need a tokenizer), and fill_task_laya.
// parse_state keeps the message and media-part decisions but counts the media
// instead of decoding it, and h2o_cut_state takes the token budget as an
// argument and splits on bytes, since the tokenizer stays native (D6).
//
// Owned decisions (compared against a transcription of the native functions):
//   - question and option normalisation: the h2o defaults, score keys from the
//     level index, the noul aliases, the true-first and criteria-order swaps,
//     the option limit, the choice sort;
//   - the kev flattening: raw strings, "- " items with their leading space,
//     tab, newline and carriage return stripped, ":\n" before a nested value;
//   - the Python layouts: repr of a float (exponent form below 1e-4 and from
//     1e16, ".0" on an integral value), str escapes, str.isspace, dumps with
//     indent < 0 using "," and ":" and with indent >= 0 using ", " and ": ";
//   - the h2o texts: compact dumps with ", " and ": ", lstrip of the
//     instructions, rstrip of the last option, the quote of the special
//     tokens, the head/tail cut with a "..." line between the halves;
//   - the answer: softmax over the variants with the second one reversed, the
//     score bias, the noul/choice/score fields, the confidence formulas.
//
// The JSON is read with the accepted parser (goose/json/json-parser.goose,
// imported as jsonparse), which keeps the document key order like the native
// nlohmann::ordered_json. Numbers use the nlohmann layout where the native code
// calls json::dump() and the Python layout where it calls decision_py_dumps().
//
// Limits: 16 questions, 64 options, 1024 JSON nodes, 16384 bytes of JSON
// strings, 32768-byte scratch strings. The gate stays below them; beyond them
// the port truncates and states it.

import std;
import jsonparse;

// The accepted parser through its export ABI: an exported function with
// const-slice parameters cannot be instantiated for an internal call when the
// module is composed into one unit with the parser.
extern "json_parse" fn json_parse_abi(
    text: const u8[:],
    jk: i64[:], jv: i64[:], jd: f64[:], js: i64[:], jl: i64[:], jf: i64[:],
    jn: i64[:], jh: i64[:],
    e_val: i64[:], e_koff: i64[:], e_klen: i64[:], e_next: i64[:],
    sbuf: u8[:], jkey: i64[:], jkl: i64[:], jchild: i64[:], st: i64[:]) -> i64;

// JSON node kinds, as jsonparse emits them (its K_* names are module globals).
const DC_NULL: i64 = 0;
const DC_BOOL: i64 = 1;
const DC_INT: i64 = 2;
const DC_UINT: i64 = 3;
const DC_FLOAT: i64 = 4;
const DC_STRING: i64 = 5;
const DC_ARRAY: i64 = 6;
const DC_OBJECT: i64 = 7;

// common_decision_type, common/common.h:974
const DC_T_NONE: i64 = 0;
const DC_T_OPENJEV: i64 = 1;
const DC_T_LEV: i64 = 2;
const DC_T_KEV: i64 = 3;
const DC_T_NIMBLE: i64 = 4;
const DC_T_LAYA: i64 = 5;
const DC_T_CLEF: i64 = 6;
const DC_T_PPLX: i64 = 7;
const DC_T_VLLM2: i64 = 8;
const DC_T_VLLM3: i64 = 9;
const DC_T_D1: i64 = 10;
const DC_T_D1_OMNI: i64 = 11;
const DC_T_GRANITE: i64 = 12;
const DC_T_H2O: i64 = 13;

// server_decision_question_type, server-decision.h:42
const DC_Q_CHOICE: i64 = 0;
const DC_Q_SCORE: i64 = 1;
const DC_Q_NOUL: i64 = 2;

const DC_MAX_Q: i64 = 16;
const DC_MAX_OPT: i64 = 64;
const DC_LEV_N_RATINGS: i64 = 9;
const DC_H2O_MAX_STATE_TOKENS: i64 = 32000;
const DC_SCRATCH: i64 = 32768;
const DC_KEYBUF: i64 = 8192;

// The C primitives the native helpers borrow: "%.*e", strtod, exp and log. The
// first two are the runtime hooks the accepted json modules already use.
extern "gs_json_strtod" fn dc_strtod(s: const u8[:]) -> f64;
extern "gs_json_fmt_e" fn dc_fmt_e(out: u8[:], prec: i64, x: f64) -> i64;
extern "gs_exp" fn dc_exp(x: f64) -> f64;
extern "gs_log" fn dc_log(x: f64) -> f64;

struct DTree {
    jk: i64[:],
    jv: i64[:],
    jd: f64[:],
    js: i64[:],
    jl: i64[:],
    jf: i64[:],
    jn: i64[:],
    jkey: i64[:],
    jkl: i64[:],
    jchild: i64[:],
    sbuf: const u8[:],
}

// ── byte output ─────────────────────────────────────────────────────────────

fn dc_put(dst: u8[:], at: i64, s: const u8[:], off: i64, len: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < len {
        if p < dst.len {
            dst[p] = s[off + i];
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn dc_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return dc_put(dst, at, s, 0, s.len);
}

fn dc_putc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn dc_puti(dst: u8[:], at: i64, v: i64) -> i64 {
    var tmp: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;
    var neg = false;

    if x < 0 {
        neg = true;
        x = 0 - x;
    }
    if x == 0 {
        tmp[0] = 48;
        n = 1;
    }
    while x > 0 {
        tmp[n] = (48 + x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var p = at;

    if neg {
        p = dc_putc(dst, p, 45);
    }
    var i = n - 1;

    while i >= 0 {
        p = dc_putc(dst, p, tmp[i]);
        i = i - 1;
    }
    return p;
}

fn dc_pad(dst: u8[:], at: i64, n: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < n {
        p = dc_putc(dst, p, 32);
        i = i + 1;
    }
    return p;
}

fn dc_min(a: i64, b: i64) -> i64 {
    if a < b {
        return a;
    }
    return b;
}

fn dc_max(a: i64, b: i64) -> i64 {
    if a > b {
        return a;
    }
    return b;
}

// a model score is a float in the native code
fn dc_f32(x: f64) -> f64 {
    return (x as f32) as f64;
}

// ── tree access ─────────────────────────────────────────────────────────────

fn dc_kind(t: const DTree, n: i64) -> i64 {
    if n < 0 {
        return 0 - 1;
    }
    return t.jk[n];
}

fn dc_elem(t: const DTree, n: i64, i: i64) -> i64 {
    return t.jchild[t.jf[n] + i];
}

fn dc_key_off(t: const DTree, n: i64, i: i64) -> i64 {
    return t.jkey[t.jf[n] + i];
}

fn dc_key_len(t: const DTree, n: i64, i: i64) -> i64 {
    return t.jkl[t.jf[n] + i];
}

fn dc_bytes_eq(s: const u8[:], off: i64, len: i64, k: const u8[:]) -> bool {
    if len != k.len {
        return false;
    }
    var i: i64 = 0;

    while i < len {
        if s[off + i] != k[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn dc_bytes_less(s: const u8[:], ao: i64, al: i64, bo: i64, bl: i64) -> bool {
    var i: i64 = 0;

    while i < al && i < bl {
        if s[ao + i] != s[bo + i] {
            return s[ao + i] < s[bo + i];
        }
        i = i + 1;
    }
    return al < bl;
}

fn dc_get(t: const DTree, obj: i64, key: const u8[:]) -> i64 {
    if obj < 0 || t.jk[obj] != DC_OBJECT {
        return 0 - 1;
    }
    let base = t.jf[obj];
    let n = t.jn[obj];
    var i: i64 = 0;

    while i < n {
        if dc_bytes_eq(t.sbuf, t.jkey[base + i], t.jkl[base + i], key) {
            return t.jchild[base + i];
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn dc_has(t: const DTree, obj: i64, key: const u8[:]) -> bool {
    return dc_get(t, obj, key) >= 0;
}

// nlohmann empty(): null, an empty container and an empty string are empty,
// a number and a boolean are not.
fn dc_is_empty(t: const DTree, n: i64) -> bool {
    if n < 0 {
        return true;
    }
    let k = t.jk[n];

    if k == DC_NULL {
        return true;
    }
    if k == DC_STRING {
        return t.jl[n] == 0;
    }
    if k == DC_ARRAY || k == DC_OBJECT {
        return t.jn[n] == 0;
    }
    return false;
}

// decision_is_payload, server-decision.cpp:216
fn dc_is_payload(t: const DTree, n: i64) -> bool {
    let k = dc_kind(t, n);
    return k == DC_STRING || k == DC_OBJECT || k == DC_ARRAY;
}

// ── UTF-8, the way decision_utf8_next reads it (server-decision.cpp:557) ─────

// One code point at s[i]; an invalid byte reads as itself. Returns the code
// point and the next index.
fn dc_utf8_next(s: const u8[:], i: i64) -> i64, i64 {
    let c = s[i] as i64;
    var n: i64 = 1;

    if c >= 0x80 {
        if (c >> 5) == 6 {
            n = 2;
        } else if (c >> 4) == 14 {
            n = 3;
        } else if (c >> 3) == 30 {
            n = 4;
        }
    }
    if i + n > s.len {
        n = 1;
    }
    var cp: i64 = c;

    if n == 2 {
        cp = c & 0x1f;
    } else if n == 3 {
        cp = c & 0x0f;
    } else if n == 4 {
        cp = c & 0x07;
    }
    var k: i64 = 1;

    while k < n {
        cp = (cp << 6) | ((s[i + k] as i64) & 0x3f);
        k = k + 1;
    }
    return cp, i + n;
}

// dump_safe(): U+FFFD for each ill-formed run, the byte that broke a sequence
// is read again, a truncated tail is one replacement.
fn dc_utf8_safe(dst: u8[:], at: i64, s: const u8[:], off: i64, len: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < len {
        let c = s[off + i] as i64;

        if c < 0x80 {
            p = dc_putc(dst, p, c as u8);
            i = i + 1;
        } else {
            var need: i64 = 0;
            var lo: i64 = 0x80;
            var hi: i64 = 0xbf;
            var lead_ok = true;

            if c >= 0xc2 && c <= 0xdf {
                need = 1;
            } else if c == 0xe0 {
                need = 2;
                lo = 0xa0;
            } else if c >= 0xe1 && c <= 0xec {
                need = 2;
            } else if c == 0xed {
                need = 2;
                hi = 0x9f;
            } else if c >= 0xee && c <= 0xef {
                need = 2;
            } else if c == 0xf0 {
                need = 3;
                lo = 0x90;
            } else if c >= 0xf1 && c <= 0xf3 {
                need = 3;
            } else if c == 0xf4 {
                need = 3;
                hi = 0x8f;
            } else {
                lead_ok = false;
            }
            var got: i64 = 0;
            var seq_ok = false;

            if lead_ok {
                var k: i64 = 0;
                var cont = true;

                while k < need && cont {
                    if i + 1 + k >= len {
                        cont = false;
                    } else {
                        let b = s[off + i + 1 + k] as i64;
                        if k == 0 {
                            if b < lo || b > hi {
                                cont = false;
                            }
                        } else if b < 0x80 || b > 0xbf {
                            cont = false;
                        }
                    }
                    if cont {
                        k = k + 1;
                    }
                }
                got = k;
                if got == need {
                    seq_ok = true;
                }
            }
            if seq_ok {
                var q: i64 = 0;

                while q <= need {
                    p = dc_putc(dst, p, s[off + i + q]);
                    q = q + 1;
                }
                i = i + need + 1;
            } else {
                p = dc_putc(dst, p, 239);
                p = dc_putc(dst, p, 191);
                p = dc_putc(dst, p, 189);
                i = i + got + 1;
            }
        }
    }
    return p;
}

// decision_py_isspace, server-decision.cpp:572
fn dc_py_isspace(cp: i64) -> bool {
    return (cp >= 0x09 && cp <= 0x0d) || (cp >= 0x1c && cp <= 0x20) || cp == 0x85 || cp == 0xa0 ||
           cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202f || cp == 0x205f || cp == 0x3000;
}

// decision_py_lstrip, server-decision.cpp:587: the first byte that is not in a
// leading whitespace code point.
fn dc_py_lstrip(s: const u8[:]) -> i64 {
    var i: i64 = 0;
    var done = false;

    while i < s.len && !done {
        let cp, next = dc_utf8_next(s, i);

        if !dc_py_isspace(cp) {
            done = true;
        } else {
            i = next;
        }
    }
    return i;
}

// decision_py_rstrip, server-decision.cpp:577: one past the last byte of the
// last code point that is not a whitespace one.
fn dc_py_rstrip(s: const u8[:]) -> i64 {
    var end: i64 = 0;
    var i: i64 = 0;

    while i < s.len {
        let cp, next = dc_utf8_next(s, i);

        if !dc_py_isspace(cp) {
            end = next;
        }
        i = next;
    }
    return end;
}

// ── floats ──────────────────────────────────────────────────────────────────

// The fewest digits that read back the same, as "%.*e" gives them: the digits
// and the power of ten of the first one.
fn dc_sci(v: f64, dg: u8[:]) -> i64, i64, bool {
    var buf: u8[64] = [0; 64];
    var prec: i64 = 0;
    var n: i64 = 0;
    var done = false;

    while prec < 17 && !done {
        n = dc_fmt_e(buf, prec, v);
        if dc_strtod(buf[0 .. n]) == v {
            done = true;
        } else {
            prec = prec + 1;
        }
    }
    var ei: i64 = 0;
    var j: i64 = 0;

    while j < n {
        if buf[j] == 101 {
            ei = j;
            j = n;
        } else {
            j = j + 1;
        }
    }
    var neg = false;
    var dn: i64 = 0;
    j = 0;
    while j < ei {
        let c = buf[j] as i64;

        if c == 45 {
            neg = true;
        } else if c != 46 {
            if dn < dg.len {
                dg[dn] = buf[j];
            }
            dn = dn + 1;
        }
        j = j + 1;
    }
    var exp: i64 = 0;
    var esign: i64 = 1;
    j = ei + 1;

    if j < n && buf[j] == 45 {
        esign = 0 - 1;
        j = j + 1;
    } else if j < n && buf[j] == 43 {
        j = j + 1;
    }
    while j < n {
        exp = exp * 10 + ((buf[j] as i64) - 48);
        j = j + 1;
    }
    exp = exp * esign;
    return dn, exp, neg;
}

// repr() of a float, server-decision.cpp:609
fn dc_py_float(dst: u8[:], at: i64, v: f64) -> i64 {
    var dg: u8[32] = [0; 32];
    let dn, exp, neg = dc_sci(v, dg);
    var p = at;

    if neg {
        p = dc_putc(dst, p, 45);
    }
    if exp < 0 - 4 || exp >= 16 {
        p = dc_putc(dst, p, dg[0]);
        if dn > 1 {
            p = dc_putc(dst, p, 46);
            p = dc_put(dst, p, dg, 1, dn - 1);
        }
        p = dc_putc(dst, p, 101);
        if exp < 0 {
            p = dc_putc(dst, p, 45);
        } else {
            p = dc_putc(dst, p, 43);
        }
        var a = exp;

        if a < 0 {
            a = 0 - a;
        }
        if a < 10 {
            p = dc_putc(dst, p, 48);
        }
        return dc_puti(dst, p, a);
    }
    if exp >= dn - 1 {
        p = dc_put(dst, p, dg, 0, dn);
        var z: i64 = exp - (dn - 1);

        while z > 0 {
            p = dc_putc(dst, p, 48);
            z = z - 1;
        }
        p = dc_putc(dst, p, 46);
        return dc_putc(dst, p, 48);
    }
    if exp >= 0 {
        p = dc_put(dst, p, dg, 0, exp + 1);
        p = dc_putc(dst, p, 46);
        return dc_put(dst, p, dg, exp + 1, dn - exp - 1);
    }
    p = dc_putc(dst, p, 48);
    p = dc_putc(dst, p, 46);
    var z2: i64 = 0 - exp - 1;

    while z2 > 0 {
        p = dc_putc(dst, p, 48);
        z2 = z2 - 1;
    }
    return dc_put(dst, p, dg, 0, dn);
}

// json::dump() of a double, the nlohmann layout (json.hpp:18610)
fn dc_nl_float(dst: u8[:], at: i64, v: f64) -> i64 {
    var dg: u8[32] = [0; 32];
    let dn, exp, neg = dc_sci(v, dg);
    // value = 0.d1d2... * 10^(exp + 1), the decimal point sits at exp + 1
    let n = exp + 1;
    var p = at;

    if neg {
        p = dc_putc(dst, p, 45);
    }
    if dn <= n && n <= 15 {
        p = dc_put(dst, p, dg, 0, dn);
        var z: i64 = n - dn;

        while z > 0 {
            p = dc_putc(dst, p, 48);
            z = z - 1;
        }
        p = dc_putc(dst, p, 46);
        return dc_putc(dst, p, 48);
    }
    if 0 < n && n <= 15 {
        p = dc_put(dst, p, dg, 0, n);
        p = dc_putc(dst, p, 46);
        return dc_put(dst, p, dg, n, dn - n);
    }
    if 0 - 4 < n && n <= 0 {
        p = dc_putc(dst, p, 48);
        p = dc_putc(dst, p, 46);
        var z2: i64 = 0 - n;

        while z2 > 0 {
            p = dc_putc(dst, p, 48);
            z2 = z2 - 1;
        }
        return dc_put(dst, p, dg, 0, dn);
    }
    p = dc_putc(dst, p, dg[0]);
    if dn > 1 {
        p = dc_putc(dst, p, 46);
        p = dc_put(dst, p, dg, 1, dn - 1);
    }
    p = dc_putc(dst, p, 101);
    var ee = n - 1;

    if ee < 0 {
        p = dc_putc(dst, p, 45);
    } else {
        p = dc_putc(dst, p, 43);
    }
    if ee < 0 {
        ee = 0 - ee;
    }
    if ee < 10 {
        p = dc_putc(dst, p, 48);
    }
    return dc_puti(dst, p, ee);
}

// ── strings ─────────────────────────────────────────────────────────────────

fn dc_uhex4(dst: u8[:], at: i64, cp: i64) -> i64 {
    let d: const u8[:] = "0123456789abcdef";
    var p = dc_putc(dst, at, 92);
    p = dc_putc(dst, p, 117);
    p = dc_putc(dst, p, d[(cp / 4096) % 16]);
    p = dc_putc(dst, p, d[(cp / 256) % 16]);
    p = dc_putc(dst, p, d[(cp / 16) % 16]);
    return dc_putc(dst, p, d[cp % 16]);
}

// decision_py_escape_cp, server-decision.cpp:600
fn dc_py_escape_cp(dst: u8[:], at: i64, cp: i64) -> i64 {
    if cp < 0x10000 {
        return dc_uhex4(dst, at, cp);
    }
    let x = cp - 0x10000;
    var p = dc_uhex4(dst, at, 0xd800 + (x >> 10));
    return dc_uhex4(dst, p, 0xdc00 + (x & 0x3ff));
}

// decision_py_str, server-decision.cpp:643
fn dc_py_str(dst: u8[:], at: i64, s: const u8[:], ensure: i64) -> i64 {
    var p = dc_putc(dst, at, 34);
    var i: i64 = 0;

    while i < s.len {
        let c = s[i] as i64;

        if c == 34 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 34);
            i = i + 1;
        } else if c == 92 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 92);
            i = i + 1;
        } else if c == 8 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 98);
            i = i + 1;
        } else if c == 12 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 102);
            i = i + 1;
        } else if c == 10 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 110);
            i = i + 1;
        } else if c == 13 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 114);
            i = i + 1;
        } else if c == 9 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 116);
            i = i + 1;
        } else if c < 0x20 || (ensure != 0 && c == 0x7f) {
            p = dc_uhex4(dst, p, c);
            i = i + 1;
        } else if c < 0x80 || ensure == 0 {
            p = dc_putc(dst, p, s[i]);
            i = i + 1;
        } else {
            let cp, next = dc_utf8_next(s, i);

            p = dc_py_escape_cp(dst, p, cp);
            i = next;
        }
    }
    return dc_putc(dst, p, 34);
}

// nlohmann's string escaping with ensure_ascii off
fn dc_nl_str(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = dc_putc(dst, at, 34);
    var i: i64 = 0;

    while i < s.len {
        let c = s[i] as i64;

        if c == 34 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 34);
        } else if c == 92 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 92);
        } else if c == 8 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 98);
        } else if c == 12 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 102);
        } else if c == 10 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 110);
        } else if c == 13 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 114);
        } else if c == 9 {
            p = dc_putc(dst, p, 92);
            p = dc_putc(dst, p, 116);
        } else if c < 0x20 {
            p = dc_uhex4(dst, p, c);
        } else {
            p = dc_putc(dst, p, s[i]);
        }
        i = i + 1;
    }
    return dc_putc(dst, p, 34);
}

fn dc_find_from(s: const u8[:], from: i64, pat: const u8[:]) -> i64 {
    if pat.len == 0 || from < 0 {
        return 0 - 1;
    }
    var i = from;

    while i + pat.len <= s.len {
        if dc_bytes_eq(s, i, pat.len, pat) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn dc_find_char(s: const u8[:], from: i64, c: u8) -> i64 {
    var i = from;

    while i < s.len {
        if s[i] == c {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// string_replace_all
fn dc_replace_all(dst: u8[:], at: i64, s: const u8[:], search: const u8[:], repl: const u8[:]) -> i64 {
    if search.len == 0 {
        return dc_put(dst, at, s, 0, s.len);
    }
    var p = at;
    var done: i64 = 0;
    var i: i64 = 0;
    var found = true;

    while found {
        let hit = dc_find_from(s, i, search);

        if hit < 0 {
            found = false;
        } else {
            p = dc_put(dst, p, s, done, hit - done);
            p = dc_put(dst, p, repl, 0, repl.len);
            done = hit + search.len;
            i = done;
        }
    }
    return dc_put(dst, p, s, done, s.len - done);
}

fn dc_is_name(c: u8) -> bool {
    return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || (c >= 48 && c <= 57) || c == 95;
}

// server_decision_escape_special_tokens, server-decision.cpp:499
fn dc_escape_tok(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var n_done: i64 = 0;
    var i = dc_find_from(s, 0, "<|");
    var p = at;

    while i >= 0 {
        var j = i + 2;

        while j < s.len && dc_is_name(s[j]) {
            j = j + 1;
        }
        if j > i + 2 && j + 1 < s.len && s[j] == 124 && s[j + 1] == 62 {
            p = dc_put(dst, p, s, n_done, i - n_done);
            p = dc_putc(dst, p, 60);
            p = dc_putc(dst, p, 194);
            p = dc_putc(dst, p, 166);
            p = dc_put(dst, p, s, i + 2, j - i - 2);
            p = dc_putc(dst, p, 194);
            p = dc_putc(dst, p, 166);
            p = dc_putc(dst, p, 62);
            n_done = j + 2;
        }
        i = dc_find_from(s, dc_max(i + 1, n_done), "<|");
    }
    if n_done == 0 {
        return dc_put(dst, p, s, 0, s.len);
    }
    return dc_put(dst, p, s, n_done, s.len - n_done);
}

// ── dumps ───────────────────────────────────────────────────────────────────

fn dc_nl_scalar(t: const DTree, n: i64, dst: u8[:], at: i64) -> i64 {
    let k = t.jk[n];

    if k == DC_NULL {
        return dc_puts(dst, at, "null");
    }
    if k == DC_BOOL {
        if t.jv[n] != 0 {
            return dc_puts(dst, at, "true");
        }
        return dc_puts(dst, at, "false");
    }
    if k == DC_FLOAT {
        return dc_nl_float(dst, at, t.jd[n]);
    }
    return dc_puti(dst, at, t.jv[n]);
}

// json::dump() of a whole value: compact, object keys sorted
recursive fn dc_nl_dump(t: const DTree, n: i64, dst: u8[:], at: i64) -> i64 {
    let k = t.jk[n];

    if k == DC_ARRAY {
        let cnt = t.jn[n];
        var p = dc_putc(dst, at, 91);
        var i: i64 = 0;

        while i < cnt {
            if i > 0 {
                p = dc_putc(dst, p, 44);
            }
            p = dc_nl_dump(t, dc_elem(t, n, i), dst, p);
            i = i + 1;
        }
        return dc_putc(dst, p, 93);
    }
    if k == DC_OBJECT {
        let cnt = t.jn[n];
        var ord: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
        var i: i64 = 0;

        while i < cnt {
            ord[i] = i;
            i = i + 1;
        }
        i = 1;
        while i < cnt {
            var j = i;

            while j > 0 && dc_bytes_less(t.sbuf, dc_key_off(t, n, ord[j]), dc_key_len(t, n, ord[j]),
                                         dc_key_off(t, n, ord[j - 1]), dc_key_len(t, n, ord[j - 1])) {
                let tmp = ord[j];
                ord[j] = ord[j - 1];
                ord[j - 1] = tmp;
                j = j - 1;
            }
            i = i + 1;
        }
        var p = dc_putc(dst, at, 123);
        i = 0;
        while i < cnt {
            let e = ord[i];

            if i > 0 {
                p = dc_putc(dst, p, 44);
            }
            p = dc_nl_str(dst, p, t.sbuf[dc_key_off(t, n, e) .. dc_key_off(t, n, e) + dc_key_len(t, n, e)]);
            p = dc_putc(dst, p, 58);
            p = dc_nl_dump(t, t.jchild[t.jf[n] + e], dst, p);
            i = i + 1;
        }
        return dc_putc(dst, p, 125);
    }
    if k == DC_STRING {
        return dc_nl_str(dst, at, t.sbuf[t.js[n] .. t.js[n] + t.jl[n]]);
    }
    return dc_nl_scalar(t, n, dst, at);
}

// The separators follow indent: "," and ":" below zero, ", " and ": " from
// zero, and the same compact pair with spaces at -2 (the h2o descriptions).
fn dc_item_sep(indent: i64) -> const u8[:] {
    if indent == 0 - 2 {
        return ", ";
    }
    return ",";
}

fn dc_key_sep(indent: i64) -> const u8[:] {
    if indent == 0 - 1 {
        return ":";
    }
    return ": ";
}

fn dc_nl(dst: u8[:], at: i64, indent: i64, level: i64) -> i64 {
    if indent < 0 {
        return at;
    }
    var p = dc_putc(dst, at, 10);
    var k: i64 = 0;

    while k < indent * level {
        p = dc_putc(dst, p, 32);
        k = k + 1;
    }
    return p;
}

// One value in the Python layout. mode 0 leaves strings alone, 1 replaces text
// (decision_replace_text), 2 escapes special tokens in strings and keys
// (decision_d1omni_escape), 3 sorts the keys of every object
// (decision_sort_keys). The separators follow indent: "," and ":" below zero,
// ", " and ": " from zero.
recursive fn dc_py_node(t: const DTree, n: i64, indent: i64, level: i64, ensure: i64, mode: i64,
              search: const u8[:], repl: const u8[:], dst: u8[:], at: i64) -> i64 {
    let k = t.jk[n];

    if k == DC_NULL {
        return dc_puts(dst, at, "null");
    }
    if k == DC_BOOL {
        if t.jv[n] != 0 {
            return dc_puts(dst, at, "true");
        }
        return dc_puts(dst, at, "false");
    }
    if k == DC_FLOAT {
        return dc_py_float(dst, at, t.jd[n]);
    }
    if k == DC_STRING {
        return dc_py_str_value(t, n, ensure, mode, search, repl, dst, at);
    }
    if k != DC_ARRAY && k != DC_OBJECT {
        return dc_puti(dst, at, t.jv[n]);
    }
    let cnt = t.jn[n];

    if k == DC_ARRAY {
        if cnt == 0 {
            return dc_puts(dst, at, "[]");
        }
        var p = dc_putc(dst, at, 91);
        var i: i64 = 0;

        while i < cnt {
            if i > 0 {
                p = dc_puts(dst, p, dc_item_sep(indent));
            }
            p = dc_nl(dst, p, indent, level + 1);
            p = dc_py_node(t, dc_elem(t, n, i), indent, level + 1, ensure, mode, search, repl, dst, p);
            i = i + 1;
        }
        p = dc_nl(dst, p, indent, level);
        return dc_putc(dst, p, 93);
    }
    if cnt == 0 {
        return dc_puts(dst, at, "{}");
    }
    var ord: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var i2: i64 = 0;

    while i2 < cnt {
        ord[i2] = i2;
        i2 = i2 + 1;
    }
    if mode == 3 {
        i2 = 1;
        while i2 < cnt {
            var j = i2;

            while j > 0 && dc_bytes_less(t.sbuf, dc_key_off(t, n, ord[j]), dc_key_len(t, n, ord[j]),
                                         dc_key_off(t, n, ord[j - 1]), dc_key_len(t, n, ord[j - 1])) {
                let tmp = ord[j];
                ord[j] = ord[j - 1];
                ord[j - 1] = tmp;
                j = j - 1;
            }
            i2 = i2 + 1;
        }
    }
    var p2 = dc_putc(dst, at, 123);
    i2 = 0;
    while i2 < cnt {
        let e = ord[i2];

        if i2 > 0 {
            p2 = dc_puts(dst, p2, dc_item_sep(indent));
        }
        p2 = dc_nl(dst, p2, indent, level + 1);
        p2 = dc_py_str_key(t, n, e, ensure, mode, dst, p2);
        p2 = dc_puts(dst, p2, dc_key_sep(indent));
        p2 = dc_py_node(t, t.jchild[t.jf[n] + e], indent, level + 1, ensure, mode, search, repl, dst, p2);
        i2 = i2 + 1;
    }
    p2 = dc_nl(dst, p2, indent, level);
    return dc_putc(dst, p2, 125);
}

fn dc_py_str_value(t: const DTree, n: i64, ensure: i64, mode: i64,
                   search: const u8[:], repl: const u8[:], dst: u8[:], at: i64) -> i64 {
    let off = t.js[n];
    let len = t.jl[n];

    if mode == 0 {
        return dc_py_str(dst, at, t.sbuf[off .. off + len], ensure);
    }
    var buf: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var m: i64 = 0;

    if mode == 1 {
        m = dc_replace_all(buf, 0, t.sbuf[off .. off + len], search, repl);
    } else if mode == 2 {
        m = dc_escape_tok(buf, 0, t.sbuf[off .. off + len]);
    } else {
        return dc_py_str(dst, at, t.sbuf[off .. off + len], ensure);
    }
    return dc_py_str(dst, at, buf[0 .. m], ensure);
}

fn dc_py_str_key(t: const DTree, n: i64, e: i64, ensure: i64, mode: i64, dst: u8[:], at: i64) -> i64 {
    let off = dc_key_off(t, n, e);
    let len = dc_key_len(t, n, e);

    if mode != 2 {
        return dc_py_str(dst, at, t.sbuf[off .. off + len], ensure);
    }
    var buf: u8[DC_KEYBUF] = [0; DC_KEYBUF];
    let m = dc_escape_tok(buf, 0, t.sbuf[off .. off + len]);

    return dc_py_str(dst, at, buf[0 .. m], ensure);
}

// decision_kev_render, server-decision.cpp:467
recursive fn dc_kev_node(t: const DTree, n: i64, indent: i64, dst: u8[:], at: i64) -> i64 {
    let k = t.jk[n];

    if k == DC_NULL {
        return at;
    }
    if k == DC_STRING {
        return dc_put(dst, at, t.sbuf, t.js[n], t.jl[n]);
    }
    if k == DC_BOOL {
        if t.jv[n] != 0 {
            return dc_puts(dst, at, "True");
        }
        return dc_puts(dst, at, "False");
    }
    if k == DC_ARRAY {
        let cnt = t.jn[n];
        var p = at;
        var i: i64 = 0;

        while i < cnt {
            if i > 0 {
                p = dc_putc(dst, p, 10);
            }
            p = dc_pad(dst, p, 2 * indent);
            p = dc_puts(dst, p, "- ");
            var buf: u8[DC_SCRATCH] = [0; DC_SCRATCH];
            let m = dc_kev_node(t, dc_elem(t, n, i), indent + 1, buf, 0);
            var q: i64 = 0;

            while q < m && (buf[q] == 32 || buf[q] == 9 || buf[q] == 10 || buf[q] == 13) {
                q = q + 1;
            }
            p = dc_put(dst, p, buf, q, m - q);
            i = i + 1;
        }
        return p;
    }
    if k == DC_OBJECT {
        let cnt = t.jn[n];
        var p = at;
        var i: i64 = 0;

        while i < cnt {
            let child = t.jchild[t.jf[n] + i];
            let ko = t.jkey[t.jf[n] + i];
            let kl = t.jkl[t.jf[n] + i];
            let nested = t.jk[child] == DC_OBJECT || t.jk[child] == DC_ARRAY;

            if i > 0 {
                p = dc_putc(dst, p, 10);
            }
            p = dc_pad(dst, p, 2 * indent);
            p = dc_put(dst, p, t.sbuf, ko, kl);
            p = dc_putc(dst, p, 58);
            if nested {
                p = dc_putc(dst, p, 10);
                p = dc_kev_node(t, child, indent + 1, dst, p);
            } else {
                p = dc_putc(dst, p, 32);
                p = dc_kev_node(t, child, 0, dst, p);
            }
            i = i + 1;
        }
        return p;
    }
    return dc_nl_scalar(t, n, dst, at);
}

// ── question parsing (server-decision.cpp:220-329) ──────────────────────────

// 0 choice, 1 score, 2 noul, 3 a name that is none of them
fn dc_tname(s: const u8[:], off: i64, len: i64) -> i64 {
    if off >= 0 {
        if dc_bytes_eq(s, off, len, "choice") {
            return 0;
        }
        if dc_bytes_eq(s, off, len, "score") {
            return 1;
        }
        if dc_bytes_eq(s, off, len, "noul") {
            return 2;
        }
    }
    return 3;
}

// The normalised questions of a request. qinstr holds the instructions node,
// -1 for null, -2 for the empty string an h2o question defaults to. The option
// list is flat: option o of question q is qfirst[q] .. qfirst[q] + qcount[q],
// its key at keybuf[koff[o] .. koff[o] + klen[o]], its description node in
// kdesc[o] (-1 for null). Returns the question count, or -1 with the message in
// msg.
fn dc_qparse(t: const DTree, typ: i64, strict: i64, csort: i64, ntf: i64, nco: i64, nomax: i64,
             qtype: i64[:], qinstr: i64[:], qfirst: i64[:], qcount: i64[:],
             koff: i64[:], klen: i64[:], kdesc: i64[:], keybuf: u8[:], kb: i64[:],
             msg: u8[:]) -> i64 {
    let root: i64 = 0;
    let st = dc_get(t, root, "state");

    if st < 0 {
        dc_puts(msg, 0, "\"state\" must be provided");
        return 0 - 1;
    }
    if t.jk[st] == DC_NULL && typ != DC_T_D1 && typ != DC_T_D1_OMNI && typ != DC_T_VLLM3 {
        dc_puts(msg, 0, "\"state\" must be provided");
        return 0 - 1;
    }
    if strict != 0 && !dc_is_payload(t, st) {
        dc_puts(msg, 0, "\"state\" must be text, an object or an array");
        return 0 - 1;
    }
    let qs = dc_get(t, root, "questions");

    if qs < 0 || t.jk[qs] != DC_OBJECT || t.jn[qs] == 0 {
        dc_puts(msg, 0, "\"questions\" must be a non-empty object");
        return 0 - 1;
    }
    let nq = t.jn[qs];

    if nq > DC_MAX_Q {
        dc_puts(msg, 0, "\"questions\" has too many entries");
        return 0 - 1;
    }
    var nopt: i64 = 0;
    var qi: i64 = 0;

    while qi < nq {
        let q = t.jchild[t.jf[qs] + qi];
        let ido = t.jkey[t.jf[qs] + qi];
        let idl = t.jkl[t.jf[qs] + qi];
        var mp: i64 = 0;

        mp = dc_puts(msg, 0, "questions.");
        mp = dc_put(msg, mp, t.sbuf, ido, idl);
        mp = dc_puts(msg, mp, ": ");

        if t.jk[q] != DC_OBJECT {
            dc_puts(msg, mp, "must be an object");
            return 0 - 1;
        }
        let is_h2o = typ == DC_T_H2O;
        // instructions
        let inode = dc_get(t, q, "instructions");
        var instr: i64 = inode;
        var is_null = false;

        if inode < 0 {
            is_null = true;
            if is_h2o {
                instr = 0 - 2;
            }
        } else if t.jk[inode] == DC_NULL {
            is_null = true;
        }
        if is_null && typ != DC_T_VLLM3 && !is_h2o {
            dc_puts(msg, mp, "\"instructions\" must be provided");
            return 0 - 1;
        }
        if strict != 0 {
            if is_null || instr == 0 - 2 {
                dc_puts(msg, mp, "\"instructions\" must be non-empty text, an object or an array");
                return 0 - 1;
            }
            if t.jk[instr] == DC_STRING && t.jl[instr] == 0 {
                dc_puts(msg, mp, "\"instructions\" must be non-empty text, an object or an array");
                return 0 - 1;
            }
            if !dc_is_payload(t, instr) {
                dc_puts(msg, mp, "\"instructions\" must be non-empty text, an object or an array");
                return 0 - 1;
            }
        }
        // type name, the default is "choice" for h2o and "" otherwise
        let tnode = dc_get(t, q, "type");
        var tkind: i64 = 0;

        if tnode >= 0 && t.jk[tnode] == DC_STRING {
            tkind = dc_tname(t.sbuf, t.js[tnode], t.jl[tnode]);
        } else if is_h2o {
            // the vllm-d3 prompt has a default question, h2o defaults to choice
            tkind = 0;
        } else {
            tkind = 3;
        }
        if tkind > 2 {
            dc_puts(msg, mp, "\"type\" must be one of: choice, score, noul");
            return 0 - 1;
        }
        qtype[qi] = tkind;
        qinstr[qi] = instr;
        qfirst[qi] = nopt;
        let crit = dc_get(t, q, "criteria");
        var n0 = nopt;

        if tkind == 0 {
            if crit < 0 || t.jk[crit] != DC_OBJECT || t.jn[crit] == 0 {
                dc_puts(msg, mp, "\"criteria\" must be a non-empty object");
                return 0 - 1;
            }
            let cnt = t.jn[crit];
            let base = t.jf[crit];
            var i: i64 = 0;

            while i < cnt {
                let ko = t.jkey[base + i];
                let kl = t.jkl[base + i];
                let d = t.jchild[base + i];

                if strict != 0 && (kl == 0 || !(t.jk[d] == DC_NULL || dc_is_payload(t, d))) {
                    dc_puts(msg, mp, "option keys must be non-empty, descriptions must be null, text, an object or an array");
                    return 0 - 1;
                }
                if nopt >= DC_MAX_OPT {
                    return 0 - 1;
                }
                koff[nopt] = kb[0];
                klen[nopt] = kl;
                kb[0] = dc_put(keybuf, kb[0], t.sbuf, ko, kl);
                kdesc[nopt] = d;
                nopt = nopt + 1;
                i = i + 1;
            }
            if strict != 0 && nopt - n0 < 2 {
                dc_puts(msg, mp, "\"criteria\" must have at least 2 options");
                return 0 - 1;
            }
            if csort != 0 {
                var a: i64 = n0 + 1;

                while a < nopt {
                    var b = a;

                    while b > n0 && dc_less_k(keybuf, koff[b], klen[b], koff[b - 1], klen[b - 1]) {
                        let tk = koff[b];
                        koff[b] = koff[b - 1];
                        koff[b - 1] = tk;
                        let tl = klen[b];
                        klen[b] = klen[b - 1];
                        klen[b - 1] = tl;
                        let td = kdesc[b];
                        kdesc[b] = kdesc[b - 1];
                        kdesc[b - 1] = td;
                        b = b - 1;
                    }
                    a = a + 1;
                }
            }
        } else if tkind == 1 {
            if is_h2o && (crit < 0 || t.jk[crit] != DC_ARRAY || t.jn[crit] == 0) {
                dc_puts(msg, mp, "\"criteria\" must be a non-empty array");
                return 0 - 1;
            }
            if !is_h2o && (crit < 0 || t.jk[crit] != DC_ARRAY || t.jn[crit] < 2 || t.jn[crit] > 10) {
                dc_puts(msg, mp, "\"criteria\" must be an array of 2 to 10 levels");
                return 0 - 1;
            }
            let cnt = t.jn[crit];
            var i: i64 = 0;

            while i < cnt {
                let d = t.jchild[t.jf[crit] + i];

                if strict != 0 && !dc_is_payload(t, d) {
                    dc_puts(msg, mp, "level descriptions must be text, an object or an array");
                    return 0 - 1;
                }
                if nopt >= DC_MAX_OPT {
                    return 0 - 1;
                }
                koff[nopt] = kb[0];
                kb[0] = dc_puti(keybuf, kb[0], i);
                klen[nopt] = kb[0] - koff[nopt];
                kdesc[nopt] = d;
                nopt = nopt + 1;
                i = i + 1;
            }
        } else {
            if crit >= 0 && t.jk[crit] != DC_NULL && t.jk[crit] != DC_OBJECT {
                dc_puts(msg, mp, "\"criteria\" must be an object");
                return 0 - 1;
            }
            let has_crit = crit >= 0 && t.jk[crit] == DC_OBJECT;

            if strict != 0 && has_crit {
                let cnt = t.jn[crit];
                var i: i64 = 0;

                while i < cnt {
                    let ko = t.jkey[t.jf[crit] + i];
                    let kl = t.jkl[t.jf[crit] + i];
                    let d = t.jchild[t.jf[crit] + i];

                    if !((dc_bytes_eq(t.sbuf, ko, kl, "false") || dc_bytes_eq(t.sbuf, ko, kl, "true")) &&
                         dc_is_payload(t, d)) {
                        dc_puts(msg, mp, "\"criteria\" may only describe \"false\" and \"true\" with text, an object or an array");
                        return 0 - 1;
                    }
                    i = i + 1;
                }
            }
            // false, then true
            var side: i64 = 0;

            while side < 2 {
                let name: const u8[:] = if side == 0 { "false" } else { "true" };
                var d: i64 = 0 - 1;

                if has_crit {
                    let hit = dc_get(t, crit, name);

                    if hit >= 0 {
                        d = hit;
                    } else if typ == DC_T_D1_OMNI || is_h2o {
                        let alias: const u8[:] = if side == 0 { "no" } else { "yes" };
                        let alt = dc_get(t, crit, alias);

                        if alt >= 0 {
                            d = alt;
                        }
                    }
                }
                if nopt >= DC_MAX_OPT {
                    return 0 - 1;
                }
                koff[nopt] = kb[0];
                klen[nopt] = name.len;
                kb[0] = dc_put(keybuf, kb[0], name, 0, name.len);
                kdesc[nopt] = d;
                nopt = nopt + 1;
                side = side + 1;
            }
            var swap = ntf != 0;

            if !swap && nco != 0 && has_crit && t.jn[crit] == 2 {
                if dc_bytes_eq(t.sbuf, dc_key_off(t, crit, 0), dc_key_len(t, crit, 0), "true") {
                    swap = true;
                }
            }
            if swap {
                let a0 = nopt - 2;
                let a1 = nopt - 1;
                let tk = koff[a0];
                koff[a0] = koff[a1];
                koff[a1] = tk;
                let tl = klen[a0];
                klen[a0] = klen[a1];
                klen[a1] = tl;
                let td = kdesc[a0];
                kdesc[a0] = kdesc[a1];
                kdesc[a1] = td;
            }
        }
        qcount[qi] = nopt - qfirst[qi];
        if qcount[qi] > nomax {
            var e: i64 = 0;

            e = dc_puts(msg, mp, "too many options (");
            e = dc_puti(msg, e, qcount[qi]);
            e = dc_puts(msg, e, "), this model supports at most ");
            e = dc_puti(msg, e, nomax);
            return 0 - 1;
        }
        qi = qi + 1;
    }
    return nq;
}

fn dc_less_k(kb: u8[:], ao: i64, al: i64, bo: i64, bl: i64) -> bool {
    var i: i64 = 0;

    while i < al && i < bl {
        if kb[ao + i] != kb[bo + i] {
            return kb[ao + i] < kb[bo + i];
        }
        i = i + 1;
    }
    return al < bl;
}

fn dc_qname(kind: i64) -> const u8[:] {
    if kind == DC_Q_CHOICE {
        return "choice";
    }
    if kind == DC_Q_SCORE {
        return "score";
    }
    return "noul";
}

// The canonical text of the parsed questions: an array of
// {"id","type","instructions","options":[{"key","description"}]}.
fn dc_qdump(t: const DTree, nq: i64, qid_off: i64[:], qid_len: i64[:],
            qtype: i64[:], qinstr: i64[:], qfirst: i64[:], qcount: i64[:],
            koff: i64[:], klen: i64[:], kdesc: i64[:], keybuf: u8[:],
            dst: u8[:], at: i64) -> i64 {
    var p = dc_putc(dst, at, 91);
    var q: i64 = 0;

    while q < nq {
        if q > 0 {
            p = dc_putc(dst, p, 44);
        }
        p = dc_puts(dst, p, "{\"id\":");
        p = dc_py_str(dst, p, t.sbuf[qid_off[q] .. qid_off[q] + qid_len[q]], 0);
        p = dc_puts(dst, p, ",\"type\":");
        p = dc_py_str(dst, p, dc_qname(qtype[q]), 0);
        p = dc_puts(dst, p, ",\"instructions\":");
        if qinstr[q] == 0 - 1 {
            p = dc_puts(dst, p, "null");
        } else if qinstr[q] == 0 - 2 {
            p = dc_puts(dst, p, "\"\"");
        } else {
            p = dc_py_node(t, qinstr[q], 0 - 1, 0, 0, 0, "", "", dst, p);
        }
        p = dc_puts(dst, p, ",\"options\":[");
        var o: i64 = qfirst[q];
        let end = qfirst[q] + qcount[q];

        while o < end {
            if o > qfirst[q] {
                p = dc_putc(dst, p, 44);
            }
            p = dc_puts(dst, p, "{\"key\":");
            p = dc_py_str(dst, p, keybuf[koff[o] .. koff[o] + klen[o]], 0);
            p = dc_puts(dst, p, ",\"description\":");
            if kdesc[o] < 0 {
                p = dc_puts(dst, p, "null");
            } else {
                p = dc_py_node(t, kdesc[o], 0 - 1, 0, 0, 0, "", "", dst, p);
            }
            p = dc_putc(dst, p, 125);
            o = o + 1;
        }
        p = dc_puts(dst, p, "]}");
        q = q + 1;
    }
    return dc_putc(dst, p, 93);
}

// ── exports: dumps and transforms ───────────────────────────────────────────

export fn dc_py_float_of(v: f64, out: u8[:]) -> i64 {
    return dc_py_float(out, 0, v);
}

export fn dc_py_dumps(text: const u8[:], indent: i64, ensure: i64, out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    return dc_py_node(t, root, indent, 0, ensure, 0, "", "", out, 0);
}

export fn dc_py_str_of(text: const u8[:], ensure: i64, out: u8[:]) -> i64 {
    return dc_py_str(out, 0, text, ensure);
}

export fn dc_py_space(text: const u8[:], out: u8[:]) -> i64 {
    var p = dc_py_str(out, 0, text[dc_py_lstrip(text) .. text.len], 0);

    p = dc_putc(out, p, 10);
    return dc_py_str(out, p, text[0 .. dc_py_rstrip(text)], 0);
}

export fn dc_escape_tokens(text: const u8[:], out: u8[:]) -> i64 {
    return dc_escape_tok(out, 0, text);
}

fn dc_h2o_cut_impl(text: const u8[:], max_tokens: i64, out: u8[:]) -> i64 {
    if text.len <= max_tokens {
        return dc_put(out, 0, text, 0, text.len);
    }
    // one token per byte: a byte-level BPE never has more tokens than bytes, and
    // the tokenizer itself stays native (D6)
    let half = max_tokens / 2;
    var p = dc_utf8_safe(out, 0, text, 0, half);

    p = dc_putc(out, p, 10);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 46);
    p = dc_putc(out, p, 10);
    return dc_utf8_safe(out, p, text, text.len - half, half);
}

export fn dc_h2o_cut(text: const u8[:], max_tokens: i64, out: u8[:]) -> i64 {
    return dc_h2o_cut_impl(text, max_tokens, out);
}

export fn dc_h2o_quote(text: const u8[:], spell: const u8[:], out: u8[:]) -> i64 {
    return dc_h2o_quote_impl(text, spell, out);
}

export fn dc_kev_render(text: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    return dc_kev_node(t, root, 0, out, 0);
}

export fn dc_kev_text(text: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    var buf: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    let m = dc_kev_node(t, root, 0, buf, 0);

    return dc_escape_tok(out, 0, buf[0 .. m]);
}

export fn dc_replace_text(text: const u8[:], search: const u8[:], repl: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    return dc_py_node(t, root, 0 - 1, 0, 0, 1, search, repl, out, 0);
}

export fn dc_sort_keys(text: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    return dc_py_node(t, root, 0 - 1, 0, 0, 3, "", "", out, 0);
}

export fn dc_d1omni_escape(text: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    return dc_py_node(t, root, 0 - 1, 0, 0, 2, "", "", out, 0);
}

// ── exports: questions and state ────────────────────────────────────────────

export fn dc_parse_questions(body: const u8[:], typ: i64, strict: i64, csort: i64, ntf: i64,
                             nco: i64, nomax: i64, out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    var qtype: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qinstr: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qfirst: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qcount: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qid_off: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qid_len: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var koff: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var klen: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var kdesc: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var kbuf: u8[DC_KEYBUF] = [0; DC_KEYBUF];
    var kb: i64[4] = [0; 4];
    let root = json_parse_abi(body, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        dc_puts(out, 0, "body is not JSON");
        return 0 - 1;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let nq = dc_qparse(t, typ, strict, csort, ntf, nco, nomax, qtype, qinstr, qfirst, qcount,
                       koff, klen, kdesc, kbuf, kb, out);

    if nq < 0 {
        return 0 - 1;
    }
    let qs = dc_get(t, 0, "questions");
    var q: i64 = 0;

    while q < nq {
        qid_off[q] = t.jkey[t.jf[qs] + q];
        qid_len[q] = t.jkl[t.jf[qs] + q];
        q = q + 1;
    }
    return dc_qdump(t, nq, qid_off, qid_len, qtype, qinstr, qfirst, qcount, koff, klen, kdesc, kbuf, out, 0);
}

fn dc_starts_with(s: const u8[:], off: i64, len: i64, p: const u8[:]) -> bool {
    if len < p.len {
        return false;
    }
    return dc_bytes_eq(s, off, p.len, p);
}

export fn dc_parse_state(body: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    var scratch: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var kinds: u8[64] = [0; 64];
    var cnt: i64[4] = [0; 4];
    let root = json_parse_abi(body, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        dc_puts(out, 0, "body is not JSON");
        return 0 - 1;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let vids = dc_get(t, root, "videos");

    if vids >= 0 && t.jk[vids] != DC_NULL && !dc_is_empty(t, vids) {
        dc_puts(out, 0, "\"videos\" is not supported");
        return 0 - 1;
    }
    // "images" is an alias of "files"
    var side: i64 = 0;

    while side < 2 {
        let tile: const u8[:] = if side == 0 { "files" } else { "images" };
        let f = dc_get(t, root, tile);

        if f >= 0 && t.jk[f] != DC_NULL {
            if t.jk[f] != DC_ARRAY {
                var e: i64 = 0;

                e = dc_putc(out, 0, 34);
                e = dc_puts(out, e, tile);
                e = dc_puts(out, e, "\" must be an array");
                return 0 - 1;
            }
            let n = t.jn[f];
            var i: i64 = 0;

            while i < n {
                if dc_media_image(t, dc_elem(t, f, i), cnt, kinds, out) < 0 {
                    return 0 - 1;
                }
                i = i + 1;
            }
        }
        side = side + 1;
    }
    let state = dc_get(t, root, "state");

    if state < 0 {
        dc_puts(out, 0, "\"state\" must be provided");
        return 0 - 1;
    }
    let wrapped = t.jk[state] == DC_OBJECT && dc_has(t, state, "messages");
    let msgs = if wrapped { dc_get(t, state, "messages") } else { state };
    var sn: i64 = 0;

    if t.jk[msgs] != DC_ARRAY {
        sn = dc_py_node(t, state, 0 - 1, 0, 0, 0, "", "", scratch, 0);
        var ph = dc_puts(out, 0, "files=");
        ph = dc_puti(out, ph, cnt[0]);
        ph = dc_puts(out, ph, " kinds=");
        ph = dc_put(out, ph, kinds, 0, cnt[0]);
        ph = dc_putc(out, ph, 10);
        return dc_put(out, ph, scratch, 0, sn);
    } else {
        // every message keeps its own keys, its media parts go to the counters
        sn = dc_putc(scratch, 0, 91);
        var i2: i64 = 0;
        let nmsg = t.jn[msgs];

        while i2 < nmsg {
            if i2 > 0 {
                sn = dc_putc(scratch, sn, 44);
            }
            let m = dc_state_msg(t, dc_elem(t, msgs, i2), scratch, sn, cnt, kinds, out);

            if m < 0 {
                return 0 - 1;
            }
            sn = m;
            i2 = i2 + 1;
        }
        sn = dc_putc(scratch, sn, 93);
    }
    var p = dc_puts(out, 0, "files=");
    p = dc_puti(out, p, cnt[0]);
    p = dc_puts(out, p, " kinds=");
    p = dc_put(out, p, kinds, 0, cnt[0]);
    p = dc_putc(out, p, 10);
    if !wrapped {
        return dc_put(out, p, scratch, 0, sn);
    }
    return dc_state_wrapped(t, state, scratch[0 .. sn], out, p);
}

// One image or audio item: a data URL, at most eight per request. The bytes
// stay in S5, the port only counts and checks them.
fn dc_media_image(t: const DTree, u: i64, cnt: i64[:], kinds: u8[:], err: u8[:]) -> i64 {
    if u < 0 || t.jk[u] != DC_STRING || !dc_starts_with(t.sbuf, t.js[u], t.jl[u], "data:") {
        dc_puts(err, 0, "images must be data URLs (data:image/...;base64,... or data:audio/...;base64,...)");
        return 0 - 1;
    }
    if cnt[0] >= 8 {
        dc_puts(err, 0, "too many images, the maximum is 8");
        return 0 - 1;
    }
    if dc_starts_with(t.sbuf, t.js[u], t.jl[u], "data:audio/") {
        kinds[cnt[0]] = 97;
    } else {
        kinds[cnt[0]] = 105;
    }
    cnt[0] = cnt[0] + 1;
    return 0;
}

fn dc_media_audio(t: const DTree, u: i64, cnt: i64[:], kinds: u8[:], err: u8[:]) -> i64 {
    if u < 0 || t.jk[u] != DC_STRING || dc_starts_with(t.sbuf, t.js[u], t.jl[u], "http") ||
       dc_starts_with(t.sbuf, t.js[u], t.jl[u], "file://") {
        dc_puts(err, 0, "audio must be base64 data");
        return 0 - 1;
    }
    if cnt[0] >= 8 {
        dc_puts(err, 0, "too many media files, the maximum is 8");
        return 0 - 1;
    }
    kinds[cnt[0]] = 97;
    cnt[0] = cnt[0] + 1;
    return 0;
}

// One chat message with its media parts taken out. Returns the end offset, or
// -1 with the message in err.
fn dc_state_msg(t: const DTree, msg: i64, dst: u8[:], at: i64, cnt: i64[:], kinds: u8[:], err: u8[:]) -> i64 {
    let content = if t.jk[msg] == DC_OBJECT { dc_get(t, msg, "content") } else { 0 - 1 };

    if t.jk[msg] != DC_OBJECT || content < 0 || t.jk[content] != DC_ARRAY {
        return dc_py_node(t, msg, 0 - 1, 0, 0, 0, "", "", dst, at);
    }
    var p = dc_putc(dst, at, 123);
    let base = t.jf[msg];
    let nm = t.jn[msg];
    var k: i64 = 0;

    while k < nm {
        let ko = t.jkey[base + k];
        let kl = t.jkl[base + k];
        let child = t.jchild[base + k];

        if k > 0 {
            p = dc_putc(dst, p, 44);
        }
        p = dc_py_str(dst, p, t.sbuf[ko .. ko + kl], 0);
        p = dc_putc(dst, p, 58);
        if child != content {
            p = dc_py_node(t, child, 0 - 1, 0, 0, 0, "", "", dst, p);
        } else {
            var c = dc_putc(dst, p, 91);
            var first = true;
            var i: i64 = 0;
            let np = t.jn[content];

            while i < np {
                let part = dc_elem(t, content, i);
                var keep = true;

                if t.jk[part] == DC_OBJECT {
                    let ty = dc_get(t, part, "type");
                    var is_img = false;
                    var is_aud = false;

                    if ty >= 0 && t.jk[ty] == DC_STRING {
                        is_img = dc_bytes_eq(t.sbuf, t.js[ty], t.jl[ty], "image_url") && dc_has(t, part, "image_url");
                        is_aud = dc_bytes_eq(t.sbuf, t.js[ty], t.jl[ty], "input_audio") && dc_has(t, part, "input_audio");
                    }
                    if is_img {
                        let iu = dc_get(t, part, "image_url");
                        var url = iu;

                        if t.jk[iu] == DC_OBJECT && dc_has(t, iu, "url") {
                            url = dc_get(t, iu, "url");
                        }
                        if dc_media_image(t, url, cnt, kinds, err) < 0 {
                            return 0 - 1;
                        }
                        keep = false;
                    } else if is_aud {
                        let ia = dc_get(t, part, "input_audio");
                        var dat = 0 - 1;

                        if t.jk[ia] == DC_OBJECT {
                            if dc_has(t, ia, "data") {
                                dat = dc_get(t, ia, "data");
                            } else if dc_has(t, ia, "url") {
                                dat = dc_get(t, ia, "url");
                            }
                        }
                        if dc_media_audio(t, dat, cnt, kinds, err) < 0 {
                            return 0 - 1;
                        }
                        keep = false;
                    }
                }
                if keep {
                    if !first {
                        c = dc_putc(dst, c, 44);
                    }
                    first = false;
                    c = dc_py_node(t, part, 0 - 1, 0, 0, 0, "", "", dst, c);
                }
                i = i + 1;
            }
            p = dc_putc(dst, c, 93);
        }
        k = k + 1;
    }
    return dc_putc(dst, p, 125);
}

// The state object with its "messages" key holding the given text.
fn dc_state_wrapped(t: const DTree, state: i64, msgs: const u8[:], dst: u8[:], at: i64) -> i64 {
    var p = dc_putc(dst, at, 123);
    let base = t.jf[state];
    let cnt = t.jn[state];
    var i: i64 = 0;

    while i < cnt {
        let ko = t.jkey[base + i];
        let kl = t.jkl[base + i];
        let child = t.jchild[base + i];

        if i > 0 {
            p = dc_putc(dst, p, 44);
        }
        p = dc_py_str(dst, p, t.sbuf[ko .. ko + kl], 0);
        p = dc_putc(dst, p, 58);
        if dc_bytes_eq(t.sbuf, ko, kl, "messages") {
            p = dc_put(dst, p, msgs, 0, msgs.len);
        } else {
            p = dc_py_node(t, child, 0 - 1, 0, 0, 0, "", "", dst, p);
        }
        i = i + 1;
    }
    return dc_putc(dst, p, 125);
}

// ── exports: h2o ────────────────────────────────────────────────────────────

export fn dc_prepare_state(text: const u8[:], typ: i64, spell: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    var cut: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var quoted: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        return 0;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };

    if typ != DC_T_H2O {
        return dc_py_node(t, root, 0 - 1, 0, 0, 0, "", "", out, 0);
    }
    var n: i64 = 0;

    if t.jk[root] == DC_STRING {
        n = dc_put(cut, 0, t.sbuf, t.js[root], t.jl[root]);
    } else {
        n = dc_py_node(t, root, 0 - 1, 0, 0, 0, "", "", cut, 0);
    }
    let cn = dc_h2o_cut_impl(cut[0 .. n], DC_H2O_MAX_STATE_TOKENS, quoted);
    let qn = dc_h2o_quote_impl(quoted[0 .. cn], spell, cut);

    return dc_py_str(out, 0, cut[0 .. qn], 0);
}

// h2o_quote, server-decision.cpp:748: a spelling is quoted with JSON escapes for
// '<' and '>' so it stays text. `spell` holds the spellings, longest first, one
// per line.
fn dc_h2o_quote_impl(text: const u8[:], spell: const u8[:], dst: u8[:]) -> i64 {
    var p = 0;
    var i: i64 = 0;

    while i < text.len {
        var hit = false;

        if !dc_is_first_byte(text[i], spell) {
            p = dc_putc(dst, p, text[i]);
            i = i + 1;
        } else {
            var sp: i64 = 0;
            var stop = false;

            while sp < spell.len && !stop {
                let e = dc_find_char(spell, sp, 10);
                let end = if e < 0 { spell.len } else { e };

                if end > sp && i + (end - sp) <= text.len && dc_bytes_eq(text, i, end - sp, spell[sp .. end]) {
                    var tmp: u8[DC_KEYBUF] = [0; DC_KEYBUF];
                    let m1 = dc_replace_all(tmp, 0, spell[sp .. end], "<", "\\u003c");

                    p = dc_replace_all(dst, p, tmp[0 .. m1], ">", "\\u003e");
                    i = i + (end - sp);
                    hit = true;
                    stop = true;
                } else {
                    sp = end + 1;
                    if e < 0 {
                        stop = true;
                    }
                }
            }
            if !hit {
                p = dc_putc(dst, p, text[i]);
                i = i + 1;
            }
        }
    }
    return p;
}

fn dc_is_first_byte(c: u8, spell: const u8[:]) -> bool {
    var sp: i64 = 0;

    while sp < spell.len {
        if spell[sp] == c {
            return true;
        }
        let e = dc_find_char(spell, sp, 10);
        if e < 0 {
            return false;
        }
        sp = e + 1;
    }
    return false;
}

// render_h2o, server-decision.cpp:782, over the body of the request: the state
// goes through parse_state and prepare_state first, as in the server.
export fn dc_render_h2o(body: const u8[:], typ: i64, strict: i64, csort: i64, ntf: i64,
                        nco: i64, nomax: i64, qi: i64, labels: const u8[:], spell: const u8[:],
                        out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    var qtype: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qinstr: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qfirst: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qcount: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var koff: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var klen: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var kdesc: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var kbuf: u8[DC_KEYBUF] = [0; DC_KEYBUF];
    var kb: i64[4] = [0; 4];
    var statev: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var utxt: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var quoted: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var cut: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var opt: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var dbuf: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var qbuf: u8[DC_KEYBUF] = [0; DC_KEYBUF];
    let root = json_parse_abi(body, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        dc_puts(out, 0, "body is not JSON");
        return 0 - 1;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let nq = dc_qparse(t, typ, strict, csort, ntf, nco, nomax, qtype, qinstr, qfirst, qcount,
                       koff, klen, kdesc, kbuf, kb, out);

    if nq < 0 {
        return 0 - 1;
    }
    if qi < 0 || qi >= nq {
        dc_puts(out, 0, "no such question");
        return 0 - 1;
    }
    // parse_state, then prepare_state
    let state = dc_get(t, root, "state");
    var sn: i64 = 0;

    if typ == DC_T_H2O {
        var n: i64 = 0;

        if t.jk[state] == DC_STRING {
            n = dc_put(cut, 0, t.sbuf, t.js[state], t.jl[state]);
        } else {
            n = dc_py_node(t, state, 0 - 1, 0, 0, 0, "", "", cut, 0);
        }
        let cn = dc_h2o_cut_impl(cut[0 .. n], DC_H2O_MAX_STATE_TOKENS, quoted);
        let qn = dc_h2o_quote_impl(quoted[0 .. cn], spell, cut);

        sn = dc_py_str(statev, 0, cut[0 .. qn], 0);
    } else {
        sn = dc_py_node(t, state, 0 - 1, 0, 0, 0, "", "", statev, 0);
    }
    // the instructions: JSON with an indent of 1, or the text itself
    let instr = qinstr[qi];
    var un: i64 = 0;

    if instr == 0 - 2 {
        un = 0;
    } else if instr < 0 {
        un = dc_puts(utxt, 0, "null");
    } else if t.jk[instr] == DC_STRING {
        un = dc_put(utxt, 0, t.sbuf, t.js[instr], t.jl[instr]);
    } else {
        un = dc_py_node(t, instr, 1, 0, 1, 0, "", "", utxt, 0);
    }
    let lstart = dc_py_lstrip(utxt[0 .. un]);
    let qn2 = dc_h2o_quote_impl(utxt[lstart .. un], spell, quoted);
    let nopt = qcount[qi];
    let qt = qtype[qi];
    // options
    var op: i64 = 0;
    let first = qfirst[qi];

    op = dc_putc(opt, 0, 91);
    var i: i64 = 0;

    while i < nopt {
        let o = first + i;
        // the description is null, the text "null" for a score level, or a
        // quoted text: dsrc says which buffer holds it
        var dkind: i64 = 0;
        var dsrc: i64 = 0;
        var dlen: i64 = 0;

        let isnull = kdesc[o] < 0 || t.jk[kdesc[o]] == DC_NULL;

        if qt == DC_Q_SCORE && isnull {
            dkind = 1;
            dlen = 4;
            dbuf[0] = 110;
            dbuf[1] = 117;
            dbuf[2] = 108;
            dbuf[3] = 108;
        } else if !isnull {
            var m: i64 = 0;

            if t.jk[kdesc[o]] == DC_STRING {
                m = dc_put(dbuf, 0, t.sbuf, t.js[kdesc[o]], t.jl[kdesc[o]]);
            } else {
                m = dc_py_node(t, kdesc[o], 0 - 2, 0, 0, 1, "", "", dbuf, 0);
            }
            dkind = 1;
            var tmp: u8[DC_SCRATCH] = [0; DC_SCRATCH];

            dlen = dc_h2o_quote_impl(dbuf[0 .. m], spell, tmp);
            m = dc_put(dbuf, 0, tmp, 0, dlen);
        }
        // the key, quoted
        let kq = dc_h2o_quote_impl(kbuf[koff[o] .. koff[o] + klen[o]], spell, qbuf);
        // the last option ends the user turn: strip its trailing whitespace
        if i + 1 == nopt {
            let is_key = qt == DC_Q_CHOICE && dkind == 0;

            if is_key {
                let end = dc_py_rstrip(qbuf[0 .. kq]);

                if end != kq {
                    dkind = 1;
                    if end == 0 {
                        dsrc = 0;
                        dbuf[0] = 32;
                        dlen = 1;
                    } else {
                        dsrc = 1;
                        dlen = end;
                    }
                }
            } else if dkind == 1 {
                if dsrc == 0 {
                    let end = dc_py_rstrip(dbuf[0 .. dlen]);

                    if end != dlen {
                        if end == 0 {
                            dbuf[0] = 32;
                            dlen = 1;
                        } else {
                            dlen = end;
                        }
                    }
                } else {
                    let end = dc_py_rstrip(qbuf[0 .. dlen]);

                    if end != dlen {
                        if end == 0 {
                            dsrc = 0;
                            dbuf[0] = 32;
                            dlen = 1;
                        } else {
                            dlen = end;
                        }
                    }
                }
            }
        }
        if i > 0 {
            op = dc_putc(opt, op, 44);
        }
        op = dc_puts(opt, op, "{\"label\":");
        op = dc_py_str(opt, op, dc_label_at(labels, i), 0);
        op = dc_puts(opt, op, ",\"key\":");
        op = dc_py_str(opt, op, qbuf[0 .. kq], 0);
        op = dc_puts(opt, op, ",\"description\":");
        if dkind == 0 {
            op = dc_puts(opt, op, "null");
        } else if dsrc == 0 {
            op = dc_py_str(opt, op, dbuf[0 .. dlen], 0);
        } else {
            op = dc_py_str(opt, op, qbuf[0 .. dlen], 0);
        }
        op = dc_putc(opt, op, 125);
        i = i + 1;
    }
    op = dc_putc(opt, op, 93);
    // the template input object
    var p = dc_puts(out, 0, "{\"type\":");
    p = dc_py_str(out, p, dc_qname(qt), 0);
    p = dc_puts(out, p, ",\"state\":");
    p = dc_put(out, p, statev, 0, sn);
    p = dc_puts(out, p, ",\"instructions\":");
    p = dc_py_str(out, p, quoted[0 .. qn2], 0);
    p = dc_puts(out, p, ",\"options\":");
    p = dc_put(out, p, opt, 0, op);
    return dc_putc(out, p, 125);
}

// The i-th label text of the comma-separated list; "" past its end.
fn dc_label_at(labels: const u8[:], i: i64) -> const u8[:] {
    var pos: i64 = 0;
    var k: i64 = 0;

    while pos <= labels.len {
        let e = dc_find_char(labels, pos, 44);
        let end = if e < 0 { labels.len } else { e };

        if k == i {
            return labels[pos .. end];
        }
        if e < 0 {
            pos = labels.len + 1;
        } else {
            pos = end + 1;
            k = k + 1;
        }
    }
    return labels[labels.len .. labels.len];
}

// ── exports: answer ─────────────────────────────────────────────────────────

export fn dc_n_variants(typ: i64, qt: i64, nopt: i64) -> i64 {
    if typ == DC_T_LEV && qt == DC_Q_CHOICE && nopt > 1 {
        return 2;
    }
    return 1;
}

export fn dc_n_outputs(typ: i64, qt: i64, nopt: i64) -> i64 {
    if typ == DC_T_LEV && qt == DC_Q_NOUL {
        return DC_LEV_N_RATINGS;
    }
    return nopt;
}

// decision_confidence_choice, server-decision.cpp:1444
fn dc_conf_choice(pr: f64[:], n: i64) -> f64 {
    if n < 2 {
        return 1.0;
    }
    let uniform = 1.0 / (n as f64);
    var pmax = pr[0];
    var i: i64 = 1;

    while i < n {
        if pr[i] > pmax {
            pmax = pr[i];
        }
        i = i + 1;
    }
    let v = (pmax - uniform) / (1.0 - uniform);

    if v < 0.0 {
        return 0.0;
    }
    return v;
}

// decision_confidence_score, server-decision.cpp:1453
fn dc_conf_score(pr: f64[:], n: i64) -> f64 {
    if n < 2 {
        return 1.0;
    }
    var mode: i64 = 0;
    var i: i64 = 1;

    while i < n {
        if pr[i] > pr[mode] {
            mode = i;
        }
        i = i + 1;
    }
    var dist = 0.0;
    var dist_u = 0.0;
    i = 0;
    while i < n {
        let d = (i as f64) - (mode as f64);

        if d < 0.0 {
            dist += pr[i] * (0.0 - d);
        } else {
            dist += pr[i] * d;
        }
        let du = (i as f64) - ((n - 1) as f64) / 2.0;

        if du < 0.0 {
            dist_u += (0.0 - du) / (n as f64);
        } else {
            dist_u += du / (n as f64);
        }
        i = i + 1;
    }
    let v = 1.0 - dist / dist_u;

    if v < 0.0 {
        return 0.0;
    }
    return v;
}

// decision_confidence_entropy, server-decision.cpp:1471
fn dc_conf_entropy(pr: f64[:], n: i64) -> f64 {
    if n < 2 {
        return 1.0;
    }
    var h = 0.0;
    var i: i64 = 0;

    while i < n {
        let p = pr[i];

        if p > 0.0 {
            h -= p * dc_log(p);
        }
        i = i + 1;
    }
    var v = 1.0 - h / dc_log(n as f64);

    if v < 0.0 {
        v = 0.0;
    }
    if v > 1.0 {
        v = 1.0;
    }
    return v;
}

// get_decision_confidence, server-decision.cpp:1484
fn dc_confidence_of(typ: i64, qt: i64, pr: f64[:], n: i64) -> f64 {
    if typ == DC_T_VLLM2 || typ == DC_T_VLLM3 {
        return dc_conf_entropy(pr, n);
    }
    if qt == DC_Q_CHOICE {
        return dc_conf_choice(pr, n);
    }
    if qt == DC_Q_SCORE {
        return dc_conf_score(pr, n);
    }
    return 0.0 - 1.0;
}

fn dc_confidence_err(typ: i64, qt: i64) -> bool {
    if typ == DC_T_VLLM2 || typ == DC_T_VLLM3 {
        return false;
    }
    return qt != DC_Q_CHOICE && qt != DC_Q_SCORE;
}

export fn dc_confidence(typ: i64, qt: i64, probs: const u8[:], out: u8[:]) -> i64 {
    var pr: f64[64] = [0.0; 64];
    var n: i64 = 0;
    var pos: i64 = 0;

    while pos <= probs.len && n < 64 {
        let e = dc_find_char(probs, pos, 44);
        let end = if e < 0 { probs.len } else { e };

        if end >= pos && end > pos {
            pr[n] = dc_strtod(probs[pos .. end]);
            n = n + 1;
        }
        if e < 0 {
            pos = probs.len + 1;
        } else {
            pos = end + 1;
        }
    }
    if dc_confidence_err(typ, qt) {
        dc_puts(out, 0, "no confidence for this question type");
        return 0 - 1;
    }
    return dc_py_float(out, 0, dc_confidence_of(typ, qt, pr, n));
}

export fn dc_format_answer(body: const u8[:], typ: i64, strict: i64, csort: i64, ntf: i64,
                           nco: i64, nomax: i64, qi: i64, scores: const u8[:], temp: f64,
                           itemp: f64, has_media: i64, canonical: i64, bias: const u8[:],
                           out: u8[:]) -> i64 {
    var jk: i64[1024] = [0; 1024];
    var jv: i64[1024] = [0; 1024];
    var jd: f64[1024] = [0.0; 1024];
    var js: i64[1024] = [0; 1024];
    var jl: i64[1024] = [0; 1024];
    var jf: i64[1024] = [0; 1024];
    var jn: i64[1024] = [0; 1024];
    var jh: i64[1024] = [0; 1024];
    var ev: i64[1024] = [0; 1024];
    var ek: i64[1024] = [0; 1024];
    var el: i64[1024] = [0; 1024];
    var en: i64[1024] = [0; 1024];
    var sb: u8[16384] = [0; 16384];
    var key: i64[1024] = [0; 1024];
    var kl: i64[1024] = [0; 1024];
    var jc: i64[1024] = [0; 1024];
    var st: i64[16] = [0; 16];
    var qtype: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qinstr: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qfirst: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var qcount: i64[DC_MAX_Q] = [0; DC_MAX_Q];
    var koff: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var klen: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var kdesc: i64[DC_MAX_OPT] = [0; DC_MAX_OPT];
    var kbuf: u8[DC_KEYBUF] = [0; DC_KEYBUF];
    var kb: i64[4] = [0; 4];
    var pr: f64[64] = [0.0; 64];
    let root = json_parse_abi(body, jk, jv, jd, js, jl, jf, jn, jh, ev, ek, el, en, sb, key, kl, jc, st);

    if root < 0 {
        dc_puts(out, 0, "body is not JSON");
        return 0 - 1;
    }
    let t = DTree { jk: jk, jv: jv, jd: jd, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let nq = dc_qparse(t, typ, strict, csort, ntf, nco, nomax, qtype, qinstr, qfirst, qcount,
                       koff, klen, kdesc, kbuf, kb, out);

    if nq < 0 {
        return 0 - 1;
    }
    if qi < 0 || qi >= nq {
        dc_puts(out, 0, "no such question");
        return 0 - 1;
    }
    let qt = qtype[qi];
    let first = qfirst[qi];
    let nopt = qcount[qi];
    let n = dc_n_outputs(typ, qt, nopt);
    let variants = dc_n_variants(typ, qt, nopt);
    // the model temperatures are float
    var temperature = dc_f32(temp);

    if has_media != 0 && typ == DC_T_D1_OMNI {
        temperature = 1.0;
    }
    if has_media != 0 && itemp >= 0.0 {
        temperature = dc_f32(itemp);
    }
    // the variants, separated by ';', the outputs by ','
    var nv: i64 = 0;
    var pos: i64 = 0;
    var declared: i64 = 0;
    while pos <= scores.len {
        let e = dc_find_char(scores, pos, 59);
        let end = if e < 0 { scores.len } else { e };

        if end > pos {
            declared = declared + 1;
        }
        if e < 0 {
            pos = scores.len + 1;
        } else {
            pos = end + 1;
        }
    }
    if declared != variants {
        dc_puts(out, 0, "decision result does not match the number of variants");
        return 0 - 1;
    }
    nv = 0;
    pos = 0;

    while pos <= scores.len && nv < variants {
        let e = dc_find_char(scores, pos, 59);
        let end = if e < 0 { scores.len } else { e };
        var vals: f64[64] = [0.0; 64];
        var got: i64 = 0;
        var q: i64 = pos;

        while q <= end && got < 64 {
            let ce = dc_find_char(scores[0 .. end], q, 44);
            let cend = if ce < 0 { end } else { ce };

            if cend > q {
                vals[got] = dc_f32(dc_strtod(scores[q .. cend]));
                got = got + 1;
            }
            if ce < 0 {
                q = end + 1;
            } else {
                q = cend + 1;
            }
        }
        if got != n {
            dc_puts(out, 0, "decision result does not match the number of options");
            return 0 - 1;
        }
        var bad = false;
        var i: i64 = 0;

        while i < n {
            let v = vals[i];

            if v != v {
                bad = true;
            }
            i = i + 1;
        }
        if bad {
            dc_puts(out, 0, "the model could not evaluate the decision");
            return 0 - 1;
        }
        var logits: f64[64] = [0.0; 64];
        i = 0;
        while i < n {
            logits[i] = vals[i];
            i = i + 1;
        }
        if qt == DC_Q_SCORE && dc_bias_count(bias, n) {
            var bpos: i64 = 0;
            i = 0;
            while bpos <= bias.len && i < n {
                let be = dc_find_char(bias, bpos, 44);
                let bend = if be < 0 { bias.len } else { be };

                if bend > bpos {
                    logits[i] = logits[i] + dc_f32(dc_strtod(bias[bpos .. bend]));
                    i = i + 1;
                }
                if be < 0 {
                    bpos = bias.len + 1;
                } else {
                    bpos = bend + 1;
                }
            }
        }
        var lmax = logits[0];
        i = 1;
        while i < n {
            if logits[i] > lmax {
                lmax = logits[i];
            }
            i = i + 1;
        }
        var sum = 0.0;
        i = 0;
        while i < n {
            let pv = dc_exp((logits[i] - lmax) / temperature);

            vals[i] = pv;
            sum += pv;
            i = i + 1;
        }
        i = 0;
        while i < n {
            let dst_i = if nv == 0 { i } else { n - 1 - i };

            pr[dst_i] += vals[i] / sum / (variants as f64);
            i = i + 1;
        }
        nv = nv + 1;
        if e < 0 {
            pos = scores.len + 1;
        } else {
            pos = end + 1;
        }
    }
    if nv != variants {
        dc_puts(out, 0, "decision result does not match the number of variants");
        return 0 - 1;
    }
    var p = dc_puts(out, 0, "{\"type\":");
    p = dc_py_str(out, p, dc_qname(qt), 0);

    if qt == DC_Q_NOUL {
        if typ == DC_T_LEV {
            var expected = 0.0;
            var i: i64 = 0;

            while i < n {
                expected += pr[i] * (i as f64) / ((n - 1) as f64);
                i = i + 1;
            }
            p = dc_puts(out, p, ",\"noul\":");
            return dc_putc(out, dc_py_float(out, p, expected), 125);
        }
        var got = false;
        var i2: i64 = 0;

        while i2 < n && !got {
            if dc_bytes_eq(kbuf, koff[first + i2], klen[first + i2], "true") {
                p = dc_puts(out, p, ",\"noul\":");
                p = dc_py_float(out, p, pr[i2]);
                got = true;
            }
            i2 = i2 + 1;
        }
        return dc_putc(out, p, 125);
    }
    // probabilities
    var prob: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var pp = dc_putc(prob, 0, 123);
    var i3: i64 = 0;

    while i3 < n {
        if i3 > 0 {
            pp = dc_putc(prob, pp, 44);
        }
        pp = dc_py_str(prob, pp, kbuf[koff[first + i3] .. koff[first + i3] + klen[first + i3]], 0);
        pp = dc_putc(prob, pp, 58);
        pp = dc_py_float(prob, pp, pr[i3]);
        i3 = i3 + 1;
    }
    pp = dc_putc(prob, pp, 125);
    if qt == DC_Q_CHOICE {
        var best: i64 = 0;
        var i4: i64 = 1;

        while i4 < n {
            if pr[i4] > pr[best] {
                best = i4;
            }
            i4 = i4 + 1;
        }
        p = dc_puts(out, p, ",\"choice\":");
        p = dc_py_str(out, p, kbuf[koff[first + best] .. koff[first + best] + klen[first + best]], 0);
        p = dc_puts(out, p, ",\"probabilities\":");
        p = dc_put(out, p, prob, 0, pp);
        p = dc_puts(out, p, ",\"confidence\":");
        return dc_putc(out, dc_py_float(out, p, dc_confidence_of(typ, qt, pr, n)), 125);
    }
    var expected = 0.0;
    var legend: u8[DC_SCRATCH] = [0; DC_SCRATCH];
    var lp = dc_putc(legend, 0, 123);
    var i5: i64 = 0;

    while i5 < n {
        if i5 > 0 {
            lp = dc_putc(legend, lp, 44);
        }
        expected += (i5 as f64) * pr[i5];
        lp = dc_py_str(legend, lp, kbuf[koff[first + i5] .. koff[first + i5] + klen[first + i5]], 0);
        lp = dc_putc(legend, lp, 58);
        let d = kdesc[first + i5];

        if canonical != 0 && d >= 0 && t.jk[d] != DC_STRING {
            var tmp: u8[DC_SCRATCH] = [0; DC_SCRATCH];
            let m = dc_nl_dump(t, d, tmp, 0);

            lp = dc_py_str(legend, lp, tmp[0 .. m], 0);
        } else if d < 0 {
            lp = dc_puts(legend, lp, "null");
        } else {
            lp = dc_py_node(t, d, 0 - 1, 0, 0, 0, "", "", legend, lp);
        }
        i5 = i5 + 1;
    }
    lp = dc_putc(legend, lp, 125);
    p = dc_puts(out, p, ",\"score\":");
    p = dc_py_float(out, p, expected);
    p = dc_puts(out, p, ",\"legend\":");
    p = dc_put(out, p, legend, 0, lp);
    p = dc_puts(out, p, ",\"probabilities\":");
    p = dc_put(out, p, prob, 0, pp);
    p = dc_puts(out, p, ",\"confidence\":");
    return dc_putc(out, dc_py_float(out, p, dc_confidence_of(typ, qt, pr, n)), 125);
}

// true when the bias text holds exactly n values
fn dc_bias_count(bias: const u8[:], n: i64) -> bool {
    var cnt: i64 = 0;
    var pos: i64 = 0;

    while pos <= bias.len {
        let e = dc_find_char(bias, pos, 44);
        let end = if e < 0 { bias.len } else { e };

        if end > pos {
            cnt = cnt + 1;
        }
        if e < 0 {
            pos = bias.len + 1;
        } else {
            pos = end + 1;
        }
    }
    return cnt == n;
}

// ── exports: group_tasks ────────────────────────────────────────────────────

// server_decision_group_tasks, server-decision.cpp:1588, over token lists:
// "id:t1,t2,t3" per line. One line per resulting task: "G <id> <shared>
// <children>" for a parent with children, "T <id>" for a task of its own.
export fn dc_group_tasks(tokens: const u8[:], n_slots_in: i64, out: u8[:]) -> i64 {
    var tid: i64[32] = [0; 32];
    var tfirst: i64[32] = [0; 32];
    var tcnt: i64[32] = [0; 32];
    var tok: i64[1024] = [0; 1024];
    var nt: i64 = 0;
    var ntok: i64 = 0;
    var pos: i64 = 0;

    while pos <= tokens.len && nt < 32 {
        let e = dc_find_char(tokens, pos, 10);
        let end = if e < 0 { tokens.len } else { e };

        if end > pos {
            let colon = dc_find_char(tokens[0 .. end], pos, 58);

            if colon < 0 {
                tid[nt] = dc_strtod(tokens[pos .. end]) as i64;
                tfirst[nt] = ntok;
                tcnt[nt] = 0;
                nt = nt + 1;
            } else {
                tid[nt] = dc_strtod(tokens[pos .. colon]) as i64;
                tfirst[nt] = ntok;
                var q: i64 = colon + 1;

                while q <= end {
                    let ce = dc_find_char(tokens[0 .. end], q, 44);
                    let cend = if ce < 0 { end } else { ce };

                    if cend > q {
                        if ntok < 1024 {
                            tok[ntok] = dc_strtod(tokens[q .. cend]) as i64;
                        }
                        ntok = ntok + 1;
                    }
                    if ce < 0 {
                        q = end + 1;
                    } else {
                        q = cend + 1;
                    }
                }
                tcnt[nt] = ntok - tfirst[nt];
                nt = nt + 1;
            }
        }
        if e < 0 {
            pos = tokens.len + 1;
        } else {
            pos = end + 1;
        }
    }
    let n_slots = dc_max(n_slots_in, 1);
    var p: i64 = 0;
    var i: i64 = 0;

    while i < nt {
        let end = dc_min(nt, i + n_slots);
        var n_shared = tcnt[i] - 1;
        var j: i64 = i + 1;

        while j < end {
            n_shared = dc_min(n_shared, dc_prefix_len(tok, tfirst[i], tcnt[i], tfirst[j], tcnt[j]));
            n_shared = dc_min(n_shared, tcnt[j] - 1);
            j = j + 1;
        }
        if end - i < 2 || n_shared == 0 {
            var k: i64 = i;

            while k < end {
                p = dc_puts(out, p, "T ");
                p = dc_puti(out, p, tid[k]);
                p = dc_putc(out, p, 10);
                k = k + 1;
            }
            i = end;
        } else {
            p = dc_puts(out, p, "G ");
            p = dc_puti(out, p, tid[i]);
            p = dc_putc(out, p, 32);
            p = dc_puti(out, p, n_shared);
            p = dc_putc(out, p, 32);
            p = dc_puti(out, p, end - i - 1);
            p = dc_putc(out, p, 32);
            var k2: i64 = i + 1;

            while k2 < end {
                if k2 > i + 1 {
                    p = dc_putc(out, p, 44);
                }
                p = dc_puti(out, p, tid[k2]);
                k2 = k2 + 1;
            }
            p = dc_putc(out, p, 10);
            i = end;
        }
    }
    return p;
}

fn dc_prefix_len(tok: i64[:], aoff: i64, acnt: i64, boff: i64, bcnt: i64) -> i64 {
    var n: i64 = 0;
    let lim = dc_min(acnt, bcnt);

    while n < lim && tok[aoff + n] == tok[boff + n] {
        n = n + 1;
    }
    return n;
}

// The templates read the marker constants; they are data for the gate.
export fn dc_markers(out: u8[:]) -> i64 {
    var p = dc_puts(out, 0, "D1OMNI_MARKER=<<d1omni:\n");
    p = dc_puts(out, p, "D1OMNI_SEP=<<d1omni:sep>>\n");
    p = dc_puts(out, p, "D1OMNI_MARK_STATE=<<d1omni:state>>\n");
    p = dc_puts(out, p, "D1OMNI_MARK_QUESTION=<<d1omni:question>>\n");
    p = dc_puts(out, p, "D1OMNI_MARK_OPTION=<<d1omni:option>>\n");
    p = dc_puts(out, p, "D1OMNI_MAX_TOKENS=16384\n");
    p = dc_puts(out, p, "D1OMNI_MAX_TOKENS_IMAGE=896\n");
    p = dc_puts(out, p, "D1OMNI_MAX_TOKENS_AUDIO=15360\n");
    p = dc_puts(out, p, "CLEF_MARKER=<<clef:\n");
    p = dc_puts(out, p, "CLEF_SEP=<<clef:sep>>\n");
    p = dc_puts(out, p, "CLEF_MARK_QUESTION=<<clef:question>>\n");
    p = dc_puts(out, p, "CLEF_MARK_OPTION=<<clef:option>>\n");
    p = dc_puts(out, p, "H2O_MAX_STATE_TOKENS=32000");

    return p;
}
