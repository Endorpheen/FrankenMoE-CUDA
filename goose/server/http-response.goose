// P8-02 S2: HTTP response serialisation the llama server actually emits.
//
// Native source: vendor/flint/vendor/cpp-httplib/httplib.cpp at the base commit,
// the writer the shipping server uses:
//
//   write_response_line          "HTTP/1.1 " + status + ' ' + reason + CRLF   4049-4056
//   write_headers                name + ": " + value + CRLF, per pair          4058-4085
//   status_message               the reason-phrase table                      6642-6700
//
// What is reproduced, each point checked against the library rather than read
// off the source by eye:
//   - the reason phrases, all 63 of them, byte for byte;
//   - an UNKNOWN code yields "Internal Server Error", not an empty phrase: the
//     switch has `default: case InternalServerError_500:`. So 599 renders as
//     "HTTP/1.1 599 Internal Server Error\r\n", and the module does the same;
//   - header order is the caller's order, never sorted;
//   - every line ends CRLF and the header block ends with one empty CRLF;
//   - a chunk is its size in lower-case hexadecimal, CRLF, the bytes, CRLF; a
//     zero-length chunk is the terminator "0\r\n\r\n".
//
// Not owned here: the socket, reading the request, TLS and which headers a route
// sets. Those are transport and routing; this module formats bytes a transport
// writes.

import std;

const RS_CRLF: const u8[:] = "\r\n";
// httplib renders an unknown status through the default arm.
const RS_FALLBACK: const u8[:] = "Internal Server Error";

const RS_STATUS_CODE: i64[63] = [100, 101, 102, 103, 200, 201, 202, 203, 204, 205, 206, 207, 208, 226, 300, 301, 302, 303, 304, 305, 306, 307, 308, 400, 401, 402, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 421, 422, 423, 424, 425, 426, 428, 429, 431, 451, 501, 502, 503, 504, 505, 506, 507, 508, 510, 511, 500];

fn rs_phrase(i: i64) -> const u8[:] {
    if i == 0 { return "Continue"; }
    if i == 1 { return "Switching Protocol"; }
    if i == 2 { return "Processing"; }
    if i == 3 { return "Early Hints"; }
    if i == 4 { return "OK"; }
    if i == 5 { return "Created"; }
    if i == 6 { return "Accepted"; }
    if i == 7 { return "Non-Authoritative Information"; }
    if i == 8 { return "No Content"; }
    if i == 9 { return "Reset Content"; }
    if i == 10 { return "Partial Content"; }
    if i == 11 { return "Multi-Status"; }
    if i == 12 { return "Already Reported"; }
    if i == 13 { return "IM Used"; }
    if i == 14 { return "Multiple Choices"; }
    if i == 15 { return "Moved Permanently"; }
    if i == 16 { return "Found"; }
    if i == 17 { return "See Other"; }
    if i == 18 { return "Not Modified"; }
    if i == 19 { return "Use Proxy"; }
    if i == 20 { return "unused"; }
    if i == 21 { return "Temporary Redirect"; }
    if i == 22 { return "Permanent Redirect"; }
    if i == 23 { return "Bad Request"; }
    if i == 24 { return "Unauthorized"; }
    if i == 25 { return "Payment Required"; }
    if i == 26 { return "Forbidden"; }
    if i == 27 { return "Not Found"; }
    if i == 28 { return "Method Not Allowed"; }
    if i == 29 { return "Not Acceptable"; }
    if i == 30 { return "Proxy Authentication Required"; }
    if i == 31 { return "Request Timeout"; }
    if i == 32 { return "Conflict"; }
    if i == 33 { return "Gone"; }
    if i == 34 { return "Length Required"; }
    if i == 35 { return "Precondition Failed"; }
    if i == 36 { return "Payload Too Large"; }
    if i == 37 { return "URI Too Long"; }
    if i == 38 { return "Unsupported Media Type"; }
    if i == 39 { return "Range Not Satisfiable"; }
    if i == 40 { return "Expectation Failed"; }
    if i == 41 { return "I'm a teapot"; }
    if i == 42 { return "Misdirected Request"; }
    if i == 43 { return "Unprocessable Content"; }
    if i == 44 { return "Locked"; }
    if i == 45 { return "Failed Dependency"; }
    if i == 46 { return "Too Early"; }
    if i == 47 { return "Upgrade Required"; }
    if i == 48 { return "Precondition Required"; }
    if i == 49 { return "Too Many Requests"; }
    if i == 50 { return "Request Header Fields Too Large"; }
    if i == 51 { return "Unavailable For Legal Reasons"; }
    if i == 52 { return "Not Implemented"; }
    if i == 53 { return "Bad Gateway"; }
    if i == 54 { return "Service Unavailable"; }
    if i == 55 { return "Gateway Timeout"; }
    if i == 56 { return "HTTP Version Not Supported"; }
    if i == 57 { return "Variant Also Negotiates"; }
    if i == 58 { return "Insufficient Storage"; }
    if i == 59 { return "Loop Detected"; }
    if i == 60 { return "Not Extended"; }
    if i == 61 { return "Network Authentication Required"; }
    if i == 62 { return "Internal Server Error"; }
    return RS_FALLBACK;
}

// The reason phrase for a status code; a code outside the table gets the same
// phrase httplib's default arm returns.
export fn rs_reason(status: i64) -> const u8[:] {
    var i: i64 = 0;
    while i < 63 {
        if RS_STATUS_CODE[i] == status {
            return rs_phrase(i);
        }
        i = i + 1;
    }
    return RS_FALLBACK;
}

fn rs_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var k: i64 = 0;
    while k < s.len {
        if at + k >= out.len { return 0 - 1; }
        out[at + k] = s[k];
        k = k + 1;
    }
    return at + s.len;
}

fn rs_num(out: u8[:], at: i64, v: i64) -> i64 {
    if v == 0 {
        if at < out.len { out[at] = 48; return at + 1; }
        return 0 - 1;
    }
    var digits: u8[20] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var n: i64 = 0;
    var x: i64 = v;
    while x > 0 && n < 20 {
        digits[n] = 48 + (x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var k: i64 = n;
    while k > 0 {
        if at + (n - k) >= out.len { return 0 - 1; }
        out[at + (n - k)] = digits[k - 1];
        k = k - 1;
    }
    return at + n;
}

// "HTTP/1.1 <status> <reason>\r\n", exactly as write_response_line emits it.
// Returns the byte count, or -(64 + 1) when `out` cannot hold a status line.
export fn rs_status_line(status: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = rs_put(out, at, "HTTP/1.1 ");
    if at < 0 { return 0 - (64 + 1); }
    at = rs_num(out, at, status);
    if at < 0 { return 0 - (64 + 1); }
    at = rs_put(out, at, " ");
    if at < 0 { return 0 - (64 + 1); }
    at = rs_put(out, at, rs_reason(status));
    if at < 0 { return 0 - (64 + 1); }
    at = rs_put(out, at, RS_CRLF);
    if at < 0 { return 0 - (64 + 1); }
    return at;
}

// One header line. Order is the caller's order: httplib writes the map as given.
export fn rs_header(name: const u8[:], value: const u8[:], out: u8[:], at_in: i64) -> i64 {
    if name.len == 0 { return 0 - 1; }
    var at: i64 = at_in;
    at = rs_put(out, at, name);
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, ": ");
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, value);
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, RS_CRLF);
    if at < 0 { return 0 - 1; }
    return at;
}

// The blank line that ends the header block.
export fn rs_end_headers(out: u8[:], at_in: i64) -> i64 {
    return rs_put(out, at_in, RS_CRLF);
}

// One chunk: lower-case hex size, CRLF, bytes, CRLF.
export fn rs_chunk(data: const u8[:], out: u8[:], at_in: i64) -> i64 {
    var at: i64 = at_in;
    var digits: u8[16] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    var n: i64 = 0;
    var x: i64 = data.len;
    if x == 0 {
        digits[0] = 48;
        n = 1;
    }
    while x > 0 && n < 16 {
        let d: i64 = x % 16;
        digits[n] = 48 + (if d < 10 { d } else { d + 39 }) as u8;
        x = x / 16;
        n = n + 1;
    }
    var k: i64 = n;
    while k > 0 {
        if at >= out.len { return 0 - 1; }
        out[at] = digits[k - 1];
        at = at + 1;
        k = k - 1;
    }
    at = rs_put(out, at, RS_CRLF);
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, data);
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, RS_CRLF);
    if at < 0 { return 0 - 1; }
    return at;
}

// The last chunk of a streamed body.
export fn rs_chunk_end(out: u8[:], at_in: i64) -> i64 {
    var at: i64 = at_in;
    at = rs_put(out, at, "0");
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, RS_CRLF);
    if at < 0 { return 0 - 1; }
    at = rs_put(out, at, RS_CRLF);
    if at < 0 { return 0 - 1; }
    return at;
}
