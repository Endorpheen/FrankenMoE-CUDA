// P8-09 S4: the CORS decision of the server's pre-routing handler, plus the
// localhost origin test it uses.
//
// Native source: upstream/llama.cpp/tools/server/server-http.cpp at base commit
// 050354e9:
//
//   origin_is_localhost              50-58   host == localhost / 127.0.0.1 / ::1
//   cors_origins == "localhost"     281-288   echo only a localhost origin
//   cors_credentials && origins "*" 278-280   echo any origin
//   static origin                   289-291   set the configured literal
//   OPTIONS preflight               292-299   credentials/methods/headers, blank
//                                             text/html body, handled early
//
// The module owns the branch decision and the three literals that belong to it:
// the credentials text, the preflight content type and the blank body. The
// allowed methods and allowed headers are configured values the native code
// copies verbatim, so they stay with the caller and are not an export here.
//
// Native rules, in native order:
//
//   - credentials set and cors_origins == "*" is the wildcard-with-credentials
//     case: the Origin header is echoed back as Access-Control-Allow-Origin even
//     when it is empty, so the header is always set and may carry zero bytes;
//   - cors_origins == "localhost": the header is set to the Origin value only
//     when the origin is non-empty and its host is localhost / 127.0.0.1 / ::1;
//     a non-empty origin that fails the test sets no header and logs a warning;
//     an empty origin is silent;
//   - every other value of cors_origins is a static Access-Control-Allow-Origin,
//     set unconditionally, empty string included;
//   - the preflight branch is an exact "OPTIONS" method match (case sensitive);
//     it sets credentials, methods and headers, sets an empty text/html body and
//     returns "handled" so no route runs.
//
// origin_is_localhost native behaviour: common_http_parse_url(origin) may throw
// (no scheme, unsupported scheme, bad port); the catch is the only other exit
// and it returns false. The host is then compared byte-for-byte against the
// three literals, case sensitively, so "LOCALHOST" and "localhost." are false
// and a bracketed IPv6 literal is unbracketed by the parser before the compare.
//
// That parse is owned by goose/server/http-url-policy.goose. This module never
// parses a URL: it takes the parsed host and a "parsed" flag, so a caller that
// caught the native exception passes parsed = 0 and gets the native false. The
// port therefore cannot re-derive the host slice and cannot disagree with the
// URL policy module.
//
// The prefix is cors_. Not owned: sockets, the Origin header lookup, the
// response object, the warning log line, header ordering, and the placeholder
// response body object.

import std;

const CORS_CREDENTIALS_TRUE: const u8[:] = "true";
const CORS_CREDENTIALS_FALSE: const u8[:] = "false";
const CORS_PREFLIGHT_TYPE: const u8[:] = "text/html";

// out[at ..] = s, and -1 when the caller buffer cannot hold the text.
fn cors_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    if at < 0 || at + s.len > out.len {
        return 0 - 1;
    }
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn cors_eq(a: const u8[:], b: const u8[:]) -> bool {
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

// The host comparison of origin_is_localhost, on the host slice that
// http-url-policy already produced. parsed = 0 is the native catch: false.
export fn cors_is_localhost(host: const u8[:], parsed: i64) -> i64 {
    if parsed == 0 {
        return 0;
    }
    if cors_eq(host, "localhost") {
        return 1;
    }
    if cors_eq(host, "127.0.0.1") {
        return 1;
    }
    if cors_eq(host, "::1") {
        return 1;
    }
    return 0;
}

// Access-Control-Allow-Origin for one request.
//
//   res[0] = 1 when the header is set, 0 when it is left out
//   res[1] = 1 when native logs "(CORS) skip non-localhost origin"
//   out    = the header value bytes
//
// Returns the number of value bytes written to out, or -1 when no header is set
// (a header value may legitimately be zero bytes: the wildcard-with-credentials
// branch echoes an absent Origin). origin_local is cors_is_localhost over the
// parsed host of this same origin.
export fn cors_allow_origin(
    origins: const u8[:],
    credentials: i64,
    origin: const u8[:],
    origin_local: i64,
    out: u8[:],
    res: i64[:]) -> i64 {
    return cors_allow_origin_impl(origins, credentials, origin, origin_local, out, res);
}

fn cors_allow_origin_impl(
    origins: const u8[:],
    credentials: i64,
    origin: const u8[:],
    origin_local: i64,
    out: u8[:],
    res: i64[:]) -> i64 {
    res[0] = 0;
    res[1] = 0;

    if credentials != 0 && cors_eq(origins, "*") {
        let n = cors_put(out, 0, origin);

        // the header is claimed only when its value made it into the buffer
        if n >= 0 {
            res[0] = 1;
        }
        return n;
    }
    if cors_eq(origins, "localhost") {
        if origin.len != 0 && origin_local != 0 {
            let n = cors_put(out, 0, origin);

            if n >= 0 {
                res[0] = 1;
            }
            return n;
        }
        if origin.len != 0 {
            res[1] = 1;
        }
        return 0 - 1;
    }
    let n = cors_put(out, 0, origins);

    if n >= 0 {
        res[0] = 1;
    }
    return n;
}

// The native `req.method == "OPTIONS"`, exact and case sensitive.
export fn cors_is_preflight(method: const u8[:]) -> i64 {
    if cors_eq(method, "OPTIONS") {
        return 1;
    }
    return 0;
}

// Access-Control-Allow-Credentials text, the native `? "true" : "false"`.
export fn cors_credentials_text(credentials: i64, out: u8[:]) -> i64 {
    if credentials != 0 {
        return cors_put(out, 0, CORS_CREDENTIALS_TRUE);
    }
    return cors_put(out, 0, CORS_CREDENTIALS_FALSE);
}

// Content type of the preflight placeholder response.
export fn cors_preflight_type(out: u8[:]) -> i64 {
    return cors_put(out, 0, CORS_PREFLIGHT_TYPE);
}
