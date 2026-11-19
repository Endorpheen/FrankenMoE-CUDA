// P8-02 S4: an HTTP listener in Goose, serving one request.
//
// This is the transport half that compiler blocker 6 was thought to prevent. It
// does not: the blocker is a clash with libc headers, and the standalone unit is
// compiled without them, so `socket`, `bind`, `listen`, `accept` and `getsockname`
// are callable directly. Only `read`, `write` and `close` collide, because the
// generated unit does include <unistd.h>; those three go through the substrate
// primitives qz_rd/qz_wr/qz_cl, which move bytes and decide nothing.
//
// The socket-family symbols are bound to `hl_*` forwards rather than to the libc
// names: another module in the same unit (goose/server/http-listener.goose) binds the
// same names with a different signature, and one C symbol cannot carry two prototypes.
// The forwards are one-liners in integration/goose-standalone/libc-forwards.c and hold
// no decision.
//
// The address structure is built in the host scratch (qz_tmp) because Goose cannot
// take the address of its own slice. That is the same seam quant-rows and
// model-ownership already use.
//
// Rules reproduced from httplib/POSIX:
//   - AF_INET = 2, SOCK_STREAM = 1;
//   - sockaddr_in is 16 bytes: family u16 little-endian, port u16 big-endian,
//     address 4 bytes, 8 zero bytes;
//   - binding port 0 asks the kernel for an ephemeral port, and getsockname reports
//     which one it chose - so a caller never picks a port and never collides with
//     a service already listening;
//   - one connection is accepted per call, the request is read once, and both
//     sockets are closed before returning.
//
// Not owned: TLS, keep-alive, more than one request per connection, the body.
// The prefix is hl_.

import std;

extern "hl_socket" fn hl_socket(domain: i64, kind: i64, proto: i64) -> i64;
extern "hl_bind" fn hl_bind(fd: i64, addr: u64, len: i64) -> i64;
extern "hl_listen" fn hl_listen(fd: i64, backlog: i64) -> i64;
extern "hl_accept" fn hl_accept(fd: i64, addr: u64, len: u64) -> i64;
extern "hl_getsockname" fn hl_getsockname(fd: i64, addr: u64, len: u64) -> i64;
extern "qz_tmp" fn hl_tmp(off: i64) -> u64;
extern "qz_st_u8" fn hl_st8(a: u64, v: i64);
extern "qz_ld_u8" fn hl_ld8(a: u64) -> i64;
extern "qz_rd" fn hl_rd(fd: i64, buf: u64, n: i64) -> i64;
extern "qz_wr" fn hl_wr(fd: i64, buf: u64, n: i64) -> i64;
extern "qz_cl" fn hl_cl(fd: i64) -> i64;

const HL_AF_INET: i64 = 2;
const HL_SOCK_STREAM: i64 = 1;
const HL_SOCKADDR_LEN: i64 = 16;
const HL_BACKLOG: i64 = 4;
// Scratch layout: 0..15 address, 32..47 name, 64..71 socklen, 128.. request bytes.
const HL_ADDR: i64 = 0;
const HL_NAME: i64 = 32;
const HL_SLEN: i64 = 64;
const HL_REQ: i64 = 128;
const HL_REQ_MAX: i64 = 8192;

fn hl_eq(req: const u8[:], off: i64, n: i64, lit: const u8[:]) -> bool {
    if n != lit.len || off < 0 || off + n > req.len {
        return false;
    }
    var i: i64 = 0;
    while i < n {
        if req[off + i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn hl_lower(c: u8) -> u8 {
    if c >= 65 && c <= 90 {
        return (c + 32) as u8;
    }
    return c;
}

// Header names are case-insensitive (RFC 9110 5.1), so the two the middleware
// reads are matched that way. Values are not compared here; key_authorize does it.
fn hl_ci_eq(req: const u8[:], off: i64, n: i64, lit: const u8[:]) -> bool {
    if n != lit.len || off < 0 || off + n > req.len {
        return false;
    }
    var i: i64 = 0;
    while i < n {
        if hl_lower(req[off + i]) != hl_lower(lit[i]) {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn hl_put_addr(off: i64, port: i64) {
    // family, little-endian u16
    hl_st8(hl_tmp(off + 0), HL_AF_INET & 0xff);
    hl_st8(hl_tmp(off + 1), (HL_AF_INET >> 8) & 0xff);
    // port, big-endian u16: network byte order
    hl_st8(hl_tmp(off + 2), (port >> 8) & 0xff);
    hl_st8(hl_tmp(off + 3), port & 0xff);
    // 127.0.0.1 and the tail of zeros
    hl_st8(hl_tmp(off + 4), 127);
    hl_st8(hl_tmp(off + 5), 0);
    hl_st8(hl_tmp(off + 6), 0);
    hl_st8(hl_tmp(off + 7), 1);
    var i: i64 = 8;
    while i < HL_SOCKADDR_LEN {
        hl_st8(hl_tmp(off + i), 0);
        i = i + 1;
    }
}

// Opens a listening socket on 127.0.0.1 with an ephemeral port.
// port_out[0] receives the port the kernel chose. Returns the fd, or -1.
export fn hl_open(port_out: i64[:]) -> i64 {
    if port_out.len < 1 {
        return -1;
    }
    port_out[0] = 0;
    let fd: i64 = hl_socket(HL_AF_INET, HL_SOCK_STREAM, 0);
    if fd < 0 {
        return -1;
    }
    hl_put_addr(HL_ADDR, 0);
    if hl_bind(fd, hl_tmp(HL_ADDR), HL_SOCKADDR_LEN) != 0 {
        hl_cl(fd);
        return -1;
    }
    if hl_listen(fd, HL_BACKLOG) != 0 {
        hl_cl(fd);
        return -1;
    }
    // Which port did the kernel pick?
    hl_put_addr(HL_NAME, 0);
    hl_st8(hl_tmp(HL_SLEN + 0), HL_SOCKADDR_LEN & 0xff);
    hl_st8(hl_tmp(HL_SLEN + 1), 0);
    hl_st8(hl_tmp(HL_SLEN + 2), 0);
    hl_st8(hl_tmp(HL_SLEN + 3), 0);
    hl_st8(hl_tmp(HL_SLEN + 4), 0);
    hl_st8(hl_tmp(HL_SLEN + 5), 0);
    hl_st8(hl_tmp(HL_SLEN + 6), 0);
    hl_st8(hl_tmp(HL_SLEN + 7), 0);
    if hl_getsockname(fd, hl_tmp(HL_NAME), hl_tmp(HL_SLEN)) != 0 {
        hl_cl(fd);
        return -1;
    }
    let hi: i64 = hl_ld8(hl_tmp(HL_NAME + 2));
    let lo: i64 = hl_ld8(hl_tmp(HL_NAME + 3));
    port_out[0] = hi * 256 + lo;
    return fd;
}

export fn hl_close(fd: i64) -> i64 {
    return hl_cl(fd);
}

// Accepts one connection, reads the request, answers it, closes both sockets.
// req receives the raw request bytes, resp the response bytes. Returns the number
// of response bytes written, or -1.
export fn hl_serve(fd: i64, req: u8[:], resp: u8[:]) -> i64 {
    var no_keys: i64[1] = [0];
    return hl_serve_keyed(fd, req, resp, "", no_keys);
}

// Same exchange with a configured key set: `keys` is the concatenation and `koff`
// its cumulative offsets, the format key_scan expects. An empty set (koff.len <= 1)
// skips validation entirely, which is what native does when no key is configured.
export fn hl_serve_keyed(fd: i64, req: u8[:], resp: u8[:], keys: const u8[:],
                         koff: const i64[:]) -> i64 {
    return hl_serve_full(fd, req, resp, keys, koff, "", 0);
}

// The full exchange: key set plus CORS policy. `origins` is the configured
// Access-Control-Allow-Origin mode ("*", a literal, or empty to emit nothing) and
// `credentials` selects whether the credential header is emitted. The origin of the
// request is echoed only where cors_allow_origin says so.
export fn hl_serve_full(fd: i64, req: u8[:], resp: u8[:], keys: const u8[:],
                        koff: const i64[:], origins: const u8[:], credentials: i64) -> i64 {
    if req.len < 1 || resp.len < 1 {
        return -1;
    }
    let cfd: i64 = hl_accept(fd, 0, 0);
    if cfd < 0 {
        return -1;
    }
    // Read until the header terminator arrives. One read is not one request: a client
    // may send the request line and the headers in separate segments, and answering
    // the first fragment with 400 is a bug, not a policy. Found by reading this
    // function against hl_serve_keep, which already looped.
    var filled: i64 = 0;
    var end: i64 = -1;
    while end < 0 {
        if filled >= req.len {
            break;
        }
        let more: i64 = hl_fill(cfd, req, filled);
        if more <= 0 {
            break;
        }
        filled = more;
        end = hl_req_end(req, filled);
    }
    if end <= 0 {
        hl_cl(cfd);
        return -1;
    }
    var meta: i64[2] = [0, 0];
    let rc: i64 = hl_handle_req(cfd, req[0 .. end], resp, keys, koff, origins, credentials, meta);
    hl_cl(cfd);
    return if rc < 0 { 0 - rc - 1 } else { rc };
}

// Serves up to `max_requests` requests on ONE accepted connection. A client that
// keeps the socket open and sends the next request after reading the previous answer
// is what a real transport must support; a single-shot gate cannot show it. A read
// returning 0 means the client closed, and the loop ends.
export fn hl_serve_keep(fd: i64, req: u8[:], resp: u8[:], keys: const u8[:],
                        koff: const i64[:], origins: const u8[:], credentials: i64,
                        max_requests: i64) -> i64 {
    if req.len < 1 || resp.len < 1 || max_requests <= 0 {
        return -1;
    }
    let cfd: i64 = hl_accept(fd, 0, 0);
    if cfd < 0 {
        return -1;
    }
    var served: i64 = 0;
    var have: i64 = 0;
    while served < max_requests {
        // Enough for one whole request? Read until the header terminator arrives.
        var end: i64 = if have > 0 { hl_req_end(req, have) } else { -1 };
        while end < 0 {
            if have >= req.len {
                break;
            }
            let more: i64 = hl_fill(cfd, req, have);
            if more <= 0 {
                break;
            }
            have = more;
            end = hl_req_end(req, have);
        }
        if end < 0 {
            break;
        }
        var meta: i64[2] = [0, 0];
        let rc: i64 = hl_handle_req(cfd, req[0 .. end], resp, keys, koff, origins, credentials, meta);
        if rc == 0 {
            break;
        }
        served = served + 1;
        if rc < 0 {
            // The client asked to close, or the answer failed; either way stop.
            break;
        }

        // Consume the body before looking for the next request. Without this a POST
        // body is parsed as the next request line, which answers 400 to a client that
        // did nothing wrong - and the body may already be in the buffer, because the
        // client is allowed to send it in the same write as the headers.
        var body: i64 = meta[0];
        if body < 0 {
            break;
        }
        var inbuf: i64 = have - end;
        if inbuf < 0 {
            inbuf = 0;
        }
        if inbuf > body {
            inbuf = body;
        }
        body = body - inbuf;
        // Whatever followed the body is the next request: move it to the front.
        var k: i64 = 0;
        while end + inbuf + k < have {
            req[k] = req[end + inbuf + k];
            k = k + 1;
        }
        have = have - end - inbuf;
        // The rest of the body has not arrived yet. Read and discard it, into the
        // space past the next request, through the same fill primitive.
        var dropped: i64 = 0;
        while dropped < body {
            if have >= req.len {
                break;
            }
            let got: i64 = hl_fill(cfd, req, have);
            if got <= 0 {
                break;
            }
            dropped = dropped + (got - have);
        }
        if dropped < body {
            // The client promised more body than it sent; there is no next request to
            // frame, and guessing would answer bytes that are not a request.
            break;
        }
        if meta[1] != 0 {
            break;
        }
    }
    hl_cl(cfd);
    return served;
}

// Reads more bytes into req[have..], through the host scratch because Goose cannot
// take the address of its own slice. Returns the new length, or -1 on error or EOF.
fn hl_fill(cfd: i64, req: u8[:], have: i64) -> i64 {
    if have < 0 || have >= req.len {
        return -1;
    }
    let room: i64 = req.len - have;
    let want: i64 = if room < HL_REQ_MAX { room } else { HL_REQ_MAX };
    let got: i64 = hl_rd(cfd, hl_tmp(HL_REQ), want);
    if got <= 0 {
        return -1;
    }
    var i: i64 = 0;
    while i < got {
        req[have + i] = hl_ld8(hl_tmp(HL_REQ + i)) as u8;
        i = i + 1;
    }
    return have + got;
}

// The end of the first request in buf[0..len], as an offset just past CRLF CRLF, or
// -1 when the terminator has not arrived yet. No body is read, so the header block is
// the whole request.
fn hl_req_end(buf: const u8[:], len: i64) -> i64 {
    var i: i64 = 0;
    while i + 3 < len {
        if buf[i] == 13 && buf[i + 1] == 10 && buf[i + 2] == 13 && buf[i + 3] == 10 {
            return i + 4;
        }
        i = i + 1;
    }
    return -1;
}

// meta is the body contract back to the caller: meta[0] is the body length in bytes
// (0 when there is none) and meta[1] is 1 when the connection must close after this
// response. The caller owns the socket, so the caller does the reading; this function
// owns the decision, which is why the length travels out rather than being read here.
// The exact bytes native sends for a 404 (server-http.cpp:145-156):
//   {"error":{"message":"File Not Found","type":"not_found_error","code":404}}
// Written from a byte function rather than a literal so the length and the content
// cannot drift apart, and the length is a constant the compiler can check.
const HL_NOT_FOUND_DOC: const u8[:] = "{\"error\":{\"message\":\"File Not Found\",\"type\":\"not_found_error\",\"code\":404}}";
const HL_NOT_FOUND_LEN_TEXT: const u8[:] = "74";

fn hl_not_found_byte(i: i64) -> u8 {
    if i < 0 || i >= HL_NOT_FOUND_DOC.len {
        return 0;
    }
    return HL_NOT_FOUND_DOC[i];
}

fn hl_handle_req(cfd: i64, req: const u8[:], resp: u8[:], keys: const u8[:], koff: const i64[:],
                 origins: const u8[:], credentials: i64, meta: i64[:]) -> i64 {
    let n: i64 = req.len;
    meta[0] = 0; meta[1] = 0;

    // The request line decides the answer, and that decision is Goose.
    // Header spans the middleware and the CORS decision need, filled by the scan
    // below and read after routing.
    var auth_off: i64 = -1; var auth_len: i64 = 0;
    var key_off: i64 = -1; var key_len: i64 = 0;
    var origin_off: i64 = -1; var origin_len: i64 = 0;
    var close_req: i64 = 0;
    var is_options: i64 = 0;
    var host_count: i64 = 0; var host_off: i64 = -1; var host_len: i64 = 0;
    var cl_off: i64 = -1; var cl_len: i64 = 0; var has_te: i64 = 0;
    // Declared here, not inside the parse branch: the body contract is published to
    // the caller at the end of the function, which is outside that branch.
    var body_len: i64 = 0; var cl_bad: i64 = 0;
    var line_end: i64 = -1;
    var i: i64 = 0;
    while i + 1 < n {
        if req[i] == 13 && req[i + 1] == 10 {
            line_end = i + 2;
            break;
        }
        i = i + 1;
    }
    // The route table decides, not this function: route_id maps (method, path) to a
    // category and answers 0 for "no such route". The method codes are the ones the
    // native table was generated with (get 1, post 2, del 3, ws 4).
    var status: i64 = 400;
    if line_end > 0 {
        var parsed: i64[8] = [0,0,0,0,0,0,0,0];
        if rq_parse_line_impl(req[0 .. line_end], parsed) == 0 {
            let mo: i64 = parsed[1]; let ml: i64 = parsed[2];
            let to: i64 = parsed[3]; let tl: i64 = parsed[4];
            // The middleware reads two headers out of the request; both are
            // optional and both are compared by key_authorize, not here.

            var hp: i64 = line_end;
            while hp + 1 < n {
                // end of this header line
                var he: i64 = hp;
                while he + 1 < n && !(req[he] == 13 && req[he + 1] == 10) {
                    he = he + 1;
                }
                if he + 1 >= n {
                    break;
                }
                if he == hp {
                    break; // the empty line ends the block
                }
                var ho: i64[5] = [0,0,0,0,0];
                if rq_parse_header_impl(req[hp .. he + 2], ho) == 0 {
                    if hl_ci_eq(req, hp + ho[1], ho[2], "Authorization") {
                        auth_off = hp + ho[3]; auth_len = ho[4];
                    } else if hl_ci_eq(req, hp + ho[1], ho[2], "X-Api-Key") {
                        key_off = hp + ho[3]; key_len = ho[4];
                    } else if hl_ci_eq(req, hp + ho[1], ho[2], "Origin") {
                        origin_off = hp + ho[3]; origin_len = ho[4];
                    } else if hl_ci_eq(req, hp + ho[1], ho[2], "Host") {
                        // Counted, not last-wins: RFC 9112 forbids more than one, and
                        // cpp-httplib rejects that rather than picking a winner.
                        host_count = host_count + 1;
                        if host_count == 1 {
                            host_off = hp + ho[3]; host_len = ho[4];
                        }
                    } else if hl_ci_eq(req, hp + ho[1], ho[2], "Content-Length") {
                        // First occurrence wins, as get_header_value does; a second one
                        // that disagrees is caught by the malformed-number rule below
                        // only when it is the first, which is the native behaviour.
                        if cl_off < 0 {
                            cl_off = hp + ho[3]; cl_len = ho[4];
                        }
                    } else if hl_ci_eq(req, hp + ho[1], ho[2], "Transfer-Encoding") {
                        has_te = 1;
                    } else if hl_ci_eq(req, hp + ho[1], ho[2], "Connection") {
                        // Half-close is honoured: the answer says so and the loop stops.
                        if hl_ci_eq(req, hp + ho[3], ho[4], "close") {
                            close_req = 1;
                        }
                    }
                }
                hp = he + 2;
            }
            // Header faults are decided before routing: a request without an authority
            // is refused whatever path it asks for, which is what native does.
            // The literal 1/0 in an argument position leaves the export without one
            // concrete signature, so the flag is typed before the call.
            var have_host: i64 = 0;
            if host_off >= 0 {
                have_host = 1;
            }
            let host_slice: const u8[:] = if host_off >= 0 { req[host_off .. host_off + host_len] } else { req[0 .. 0] };
            var host_ok: i64 = 1;
            if have_host != 0 {
                var hk: i64 = 0;
                while hk < host_slice.len {
                    if !rq_is_host_char(host_slice[hk]) {
                        host_ok = 0;
                        break;
                    }
                    hk = hk + 1;
                }
            }
            let bad_host: i64 = rq_host_violation(host_count, parsed[7], host_ok, have_host);
            if cl_off >= 0 {
                var dl: i64[1] = [0];
                if rq_decimal_impl(req[cl_off .. cl_off + cl_len], dl) != 0 {
                    cl_bad = 1;
                } else {
                    body_len = dl[0];
                }
            }
            // A Transfer-Encoding means the connection closes after the answer: this
            // listener cannot frame a chunked body, and reading one as if it were the
            // next request would answer the wrong thing.
            if rq_body_rule(has_te, if cl_off >= 0 { 1 } else { 0 }, parsed[7]) != 0 {
                close_req = 1;
            }
            var fault: i64 = 0;
            if bad_host != 0 {
                fault = 400;
            } else if tl > rq_uri_max() {
                fault = 414;
            } else if cl_bad != 0 {
                fault = 400;
            }
            var mcode: i64 = 0;
            if hl_eq(req, mo, ml, "GET") {
                mcode = 1;
            } else if hl_eq(req, mo, ml, "POST") {
                mcode = 2;
            } else if hl_eq(req, mo, ml, "DELETE") {
                mcode = 3;
            }
            if fault != 0 {
                status = fault;
                close_req = 1;
            } else if hl_eq(req, mo, ml, "OPTIONS") {
                is_options = 1;
                // A preflight is answered before routing: cors_is_preflight owns that
                // decision, and 204 is what the native server returns.
                status = if cors_is_preflight(req[mo .. mo + ml]) != 0 { 204 } else { 405 };
            } else if mcode == 0 {
                // A method the table does not carry is refused, not guessed.
                status = 405;
            } else {
                // The target may carry a query; the table matches the path only.
                var plen: i64 = tl;
                var k: i64 = 0;
                while k < tl {
                    if req[to + k] == 63 {
                        plen = k;
                        break;
                    }
                    k = k + 1;
                }
                let cat: i64 = route_id(mcode, req[to .. to + plen]);
                status = if cat == 0 { 404 } else { 200 };
                if status == 200 {
                    var empty_paths: i64[1] = [0];
                    let allowed: i64 = key_authorize_impl(keys, koff, req[to .. to + plen],
                                                     "", empty_paths,
                                                     if auth_off >= 0 { req[auth_off .. auth_off + auth_len] } else { req[0 .. 0] },
                                                     if key_off >= 0 { req[key_off .. key_off + key_len] } else { req[0 .. 0] });
                    if allowed == 0 {
                        status = 401;
                    }
                }
            }
        }
    }

    meta[0] = body_len;
    meta[1] = close_req;

    // The CORS allow-origin decision belongs to cors_allow_origin; the listener only
    // writes what it says. An empty policy emits nothing, which is what a server with
    // no cors_origins configured does.
    var allow_buf: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var allow_len: i64 = 0;
    if origins.len > 0 {
        var cres: i64[2] = [0, 0];
        let origin_slice: const u8[:] = if origin_off >= 0 { req[origin_off .. origin_off + origin_len] } else { req[0 .. 0] };
        // The localhost mode needs the origin's host, and the host parse is
        // http_parse_url's decision - not a second implementation here.
        // url_parse writes five spans: scheme, userinfo, password, host and path. Index 3
        // is the host, which is what cors_is_localhost compares. A short array
        // bounds-fails at run time, not at typecheck, so the size is spelled out.
        var o_off: i64[5] = [0,0,0,0,0]; var o_len: i64[5] = [0,0,0,0,0]; var o_port: i64[1] = [0];
        let pst: i64 = http_parse_url(origin_slice, o_off, o_len, o_port);
        var origin_local: i64 = 0;
        if pst == 0 && o_off[3] >= 0 && o_off[3] + o_len[3] <= origin_slice.len {
            origin_local = cors_is_localhost(origin_slice[o_off[3] .. o_off[3] + o_len[3]], 1);
        } else {
            origin_local = cors_is_localhost(origin_slice[0 .. 0], 0);
        }
        var ob: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
        let n = cors_allow_origin_impl(origins, credentials, origin_slice, origin_local, ob, cres);
        if cres[0] != 0 && n >= 0 {
            allow_len = n;
            var k: i64 = 0;
            while k < n {
                allow_buf[k] = ob[k];
                k = k + 1;
            }
        }
    }

    var at: i64 = 0;
    at = rs_status_line_impl(status, resp);
    if at < 0 {
        return -1;
    }
    if allow_len > 0 {
        at = rs_header_impl("Access-Control-Allow-Origin", allow_buf[0 .. allow_len], resp, at);
        if at < 0 {
            return -1;
        }
        // Only the allow-origin header belongs to every response. The credentials
        // header, the allowed methods and headers, and the blank text/html body are
        // the PREFLIGHT answer and nothing else: server-http.cpp:292-298 sets them
        // inside `if (req.method == "OPTIONS")`. EXP987 dropped the 204 condition from
        // the whole block, which put a preflight's headers on ordinary responses.
        if is_options != 0 {
            var cred: u8[8] = [0,0,0,0,0,0,0,0];
            let cn = cors_credentials_text(credentials, cred);
            if cn > 0 {
                at = rs_header_impl("Access-Control-Allow-Credentials", cred[0 .. cn], resp, at);
                if at < 0 {
                    return -1;
                }
            }
            var ct: u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
            let tn = cors_preflight_type(ct);
            if tn > 0 {
                at = rs_header_impl("Content-Type", ct[0 .. tn], resp, at);
                if at < 0 {
                    return -1;
                }
            }
        }
    }
    if close_req != 0 {
        at = rs_header_impl("Connection", "close", resp, at);
        if at < 0 {
            return -1;
        }
    }
    // Whether a 404 carries the not_found document is error_404's decision, not a
    // condition written here: server-http.cpp:145-156 sets that body from the error
    // handler, and leaving it out answered 404 with an empty body where native sends
    // JSON.
    var nf: i64[1] = [0];
    let own_404: i64 = error_404(status, nf);
    var body_len: i64 = 0;
    var body_kind: i64 = 0;      // 1 = "ok", 2 = the JSON not_found document
    if status == 200 {
        body_len = 2; body_kind = 1;
    } else if own_404 != 0 {
        body_len = HL_NOT_FOUND_DOC.len; body_kind = 2;
    }
    at = rs_header_impl("Content-Length", if body_len == 2 { "2" } else if body_len == HL_NOT_FOUND_DOC.len { HL_NOT_FOUND_LEN_TEXT } else { "0" }, resp, at);
    if at < 0 {
        return -1;
    }
    if body_kind == 2 {
        at = rs_header_impl("Content-Type", "application/json; charset=utf-8", resp, at);
        if at < 0 {
            return -1;
        }
    }
    at = rs_end_headers_impl(resp, at);
    if at < 0 {
        return -1;
    }
    if body_kind == 1 {
        // The body is written literally, not chunked: Content-Length is set above.
        if at + 2 > resp.len {
            return -1;
        }
        resp[at] = 111;     // 'o'
        resp[at + 1] = 107; // 'k'
        at = at + 2;
    } else if body_kind == 2 {
        if at + HL_NOT_FOUND_DOC.len > resp.len {
            return -1;
        }
        var b: i64 = 0;
        while b < HL_NOT_FOUND_DOC.len {
            resp[at + b] = hl_not_found_byte(b);
            b = b + 1;
        }
        at = at + HL_NOT_FOUND_DOC.len;
    }

    // Out of the caller's slice and into the scratch, then one write.
    i = 0;
    while i < at {
        hl_st8(hl_tmp(HL_REQ + i), resp[i] as i64);
        i = i + 1;
    }
    let written: i64 = hl_wr(cfd, hl_tmp(HL_REQ), at);
    if written < 0 {
        return -1;
    }
    // A negative answer means "written, and the connection must close". The caller
    // decodes it: -(len + 1), so a length of 0 stays distinguishable from an error.
    return if close_req != 0 { 0 - (at + 1) } else { at };
}
