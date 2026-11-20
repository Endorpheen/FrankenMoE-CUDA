// HTTP listener in Goose.
//
// The transport decisions are Goose's: the socket lifecycle, the request line
// parse, the header-scan bounds, the response framing and the keep-alive rule.
// Every libc call here is reached through a `gx_*` forward with no decision in it
// (EXP-2026-11-17-938). The socket family is forwarded as well: another module in the
// same unit (goose/server/http-listen.goose) binds `socket`, `bind` and `listen` with
// a different signature, and one C symbol cannot carry two prototypes.
//
// Method codes match route-table.goose: 1 = GET, 2 = POST.

extern "gx_socket" fn gx_socket(domain: i64, kind: i64, proto: i64) -> i64;
extern "gx_bind" fn gx_bind(fd: i64, addr: u8[:], len: i64) -> i64;
extern "gx_listen" fn gx_listen(fd: i64, backlog: i64) -> i64;
// accept takes pointers, and a Goose slice is passed by value, so the call
// goes through the one-line forward like read/write/close.
extern "gx_accept" fn gx_accept(fd: i64, addr: u8[:], len: i64[:]) -> i64;
extern "gx_setsockopt" fn gx_setsockopt(fd: i64, level: i64, name: i64, val: u8[:], len: i64) -> i64;
extern "gx_close" fn gx_close(fd: i64) -> i64;
extern "gx_read" fn gx_read(fd: i64, buf: u8[:], n: i64) -> i64;
extern "gx_write" fn gx_write(fd: i64, buf: const u8[:], n: i64) -> i64;

const HT_SOL_SOCKET: i64 = 1;
const HT_SO_REUSEADDR: i64 = 2;
const HT_AF_INET: i64 = 2;
const HT_SOCK_STREAM: i64 = 1;
const HT_MAX_HEADER: i64 = 16384;

// sockaddr_in 127.0.0.1:port, network byte order.
fn ht_addr(port: i64) -> u8[16] {
    var a: u8[16] = [2, 0, 0, 0, 127, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0];
    a[2] = ((port / 256) % 256) as! u8;
    a[3] = (port % 256) as! u8;
    return a;
}

fn ht_eq_lit(s: const u8[:], lit: const u8[:]) -> bool {
    if s.len != lit.len {
        return false;
    }
    var i: i64 = 0;
    while i < s.len {
        if s[i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// Bind and listen. Returns the descriptor, or -(step) on failure:
// 1 socket, 2 setsockopt, 3 bind, 4 listen.
export fn ht_open(port: i64, opts: i64[:]) -> i64 {
    return ht_open_impl(port, opts);
}

fn ht_open_impl(port: i64, opts: i64[:]) -> i64 {
    let fd: i64 = gx_socket(HT_AF_INET, HT_SOCK_STREAM, 0);
    if fd < 0 {
        return -1;
    }
    var one: u8[4] = [1, 0, 0, 0];
    if gx_setsockopt(fd, HT_SOL_SOCKET, HT_SO_REUSEADDR, one[0 .. 4], 4) != 0 {
        gx_close(fd);
        return -2;
    }
    var a: u8[16] = ht_addr(port);
    if gx_bind(fd, a[0 .. 16], 16) != 0 {
        gx_close(fd);
        return -3;
    }
    if gx_listen(fd, 8) != 0 {
        gx_close(fd);
        return -4;
    }
    if opts.len >= 1 {
        opts[0] = fd;
    }
    return fd;
}

// Accept one connection. Returns the connection descriptor or -1.
export fn ht_accept(fd: i64, addr: u8[:], alen: i64[:], info: i64[:]) -> i64 {
    let conn: i64 = gx_accept(fd, addr, alen);
    if conn < 0 {
        return -1;
    }
    if info.len >= 2 {
        info[0] = conn;
        info[1] = alen[0];
    }
    return conn;
}

export fn ht_close(fd: i64) -> i64 {
    return gx_close(fd);
}

// Read one request into `buf`. Returns the byte count, 0 when the peer closed
// before sending anything, -1 on a read error, -2 when the request exceeds
// `buf` or the header limit.
export fn ht_read_request(fd: i64, buf: u8[:], info: i64[:]) -> i64 {
    if buf.len == 0 || info.len < 4 {
        return -2;
    }
    var total: i64 = 0;
    while total < buf.len {
        let n: i64 = gx_read(fd, buf[total .. buf.len], buf.len - total);
        if n < 0 {
            return -1;
        }
        if n == 0 {
            break;
        }
        total = total + n;
        // headers end at the first blank line; the body length is whatever the
        // header announced, and this listener only answers bodiless requests
        let hend: i64 = ht_header_end(buf[0 .. total]);
        if hend >= 0 {
            info[0] = hend;
            let m: i64 = ht_method_code(buf[0 .. total]);
            info[1] = m;
            let pl: i64 = ht_path_span(buf[0 .. total], info);
            if pl < 0 {
                return -2;
            }
            return total;
        }
        if total > HT_MAX_HEADER {
            return -2;
        }
    }
    return total;
}

// Index just past CRLFCRLF, or -1 when the headers are not complete yet.
export fn ht_header_end(buf: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i + 3 < buf.len {
        if buf[i] == 13 && buf[i + 1] == 10 && buf[i + 2] == 13 && buf[i + 3] == 10 {
            return i + 4;
        }
        i = i + 1;
    }
    return -1;
}

// Method code from the request line: 1 GET, 2 POST, 0 anything else.
export fn ht_method_code(buf: const u8[:]) -> i64 {
    if buf.len >= 3 && buf[0] == 71 && buf[1] == 69 && buf[2] == 84 {
        return 1;
    }
    if buf.len >= 4 && buf[0] == 80 && buf[1] == 79 && buf[2] == 83 && buf[3] == 84 {
        return 2;
    }
    return 0;
}

// Path span inside the request line: info[2] = offset, info[3] = length.
// The query string is excluded, so /props?x=1 matches /props.
export fn ht_path_span(buf: const u8[:], info: i64[:]) -> i64 {
    if info.len < 4 || buf.len == 0 {
        return -1;
    }
    var i: i64 = 0;
    while i < buf.len && buf[i] != 32 {
        i = i + 1;
    }
    if i >= buf.len {
        return -1;
    }
    i = i + 1;
    let start: i64 = i;
    while i < buf.len && buf[i] != 32 && buf[i] != 63 && buf[i] != 10 && buf[i] != 13 {
        i = i + 1;
    }
    if i == start {
        return -1;
    }
    info[2] = start;
    info[3] = i - start;
    return i - start;
}

// Whether the client asked to keep the connection open.
export fn ht_keep_alive(buf: const u8[:]) -> i64 {
    let h: i64 = ht_contains_ci(buf, "connection:");
    if h < 0 {
        return 0;
    }
    var rest: i64 = h;
    let keep: i64 = ht_contains_ci(buf, "keep-alive");
    let close_: i64 = ht_contains_ci(buf, "close");
    if keep < 0 {
        return 0;
    }
    if close_ >= 0 && close_ < keep {
        return 0;
    }
    return 1;
}

// Case-insensitive byte search. Returns the offset or -1.
export fn ht_contains_ci(hay: const u8[:], needle: const u8[:]) -> i64 {
    if needle.len == 0 || needle.len > hay.len {
        return -1;
    }
    var i: i64 = 0;
    while i + needle.len <= hay.len {
        var k: i64 = 0;
        var hit: bool = true;
        while k < needle.len {
            var a: u8 = hay[i + k];
            var b: u8 = needle[k];
            if a >= 65 && a <= 90 {
                a = (a + 32) as! u8;
            }
            if b >= 65 && b <= 90 {
                b = (b + 32) as! u8;
            }
            if a != b {
                hit = false;
                k = needle.len;
            } else {
                k = k + 1;
            }
        }
        if hit {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

// Write the whole buffer, looping over short writes. Returns 0 or -1.
export fn ht_write_all(fd: i64, buf: const u8[:]) -> i64 {
    return ht_write_all_impl(fd, buf);
}

fn ht_write_all_impl(fd: i64, buf: const u8[:]) -> i64 {
    var sent: i64 = 0;
    while sent < buf.len {
        let n: i64 = gx_write(fd, buf[sent .. buf.len], buf.len - sent);
        if n <= 0 {
            return -1;
        }
        sent = sent + n;
    }
    return 0;
}

// Response head: status line, Content-Length, content type, connection rule.
// `out` receives the head; returns its length, or -1 when `out` is too small.
export fn ht_response_head(status: i64, body_len: i64, keep: i64, out: u8[:]) -> i64 {
    return ht_response_head_impl(status, body_len, keep, out);
}

fn ht_response_head_impl(status: i64, body_len: i64, keep: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = ht_put(out, at, "HTTP/1.1 ");
    at = ht_put_int(out, at, status);
    if status == 200 {
        at = ht_put(out, at, " OK\r\n");
    } else if status == 400 {
        at = ht_put(out, at, " Bad Request\r\n");
    } else if status == 404 {
        at = ht_put(out, at, " Not Found\r\n");
    } else if status == 413 {
        at = ht_put(out, at, " Content Too Large\r\n");
    } else if status == 500 {
        at = ht_put(out, at, " Internal Server Error\r\n");
    } else {
        at = ht_put(out, at, "\r\n");
    }
    at = ht_put(out, at, "Content-Type: application/json\r\nContent-Length: ");
    at = ht_put_int(out, at, body_len);
    at = ht_put(out, at, "\r\n");
    if keep != 0 {
        at = ht_put(out, at, "Connection: keep-alive\r\n");
    } else {
        at = ht_put(out, at, "Connection: close\r\n");
    }
    at = ht_put(out, at, "\r\n");
    return at;
}

fn ht_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    if at < 0 || s.len > out.len - at {
        return -1;
    }
    var i: i64 = 0;
    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn ht_put_int(out: u8[:], at: i64, v: i64) -> i64 {
    if at < 0 || v < 0 {
        return -1;
    }
    var digits: u8[20] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var n: i64 = 0;
    var x: i64 = v;
    if x == 0 {
        digits[0] = 48;
        n = 1;
    }
    while x > 0 {
        digits[n] = (48 + (x % 10)) as! u8;
        x = x / 10;
        n = n + 1;
    }
    if at + n > out.len {
        return -1;
    }
    var i: i64 = 0;
    while i < n {
        out[at + i] = digits[n - 1 - i];
        i = i + 1;
    }
    return at + n;
}
