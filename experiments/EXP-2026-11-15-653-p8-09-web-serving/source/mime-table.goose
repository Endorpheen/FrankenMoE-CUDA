// P8-09 S1: the embedded UI asset table's naming and content types.
//
// Native source: upstream/llama.cpp/tools/ui/embed.cpp at base commit
// ec50a32d. The file is the generator for ui.cpp/ui.h, which embed the web UI
// as C arrays and are what the server serves; the three text decisions in it
// are owned here:
//
//   mime_from_ext   the extension table and the octet-stream fallback   20-40
//   path_basename   the last slash of an asset path                    113-116
//   required checks the eight labels a complete UI bundle must satisfy 178-196
//
// The rules exactly as native applies them:
//   - the extension is everything after the LAST '.' of the WHOLE asset name,
//     not of the basename: for the asset "_app/x/y" the last dot may live in a
//     directory component, and then the table sees "app/x/y" and misses;
//   - the extension compare is byte exact and case sensitive: "INDEX.HTML" and
//     ".HTML" are octet-stream, only lower-case names hit the table;
//   - a name with no '.' at all is octet-stream;
//   - an empty extension ("index.") is octet-stream;
//   - jpg and jpeg share one type, and both take precedence over nothing else;
//   - the required checks run on the basename (path_basename), so
//     "_app/immutable/index.html" satisfies the "index.html" label;
//   - bundle/workbox are prefix+suffix matches, not globs: "bundle.css.js"
//     satisfies both the .js label and, if it also ends in .css, no;
//     "x-bundle.js" satisfies neither because the prefix is not at the start.
//
// Asset type strings are 11 distinct byte strings plus the fallback; the ids
// are the table order below and every export resolves ids back to bytes, so no
// pointer into static storage crosses the boundary.
//
// The prefix is mime_. Not owned: walking the asset directory, gzip staging,
// emitting ui.cpp, FNV/ETag (S3), the serve decisions (S2).

import std;

const MIME_NONE: i64 = 0 - 1;

// Content-type ids. Application/octet-stream is also the fallback.
const MIME_HTML: i64 = 0;
const MIME_CSS: i64 = 1;
const MIME_JS: i64 = 2;
const MIME_JSON: i64 = 3;
const MIME_WEBMANIFEST: i64 = 4;
const MIME_SVG: i64 = 5;
const MIME_PNG: i64 = 6;
const MIME_JPEG: i64 = 7;
const MIME_ICO: i64 = 8;
const MIME_WOFF: i64 = 9;
const MIME_WOFF2: i64 = 10;
const MIME_OCTET: i64 = 11;

const MIME_N: i64 = 12;
const MIME_TEXT: const u8[:][12] = [
    "text/html; charset=utf-8",
    "text/css",
    "application/javascript",
    "application/json",
    "application/manifest+json",
    "image/svg+xml",
    "image/png",
    "image/jpeg",
    "image/x-icon",
    "font/woff",
    "font/woff2",
    "application/octet-stream"
];

// The twelve extension spellings of the native if-chain, in table order.
// jpg and jpeg both map to MIME_JPEG.
const MIME_EXT_N: i64 = 12;
const MIME_EXT: const u8[:][12] = [
    "html",
    "css",
    "js",
    "json",
    "webmanifest",
    "svg",
    "png",
    "jpg",
    "jpeg",
    "ico",
    "woff",
    "woff2"
];
const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10];

fn mime_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < s.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + s.len;
}

fn mime_slice_eq(a: const u8[:], b: const u8[:]) -> bool {
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

// name.rfind('.') from the end; -1 when the name has no dot.
fn mime_last_dot(name: const u8[:]) -> i64 {
    var i: i64 = name.len - 1;

    while i >= 0 {
        if name[i] == 46 {
            return i;
        }
        i = i - 1;
    }
    return 0 - 1;
}

fn mime_of_name(name: const u8[:]) -> i64 {
    let dot = mime_last_dot(name);

    if dot < 0 {
        return MIME_OCTET;
    }
    let ext = name[dot + 1 .. name.len];

    var i: i64 = 0;
    while i < MIME_EXT_N {
        if mime_slice_eq(ext, MIME_EXT[i]) {
            return MIME_EXT_ID[i];
        }
        i = i + 1;
    }
    return MIME_OCTET;
}

// Content-type id for an asset name, exactly mime_from_ext.
export fn mime_of(name: const u8[:]) -> i64 {
    return mime_of_name(name);
}

// The type string for an id; -1 on an unknown id so the caller keeps its own
// buffer rather than receiving a pointer into the table.
export fn mime_text(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= MIME_N {
        return MIME_NONE;
    }
    return mime_put(out, 0, MIME_TEXT[id]);
}

// path_basename: everything after the last '/', the whole name when there is
// no slash. An empty name stays empty.
export fn mime_base(name: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = name.len - 1;

    while at >= 0 {
        if name[at] == 47 {
            at = at + 1;
            break;
        }
        at = at - 1;
    }
    if at < 0 {
        at = 0;
    }
    return mime_put(out, 0, name[at .. name.len]);
}

fn mime_starts(base: const u8[:], pre: const u8[:]) -> bool {
    if base.len < pre.len {
        return false;
    }
    var i: i64 = 0;

    while i < pre.len {
        if base[i] != pre[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn mime_ends(base: const u8[:], suf: const u8[:]) -> bool {
    if base.len < suf.len {
        return false;
    }
    return mime_slice_eq(base[base.len - suf.len .. base.len], suf);
}

// The eight required-bundle labels of the native checks[] table, as a bitmask;
// bit i is set when the basename satisfies label i. The labels are ordered as
// in embed.cpp so a missing-label report keeps the native order.
const MIME_REQ_N: i64 = 8;
export fn mime_required(base: const u8[:]) -> i64 {
    var mask: i64 = 0;

    if mime_slice_eq(base, "index.html") {
        mask = mask | 1;
    }
    if mime_slice_eq(base, "manifest.webmanifest") {
        mask = mask | 2;
    }
    if mime_slice_eq(base, "sw.js") {
        mask = mask | 4;
    }
    if mime_slice_eq(base, "build.json") {
        mask = mask | 8;
    }
    if mime_slice_eq(base, "version.json") {
        mask = mask | 16;
    }
    if mime_starts(base, "bundle") && mime_ends(base, ".js") {
        mask = mask | 32;
    }
    if mime_starts(base, "bundle") && mime_ends(base, ".css") {
        mask = mask | 64;
    }
    if mime_starts(base, "workbox") && mime_ends(base, ".js") {
        mask = mask | 128;
    }
    return mask;
}

// The label text for a bit index, so the caller can print the native missing
// list without owning the strings.
export fn mime_required_label(bit: i64, out: u8[:]) -> i64 {
    if bit < 0 || bit >= MIME_REQ_N {
        return MIME_NONE;
    }
    if bit == 0 {
        return mime_put(out, 0, "index.html");
    }
    if bit == 1 {
        return mime_put(out, 0, "manifest.webmanifest");
    }
    if bit == 2 {
        return mime_put(out, 0, "sw.js");
    }
    if bit == 3 {
        return mime_put(out, 0, "build.json");
    }
    if bit == 4 {
        return mime_put(out, 0, "version.json");
    }
    if bit == 5 {
        return mime_put(out, 0, "bundle[hash].js");
    }
    if bit == 6 {
        return mime_put(out, 0, "bundle[hash].css");
    }
    return mime_put(out, 0, "workbox[hash].js");
}
