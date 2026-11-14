// CLI line protocol (P9-06 S2) in Goose: the stdin request reader and the six
// stdout emitters of bmoe-cli --session.
//
// Native sources: runtime/cli/main.cpp (json reads at 124-215, emitters at
// 100-118 and 240-355) and upstream/BigMoeOnEdge/docs/telemetry.md §"Session
// mode". The JSON text is parsed with the accepted parser
// (goose/json/json-parser.goose, EXP493), called through its export ABI because
// an exported function with a const-slice parameter cannot be instantiated for
// an internal call in this toolchain.
//
// Owned decisions:
//   - a line that fails to parse, or has no "cmd", or names an unknown command
//     is skipped (kind 3), exactly like the native `continue`;
//   - fields are read from the parsed object rather than by the native
//     substring scan, so a prompt containing the text "cmd" cannot confuse the
//     reader (the native scanner can);
//   - an "id"/"n_predict" of a non-integer kind is 0/absent (native atoi on a
//     quoted value is 0), and a non-boolean "think"/"clear_kv" is false
//     (native compares the text "true");
//   - numbers are printed with a fixed-point formatter (half-to-even on the
//     scaled value) instead of the host printf; see cli_fixed's limits;
//   - the progress snapshot is a caller buffer: reasoning bytes then text
//     bytes, lengths in the state, updated by cli_progress_delta.
//
// Flat layouts:
//   command state st: 0 kind (0 generate, 1 cancel, 2 close, 3 skip),
//   1 id, 2 n_predict, 3 think, 4 clear_kv, 5 prompt length in `prompt`;
//   progress state st: 0 previous reasoning length, 1 previous text length,
//   2 delta reasoning length, 3 delta text length in `delta`.
//   DONE metrics n (i64): 0 id, 1 cancelled, 2 tokens, 3 n_prompt, 4 n_past,
//   5 mtp_drafted, 6 mtp_accepted, 7 mtp_decodes, 8 drafted_steps;
//   m (f64): 0 tok_s, 1 prefill_s, 2 prefill_tps, 3 load_s, 4 cache_hit_pct,
//   5 compute_s_tok, 6 io_s_tok, 7 cache_resident_mib, 8 cache_budget_mib,
//   9 read_mib, 10 stall_s_tok, 11 mgmt_s_tok, 12 majflt_tok, 13 cpu_s_tok,
//   14 prefill_cpu_s, 15 prefill_read_mib, 16 prefill_io_s, 17 prefill_stall_s,
//   18 prefill_mgmt_s, 19 token_demand_mib, 20 mtp_draft_s_tok,
//   21 loop_overhead_s_tok.
//   PROGRESS metrics n: 0 step, 1 steps, 2 majflt;
//   m: 0 wall_ms, 1 io_ms, 2 compute_ms, 3 mgmt_ms, 4 stall_ms, 5 read_mb,
//   6 cache_hit_pct, 7 cpu_ms, 8 dense_resident_frac.

import std;
import jsonparse;

const PJ_K_NULL: i64 = 0;
const PJ_K_BOOL: i64 = 1;
const PJ_K_INT: i64 = 2;
const PJ_K_UINT: i64 = 3;
const PJ_K_FLOAT: i64 = 4;
const PJ_K_STRING: i64 = 5;
const PJ_K_ARRAY: i64 = 6;
const PJ_K_OBJECT: i64 = 7;

// The accepted parser, through its export ABI.
extern "json_parse" fn json_parse_abi(
    text: const u8[:],
    jk: i64[:], jv: i64[:], jd: f64[:], js: i64[:], jl: i64[:], jf: i64[:],
    jn: i64[:], jh: i64[:],
    e_val: i64[:], e_koff: i64[:], e_klen: i64[:], e_next: i64[:],
    sbuf: u8[:], jkey: i64[:], jkl: i64[:], jchild: i64[:], st: i64[:]) -> i64;

fn cp_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        if at + i < dst.len {
            dst[at + i] = s[i];
        }
        i = i + 1;
    }
    return at + s.len;
}

fn cp_chr(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

// Unsigned decimal.
fn cp_uint(dst: u8[:], at_in: i64, v: i64) -> i64 {
    var at = at_in;
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;

    if x < 0 {
        // The protocol only prints non-negative counters; clamp like an unsigned
        // would in practice only for a broken caller.
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
        at = cp_chr(dst, at, buf[i]);
        i = i - 1;
    }
    return at;
}

fn cp_pow10(prec: i64) -> i64 {
    var s: i64 = 1;
    var i: i64 = 0;

    while i < prec {
        s = s * 10;
        i = i + 1;
    }
    return s;
}

// Fixed-point like printf("%.<prec>f"): round the scaled value half to even.
// Limits: |v| * 10^prec must fit i64 and the f64 product must be exact enough;
// the tested protocol values satisfy both. Negative zero prints as "0.0".
fn cp_fixed(dst: u8[:], at_in: i64, v: f64, prec: i64) -> i64 {
    var at = at_in;
    var x = v;

    if x < 0.0 {
        at = cp_chr(dst, at, 45);
        x = 0.0 - x;
    }
    let scale = cp_pow10(prec);
    let scaled = x * (scale as f64);
    var n = scaled as i64;
    let frac = scaled - (n as f64);

    if frac > 0.5 {
        n = n + 1;
    } else {
        if frac == 0.5 {
            if (n - (n / 2) * 2) != 0 {
                n = n + 1;
            }
        }
    }
    if prec == 0 {
        return cp_uint(dst, at, n);
    }
    at = cp_uint(dst, at, n / scale);
    at = cp_chr(dst, at, 46);
    var f = n - (n / scale) * scale;
    var p = prec - 1;
    var div = scale / 10;

    while p > 0 {
        at = cp_chr(dst, at, (48 + ((f / div) - ((f / div / 10) * 10))) as u8);
        f = f - (f / div) * div;
        div = div / 10;
        p = p - 1;
    }
    at = cp_chr(dst, at, (48 + (f - (f / 10) * 10)) as u8);
    return at;
}

// json_escape: quote, backslash, the three short escapes, \u00xx below 0x20.
fn cp_escape(dst: u8[:], at_in: i64, s: const u8[:]) -> i64 {
    var at = at_in;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if c == 34 {
            at = cp_put(dst, at, "\\\"");
        } else {
            if c == 92 {
                at = cp_put(dst, at, "\\\\");
            } else {
                if c == 10 {
                    at = cp_put(dst, at, "\\n");
                } else {
                    if c == 13 {
                        at = cp_put(dst, at, "\\r");
                    } else {
                        if c == 9 {
                            at = cp_put(dst, at, "\\t");
                        } else {
                            if c < 32 {
                                at = cp_put(dst, at, "\\u00");
                                let hi = (c >> 4) & 15;
                                let lo = c & 15;
                                at = cp_chr(dst, at, (48 + hi) as u8);
                                at = cp_chr(dst, at, (48 + lo) as u8);
                            } else {
                                at = cp_chr(dst, at, c);
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

fn cp_key(dst: u8[:], at_in: i64, key: const u8[:]) -> i64 {
    var at = at_in;
    at = cp_put(dst, at, "\"");
    at = cp_put(dst, at, key);
    return cp_put(dst, at, "\":");
}

// Child lookup in a parsed object: key bytes -> node id, or -1.
fn cp_obj_get(
    node: i64, key: const u8[:], sbuf: const u8[:],
    jf: const i64[:], jn: const i64[:], jkey: const i64[:], jkl: const i64[:],
    jchild: const i64[:]) -> i64 {
    let base = jf[node];
    let n = jn[node];
    var k = 0;

    while k < n {
        let ko = jkey[base + k];
        let kl = jkl[base + k];

        if kl == key.len {
            var same: i64 = 1;
            var i: i64 = 0;

            while i < kl {
                if sbuf[ko + i] != key[i] {
                    same = 0;
                    i = kl;
                } else {
                    i = i + 1;
                }
            }
            if same == 1 {
                return jchild[base + k];
            }
        }
        k = k + 1;
    }
    return 0 - 1;
}

// One stdin line -> one command in st. Unknown or malformed lines are kind 3.
export fn cli_read_line(
    line: const u8[:],
    dflt_npredict: i64,
    dflt_think: i64,
    prompt: u8[:],
    st: i64[:]) -> i64 {
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

    st[0] = 3;
    st[1] = 0;
    st[2] = dflt_npredict;
    st[3] = dflt_think;
    st[4] = 1;
    st[5] = 0;

    let root = json_parse_abi(line, jk, jv, jd, js, jl, jf, jn, jh,
                             ev, ek, el, en, sb, key, kl, jc, pst);

    if root < 0 {
        return 3;
    }
    if jk[root] != PJ_K_OBJECT {
        return 3;
    }
    let cmd = cp_obj_get(root, "cmd", sb, jf, jn, key, kl, jc);

    if cmd < 0 || jk[cmd] != PJ_K_STRING {
        return 3;
    }
    let cmd_s = sb[js[cmd] .. js[cmd] + jl[cmd]];

    if cp_eq(cmd_s, "cancel") == 1 {
        st[0] = 1;
        return 1;
    }
    if cp_eq(cmd_s, "close") == 1 {
        st[0] = 2;
        return 2;
    }
    if cp_eq(cmd_s, "generate") != 1 {
        return 3;
    }
    st[0] = 0;

    let pr = cp_obj_get(root, "prompt", sb, jf, jn, key, kl, jc);

    if pr >= 0 && jk[pr] == PJ_K_STRING {
        let off = js[pr];
        let len = jl[pr];

        st[5] = cp_put(prompt, 0, sb[off .. off + len]);
    }
    let idn = cp_obj_get(root, "id", sb, jf, jn, key, kl, jc);

    if idn >= 0 && (jk[idn] == PJ_K_INT || jk[idn] == PJ_K_UINT) {
        st[1] = jv[idn];
    }
    let np = cp_obj_get(root, "n_predict", sb, jf, jn, key, kl, jc);

    if np >= 0 && (jk[np] == PJ_K_INT || jk[np] == PJ_K_UINT) {
        st[2] = jv[np];
    }
    let th = cp_obj_get(root, "think", sb, jf, jn, key, kl, jc);

    if th >= 0 && jk[th] == PJ_K_BOOL {
        st[3] = jv[th];
    }
    let ck = cp_obj_get(root, "clear_kv", sb, jf, jn, key, kl, jc);

    if ck >= 0 && jk[ck] == PJ_K_BOOL {
        st[4] = jv[ck];
    }
    return 0;
}

fn cp_eq(a: const u8[:], b: const u8[:]) -> i64 {
    if a.len != b.len {
        return 0;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn cp_prefix(full: const u8[:], prev: const u8[:]) -> bool {
    if full.len < prev.len {
        return false;
    }
    var i: i64 = 0;

    while i < prev.len {
        if full[i] != prev[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// Delta and reset decision for one token: st[0]/st[1] are the previous lengths,
// st[2]/st[3] the delta lengths in `delta`. Returns 1 when "reset":1 is needed.
export fn cli_progress_delta(
    reasoning: const u8[:],
    text: const u8[:],
    snap: u8[:],
    st: i64[:],
    delta: u8[:]) -> i64 {
    let pr = st[0];
    let pt = st[1];
    let ext = cp_prefix(reasoning, snap[0 .. pr]) &&
              cp_prefix(text, snap[pr .. pr + pt]);
    var dr: i64 = 0;
    var dt: i64 = 0;

    if ext {
        dr = reasoning.len - pr;
        dt = text.len - pt;
        st[2] = cp_put(delta, 0, reasoning[pr .. reasoning.len]);
        st[3] = cp_put(delta, st[2], text[pt .. text.len]) - st[2];
    } else {
        st[2] = cp_put(delta, 0, reasoning);
        st[3] = cp_put(delta, st[2], text) - st[2];
    }
    // The snapshot always holds the full current texts.
    cp_put(snap, 0, reasoning);
    cp_put(snap, reasoning.len, text);
    st[0] = reasoning.len;
    st[1] = text.len;
    if ext {
        return 0;
    }
    return 1;
}

// ── emitters ────────────────────────────────────────────────────────────────

export fn cli_emit_ready(
    load_s: f64, arch: const u8[:], n_ctx: i64,
    think_ctl: const u8[:], n_expert_used: i64, out: u8[:]) -> i64 {
    var p = 0;

    p = cp_put(out, p, "BMOE_READY {\"load_s\":");
    p = cp_fixed(out, p, load_s, 3);
    p = cp_put(out, p, ",\"arch\":\"");
    p = cp_escape(out, p, arch);
    p = cp_put(out, p, "\",\"n_ctx\":");
    p = cp_uint(out, p, n_ctx);
    p = cp_put(out, p, ",\"think_ctl\":\"");
    p = cp_escape(out, p, think_ctl);
    p = cp_put(out, p, "\",\"n_expert_used\":");
    p = cp_uint(out, p, n_expert_used);
    p = cp_put(out, p, "}");
    return cp_chr(out, p, 10);
}

export fn cli_emit_begin(id: i64, out: u8[:]) -> i64 {
    var p = cp_put(out, 0, "BMOE_BEGIN {\"id\":");

    p = cp_uint(out, p, id);
    return cp_chr(out, p, 10);
}

export fn cli_emit_load(mb: f64, ms: f64, out: u8[:]) -> i64 {
    var p = cp_put(out, 0, "BMOE_LOAD {\"mb\":");

    p = cp_fixed(out, p, mb, 2);
    p = cp_put(out, p, ",\"ms\":");
    p = cp_fixed(out, p, ms, 1);
    p = cp_put(out, p, "}");
    return cp_chr(out, p, 10);
}

export fn cli_emit_progress(
    n: const i64[:], m: const f64[:],
    reset: i64,
    delta_reasoning: const u8[:], delta_text: const u8[:],
    out: u8[:]) -> i64 {
    var p = cp_put(out, 0, "BMOE_PROGRESS {\"step\":");

    p = cp_uint(out, p, n[0]);
    p = cp_put(out, p, ",\"steps\":");
    p = cp_uint(out, p, n[1]);
    p = cp_put(out, p, ",\"wall_ms\":");
    p = cp_fixed(out, p, m[0], 1);
    p = cp_put(out, p, ",\"io_ms\":");
    p = cp_fixed(out, p, m[1], 1);
    p = cp_put(out, p, ",\"compute_ms\":");
    p = cp_fixed(out, p, m[2], 1);
    p = cp_put(out, p, ",\"mgmt_ms\":");
    p = cp_fixed(out, p, m[3], 1);
    p = cp_put(out, p, ",\"stall_ms\":");
    p = cp_fixed(out, p, m[4], 1);
    p = cp_put(out, p, ",\"read_mb\":");
    p = cp_fixed(out, p, m[5], 2);
    p = cp_put(out, p, ",\"cache_hit_pct\":");
    p = cp_fixed(out, p, m[6], 1);
    p = cp_put(out, p, ",\"majflt\":");
    p = cp_uint(out, p, n[2]);
    p = cp_put(out, p, ",\"cpu_ms\":");
    p = cp_fixed(out, p, m[7], 1);
    p = cp_put(out, p, ",\"dense_resident_frac\":");
    p = cp_fixed(out, p, m[8], 3);
    p = cp_put(out, p, ",");
    if reset != 0 {
        p = cp_put(out, p, "\"reset\":1,");
    }
    p = cp_put(out, p, "\"delta_reasoning\":\"");
    p = cp_escape(out, p, delta_reasoning);
    p = cp_put(out, p, "\",\"delta_text\":\"");
    p = cp_escape(out, p, delta_text);
    p = cp_put(out, p, "\"}");
    return cp_chr(out, p, 10);
}

export fn cli_emit_done(
    n: const i64[:], m: const f64[:],
    reasoning: const u8[:], text: const u8[:],
    out: u8[:]) -> i64 {
    var p = cp_put(out, 0, "BMOE_DONE {\"id\":");

    p = cp_uint(out, p, n[0]);
    p = cp_put(out, p, ",\"cancelled\":");
    if n[1] != 0 {
        p = cp_put(out, p, "true");
    } else {
        p = cp_put(out, p, "false");
    }
    p = cp_put(out, p, ",\"tokens\":");
    p = cp_uint(out, p, n[2]);
    p = cp_put(out, p, ",\"tok_s\":");
    p = cp_fixed(out, p, m[0], 3);
    p = cp_put(out, p, ",\"prefill_s\":");
    p = cp_fixed(out, p, m[1], 3);
    p = cp_put(out, p, ",\"prefill_tps\":");
    p = cp_fixed(out, p, m[2], 2);
    p = cp_put(out, p, ",\"load_s\":");
    p = cp_fixed(out, p, m[3], 3);
    p = cp_put(out, p, ",\"cache_hit_pct\":");
    p = cp_fixed(out, p, m[4], 1);
    p = cp_put(out, p, ",\"n_prompt\":");
    p = cp_uint(out, p, n[3]);
    p = cp_put(out, p, ",\"n_past\":");
    p = cp_uint(out, p, n[4]);
    p = cp_put(out, p, ",\"compute_s_tok\":");
    p = cp_fixed(out, p, m[5], 4);
    p = cp_put(out, p, ",\"io_s_tok\":");
    p = cp_fixed(out, p, m[6], 4);
    p = cp_put(out, p, ",\"cache_resident_mib\":");
    p = cp_fixed(out, p, m[7], 0);
    p = cp_put(out, p, ",\"cache_budget_mib\":");
    p = cp_fixed(out, p, m[8], 0);
    p = cp_put(out, p, ",\"read_mib\":");
    p = cp_fixed(out, p, m[9], 1);
    p = cp_put(out, p, ",\"stall_s_tok\":");
    p = cp_fixed(out, p, m[10], 4);
    p = cp_put(out, p, ",\"mgmt_s_tok\":");
    p = cp_fixed(out, p, m[11], 4);
    p = cp_put(out, p, ",\"majflt_tok\":");
    p = cp_fixed(out, p, m[12], 2);
    p = cp_put(out, p, ",\"cpu_s_tok\":");
    p = cp_fixed(out, p, m[13], 4);
    p = cp_put(out, p, ",\"prefill_cpu_s\":");
    p = cp_fixed(out, p, m[14], 3);
    p = cp_put(out, p, ",\"prefill_read_mib\":");
    p = cp_fixed(out, p, m[15], 1);
    p = cp_put(out, p, ",\"prefill_io_s\":");
    p = cp_fixed(out, p, m[16], 3);
    p = cp_put(out, p, ",\"prefill_stall_s\":");
    p = cp_fixed(out, p, m[17], 3);
    p = cp_put(out, p, ",\"prefill_mgmt_s\":");
    p = cp_fixed(out, p, m[18], 3);
    p = cp_put(out, p, ",\"token_demand_mib\":");
    p = cp_fixed(out, p, m[19], 1);
    p = cp_put(out, p, ",\"mtp_drafted\":");
    p = cp_uint(out, p, n[5]);
    p = cp_put(out, p, ",\"mtp_accepted\":");
    p = cp_uint(out, p, n[6]);
    p = cp_put(out, p, ",\"mtp_decodes\":");
    p = cp_uint(out, p, n[7]);
    p = cp_put(out, p, ",\"mtp_draft_s_tok\":");
    p = cp_fixed(out, p, m[20], 4);
    p = cp_put(out, p, ",\"drafted_steps\":");
    p = cp_uint(out, p, n[8]);
    p = cp_put(out, p, ",\"loop_overhead_s_tok\":");
    p = cp_fixed(out, p, m[21], 4);
    p = cp_put(out, p, ",\"reasoning\":\"");
    p = cp_escape(out, p, reasoning);
    p = cp_put(out, p, "\",\"text\":\"");
    p = cp_escape(out, p, text);
    p = cp_put(out, p, "\"}");
    return cp_chr(out, p, 10);
}

// A generate() failure: fatal unless it is one of the two the session survives.
export fn cli_error_recoverable(msg: const u8[:]) -> i64 {
    if cp_contains(msg, "exceeds the session n_ctx") == 1 {
        return 1;
    }
    if cp_contains(msg, "empty prompt") == 1 {
        return 1;
    }
    return 0;
}

fn cp_contains(hay: const u8[:], needle: const u8[:]) -> i64 {
    if needle.len == 0 || hay.len < needle.len {
        return 0;
    }
    var i: i64 = 0;

    while i + needle.len <= hay.len {
        var same: i64 = 1;
        var k: i64 = 0;

        while k < needle.len {
            if hay[i + k] != needle[k] {
                same = 0;
                k = needle.len;
            } else {
                k = k + 1;
            }
        }
        if same == 1 {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

export fn cli_emit_error(id: i64, fatal: i64, msg: const u8[:], out: u8[:]) -> i64 {
    var p = cp_put(out, 0, "BMOE_ERROR {\"id\":");

    p = cp_uint(out, p, id);
    p = cp_put(out, p, ",\"fatal\":");
    if fatal != 0 {
        p = cp_put(out, p, "true");
    } else {
        p = cp_put(out, p, "false");
    }
    p = cp_put(out, p, ",\"msg\":\"");
    p = cp_escape(out, p, msg);
    p = cp_put(out, p, "\"}");
    return cp_chr(out, p, 10);
}
