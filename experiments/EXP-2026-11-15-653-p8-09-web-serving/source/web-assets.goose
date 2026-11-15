// P8-09 S2: the web UI serve decision - which asset a request gets, under
// which caching policy, with which headers.
//
// Native source: upstream/llama.cpp/tools/server/server-http.cpp at base commit
// ec50a32d, the embedded-asset branch of server_http_context::init:
//
//   handle_gzip_header        the 415 gate and Content-Encoding          341-355
//   cache_immutable / revalidate                                      357-360
//   serve_asset_cached        ETag, 304, isolation, Cache-Control      362-387
//   serve_asset_nocache       no ETag, no isolation                    389-401
//   index registration        "/" and "/index.html"                    403-404
//   no_cache_names            sw.js, manifest, version, build.json     409-414
//   remaining registrations   prefix + "/" + name, index skipped        416-425
//
// The module decides, for one request path and the build's asset table, the
// response shape: status, which asset, which of the five response headers are
// set, the gzip encoding flag, the isolation flag and the bytes of
// Cache-Control. Native rules, in native order:
//
//   - a route is prefix + "/" + name, an exact whole-path match; "/" and
//     "/index.html" are both the index asset;
//   - an unknown path is 404 and carries no headers at all. The native handlers
//     are registered per route, so an unregistered path never enters one: the
//     gzip gate below is not consulted and no Content-Encoding is set;
//   - a registered asset in a gzip build with no "gzip" in Accept-Encoding is
//     415 text/plain, before the ETag is looked at;
//   - the two cached shapes (the index and every immutable asset) get ETag, and
//     then If-None-Match equal to the ETag or to "W/" + ETag turns the answer
//     into 304 and returns immediately: a 304 therefore has ETag and
//     Content-Encoding but NO Cache-Control and NO cross-origin isolation;
//   - the four no_cache_names use serve_asset_nocache: no ETag at all, so their
//     If-None-Match is ignored and they always answer 200;
//   - only the index asset gets Cross-Origin-Embedder-Policy / -Opener-Policy;
//   - Cache-Control is "no-cache" for the index and for no_cache_names, and
//     "public, max-age=31536000, immutable" for everything else;
//   - Content-Encoding: gzip is set whenever the gzip build accepted the
//     request, including on 304; a non-gzip build never sets it;
//   - Content-Type is the asset type on 200 and text/plain on 415.
//
// The asset table arrives as one packed name blob plus per-asset offsets, and a
// parallel ETag blob plus offsets: the runtime owns the table, this module owns
// the decision. Header text goes into a caller buffer, never a static pointer.
//
// The prefix is web_. Not owned: sockets, threads, TLS, the mount point for
// --path (httplib does the file I/O there), UUID/templating, CORS, percent
// decoding, request queuing and the API routes.

import std;

// Response header bits, in the order native sets them.
const WEB_H_ETAG: i64 = 1;
const WEB_H_ISOLATION: i64 = 2;
const WEB_H_CACHE: i64 = 4;
const WEB_H_ENCODING: i64 = 8;
const WEB_H_TYPE: i64 = 16;

const WEB_OK: i64 = 200;
const WEB_NOT_MODIFIED: i64 = 304;
const WEB_NOT_FOUND: i64 = 404;
const WEB_UNSUPPORTED_MEDIA: i64 = 415;

const WEB_POLICY_IMMUTABLE: i64 = 0;
const WEB_POLICY_REVALIDATE: i64 = 1;
const WEB_POLICY_NOCACHE: i64 = 2;

const WEB_CACHE_IMMUTABLE: const u8[:] = "public, max-age=31536000, immutable";
const WEB_CACHE_NO_CACHE: const u8[:] = "no-cache";

fn web_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn web_eq(a: const u8[:], b: const u8[:]) -> bool {
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

// An asset name out of the packed table: names[off[i] .. off[i+1]] with the
// last entry ending at names.len. off has cnt + 1 entries.
fn web_name(names: const u8[:], off: const i64[:], i: i64) -> const u8[:] {
    let from: i64 = off[i];
    let to: i64 = off[i + 1];

    if from < 0 || to < from || to > names.len {
        return names[0 .. 0];
    }
    return names[from .. to];
}

fn web_name_is(names: const u8[:], off: const i64[:], i: i64, lit: const u8[:]) -> bool {
    return web_eq(web_name(names, off, i), lit);
}

// Index of "index.html" in the table, -1 when the bundle has none.
fn web_index_of(names: const u8[:], off: const i64[:], cnt: i64) -> i64 {
    var i: i64 = 0;

    while i < cnt {
        if web_name_is(names, off, i, "index.html") {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

fn web_starts(path: const u8[:], pre: const u8[:]) -> bool {
    if path.len < pre.len {
        return false;
    }
    var i: i64 = 0;

    while i < pre.len {
        if path[i] != pre[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// The native route table, as one decision: the asset index for a whole request
// path, or -1 for an unregistered path. "/" and "/index.html" resolve to the
// index asset; every other asset needs an exact prefix + "/" + name match.
// Goose refuses a call to an export fn from another function, so the body is
// private and web_serve below calls it directly; the export wrapper is the
// public surface.
fn web_route_idx(
    path: const u8[:],
    prefix: const u8[:],
    names: const u8[:],
    off: const i64[:],
    cnt: i64) -> i64 {
    if !web_starts(path, prefix) || path.len < prefix.len {
        return 0 - 1;
    }
    let rest = path[prefix.len .. path.len];

    // every registered route is prefix + "/" + something, index included
    if rest.len < 1 || rest[0] != 47 {
        return 0 - 1;
    }
    let idx = web_index_of(names, off, cnt);

    if rest.len == 1 {
        return idx;
    }
    if web_eq(rest, "/index.html") {
        return idx;
    }
    var i: i64 = 0;
    while i < cnt {
        // index.html is already handled above, so the table entry matches only
        // the unusable duplicate spelling
        if rest.len == web_name(names, off, i).len + 1 {
            if web_eq(rest[1 .. rest.len], web_name(names, off, i)) {
                return i;
            }
        }
        i = i + 1;
    }
    return 0 - 1;
}

export fn web_route(
    path: const u8[:],
    prefix: const u8[:],
    names: const u8[:],
    off: const i64[:],
    cnt: i64) -> i64 {
    return web_route_idx(path, prefix, names, off, cnt);
}

fn web_is(target: i64, id: i64) -> bool {
    return target == id;
}

// Cache policy id of an asset name, exactly the native registration split.
fn web_policy_of(names: const u8[:], off: const i64[:], idx: i64) -> i64 {
    if web_name_is(names, off, idx, "index.html") {
        return WEB_POLICY_REVALIDATE;
    }
    if web_name_is(names, off, idx, "sw.js")
        || web_name_is(names, off, idx, "manifest.webmanifest")
        || web_name_is(names, off, idx, "_app/version.json")
        || web_name_is(names, off, idx, "build.json") {
        return WEB_POLICY_NOCACHE;
    }
    return WEB_POLICY_IMMUTABLE;
}

// Accept-Encoding contains "gzip" (the native std::string::find).
fn web_accepts_gzip(accept: const u8[:]) -> bool {
    var i: i64 = 0;
    while i + 4 <= accept.len {
        if accept[i] == 103 && accept[i + 1] == 122 && accept[i + 2] == 105 && accept[i + 3] == 112 {
            return true;
        }
        i = i + 1;
    }
    return false;
}

// If-None-Match matches the asset ETag: exact, or the weak form "W/" + etag.
fn web_etag_matches(inm: const u8[:], etag: const u8[:]) -> bool {
    if inm.len == 0 {
        return false;
    }
    if web_eq(inm, etag) {
        return true;
    }
    if inm.len == etag.len + 2 && inm[0] == 87 && inm[1] == 47 && web_eq(inm[2 .. inm.len], etag) {
        return true;
    }
    return false;
}

fn web_etag_text(etags: const u8[:], eoff: const i64[:], i: i64) -> const u8[:] {
    let from: i64 = eoff[i];
    let to: i64 = eoff[i + 1];

    if from < 0 || to < from || to > etags.len {
        return etags[0 .. 0];
    }
    return etags[from .. to];
}

// The whole serve decision for one request path.
//
//   out[0] status          200, 304, 404 or 415
//   out[1] asset index     -1 when the path is not registered
//   out[2] header mask     WEB_H_* bits that native actually sets
//   out[3] encoding        1 when Content-Encoding: gzip is set
//   out[4] isolation       1 when the two cross-origin policies are set
//   out[5] policy          WEB_POLICY_* id; web_cache_text turns it into the
//                          Cache-Control bytes (unused on 304 and 404)
//
// The asset count is read from off.len - 1, so the tables cannot disagree with
// their own offsets. Returns out[0].
export fn web_serve(
    path: const u8[:],
    prefix: const u8[:],
    names: const u8[:],
    off: const i64[:],
    etags: const u8[:],
    eoff: const i64[:],
    use_gzip: i64,
    accept: const u8[:],
    inm: const u8[:],
    out: i64[:]) -> i64 {
    let cnt: i64 = off.len - 1;

    out[0] = WEB_NOT_FOUND;
    out[1] = 0 - 1;
    out[2] = 0;
    out[3] = 0;
    out[4] = 0;
    out[5] = 0 - 1;

    // an unregistered path never reaches a native handler: 404, no headers
    let idx = web_route_idx(path, prefix, names, off, cnt);

    if idx < 0 {
        return out[0];
    }
    out[1] = idx;

    // the gzip gate of the registered handler
    if use_gzip != 0 {
        if !web_accepts_gzip(accept) {
            out[0] = WEB_UNSUPPORTED_MEDIA;
            out[2] = WEB_H_TYPE;
            return out[0];
        }
        out[2] = WEB_H_ENCODING;
        out[3] = 1;
    }

    let policy = web_policy_of(names, off, idx);

    // serve_asset_nocache has no ETag and no 304 branch
    if policy != WEB_POLICY_NOCACHE {
        out[2] = out[2] | WEB_H_ETAG;
        if web_etag_matches(inm, web_etag_text(etags, eoff, idx)) {
            out[0] = WEB_NOT_MODIFIED;
            return out[0];
        }
    }
    if web_name_is(names, off, idx, "index.html") {
        out[2] = out[2] | WEB_H_ISOLATION;
        out[4] = 1;
    }
    out[2] = out[2] | WEB_H_CACHE | WEB_H_TYPE;
    out[0] = WEB_OK;
    out[5] = policy;
    return out[0];
}

// Cache-Control text for a policy id, so a caller that only needs the header
// does not duplicate the two literals.
export fn web_cache_text(policy: i64, out: u8[:]) -> i64 {
    if policy == WEB_POLICY_IMMUTABLE {
        return web_put(out, 0, WEB_CACHE_IMMUTABLE);
    }
    if policy == WEB_POLICY_REVALIDATE || policy == WEB_POLICY_NOCACHE {
        return web_put(out, 0, WEB_CACHE_NO_CACHE);
    }
    return 0 - 1;
}

// Isolation decision on its own: only the index asset carries the two headers.
export fn web_isolation_of(names: const u8[:], off: const i64[:], idx: i64) -> i64 {
    if idx < 0 {
        return 0;
    }
    if web_name_is(names, off, idx, "index.html") {
        return 1;
    }
    return 0;
}
