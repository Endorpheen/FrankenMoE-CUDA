// MCP stdio integration glue (P8-07 S5b) in Goose, from
// tools/server/server-mcp.cpp: mcp_pump_ndjson (24), send_rpc (215),
// ensure_init (255), list_tools (284), call_tool (316),
// mcp_result_to_response (197) and diagnostics (521).
//
// The host owns the child process, the pipes and the clock. This module owns the
// protocol decisions that sit between them: NDJSON framing, the incremental
// line pump over a live pipe, reply matching, the stop verdict, the request
// order and id walk, the call branch and the diagnostics text.
//
// The file is composed into the server unit together with the accepted modules
// (mcp-core, mcp-stdio, decision-core, server-schema, tools-pure), so it reuses
// their private helpers directly: Goose cannot instantiate an exported function
// with a const-slice parameter for an internal call.

import std;

const MI_PARSE: i64 = 512;
const MI_SBUF: i64 = 8192;

// ── framing (mcp_write_all 537, writer_loop 604) ────────────────────────────

// One NDJSON frame: the payload and one '\n'. A payload that already carries a
// newline cannot be framed as a single message, so the frame is refused.
export fn mi_frame(msg: const u8[:], out: u8[:]) -> i64 {
    var i: i64 = 0;

    while i < msg.len {
        if msg[i] == 10 {
            return 0;
        }
        i = i + 1;
    }
    let p = ms_puts(out, 0, msg);
    return ms_putc(out, p, 10);
}

// ── incremental pump (mcp_pump_ndjson 24-129) ──────────────────────────────

// st carries the pump state across poll chunks: st[0] bytes used in buf, st[1]
// skipping an oversized line, st[2] oversized drops, st[3] bytes written to
// out by the last call. Every emitted line is
// written to out as "<decimal length>\n<bytes>" and the return value is the
// number of lines, so the host gets the raw frame without an escaping layer.
//
// Rule set, in native order: split on '\n' only, strip one trailing '\r', skip
// an empty line, drop an unterminated line at the end of the buffer, drop an
// oversized unterminated line and resynchronise at its next '\n'. `max_line`
// is an argument (native 8 MiB) so the corpus can exercise the cap.
export fn mi_lines(buf: u8[:], st: i64[:], chunk: const u8[:], max_line: i64, out: u8[:]) -> i64 {
    var bn = st[0];
    var skipping = st[1];
    var drops = st[2];
    var cap = max_line;
    var ln: i64 = 0;
    var emitted: i64 = 0;

    if cap <= 0 {
        cap = 8388608;
    }
    var k: i64 = 0;

    while k < chunk.len {
        if bn < buf.len {
            buf[bn] = chunk[k];
            bn = bn + 1;
        }
        k = k + 1;
    }

    // resync after an over-long, unterminated line: discard up to its newline
    var blocked: i64 = 0;

    if skipping != 0 {
        let nl = ms_find_range(buf, 0, bn, 10);

        if nl >= bn {
            if bn > cap {
                bn = 0;
            }
            blocked = 1;
        } else {
            bn = ms_erase(buf, bn, nl + 1);
            skipping = 0;
        }
    }
    if blocked == 0 {
        var nl = ms_find_range(buf, 0, bn, 10);

        while nl < bn {
            var ll = nl;

            if ll > 0 && buf[ll - 1] == 13 {
                ll = ll - 1;
            }
            if ll > 0 {
                ln = ms_put_i(out, ln, ll);
                ln = ms_putc(out, ln, 10);
                ln = ms_put(out, ln, buf, 0, ll);
                emitted = emitted + 1;
            }
            bn = ms_erase(buf, bn, nl + 1);
            nl = ms_find_range(buf, 0, bn, 10);
        }
        // a partial line already over the cap and still unterminated: drop it
        if bn > cap {
            drops = drops + 1;
            bn = 0;
            skipping = 1;
        }
    }
    st[0] = bn;
    st[1] = skipping;
    st[2] = drops;
    st[3] = ln;
    return emitted;
}

// ── reply matching (send_rpc 235-250) ──────────────────────────────────────

// 1 = deliver this frame, 0 = skip it. Native order: a frame that does not
// parse is skipped, a frame without an id is a notification and is skipped, a
// stale id (a reply to a request that already timed out) is skipped, and an
// id-less request takes the first frame.
export fn mi_match(has_id: i64, req_id: i64, line: const u8[:]) -> i64 {
    var jk: i64[MI_PARSE] = [0; MI_PARSE];
    var jv: i64[MI_PARSE] = [0; MI_PARSE];
    var jd: f64[MI_PARSE] = [0.0; MI_PARSE];
    var js: i64[MI_PARSE] = [0; MI_PARSE];
    var jl: i64[MI_PARSE] = [0; MI_PARSE];
    var jf: i64[MI_PARSE] = [0; MI_PARSE];
    var jn: i64[MI_PARSE] = [0; MI_PARSE];
    var jh: i64[MI_PARSE] = [0; MI_PARSE];
    var ev: i64[MI_PARSE] = [0; MI_PARSE];
    var ek: i64[MI_PARSE] = [0; MI_PARSE];
    var el: i64[MI_PARSE] = [0; MI_PARSE];
    var en: i64[MI_PARSE] = [0; MI_PARSE];
    var sb: u8[MI_SBUF] = [0; MI_SBUF];
    var key: i64[MI_PARSE] = [0; MI_PARSE];
    var kl: i64[MI_PARSE] = [0; MI_PARSE];
    var jc: i64[MI_PARSE] = [0; MI_PARSE];
    var pst: i64[16] = [0; 16];

    let root = json_parse_abi(line, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);
    if root < 0 {
        return 0; // malformed frame
    }
    if has_id == 0 {
        return 1; // an id-less request takes the first frame
    }
    let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };
    let idn = pget(p, root, "id");

    if idn < 0 {
        return 0; // notification
    }
    if p.jk[idn] == PJ_K_INT || p.jk[idn] == PJ_K_UINT {
        if p.jv[idn] == req_id {
            return 1;
        }
    }
    return 0; // stale or non-numeric id
}

// ── stop verdict (send_rpc 243-252) ────────────────────────────────────────

// The message rpc_error_message() reports for the transport error: a cancel
// wins over the deadline, the deadline wins over a closed transport, and a
// closed transport is the fallback.
export fn mi_stop_text(cancelled: i64, timed_out: i64, out: u8[:]) -> i64 {
    if cancelled != 0 {
        return ms_puts(out, 0, "cancelled");
    }
    if timed_out != 0 {
        return ms_puts(out, 0, "request timed out");
    }
    return ms_puts(out, 0, "transport closed");
}

// A per-request deadline is reached when the elapsed time is at least the
// configured timeout (native: steady_clock::now() >= deadline).
export fn mi_deadline_hit(elapsed_ms: i64, timeout_ms: i64) -> i64 {
    if elapsed_ms >= timeout_ms {
        return 1;
    }
    return 0;
}

// ── request order and ids (ensure_init 255, list_tools 284, call_tool 316) ──

// The method of one session stage, in native order: initialize, then
// notifications/initialized, then tools/list, then tools/call.
export fn mi_next_method(stage: i64, out: u8[:]) -> i64 {
    if stage == 0 {
        return ms_puts(out, 0, "initialize");
    }
    if stage == 1 {
        return ms_puts(out, 0, "notifications/initialized");
    }
    if stage == 2 {
        return ms_puts(out, 0, "tools/list");
    }
    if stage == 3 {
        return ms_puts(out, 0, "tools/call");
    }
    return ms_puts(out, 0, "done");
}

// next_id starts at 1 and increments per request; a respawn resets it.
export fn mi_next_id(cur: i64) -> i64 {
    return cur + 1;
}

// ── call branch (call_tool 316-341) ────────────────────────────────────────

// 0 = the reply carries an error object, 1 = it carries a result, 2 = neither
// (native: "invalid response from MCP server"). The caller keeps the mapping
// itself: mcp_rpc_error() for 0, mcp_result_response() for 1.
export fn mi_call_kind(reply: const u8[:]) -> i64 {
    var jk: i64[MI_PARSE] = [0; MI_PARSE];
    var jv: i64[MI_PARSE] = [0; MI_PARSE];
    var jd: f64[MI_PARSE] = [0.0; MI_PARSE];
    var js: i64[MI_PARSE] = [0; MI_PARSE];
    var jl: i64[MI_PARSE] = [0; MI_PARSE];
    var jf: i64[MI_PARSE] = [0; MI_PARSE];
    var jn: i64[MI_PARSE] = [0; MI_PARSE];
    var jh: i64[MI_PARSE] = [0; MI_PARSE];
    var ev: i64[MI_PARSE] = [0; MI_PARSE];
    var ek: i64[MI_PARSE] = [0; MI_PARSE];
    var el: i64[MI_PARSE] = [0; MI_PARSE];
    var en: i64[MI_PARSE] = [0; MI_PARSE];
    var sb: u8[MI_SBUF] = [0; MI_SBUF];
    var key: i64[MI_PARSE] = [0; MI_PARSE];
    var kl: i64[MI_PARSE] = [0; MI_PARSE];
    var jc: i64[MI_PARSE] = [0; MI_PARSE];
    var pst: i64[16] = [0; 16];

    let root = json_parse_abi(reply, jk, jv, jd, js, jl, jf, jn, jh,
                              ev, ek, el, en, sb, key, kl, jc, pst);
    if root < 0 {
        return 2;
    }
    if jk[root] != PJ_K_OBJECT {
        return 2;
    }
    let p = PNode { jk: jk, jv: jv, js: js, jl: jl, jf: jf, jn: jn,
                    jkey: key, jkl: kl, jchild: jc, sbuf: sb };

    if pget(p, root, "error") >= 0 {
        return 0;
    }
    if pget(p, root, "result") >= 0 {
        return 1;
    }
    return 2;
}

// ── stderr tail (618-632) ──────────────────────────────────────────────────

// errlog_loop appends every line with a newline and keeps the last `cap` bytes,
// so a truncation may start mid-line. The host keeps the result between lines.
export fn mi_err_tail(tail: const u8[:], line: const u8[:], cap: i64, out: u8[:]) -> i64 {
    var c = cap;
    var total = tail.len + line.len + 1;

    if c <= 0 {
        c = 4096;
    }
    var skip: i64 = 0;

    if total > c {
        skip = total - c;
    }
    var p: i64 = 0;
    var i = skip;

    while i < total {
        var ch: u8 = 10;

        if i < tail.len {
            ch = tail[i];
        } else if i < tail.len + line.len {
            ch = line[i - tail.len];
        }
        if p < out.len {
            out[p] = ch;
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// ── diagnostics (521-535) ──────────────────────────────────────────────────

// "last_error" then, when a stderr tail exists, "; last stderr: <tail>".
export fn mi_diag(last_error: const u8[:], tail: const u8[:], out: u8[:]) -> i64 {
    var p = ms_puts(out, 0, last_error);

    if tail.len > 0 {
        if p > 0 {
            p = ms_puts(out, p, "; ");
        }
        p = ms_puts(out, p, "last stderr: ");
        p = ms_put(out, p, tail, 0, tail.len);
    }
    return p;
}
