// P10-05 S1: release and package planning for the owned executable
// infrastructure.
//
// Native sources at the base commit 792293ad, copied byte-identical into
// tests/native/ (hashes in results/native-sha256.txt):
//
//   scripts/make-release.sh        tag validation (bash ERE, line 40),
//                                  prerelease detection (98), runtime asset
//                                  class (118), asset size limit (82), tree
//                                  size limit (86), retention loop (117-129)
//   tools/package-main-runtime.sh  dist file list (26-27), SHA256SUMS (36-38),
//                                  manifest key order (40-49), module count
//                                  from the build script (31)
//
// Every export is a decision over release inputs passed in by the caller: no
// build, no GPU, no model, no network, no git. The gate runs the native text
// itself — bash for the shell rules, the script's own python block for the
// manifest and the sums — and compares it with these exports.
//
// Not owned here: git and gh invocation, archive creation, file sizes, the
// compiler version string and the SHA-256 of file contents (the caller hashes;
// this module composes the documents).
//
// Conventions: 0/1 answers are 0 = false/no, 1 = true/yes. Every out buffer is
// caller-sized and receives exactly the returned number of bytes; nothing is
// written past that point.

import std;

// 'v', '.', '-', '/', ' ', tab, '\n'
const RLP_DOT: u8 = 46;
const RLP_SLASH: u8 = 47;
const RLP_DASH: u8 = 45;
const RLP_TAB: u8 = 9;
const RLP_NL: u8 = 10;
const RLP_SPACE: u8 = 32;
const RLP_QUOTE: u8 = 34;
const RLP_BACKSLASH: u8 = 92;
const RLP_COMMA: u8 = 44;

const RLP_ASSET_LIMIT: i64 = 2147483648;
const RLP_TREE_LIMIT: i64 = 100000000;

fn rlp_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

fn rlp_eq(s: const u8[:], lit: const u8[:]) -> bool {
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

fn rlp_at(s: const u8[:], p0: i64, lit: const u8[:]) -> bool {
    if p0 < 0 || p0 + lit.len > s.len {
        return false;
    }
    var i: i64 = 0;
    while i < lit.len {
        if s[p0 + i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn rlp_find(s: const u8[:], from: i64, lit: const u8[:]) -> i64 {
    if lit.len == 0 {
        return from;
    }
    var p: i64 = from;
    while p + lit.len <= s.len {
        if rlp_at(s, p, lit) {
            return p;
        }
        p = p + 1;
    }
    return 0 - 1;
}

// At least one digit at p0; returns the position after the run, or -1.
fn rlp_digits(s: const u8[:], p0: i64) -> i64 {
    var p: i64 = p0;
    var n: i64 = 0;
    while p < s.len && rlp_digit(s[p]) {
        p = p + 1;
        n = n + 1;
    }
    if n == 0 {
        return 0 - 1;
    }
    return p;
}

fn rlp_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn rlp_put(out: u8[:], at0: i64, s: const u8[:]) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len {
        at = rlp_putc(out, at, s[i]);
        i = i + 1;
    }
    return at;
}

fn rlp_put_int(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        at = rlp_putc(out, at, RLP_DASH);
        if v == (0 - 9223372036854775807 - 1) {
            return rlp_put(out, at, "9223372036854775808");
        }
        v = 0 - v;
    }
    if v == 0 {
        return rlp_putc(out, at, 48);
    }
    var buf: i64[24] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    var n: i64 = 0;
    while v > 0 {
        buf[n] = v % 10;
        v = v / 10;
        n = n + 1;
    }
    var i: i64 = n;
    while i > 0 {
        i = i - 1;
        at = rlp_putc(out, at, 48 + (buf[i] as u8));
    }
    return at;
}

// json.dumps(..., ensure_ascii=True) for one string body, without the quotes.
fn rlp_json_str(out: u8[:], at0: i64, s: const u8[:]) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len {
        let c: u8 = s[i];
        if c == RLP_QUOTE {
            at = rlp_put(out, at, "\\\"");
        } else if c == RLP_BACKSLASH {
            at = rlp_put(out, at, "\\\\");
        } else if c == 8 {
            at = rlp_put(out, at, "\\b");
        } else if c == 12 {
            at = rlp_put(out, at, "\\f");
        } else if c == RLP_NL {
            at = rlp_put(out, at, "\\n");
        } else if c == 13 {
            at = rlp_put(out, at, "\\r");
        } else if c == RLP_TAB {
            at = rlp_put(out, at, "\\t");
        } else if c < 32 || c > 126 {
            at = rlp_put(out, at, "\\u00");
            at = rlp_putc(out, at, "0123456789abcdef"[(c / 16) as i64]);
            at = rlp_putc(out, at, "0123456789abcdef"[(c % 16) as i64]);
        } else {
            at = rlp_putc(out, at, c);
        }
        i = i + 1;
    }
    return at;
}

// make-release.sh:40 — [[ "$version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[0-9]+)?$ ]]
export fn pkg_version_ok(s: const u8[:]) -> i64 {
    if s.len < 1 || s[0] != 118 {
        return 0;
    }
    var p: i64 = rlp_digits(s, 1);
    if p < 0 || p >= s.len || s[p] != RLP_DOT {
        return 0;
    }
    p = rlp_digits(s, p + 1);
    if p < 0 || p >= s.len || s[p] != RLP_DOT {
        return 0;
    }
    p = rlp_digits(s, p + 1);
    if p < 0 {
        return 0;
    }
    if p == s.len {
        return 1;
    }
    if !rlp_at(s, p, "-rc.") {
        return 0;
    }
    p = rlp_digits(s, p + 4);
    if p < 0 || p != s.len {
        return 0;
    }
    return 1;
}

// make-release.sh:98 — [[ "$version" == *-rc.* ]]
export fn pkg_prerelease(s: const u8[:]) -> i64 {
    if rlp_find(s, 0, "-rc.") >= 0 {
        return 1;
    }
    return 0;
}

// POSIX basename: trailing slashes drop, a string of slashes gives "/".
export fn pkg_basename(s: const u8[:], out: u8[:]) -> i64 {
    var end: i64 = s.len;
    while end > 0 && s[end - 1] == RLP_SLASH {
        end = end - 1;
    }
    if end == 0 {
        if s.len == 0 {
            return 0;
        }
        return rlp_putc(out, 0, RLP_SLASH);
    }
    var start: i64 = 0;
    var i: i64 = end;
    var found: i64 = 0;
    while i > 0 && found == 0 {
        if s[i - 1] == RLP_SLASH {
            start = i;
            found = 1;
        }
        i = i - 1;
    }
    var at: i64 = 0;
    i = start;
    while i < end {
        at = rlp_putc(out, at, s[i]);
        i = i + 1;
    }
    return at;
}

// make-release.sh:118 — case "$(basename "$asset")" in goose-runtime-*)
export fn pkg_runtime_asset(name: const u8[:]) -> i64 {
    var end: i64 = name.len;
    while end > 0 && name[end - 1] == RLP_SLASH {
        end = end - 1;
    }
    var start: i64 = 0;
    var i: i64 = end;
    var found: i64 = 0;
    while i > 0 && found == 0 {
        if name[i - 1] == RLP_SLASH {
            start = i;
            found = 1;
        }
        i = i - 1;
    }
    let base: const u8[:] = name[start..end];
    if base.len >= 14 && rlp_at(base, 0, "goose-runtime-") {
        return 1;
    }
    return 0;
}

// make-release.sh:120-122 — the retention loop's jq selector
// `.name | startswith("goose-runtime-")` on the asset name as gh reports it.
// Unlike the has_runtime test this does not take a basename, so a release asset
// stored under a directory is not selected. Returns 1 when selected.
fn rlp_selected(name: const u8[:]) -> i64 {
    if name.len >= 14 && rlp_at(name, 0, "goose-runtime-") {
        return 1;
    }
    return 0;
}

export fn pkg_asset_selected(name: const u8[:]) -> i64 {
    return rlp_selected(name);
}

// make-release.sh:82 — [ "$size" -gt 2147483648 ]
export fn pkg_asset_over_limit(size: i64) -> i64 {
    if size > RLP_ASSET_LIMIT {
        return 1;
    }
    return 0;
}

// make-release.sh:86 — awk '$4 > 100000000 {print $5}'
export fn pkg_tree_over_limit(size: i64) -> i64 {
    if size > RLP_TREE_LIMIT {
        return 1;
    }
    return 0;
}

// make-release.sh:117-129 — the retention guard chain for one (release, asset)
// pair: the new release must carry a runtime archive, --keep-old-runtime must be
// absent, the release tag must be non-empty, the release must not be the new
// tag, and the asset name must pass the jq selector. Returns 0 delete, 1 keep.
// tag_present and same_version are the caller's tag facts, the selector is
// rlp_selected().
export fn pkg_keep_asset(has_runtime: i64, keep_old: i64, tag_present: i64,
                         same_version: i64, name: const u8[:]) -> i64 {
    if has_runtime == 0 || keep_old != 0 || tag_present == 0 || same_version != 0 {
        return 1;
    }
    if rlp_selected(name) == 0 {
        return 1;
    }
    return 0;
}

// tools/package-main-runtime.sh:26-27 — the files the dist directory receives,
// in the order the script writes them.
export fn pkg_dist_files(out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = rlp_put(out, at, "main.goose\n");
    at = rlp_put(out, at, "main.c\n");
    at = rlp_put(out, at, "main.h\n");
    at = rlp_put(out, at, "main.o\n");
    at = rlp_put(out, at, "SHA256SUMS\n");
    at = rlp_put(out, at, "manifest.json\n");
    return at;
}

// tools/package-main-runtime.sh:36-38 — "\n".join(f"{sha}  {name}") + "\n".
export fn pkg_sha256sums(hash: const u8[:], ho: const i64[:], hl: const i64[:],
                         name: const u8[:], no: const i64[:], nl: const i64[:],
                         n: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    var k: i64 = 0;
    while k < n {
        if k > 0 {
            at = rlp_putc(out, at, RLP_NL);
        }
        var i: i64 = 0;
        while i < hl[k] {
            at = rlp_putc(out, at, hash[ho[k] + i]);
            i = i + 1;
        }
        at = rlp_putc(out, at, RLP_SPACE);
        at = rlp_putc(out, at, RLP_SPACE);
        i = 0;
        while i < nl[k] {
            at = rlp_putc(out, at, name[no[k] + i]);
            i = i + 1;
        }
        k = k + 1;
    }
    return rlp_putc(out, at, RLP_NL);
}

// tools/package-main-runtime.sh:40-49 — json.dumps(manifest, indent=2) + "\n"
// with the insertion order the script builds: unit, compiler, modules,
// generated_c_bytes, object_bytes, global_symbols, extern_symbols, files.
export fn pkg_manifest_json(unit: const u8[:], compiler: const u8[:], modules: i64,
                            c_bytes: i64, obj_bytes: i64, defined: i64, extern_syms: i64,
                            hashes: const u8[:], ho: const i64[:], hl: const i64[:],
                            out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = rlp_put(out, at, "{\n  \"unit\": \"");
    at = rlp_json_str(out, at, unit);
    at = rlp_put(out, at, "\",\n  \"compiler\": \"");
    at = rlp_json_str(out, at, compiler);
    at = rlp_put(out, at, "\",\n  \"modules\": ");
    at = rlp_put_int(out, at, modules);
    at = rlp_put(out, at, ",\n  \"generated_c_bytes\": ");
    at = rlp_put_int(out, at, c_bytes);
    at = rlp_put(out, at, ",\n  \"object_bytes\": ");
    at = rlp_put_int(out, at, obj_bytes);
    at = rlp_put(out, at, ",\n  \"global_symbols\": ");
    at = rlp_put_int(out, at, defined);
    at = rlp_put(out, at, ",\n  \"extern_symbols\": ");
    at = rlp_put_int(out, at, extern_syms);
    at = rlp_put(out, at, ",\n  \"files\": {\n");

    var k: i64 = 0;
    var first: i64 = 1;
    while k < 4 {
        if first == 0 {
            at = rlp_put(out, at, ",\n");
        }
        first = 0;
        at = rlp_put(out, at, "    \"");
        if k == 0 {
            at = rlp_put(out, at, "main.goose");
        } else if k == 1 {
            at = rlp_put(out, at, "main.c");
        } else if k == 2 {
            at = rlp_put(out, at, "main.h");
        } else {
            at = rlp_put(out, at, "main.o");
        }
        at = rlp_put(out, at, "\": \"");
        var i: i64 = 0;
        while i < hl[k] {
            at = rlp_putc(out, at, hashes[ho[k] + i]);
            i = i + 1;
        }
        at = rlp_putc(out, at, RLP_QUOTE);
        k = k + 1;
    }
    at = rlp_put(out, at, "\n  }\n}\n");
    return at;
}

// tools/package-main-runtime.sh:31 — grep -cE '^\s+goose/.*\.goose\s*$'
fn rlp_ws(c: u8) -> bool {
    return c == RLP_SPACE || c == RLP_TAB || c == 11 || c == 12 || c == 13;
}

fn rlp_line_counted(text: const u8[:], i: i64, j: i64) -> i64 {
    var p: i64 = i;
    var lead: i64 = 0;
    while p < j && rlp_ws(text[p]) {
        p = p + 1;
        lead = lead + 1;
    }
    if lead == 0 {
        return 0;
    }
    if !rlp_at(text, p, "goose/") {
        return 0;
    }
    var m: i64 = p + 6;
    while m + 6 <= j {
        if rlp_at(text, m, ".goose") {
            var t: i64 = m + 6;
            var ok: i64 = 1;
            while t < j {
                if !rlp_ws(text[t]) {
                    ok = 0;
                }
                t = t + 1;
            }
            if ok != 0 {
                return 1;
            }
        }
        m = m + 1;
    }
    return 0;
}

export fn pkg_module_count(text: const u8[:]) -> i64 {
    var n: i64 = 0;
    var i: i64 = 0;
    while i < text.len {
        var j: i64 = i;
        while j < text.len && text[j] != RLP_NL {
            j = j + 1;
        }
        n = n + rlp_line_counted(text, i, j);
        i = j + 1;
    }
    return n;
}

// tools/package-main-runtime.sh:34 — `$(goose version | tail -1)`: the last
// terminated line, then the command substitution drops the trailing newlines.
export fn pkg_version_line(text: const u8[:], out: u8[:]) -> i64 {
    var end: i64 = text.len;
    if end > 0 && text[end - 1] == RLP_NL {
        end = end - 1;
    }
    var start: i64 = 0;
    var p: i64 = 0;
    while p < end {
        if text[p] == RLP_NL {
            start = p + 1;
        }
        p = p + 1;
    }
    var at: i64 = 0;
    p = start;
    while p < end {
        at = rlp_putc(out, at, text[p]);
        p = p + 1;
    }
    return at;
}
