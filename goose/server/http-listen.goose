// P8-02 S4: an HTTP listener in Goose, serving one request.
//
// This is the transport half that compiler blocker 6 was thought to prevent. It
// does not: the blocker is a clash with libc headers, and the standalone unit is
// compiled without them, so `socket`, `bind`, `listen`, `accept` and `getsockname`
// are callable directly. Only `read`, `write` and `close` collide, because the
// generated unit does include <unistd.h>; those three go through the substrate
// primitives qz_rd/qz_wr/qz_cl, which move bytes and decide nothing.
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

extern "socket" fn hl_socket(domain: i64, kind: i64, proto: i64) -> i64;
extern "bind" fn hl_bind(fd: i64, addr: u64, len: i64) -> i64;
extern "listen" fn hl_listen(fd: i64, backlog: i64) -> i64;
extern "accept" fn hl_accept(fd: i64, addr: u64, len: u64) -> i64;
extern "getsockname" fn hl_getsockname(fd: i64, addr: u64, len: u64) -> i64;
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
    if req.len < 1 || resp.len < 1 {
        return -1;
    }
    let cfd: i64 = hl_accept(fd, 0, 0);
    if cfd < 0 {
        return -1;
    }
    let got: i64 = hl_rd(cfd, hl_tmp(HL_REQ), HL_REQ_MAX);
    if got < 0 {
        hl_cl(cfd);
        return -1;
    }
    // Copy out of the scratch and into the caller's slice, byte by byte: Goose
    // cannot form a slice over a raw pointer.
    var n: i64 = got;
    if n > req.len {
        n = req.len;
    }
    var i: i64 = 0;
    while i < n {
        req[i] = hl_ld8(hl_tmp(HL_REQ + i)) as u8;
        i = i + 1;
    }

    // The request line decides the answer, and that decision is Goose.
    var line_end: i64 = -1;
    i = 0;
    while i + 1 < n {
        if req[i] == 13 && req[i + 1] == 10 {
            line_end = i + 2;
            break;
        }
        i = i + 1;
    }
    var status: i64 = 400;
    if line_end > 0 {
        var parsed: i64[8] = [0,0,0,0,0,0,0,0];
        if rq_parse_line_impl(req[0 .. line_end], parsed) == 0 {
            status = 200;
        }
    }

    var at: i64 = 0;
    at = rs_status_line_impl(status, resp);
    if at < 0 {
        hl_cl(cfd);
        return -1;
    }
    at = rs_header_impl("Content-Length", if status == 200 { "2" } else { "0" }, resp, at);
    if at < 0 {
        hl_cl(cfd);
        return -1;
    }
    at = rs_end_headers_impl(resp, at);
    if at < 0 {
        hl_cl(cfd);
        return -1;
    }
    if status == 200 {
        // The body is written literally, not chunked: Content-Length is set above.
        if at + 2 > resp.len {
            hl_cl(cfd);
            return -1;
        }
        resp[at] = 111;     // 'o'
        resp[at + 1] = 107; // 'k'
        at = at + 2;
    }

    // Out of the caller's slice and into the scratch, then one write.
    i = 0;
    while i < at {
        hl_st8(hl_tmp(HL_REQ + i), resp[i] as i64);
        i = i + 1;
    }
    let written: i64 = hl_wr(cfd, hl_tmp(HL_REQ), at);
    hl_cl(cfd);
    return if written < 0 { -1 } else { at };
}
