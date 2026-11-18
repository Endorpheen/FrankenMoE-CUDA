// P8-02 S3: HTTP request parsing, the half that needs no socket.
//
// Native source: vendor/flint/vendor/cpp-httplib/httplib.cpp at the base commit -
// Server::parse_request_line (9081-9098) and the field validators in
// namespace fields (5689-5760). A transport needs to read bytes and then decide
// what they mean; this module owns the decision, so the missing half is a socket
// and a loop, not a parser.
//
// The rules exactly as native applies them:
//   - the line must end with CRLF; without it the request is rejected;
//   - there must be EXACTLY two SP in the line. Other whitespace is not a
//     separator: a recipient that parses more strictly would see a different
//     request, so native refuses rather than guessing;
//   - the method is non-empty, the target is non-empty and the version is
//     non-empty; native refuses `sp1 == s`, `sp2 == sp1 + 1` and `sp2 + 1 == end`;
//   - the version must be "HTTP/1.1" or "HTTP/1.0";
//   - the target has no '#' - a fragment stripped here would make this server route
//     a different path than a cache in front of it sees;
//   - a method is a token: ASCII alphanumeric plus !#$%&'*+-.^_`|~;
//   - a field value is field-content: empty is valid, otherwise it starts and ends
//     with a visible character and may contain SP, HTAB and visible/obs-text
//     between them.
//
// Not owned here: the socket, TLS, reading the body, routing the parsed path.
// The prefix is rq_.

import std;

fn rq_is_alnum(c: u8) -> bool {
    return (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122);
}

// httplib fields::is_token_char
export fn rq_is_token_char(c: i64) -> bool {
    if rq_is_alnum(c as u8) {
        return true;
    }
    return c == 33 || c == 35 || c == 36 || c == 37 || c == 38 || c == 39 || c == 42 ||
           c == 43 || c == 45 || c == 46 || c == 94 || c == 95 || c == 96 || c == 124 ||
           c == 126;
}

export fn rq_is_token(s: const u8[:]) -> bool {
    return rq_is_token_impl(s);
}

// Same decision for callers inside the unit: an export keeps one concrete
// signature, so internal callers use this body.
fn rq_is_token_impl(s: const u8[:]) -> bool {
    if s.len == 0 {
        return false;
    }
    var i: i64 = 0;
    while i < s.len {
        if !rq_is_token_char(s[i] as i64) {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// httplib fields::is_vchar / is_obs_text / is_field_vchar
export fn rq_is_field_vchar(c: i64) -> bool {
    return (c >= 33 && c <= 126) || c >= 128;
}

// httplib fields::is_field_content, which is what is_field_value delegates to.
export fn rq_is_field_content(s: const u8[:]) -> bool {
    return rq_is_field_content_impl(s);
}

fn rq_is_field_content_impl(s: const u8[:]) -> bool {
    if s.len == 0 {
        return true;
    }
    if s.len == 1 {
        return rq_is_field_vchar(s[0] as i64);
    }
    if s.len == 2 {
        return rq_is_field_vchar(s[0] as i64) && rq_is_field_vchar(s[1] as i64);
    }
    if !rq_is_field_vchar(s[0] as i64) {
        return false;
    }
    var i: i64 = 1;
    while i < s.len - 1 {
        let c: i64 = s[i] as i64;
        if c != 32 && c != 9 && !rq_is_field_vchar(c) {
            return false;
        }
        i = i + 1;
    }
    return rq_is_field_vchar(s[s.len - 1] as i64);
}

export fn rq_is_field_valid(name: const u8[:], value: const u8[:]) -> bool {
    return rq_is_field_valid_impl(name, value);
}

fn rq_is_field_valid_impl(name: const u8[:], value: const u8[:]) -> bool {
    return rq_is_token_impl(name) && rq_is_field_content_impl(value);
}

fn rq_eq(s: const u8[:], lit: const u8[:]) -> bool {
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

// Parses a request line INCLUDING its CRLF, as the server reads it.
// out: 0 accept flag, 1/2 method off/len, 3/4 target off/len, 5/6 version off/len,
//      7 version code (2 = HTTP/1.1, 1 = HTTP/1.0, 0 refused).
// Returns 0 when accepted, -1 otherwise; out[0] mirrors that for a caller that
// only has the array.
export fn rq_parse_line(line: const u8[:], out: i64[:]) -> i64 {
    return rq_parse_line_impl(line, out);
}

fn rq_parse_line_impl(line: const u8[:], out: i64[:]) -> i64 {
    if out.len < 8 {
        return -1;
    }
    var z: i64 = 0;
    while z < 8 {
        out[z] = 0;
        z = z + 1;
    }
    // CRLF is required, and the length must leave room for three fields.
    if line.len < 3 || line[line.len - 2] != 13 || line[line.len - 1] != 10 {
        return -1;
    }
    let end: i64 = line.len - 2;
    var spaces: i64 = 0;
    var i: i64 = 0;
    while i < end {
        if line[i] == 32 {
            spaces = spaces + 1;
        }
        i = i + 1;
    }
    if spaces != 2 {
        return -1;
    }
    var sp1: i64 = 0;
    while line[sp1] != 32 {
        sp1 = sp1 + 1;
    }
    var sp2: i64 = sp1 + 1;
    while line[sp2] != 32 {
        sp2 = sp2 + 1;
    }
    if sp1 == 0 || sp2 == sp1 + 1 || sp2 + 1 == end {
        return -1;
    }
    let method: const u8[:] = line[0 .. sp1];
    let target: const u8[:] = line[sp1 + 1 .. sp2];
    let version: const u8[:] = line[sp2 + 1 .. end];

    var vcode: i64 = 0;
    if rq_eq(version, "HTTP/1.1") {
        vcode = 2;
    } else if rq_eq(version, "HTTP/1.0") {
        vcode = 1;
    } else {
        return -1;
    }
    // The target: visible/obs-text only, and no fragment.
    i = 0;
    while i < target.len {
        if !rq_is_field_vchar(target[i] as i64) {
            return -1;
        }
        if target[i] == 35 {
            return -1;
        }
        i = i + 1;
    }
    out[0] = 1;
    out[1] = 0; out[2] = sp1;
    out[3] = sp1 + 1; out[4] = sp2 - sp1 - 1;
    out[5] = sp2 + 1; out[6] = end - sp2 - 1;
    out[7] = vcode;
    return 0;
}

// Parses one header line INCLUDING its CRLF. `out`: 0 accept flag, 1/2 name
// off/len, 3/4 value off/len after OWS trim. Returns 0 or -1.
export fn rq_parse_header(line: const u8[:], out: i64[:]) -> i64 {
    if out.len < 5 {
        return -1;
    }
    var z: i64 = 0;
    while z < 5 {
        out[z] = 0;
        z = z + 1;
    }
    if line.len < 3 || line[line.len - 2] != 13 || line[line.len - 1] != 10 {
        return -1;
    }
    let end: i64 = line.len - 2;
    var colon: i64 = 0;
    while colon < end && line[colon] != 58 {
        colon = colon + 1;
    }
    if colon == 0 || colon >= end {
        return -1;
    }
    let name: const u8[:] = line[0 .. colon];
    var v0: i64 = colon + 1;
    var v1: i64 = end;
    // OWS: SP and HTAB around the value are not part of it.
    while v0 < v1 && (line[v0] == 32 || line[v0] == 9) {
        v0 = v0 + 1;
    }
    while v1 > v0 && (line[v1 - 1] == 32 || line[v1 - 1] == 9) {
        v1 = v1 - 1;
    }
    let value: const u8[:] = line[v0 .. v1];
    if !rq_is_field_valid_impl(name, value) {
        return -1;
    }
    out[0] = 1;
    out[1] = 0; out[2] = colon;
    out[3] = v0; out[4] = v1 - v0;
    return 0;
}
