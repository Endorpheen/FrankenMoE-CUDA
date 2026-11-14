// HF cache naming and path policy, ported from common/hf-cache.cpp.
//
// Owned decisions:
//   - is_valid_repo_id: length 1..256, base chars [A-Za-z0-9_], the specials
//     [/.-] must be surrounded by base chars, exactly one '/';
//   - is_valid_hf_token: "hf_" plus 34..253 alphanumeric bytes, length 37..256;
//   - is_valid_commit (40 hex) and is_valid_oid (40 or 64 hex);
//   - repo_to_folder_name / folder_name_to_repo: the "models--" prefix and the
//     separator rule '/' <-> "--", including the missing-prefix rejection;
//   - is_valid_subpath: a relative subpath that stays under the base after
//     lexical normalization, component by component;
//   - path assembly: repo folder, "snapshots/<commit>/<subpath>", the
//     "blobs/<oid>" redirect and the repo "refs" file;
//   - get_cached_ref's selection rule: regular file, non-empty first line,
//     valid commit, the "main" preference, the first valid fallback;
//   - finalize_file's early-exit order: already in place, missing local file,
//     then link/move.
//
// Not owned: HF_HOME/XDG_CACHE_HOME/LLAMA_CACHE reading, the directory walk,
// HTTP, JSON, symlink creation and every destructive path. The cache root is a
// caller-supplied prefix, so the module has no environment dependency.
//
// Lexical normalization is modelled for absolute POSIX paths: "." components
// are dropped, ".." pops one component and is clamped at the root, empty
// components collapse, and a trailing '/' survives when the last raw component
// is empty, "." or "..". The native oracle is fs::path::lexically_normal, not
// this comment.
//
// Bodies are private h_* functions; the exported hf_* wrappers are the API and
// are never called from inside the module, so every export has one concrete
// signature.
//
// Every out buffer is sized by the caller and must hold the assembled length
// plus one byte; no function writes past the assembled length and no function
// reads uninitialized bytes it produced itself.

fn h_alnum(c: u8) -> bool {
    return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || (c >= 48 && c <= 57);
}

fn h_hex(c: u8) -> bool {
    return (c >= 65 && c <= 70) || (c >= 97 && c <= 102) || (c >= 48 && c <= 57);
}

fn h_special(c: u8) -> bool {
    return c == 47 || c == 46 || c == 45;
}

fn h_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        dst[p] = s[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn h_sep(dst: u8[:], at: i64) -> i64 {
    dst[at] = 47;
    return at + 1;
}

fn h_hex_len(s: const u8[:], want: i64) -> i64 {
    if s.len != want {
        return 0;
    }
    var i: i64 = 0;

    while i < s.len {
        if !h_hex(s[i]) {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn h_repo_valid(s: const u8[:]) -> i64 {
    if s.len == 0 || s.len > 256 {
        return 0;
    }
    var slash: i64 = 0;
    var special: bool = true;
    var i: i64 = 0;

    while i < s.len {
        let c = s[i];

        if h_alnum(c) || c == 95 {
            special = false;
        } else if h_special(c) {
            if special {
                return 0;
            }
            if c == 47 {
                slash = slash + 1;
            }
            special = true;
        } else {
            return 0;
        }
        i = i + 1;
    }
    if special {
        return 0;
    }
    if slash != 1 {
        return 0;
    }
    return 1;
}

fn h_token_valid(t: const u8[:]) -> i64 {
    if t.len < 37 || t.len > 256 {
        return 0;
    }
    if t[0] != 104 || t[1] != 102 || t[2] != 95 {
        return 0;
    }
    var i: i64 = 3;

    while i < t.len {
        if !h_alnum(t[i]) {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn h_commit_valid(s: const u8[:]) -> i64 {
    return h_hex_len(s, 40);
}

fn h_oid_valid(s: const u8[:]) -> i64 {
    if h_hex_len(s, 40) != 0 {
        return 1;
    }
    return h_hex_len(s, 64);
}

fn h_repo_folder(repo: const u8[:], out: u8[:]) -> i64 {
    var p: i64 = 0;
    p = h_put(out, p, "models--");
    var i: i64 = 0;

    while i < repo.len {
        if repo[i] == 47 {
            out[p] = 45;
            out[p + 1] = 45;
            p = p + 2;
        } else {
            out[p] = repo[i];
            p = p + 1;
        }
        i = i + 1;
    }
    return p;
}

// Returns -1 when the "models--" prefix is absent, which is the native empty
// result distinguished from an empty repo id.
fn h_folder_repo(folder: const u8[:], out: u8[:]) -> i64 {
    if folder.len < 8 {
        return 0 - 1;
    }
    if folder[0] != 109 || folder[1] != 111 || folder[2] != 100 || folder[3] != 101 ||
       folder[4] != 108 || folder[5] != 115 || folder[6] != 45 || folder[7] != 45 {
        return 0 - 1;
    }
    var p: i64 = 0;
    var i: i64 = 8;

    while i < folder.len {
        if folder[i] == 45 && i + 1 < folder.len && folder[i + 1] == 45 {
            out[p] = 47;
            p = p + 1;
            i = i + 2;
        } else {
            out[p] = folder[i];
            p = p + 1;
            i = i + 1;
        }
    }
    return p;
}

fn h_is_abs(s: const u8[:]) -> i64 {
    if s.len > 0 && s[0] == 47 {
        return 1;
    }
    return 0;
}

fn h_raw_at(base: const u8[:], sub: const u8[:], i: i64) -> u8 {
    if i < base.len {
        return base[i];
    }
    if i == base.len {
        return 47;
    }
    return sub[i - base.len - 1];
}

// base + "/" + sub, lexically normalized. Absolute subpaths are not a valid
// input for this helper and return -1 (the native operator/ would replace the
// base instead; is_valid_subpath rejects them before that happens).
fn h_path_norm(base: const u8[:], sub: const u8[:], out: u8[:]) -> i64 {
    if h_is_abs(sub) != 0 {
        return 0 - 1;
    }
    let total = base.len + 1 + sub.len;
    var starts: i64[128];
    var z: i64 = 0;

    while z < 128 {
        starts[z] = 0;
        z = z + 1;
    }
    var p: i64 = 0;
    out[0] = 47;
    p = 1;
    var sp: i64 = 0;
    var seg_start: i64 = 0;
    var seg_len: i64 = 0;
    var last_kind: i64 = 0;
    var k: i64 = 0;

    while k <= total {
        let at_end: bool = k == total;
        var c: u8 = 0;

        if !at_end {
            c = h_raw_at(base, sub, k);
        }
        if at_end || c == 47 {
            if seg_len == 0 {
                last_kind = 0;
            } else if seg_len == 1 && h_raw_at(base, sub, seg_start) == 46 {
                last_kind = 1;
            } else if seg_len == 2 && h_raw_at(base, sub, seg_start) == 46 &&
                      h_raw_at(base, sub, seg_start + 1) == 46 {
                last_kind = 2;
                if sp > 0 {
                    sp = sp - 1;
                    p = starts[sp];
                }
            } else {
                last_kind = 3;
                let sstart = p;

                if p > 1 {
                    out[p] = 47;
                    p = p + 1;
                }
                starts[sp] = sstart;
                sp = sp + 1;
                var j: i64 = 0;

                while j < seg_len {
                    out[p] = h_raw_at(base, sub, seg_start + j);
                    p = p + 1;
                    j = j + 1;
                }
            }
            seg_start = k + 1;
            seg_len = 0;
        } else {
            seg_len = seg_len + 1;
        }
        k = k + 1;
    }
    if last_kind != 3 && p > 1 {
        out[p] = 47;
        p = p + 1;
    }
    return p;
}

fn h_subpath_ok(base: const u8[:], sub: const u8[:]) -> i64 {
    if h_is_abs(sub) != 0 {
        return 0;
    }
    var tmp: u8[1024];
    var z: i64 = 0;

    while z < 1024 {
        tmp[z] = 0;
        z = z + 1;
    }
    let n = h_path_norm(base, sub, tmp[0..1024]);

    if n < 0 {
        return 0;
    }
    if n < base.len {
        return 0;
    }
    var i: i64 = 0;

    while i < base.len {
        if tmp[i] != base[i] {
            return 0;
        }
        i = i + 1;
    }
    if n == base.len {
        return 1;
    }
    if tmp[base.len] != 47 {
        return 0;
    }
    return 1;
}

// final_path = <root>/<models--repo>/snapshots/<commit>/<sub>,
// local_path = <root>/<models--repo>/blobs/<oid> when the oid is known and the
// snapshot is absent, otherwise local_path = final_path.
// lens[0] is the final length, lens[1] the local length.
// Returns 0 for a blob local path, 1 when the local path is the final path.
fn h_file_paths(
    root: const u8[:],
    repo: const u8[:],
    commit: const u8[:],
    sub: const u8[:],
    oid: const u8[:],
    final_exists: i64,
    lens: i64[:],
    out_final: u8[:],
    out_local: u8[:]) -> i64 {
    var folder: u8[512];
    var z: i64 = 0;

    while z < 512 {
        folder[z] = 0;
        z = z + 1;
    }
    let fl = h_repo_folder(repo, folder[0..512]);
    var p: i64 = 0;

    p = h_put(out_final, p, root);
    p = h_sep(out_final, p);
    p = h_put(out_final, p, folder[0..fl]);
    p = h_put(out_final, p, "/snapshots/");
    p = h_put(out_final, p, commit);
    p = h_sep(out_final, p);
    p = h_put(out_final, p, sub);
    let flen = p;
    lens[0] = flen;

    if oid.len != 0 && final_exists == 0 {
        var q: i64 = 0;

        q = h_put(out_local, q, root);
        q = h_sep(out_local, q);
        q = h_put(out_local, q, folder[0..fl]);
        q = h_put(out_local, q, "/blobs/");
        q = h_put(out_local, q, oid);
        lens[1] = q;
        return 0;
    }
    var j: i64 = 0;

    while j < flen {
        out_local[j] = out_final[j];
        j = j + 1;
    }
    lens[1] = flen;
    return 1;
}

// get_cached_ref as a table scan. Entries are (name, first line, regular file)
// in directory order; the caller supplies that order because the native code
// inherits it from fs::directory_iterator.
fn h_refs_pick(
    names: const u8[:],
    noff: i64[:],
    nlen: i64[:],
    conts: const u8[:],
    coff: i64[:],
    clen: i64[:],
    reg: i64[:],
    n: i64) -> i64 {
    var fallback: i64 = 0 - 1;
    var i: i64 = 0;

    while i < n {
        if reg[i] != 0 {
            let name = names[noff[i] .. noff[i] + nlen[i]];
            let text = conts[coff[i] .. coff[i] + clen[i]];

            if text.len != 0 && h_hex_len(text, 40) != 0 {
                if name.len == 4 && name[0] == 109 && name[1] == 97 &&
                   name[2] == 105 && name[3] == 110 {
                    return i;
                }
                if fallback < 0 {
                    fallback = i;
                }
            }
        }
        i = i + 1;
    }
    return fallback;
}

// finalize_file's branch order: 0 already in place (local == final or the final
// path exists), 1 nothing to move (local missing), 2 link or move.
fn h_finalize_action(local_eq_final: i64, final_exists: i64, local_exists: i64) -> i64 {
    if local_eq_final != 0 || final_exists != 0 {
        return 0;
    }
    if local_exists == 0 {
        return 1;
    }
    return 2;
}

export fn hf_repo_valid(s: const u8[:]) -> i64 {
    return h_repo_valid(s);
}

export fn hf_token_valid(t: const u8[:]) -> i64 {
    return h_token_valid(t);
}

export fn hf_commit_valid(s: const u8[:]) -> i64 {
    return h_commit_valid(s);
}

export fn hf_oid_valid(s: const u8[:]) -> i64 {
    return h_oid_valid(s);
}

export fn hf_repo_folder(repo: const u8[:], out: u8[:]) -> i64 {
    return h_repo_folder(repo, out);
}

export fn hf_folder_repo(folder: const u8[:], out: u8[:]) -> i64 {
    return h_folder_repo(folder, out);
}

export fn hf_is_abs(s: const u8[:]) -> i64 {
    return h_is_abs(s);
}

export fn hf_path_norm(base: const u8[:], sub: const u8[:], out: u8[:]) -> i64 {
    return h_path_norm(base, sub, out);
}

export fn hf_subpath_ok(base: const u8[:], sub: const u8[:]) -> i64 {
    return h_subpath_ok(base, sub);
}

export fn hf_file_paths(
    root: const u8[:],
    repo: const u8[:],
    commit: const u8[:],
    sub: const u8[:],
    oid: const u8[:],
    final_exists: i64,
    lens: i64[:],
    out_final: u8[:],
    out_local: u8[:]) -> i64 {
    return h_file_paths(root, repo, commit, sub, oid, final_exists, lens, out_final, out_local);
}

export fn hf_refs_pick(
    names: const u8[:],
    noff: i64[:],
    nlen: i64[:],
    conts: const u8[:],
    coff: i64[:],
    clen: i64[:],
    reg: i64[:],
    n: i64) -> i64 {
    return h_refs_pick(names, noff, nlen, conts, coff, clen, reg, n);
}

export fn hf_finalize_action(local_eq_final: i64, final_exists: i64, local_exists: i64) -> i64 {
    return h_finalize_action(local_eq_final, final_exists, local_exists);
}
