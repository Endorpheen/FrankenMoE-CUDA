// P10-07 S2: provenance of every vendored file — where it came from, whether
// the pin is reproducible, and whether the local patch still applies.
//
// Native source at the base commit 70de805c, copied byte-identical into
// tests/native/sync_vendor.py (upstream llama.cpp 0b5be7e4):
//
//   sync_vendor.py:8-12    the pin constants: HTTPLIB_VERSION is a release tag,
//                          the four hash projects are pinned by commit
//   sync_vendor.py:14-42   the vendor table, one URL -> destination path per
//                          file; the pin lives in the URL text
//   sync_vendor.py:44-100  the patch table and the one rule that guards it:
//                          `if content.count(old) != 1: Error ... sys.exit(1)`
//
// The slice decides, per file, the pin class found in its download URL, whether
// that class is reproducible, the occurrence count of a patch anchor, and the
// patched bytes. It also decides the admission state of a file on disk from
// three booleans the caller supplies. Nothing here reads the network or the
// filesystem, so the gate is offline.
//
// Not owned: urllib, `split.py`, the httplib split step, `os.remove`, and the
// destination paths themselves (data, not a decision).

import std;

const VND_SLASH: u8 = 47;
const VND_LF: u8 = 10;

// pin classes, ordered as the constant block in sync_vendor.py
const VND_NONE: i64 = 0;
const VND_TAG: i64 = 1;
const VND_COMMIT: i64 = 2;
const VND_BRANCH: i64 = 3;
const VND_FLOATING: i64 = 4;

fn vnd_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn vnd_put(out: u8[:], at: i64, lit: const u8[:]) -> i64 {
    var p: i64 = at;
    var i: i64 = 0;
    while i < lit.len {
        p = vnd_putc(out, p, lit[i]);
        i = i + 1;
    }
    return p;
}

fn vnd_at(s: const u8[:], p0: i64, lit: const u8[:]) -> bool {
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

fn vnd_contains(s: const u8[:], lit: const u8[:]) -> bool {
    var p: i64 = 0;
    while p + lit.len <= s.len {
        if vnd_at(s, p, lit) {
            return true;
        }
        p = p + 1;
    }
    return false;
}

fn vnd_hexval(c: u8) -> i64 {
    if c >= 48 && c <= 57 {
        return c - 48;
    }
    if c >= 97 && c <= 102 {
        return c - 87;
    }
    if c >= 65 && c <= 70 {
        return c - 55;
    }
    return 0 - 1;
}

fn vnd_hex40(s: const u8[:], p0: i64) -> bool {
    if p0 < 0 || p0 + 40 > s.len {
        return false;
    }
    if p0 > 0 && s[p0 - 1] != VND_SLASH {
        return false;
    }
    if p0 + 40 < s.len && s[p0 + 40] != VND_SLASH {
        return false;
    }
    var i: i64 = 0;
    while i < 40 {
        if vnd_hexval(s[p0 + i]) < 0 {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// The pin class of one download URL from the vendor table.
//
// `releases/latest/download` floats: the file changes with every upstream
// release and no revision is recorded. `refs/heads/<branch>` is a branch and
// moves; `refs/tags/<tag>` names a release; a bare 40-hex path segment is a
// commit. Anything else — including a bare branch name such as `raw/master/`
// and a PR ref such as `refs/pull/123/head` — carries no recognised pin and is
// reported as NONE, which is the state the vendor table never uses.
export fn vnd_pin_kind(url: const u8[:]) -> i64 {
    if vnd_contains(url, "releases/latest/download") {
        return VND_FLOATING;
    }
    if vnd_contains(url, "/refs/heads/") {
        return VND_BRANCH;
    }
    if vnd_contains(url, "/refs/tags/") {
        return VND_TAG;
    }
    var p: i64 = 0;
    while p < url.len {
        if vnd_hex40(url, p) {
            return VND_COMMIT;
        }
        p = p + 1;
    }
    return VND_NONE;
}

// A tag and a commit both name one immutable revision; a branch, a floating
// release download and an unrecognised pin do not.
export fn vnd_reproducible(kind: i64) -> i64 {
    if kind == VND_TAG || kind == VND_COMMIT {
        return 1;
    }
    return 0;
}

// Python's str.count(): non-overlapping occurrences, left to right.
fn vnd_count(text: const u8[:], pat: const u8[:]) -> i64 {
    if pat.len == 0 {
        // str.count("") is len(text) + 1 in Python; the native patch table
        // never holds an empty anchor, and the rule below rejects any count
        // other than 1, so an empty anchor is simply an error.
        return 0;
    }
    var n: i64 = 0;
    var p: i64 = 0;
    while p + pat.len <= text.len {
        if vnd_at(text, p, pat) {
            n = n + 1;
            p = p + pat.len;
        } else {
            p = p + 1;
        }
    }
    return n;
}

export fn vnd_anchor_count(text: const u8[:], pat: const u8[:]) -> i64 {
    return vnd_count(text, pat);
}

// sync_vendor.py:95 — `if content.count(old) != 1: Error: cannot apply patch
// on {filename}, upstream code has changed; sys.exit(1)`. Exactly one
// occurrence applies, zero and two are both fatal.
export fn vnd_anchor_state(count: i64) -> i64 {
    if count == 1 {
        return 1;
    }
    return 0;
}

// The patched bytes: -1 when the anchor does not occur exactly once, otherwise
// the byte count written to `out` (prefix + replacement + suffix).
export fn vnd_patch(text: const u8[:], old: const u8[:], new: const u8[:], out: u8[:]) -> i64 {
    if vnd_count(text, old) != 1 {
        return 0 - 1;
    }
    var hit: i64 = 0 - 1;
    var p: i64 = 0;
    while p + old.len <= text.len && hit < 0 {
        if vnd_at(text, p, old) {
            hit = p;
        }
        p = p + 1;
    }
    var at: i64 = 0;
    var i: i64 = 0;
    while i < hit {
        at = vnd_putc(out, at, text[i]);
        i = i + 1;
    }
    at = vnd_put(out, at, new);
    i = hit + old.len;
    while i < text.len {
        at = vnd_putc(out, at, text[i]);
        i = i + 1;
    }
    return at;
}

// Admission state of one vendored file. `owned` marks a file the project writes
// itself — a build description, or an output of the httplib split step — which
// is neither downloaded nor licensed third-party code.
//   0 admitted, 1 present but not in the vendor table and not owned (an
//   untracked third-party file), 2 named by the table but absent from the tree,
//   3 present and declared but not pinned reproducibly.
export fn vnd_file_state(declared: i64, present: i64, reproducible: i64, owned: i64) -> i64 {
    if owned != 0 {
        return 0;
    }
    if declared == 0 {
        return 1;
    }
    if present == 0 {
        return 2;
    }
    if reproducible == 0 {
        return 3;
    }
    return 0;
}

fn vnd_names(kind: i64, out: u8[:]) -> i64 {
    if kind == VND_TAG {
        return vnd_put(out, 0, "tag");
    }
    if kind == VND_COMMIT {
        return vnd_put(out, 0, "commit");
    }
    if kind == VND_BRANCH {
        return vnd_put(out, 0, "branch");
    }
    if kind == VND_FLOATING {
        return vnd_put(out, 0, "floating");
    }
    return vnd_put(out, 0, "none");
}

// The pin class as the audit table spells it.
export fn vnd_pin_name(kind: i64, out: u8[:]) -> i64 {
    return vnd_names(kind, out);
}
