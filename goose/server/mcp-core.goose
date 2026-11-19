// MCP session core (P8-07 S3) in Goose, from tools/server/server-mcp.cpp
// (parse_cursor_format 140, rpc_error_message 182, mcp_result_to_response 197,
// ensure_init 255, list_tools 284, call_tool 316). The stdio transport (344-636)
// and the manager (639-799) are S5: here the transport is data.
//
// The JSON is read with the accepted parser (goose/json/json-parser.goose,
// imported as the sibling module jsonparse). Every export is a thin wrapper over
// a private body because Goose cannot instantiate an exported function with a
// const-slice parameter for an internal call.

import std;
import jsonparse;

const PJ_K_BOOL: i64 = 1;
const PJ_K_INT: i64 = 2;
const PJ_K_UINT: i64 = 3;
const PJ_K_STRING: i64 = 5;
const PJ_K_ARRAY: i64 = 6;
const PJ_K_OBJECT: i64 = 7;

const MC_MAX_SERVERS: i64 = 64;
const MC_LINE: i64 = 4096;

extern "json_parse" fn json_parse_abi(
    text: const u8[:],
    jk: i64[:], jv: i64[:], jd: f64[:], js: i64[:], jl: i64[:], jf: i64[:],
    jn: i64[:], jh: i64[:],
    e_val: i64[:], e_koff: i64[:], e_klen: i64[:], e_next: i64[:],
    sbuf: u8[:], jkey: i64[:], jkl: i64[:], jchild: i64[:], st: i64[:]) -> i64;

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
    if obj < 0 || p.jk[obj] != PJ_K_OBJECT {
        return 0 - 1;
    }
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

fn pget_str(p: const PNode, obj: i64, key: const u8[:]) -> i64, i64 {
    let n = pget(p, obj, key);

    if n < 0 || p.jk[n] != PJ_K_STRING {
        return 0 - 1, 0;
    }
    return p.js[n], p.jl[n];
}

fn mput(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        if p < dst.len {
            dst[p] = s[i];
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn mput_i(dst: u8[:], at: i64, v: i64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;
    var q = at;

    if x < 0 {
        if q < dst.len {
            dst[q] = 45;
        }
        q = q + 1;
        x = 0 - x;
    }
    if x == 0 {
        if q < dst.len {
            dst[q] = 48;
        }
        return q + 1;
    }
    while x > 0 {
        buf[n] = (48 + x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        if q < dst.len {
            dst[q] = buf[i];
        }
        q = q + 1;
    }
    return q;
}

// JSON string body: quotes, backslash and control bytes are escaped like the
// library's dumper; the corpus stays ASCII.
fn mput_esc(dst: u8[:], at: i64, s: const u8[:], pos_from: i64, n: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < n {
        let c = s[pos_from + i];

        if c == 34 {
            p = mput(dst, p, "\\\"");
        } else if c == 92 {
            p = mput(dst, p, "\\\\");
        } else if c == 10 {
            p = mput(dst, p, "\\n");
        } else if c == 9 {
            p = mput(dst, p, "\\t");
        } else if c == 13 {
            p = mput(dst, p, "\\r");
        } else if c < 32 {
            let hex: u8[16] = [48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102];
            p = mput(dst, p, "\\u00");
            if p < dst.len {
                dst[p] = hex[(c / 16) as i64];
            }
            if p + 1 < dst.len {
                dst[p + 1] = hex[(c % 16) as i64];
            }
            p = p + 2;
        } else {
            if p < dst.len {
                dst[p] = c;
            }
            p = p + 1;
        }
        i = i + 1;
    }
    return p;
}

fn mc_eq(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    if al != bl {
        return false;
    }
    var i: i64 = 0;

    while i < al {
        if a[ao + i] != b[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn mc_less(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    let n = al;
    var i: i64 = 0;

    while i < n && i < bl {
        if a[ao + i] != b[bo + i] {
            return a[ao + i] < b[bo + i];
        }
        i = i + 1;
    }
    return al < bl;
}

// ── config ─────────────────────────────────────────────────────────────────

// One line per server: name|command|cwd|timeout|args|env, in sorted name order
// (the native objects are std::map, so the iteration is byte order).
fn mcp_config_impl(text: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[512] = [0; 512];
    var jv: i64[512] = [0; 512];
    var jd: f64[512] = [0.0; 512];
    var js: i64[512] = [0; 512];
    var jl: i64[512] = [0; 512];
    var jf: i64[512] = [0; 512];
    var jn: i64[512] = [0; 512];
    var jh: i64[512] = [0; 512];
    var ev: i64[256] = [0; 256];
    var ek: i64[256] = [0; 256];
    var el: i64[256] = [0; 256];
    var en: i64[256] = [0; 256];
    var sb: u8[16384] = [0; 16384];
    var key: i64[512] = [0; 512];
    var kl: i64[512] = [0; 512];
    var jc: i64[512] = [0; 512];
    var pst: i64[16] = [0; 16];

    let root = json_parse_abi(text, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);
    if root < 0 || jk[root] != PJ_K_OBJECT {
        return 0;
    }
    let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let servers = pget(p, root, "mcpServers");

    if servers < 0 || p.jk[servers] != PJ_K_OBJECT {
        return 0;
    }

    let base = p.jf[servers];
    let n = p.jn[servers];
    var noff: i64[MC_MAX_SERVERS] = [0; MC_MAX_SERVERS];
    var nlen: i64[MC_MAX_SERVERS] = [0; MC_MAX_SERVERS];
    var cnt: i64 = 0;
    var i: i64 = 0;

    while i < n && cnt < MC_MAX_SERVERS {
        let id = p.jchild[base + i];

        if p.jk[id] == PJ_K_OBJECT {
            noff[cnt] = p.jkey[base + i];
            nlen[cnt] = p.jkl[base + i];
            cnt = cnt + 1;
        }
        i = i + 1;
    }

    // insertion sort by byte order
    i = 1;
    while i < cnt {
        var j = i;

        while j > 0 && mc_less(sb, noff[j], nlen[j], sb, noff[j - 1], nlen[j - 1]) {
            let to_ = noff[j]; noff[j] = noff[j - 1]; noff[j - 1] = to_;
            let tl = nlen[j]; nlen[j] = nlen[j - 1]; nlen[j - 1] = tl;
            j = j - 1;
        }
        i = i + 1;
    }

    var pout: i64 = 0;
    i = 0;
    while i < cnt {
        let id = pget(p, servers, sb[noff[i] .. noff[i] + nlen[i]]);
        let cmd_o, cmd_l = pget_str(p, id, "command");

        if cmd_l == 0 {
            i = i + 1;
        } else {
            let cwd_o, cwd_l = pget_str(p, id, "cwd");
            let to = pget_int(p, id, "timeout_ms", 30000);
            pout = mput(out, pout, sb[noff[i] .. noff[i] + nlen[i]]);
            pout = mput(out, pout, "|");
            pout = mput(out, pout, sb[cmd_o .. cmd_o + cmd_l]);
            pout = mput(out, pout, "|");
            if cwd_l > 0 {
                pout = mput(out, pout, sb[cwd_o .. cwd_o + cwd_l]);
            }
            pout = mput(out, pout, "|");
            pout = mput_i(out, pout, to);
            pout = mput(out, pout, "|");

            let args = pget(p, id, "args");
            if args >= 0 && p.jk[args] == PJ_K_ARRAY {
                var a: i64 = 0;
                var first: i64 = 1;
                while a < p.jn[args] {
                    let c = p.jchild[p.jf[args] + a];
                    if p.jk[c] == PJ_K_STRING {
                        if first == 0 {
                            pout = mput(out, pout, ",");
                        }
                        pout = mput(out, pout, sb[p.js[c] .. p.js[c] + p.jl[c]]);
                        first = 0;
                    }
                    a = a + 1;
                }
            }
            pout = mput(out, pout, "|");

            let env = pget(p, id, "env");
            if env >= 0 && p.jk[env] == PJ_K_OBJECT {
                var e: i64 = 0;
                var first2: i64 = 1;
                while e < p.jn[env] {
                    let c = p.jchild[p.jf[env] + e];
                    if p.jk[c] == PJ_K_STRING {
                        if first2 == 0 {
                            pout = mput(out, pout, ";");
                        }
                        pout = mput(out, pout, sb[p.jkey[p.jf[env] + e] .. p.jkey[p.jf[env] + e] + p.jkl[p.jf[env] + e]]);
                        pout = mput(out, pout, "=");
                        pout = mput(out, pout, sb[p.js[c] .. p.js[c] + p.jl[c]]);
                        first2 = 0;
                    }
                    e = e + 1;
                }
            }
            pout = mput(out, pout, "\n");
            i = i + 1;
        }
    }
    return pout;
}

export fn mcp_config_lines(text: const u8[:], out: u8[:]) -> i64 {
    return mcp_config_impl(text, out);
}

// ── responses ──────────────────────────────────────────────────────────────

// "unknown error" unless error.message (object) or error (string) exists.
fn mcp_error_impl(resp: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[256] = [0; 256];
    var jv: i64[256] = [0; 256];
    var jd: f64[256] = [0.0; 256];
    var js: i64[256] = [0; 256];
    var jl: i64[256] = [0; 256];
    var jf: i64[256] = [0; 256];
    var jn: i64[256] = [0; 256];
    var jh: i64[256] = [0; 256];
    var ev: i64[128] = [0; 128];
    var ek: i64[128] = [0; 128];
    var el: i64[128] = [0; 128];
    var en: i64[128] = [0; 128];
    var sb: u8[8192] = [0; 8192];
    var key: i64[256] = [0; 256];
    var kl: i64[256] = [0; 256];
    var jc: i64[256] = [0; 256];
    var pst: i64[16] = [0; 16];

    let root = json_parse_abi(resp, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);
    if root < 0 {
        return mput(out, 0, "unknown error");
    }
    let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let err = pget(p, root, "error");

    if err < 0 {
        return mput(out, 0, "unknown error");
    }
    if p.jk[err] == PJ_K_STRING {
        return mput(out, 0, sb[p.js[err] .. p.js[err] + p.jl[err]]);
    }
    if p.jk[err] == PJ_K_OBJECT {
        let mo, ml = pget_str(p, err, "message");

        if ml > 0 {
            return mput(out, 0, sb[mo .. mo + ml]);
        }
    }
    return mput(out, 0, "unknown error");
}

export fn mcp_rpc_error(resp: const u8[:], out: u8[:]) -> i64 {
    return mcp_error_impl(resp, out);
}

// Normalize a tools/call result: join text content with newlines; isError maps
// to {"error": text or "MCP tool returned an error"}, otherwise
// {"plain_text_response": text}.
fn mcp_result_impl(result: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[512] = [0; 512];
    var jv: i64[512] = [0; 512];
    var jd: f64[512] = [0.0; 512];
    var js: i64[512] = [0; 512];
    var jl: i64[512] = [0; 512];
    var jf: i64[512] = [0; 512];
    var jn: i64[512] = [0; 512];
    var jh: i64[512] = [0; 512];
    var ev: i64[256] = [0; 256];
    var ek: i64[256] = [0; 256];
    var el: i64[256] = [0; 256];
    var en: i64[256] = [0; 256];
    var sb: u8[16384] = [0; 16384];
    var key: i64[512] = [0; 512];
    var kl: i64[512] = [0; 512];
    var jc: i64[512] = [0; 512];
    var pst: i64[16] = [0; 16];

    let root = json_parse_abi(result, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);
    var text: u8[8192] = [0; 8192];
    var tn: i64 = 0;
    var is_err: i64 = 0;

    if root >= 0 {
        let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                        jkey: key, jkl: kl, jchild: jc, sbuf: sb };
        let content = pget(p, root, "content");

        if content >= 0 && p.jk[content] == PJ_K_ARRAY {
            var i: i64 = 0;
            while i < p.jn[content] {
                let part = p.jchild[p.jf[content] + i];
                if p.jk[part] == PJ_K_OBJECT {
                    let to, tl = pget_str(p, part, "type");
                    if tl == 4 && mc_eq(sb, to, tl, "text", 0, 4) {
                        let xo, xl = pget_str(p, part, "text");
                        if xl >= 0 {
                            if tn > 0 {
                                text[tn] = 10;
                                tn = tn + 1;
                            }
                            var q: i64 = 0;
                            while q < xl {
                                text[tn] = sb[xo + q];
                                tn = tn + 1;
                                q = q + 1;
                            }
                        }
                    }
                }
                i = i + 1;
            }
        }
        let ie = pget(p, root, "isError");
        if ie >= 0 && p.jk[ie] == PJ_K_BOOL && p.jv[ie] != 0 {
            is_err = 1;
        }
    }

    var pout = mput(out, 0, "{");
    if is_err != 0 {
        pout = mput(out, pout, "\"error\":\"");
        if tn == 0 {
            pout = mput(out, pout, "MCP tool returned an error");
        } else {
            pout = mput_esc(out, pout, text, 0, tn);
        }
        pout = mput(out, pout, "\"}");
    } else {
        pout = mput(out, pout, "\"plain_text_response\":\"");
        pout = mput_esc(out, pout, text, 0, tn);
        pout = mput(out, pout, "\"}");
    }
    return pout;
}

export fn mcp_result_response(result: const u8[:], out: u8[:]) -> i64 {
    return mcp_result_impl(result, out);
}

// ── request builders (nlohmann dumps objects in key order) ─────────────────

export fn mcp_init_request(id: i64, out: u8[:]) -> i64 {
    var p = mput(out, 0, "{\"id\":");
    p = mput_i(out, p, id);
    p = mput(out, p, ",\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{\"capabilities\":{},\"clientInfo\":{\"name\":\"llama.cpp\",\"version\":\"1.0\"},\"protocolVersion\":\"2024-11-05\"}}");
    return p;
}

export fn mcp_initialized_notification(out: u8[:]) -> i64 {
    return mput(out, 0, "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
}

export fn mcp_list_tools_request(id: i64, out: u8[:]) -> i64 {
    var p = mput(out, 0, "{\"id\":");
    p = mput_i(out, p, id);
    p = mput(out, p, ",\"jsonrpc\":\"2.0\",\"method\":\"tools/list\"}");
    return p;
}

// `args` is the canonical arguments object; the fixture passes sorted JSON.
export fn mcp_call_tool_request(id: i64, name: const u8[:], args: const u8[:], out: u8[:]) -> i64 {
    var p = mput(out, 0, "{\"id\":");
    p = mput_i(out, p, id);
    p = mput(out, p, ",\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"arguments\":");
    p = mput(out, p, args);
    p = mput(out, p, ",\"name\":\"");
    p = mput_esc(out, p, name, 0, name.len);
    p = mput(out, p, "\"}}");
    return p;
}

// One line per tool: name|description|schema, schema is "y" or "n".
fn mcp_tools_impl(result: const u8[:], out: u8[:]) -> i64 {
    var jk: i64[512] = [0; 512];
    var jv: i64[512] = [0; 512];
    var jd: f64[512] = [0.0; 512];
    var js: i64[512] = [0; 512];
    var jl: i64[512] = [0; 512];
    var jf: i64[512] = [0; 512];
    var jn: i64[512] = [0; 512];
    var jh: i64[512] = [0; 512];
    var ev: i64[256] = [0; 256];
    var ek: i64[256] = [0; 256];
    var el: i64[256] = [0; 256];
    var en: i64[256] = [0; 256];
    var sb: u8[16384] = [0; 16384];
    var key: i64[512] = [0; 512];
    var kl: i64[512] = [0; 512];
    var jc: i64[512] = [0; 512];
    var pst: i64[16] = [0; 16];

    let root = json_parse_abi(result, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);
    if root < 0 {
        return 0;
    }
    let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let result = pget(p, root, "result");
    let tools = pget(p, result, "tools");

    if tools < 0 || p.jk[tools] != PJ_K_ARRAY {
        return 0;
    }
    var pout: i64 = 0;
    var i: i64 = 0;

    while i < p.jn[tools] {
        let t = p.jchild[p.jf[tools] + i];
        if p.jk[t] == PJ_K_OBJECT {
            let no, nl = pget_str(p, t, "name");
            let do_, dl = pget_str(p, t, "description");
            if nl > 0 {
                pout = mput(out, pout, sb[no .. no + nl]);
            }
            pout = mput(out, pout, "|");
            if dl > 0 {
                pout = mput(out, pout, sb[do_ .. do_ + dl]);
            }
            pout = mput(out, pout, "|");
            let sc = pget(p, t, "inputSchema");
            if sc >= 0 {
                pout = mput(out, pout, "y");
            } else {
                pout = mput(out, pout, "n");
            }
            pout = mput(out, pout, "\n");
        }
        i = i + 1;
    }
    return pout;
}

export fn mcp_parse_tools(result: const u8[:], out: u8[:]) -> i64 {
    return mcp_tools_impl(result, out);
}

// ── send_rpc verdict ───────────────────────────────────────────────────────

// 0 reply (written to out), 1 error object (written to out). Order follows the
// native function: write failure, matching reply, cancel, deadline, closed.
export fn mcp_rpc_verdict(written: i64, has_id: i64, reply_matches: i64,
                          cancelled: i64, timed_out: i64, out: u8[:]) -> i64 {
    if written == 0 {
        mput(out, 0, "{\"error\":{\"code\":-32603,\"message\":\"transport closed\"}}");
        return 1;
    }
    if has_id == 0 {
        return 0;
    }
    if reply_matches != 0 {
        return 0;
    }
    if cancelled != 0 {
        mput(out, 0, "{\"error\":{\"code\":-32603,\"message\":\"cancelled\"}}");
        return 1;
    }
    if timed_out != 0 {
        mput(out, 0, "{\"error\":{\"code\":-32603,\"message\":\"request timed out\"}}");
        return 1;
    }
    mput(out, 0, "{\"error\":{\"code\":-32603,\"message\":\"transport closed\"}}");
    return 1;
}
