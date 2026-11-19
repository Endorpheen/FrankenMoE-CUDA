// P8-09 S6: the API-key middleware of the server's pre-routing handler.
//
// Native source: upstream/llama.cpp/tools/server/server-http.cpp at base commit
// 050354e9:
//
//   frontend_paths           187-194   "/" plus "/" + every embedded asset name
//   get_public_endpoints     196-204   those paths plus /health and /v1/health
//   middleware_validate_api_key 206-251
//   the 401 body             235-246   json {error:{message,type,code}} dumped
//   the register call order  300-305   server state, then the key check
//
// The middleware is a chain of four decisions and nothing else:
//
//   1. no configured key at all: every request passes, the header is not read;
//   2. the request path is one of the public endpoints: it passes. The set is an
//      exact match on the request path, so "/health/" and "/Health" are not
//      public, and every UI asset is public by name, "/" included;
//   3. the credential is the Authorization header, and only when that header is
//      empty is X-Api-Key read instead (an empty header and an absent header are
//      the same event: httplib returns "");
//   4. an exact "Bearer " prefix is removed once, case sensitively, and the rest
//      must equal a configured key byte for byte. A value shorter than the
//      prefix is unchanged, "bearer " and "BEARER " are not prefixes, and a
//      stripped-to-empty credential only matches an empty configured key.
//
// A rejected request is 401 with the JSON body below and the JSON content type.
// The body is the common_json dump of the native initializer: common_json keeps
// insertion order (server-common.cpp:1553-1555, dump_safe), so the text is
// "error" then message/type/code. The literal is exported so the caller never
// rebuilds it and cannot drift from it.
//
// The public path table arrives as a packed blob plus count + 1 offsets, the
// layout goose/server/web-assets.goose already uses; the caller builds it once
// from llama_ui_get_assets() exactly as the web slice does, plus "/" first. The
// fixed /health and /v1/health entries are owned here, not by the table.
//
// The prefix is key_. Not owned: header lookup and case folding (the caller
// hands in the two header values), the HTTP response object, the warning log
// line, the configuration parser that fills the key list, and the separate
// "model not ready" middleware.

import std;

const KEY_BEARER: const u8[:] = "Bearer ";
const KEY_ERROR_BODY: const u8[:] = "{\"error\":{\"message\":\"Invalid API Key\",\"type\":\"authentication_error\",\"code\":401}}";
const KEY_ERROR_TYPE: const u8[:] = "application/json; charset=utf-8";

// out[at ..] = s, and -1 when the caller buffer cannot hold the text.
fn key_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
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

fn key_eq(a: const u8[:], b: const u8[:]) -> bool {
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

fn key_name(names: const u8[:], off: const i64[:], i: i64) -> const u8[:] {
    let from: i64 = off[i];
    let to: i64 = off[i + 1];

    if from < 0 || to < from || to > names.len {
        return names[0 .. 0];
    }
    return names[from .. to];
}

// The public-endpoint test: the two fixed paths, then the frontend path table.
fn key_public(path: const u8[:], paths: const u8[:], off: const i64[:]) -> i64 {
    let cnt: i64 = off.len - 1;

    if key_eq(path, "/health") {
        return 1;
    }
    if key_eq(path, "/v1/health") {
        return 1;
    }
    var i: i64 = 0;

    while i < cnt {
        if key_eq(path, key_name(paths, off, i)) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

export fn key_is_public(path: const u8[:], paths: const u8[:], off: const i64[:]) -> i64 {
    return key_public(path, paths, off);
}

// The credential selection: Authorization unless it is empty, then X-Api-Key.
export fn key_credential(auth: const u8[:], apikey: const u8[:], out: u8[:]) -> i64 {
    if auth.len != 0 {
        return key_put(out, 0, auth);
    }
    return key_put(out, 0, apikey);
}

// One exact "Bearer " prefix removed, or the value unchanged.
export fn key_strip_bearer(value: const u8[:], out: u8[:]) -> i64 {
    if value.len >= KEY_BEARER.len && key_eq(value[0 .. KEY_BEARER.len], KEY_BEARER) {
        return key_put(out, 0, value[KEY_BEARER.len .. value.len]);
    }
    return key_put(out, 0, value);
}

// An exact match against one configured key. Goose refuses a call to an export
// from another function, so the body is private and the exports below wrap it.
fn key_scan(keys: const u8[:], koff: const i64[:], candidate: const u8[:]) -> i64 {
    let cnt: i64 = koff.len - 1;
    var i: i64 = 0;

    while i < cnt {
        if key_eq(key_name(keys, koff, i), candidate) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

export fn key_matches(keys: const u8[:], koff: const i64[:], candidate: const u8[:]) -> i64 {
    return key_scan(keys, koff, candidate);
}

// Selection, prefix removal and match in one pass over the key list, so a
// caller that only needs the verdict never allocates a credential buffer.
fn key_match_credential_impl(
    keys: const u8[:],
    koff: const i64[:],
    auth: const u8[:],
    apikey: const u8[:]) -> i64 {
    var cand: const u8[:] = auth;

    if cand.len == 0 {
        cand = apikey;
    }
    if cand.len >= KEY_BEARER.len && key_eq(cand[0 .. KEY_BEARER.len], KEY_BEARER) {
        return key_scan(keys, koff, cand[KEY_BEARER.len .. cand.len]);
    }
    return key_scan(keys, koff, cand);
}

export fn key_match_credential(
    keys: const u8[:],
    koff: const i64[:],
    auth: const u8[:],
    apikey: const u8[:]) -> i64 {
    return key_match_credential_impl(keys, koff, auth, apikey);
}

// The whole middleware decision: 1 lets the request through, 0 is the 401 path.
export fn key_authorize(
    keys: const u8[:],
    koff: const i64[:],
    path: const u8[:],
    paths: const u8[:],
    poff: const i64[:],
    auth: const u8[:],
    apikey: const u8[:]) -> i64 {
    return key_authorize_impl(keys, koff, path, paths, poff, auth, apikey);
}

fn key_authorize_impl(
    keys: const u8[:],
    koff: const i64[:],
    path: const u8[:],
    paths: const u8[:],
    poff: const i64[:],
    auth: const u8[:],
    apikey: const u8[:]) -> i64 {
    // no key configured: validation is skipped entirely
    if koff.len - 1 <= 0 {
        return 1;
    }
    if key_public(path, paths, poff) != 0 {
        return 1;
    }
    return key_match_credential_impl(keys, koff, auth, apikey);
}

// The 401 response body, byte for byte the native dump.
export fn key_error_body(out: u8[:]) -> i64 {
    return key_put(out, 0, KEY_ERROR_BODY);
}

// The 401 response content type.
export fn key_error_type(out: u8[:]) -> i64 {
    return key_put(out, 0, KEY_ERROR_TYPE);
}
