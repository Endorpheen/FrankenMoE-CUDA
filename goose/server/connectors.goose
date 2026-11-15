// Connectors (P8-06 S3) in Goose, from tools/server/server-connect.cpp and the
// model-source half of tools/server/server-models.cpp.
//
// Two connectors meet here, both chosen from data:
//
//   - the remote-access tunnel: the share code (generated, pasted back with
//     whitespace, normalised, printed in blocks), the address llama-connect
//     dials, the reason the tunnel cannot start and the order those reasons are
//     checked in, the binary search order and the child log level mapping;
//   - the model source: which HF repo a model resolves from, the sidecar kinds
//     a download pulls and the environment switch those kinds turn into.
//
// Nothing is spawned: the binary search is a list of directories that exist,
// the share code comes from the caller, and the dial address is a string.

import std;

const CN_MAX_HOSTS: i64 = 16;

const CN_ROOM_LEN: i64 = 8;
const CN_PASS_LEN: i64 = 32;

// share code = room code + pass code, the alphabet llama-connect and the Web UI
// use: no I, L, O, 0 or 1
const CN_CHARS: u8[54] = [65, 66, 67, 68, 69, 70, 71, 72, 74, 75, 77, 78, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98, 99, 100, 101, 102, 103, 104, 106, 107, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 50, 51, 52, 53, 54, 55, 56, 57];

const CN_SRC_CACHE: i64 = 2;

// ── byte helpers ───────────────────────────────────────────────────────────

fn cn_put_range(dst: u8[:], at: i64, s: const u8[:], from: i64, n: i64) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < n {
        if p < dst.len {
            dst[p] = s[from + i];
        }
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn cn_puts(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    return cn_put_range(dst, at, s, 0, s.len);
}

fn cn_putc(dst: u8[:], at: i64, c: u8) -> i64 {
    if at < dst.len {
        dst[at] = c;
    }
    return at + 1;
}

fn cn_put_i(dst: u8[:], at: i64, v: i64) -> i64 {
    var buf: u8[24] = [0; 24];
    var n: i64 = 0;
    var x = v;
    var q = at;

    if x < 0 {
        q = cn_putc(dst, q, 45);
        x = 0 - x;
    }
    if x == 0 {
        return cn_putc(dst, q, 48);
    }
    while x > 0 {
        buf[n] = (48 + x % 10) as u8;
        x = x / 10;
        n = n + 1;
    }
    var i = n;

    while i > 0 {
        i = i - 1;
        q = cn_putc(dst, q, buf[i]);
    }
    return q;
}

fn cn_eq(a: const u8[:], ao: i64, al: i64, b: const u8[:], bo: i64, bl: i64) -> bool {
    if al != bl {
        return false;
    }
    var i: i64 = 0;

    while i < al {
        if a[ao + i] != b[bo + i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn cn_eq_lit(a: const u8[:], ao: i64, al: i64, lit: const u8[:]) -> bool {
    return cn_eq(a, ao, al, lit, 0, lit.len);
}

fn cn_starts_with(s: const u8[:], from: i64, len: i64, head: const u8[:]) -> i64 {
    if len < head.len {
        return 0;
    }
    if cn_eq(s, from, head.len, head, 0, head.len) {
        return 1;
    }
    return 0;
}

fn cn_ends_with(s: const u8[:], from: i64, len: i64, tail: const u8[:]) -> i64 {
    if len < tail.len {
        return 0;
    }
    if cn_eq(s, from + len - tail.len, tail.len, tail, 0, tail.len) {
        return 1;
    }
    return 0;
}

fn cn_find_range(s: const u8[:], from: i64, to: i64, c: u8) -> i64 {
    var i = from;

    while i < to {
        if s[i] == c {
            return i;
        }
        i = i + 1;
    }
    return to;
}

// std::isspace in the C locale
fn cn_space(c: u8) -> i64 {
    if c == 32 {
        return 1;
    }
    if c == 9 || c == 10 || c == 11 || c == 12 || c == 13 {
        return 1;
    }
    return 0;
}

fn cn_in_alphabet(c: u8) -> i64 {
    var i: i64 = 0;

    while i < CN_CHARS.len {
        if CN_CHARS[i] == c {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

// ── the share code (normalize_share_code 60-78) ─────────────────────────────────────────────────────────

// normalize_share_code 60-78: whitespace is tolerated, the Web UI shows the code
// in blocks and users paste
// it back. Returns the normalised length, or -1 when the key is not a share
// code: the length must be exactly room+pass and every byte an alphabet byte.
fn cn_normalize_body(key: const u8[:], out: u8[:]) -> i64 {
    var n: i64 = 0;
    var i: i64 = 0;

    while i < key.len {
        if cn_space(key[i]) == 0 {
            if n < out.len {
                out[n] = key[i];
            }
            n = n + 1;
        }
        i = i + 1;
    }
    if n != CN_ROOM_LEN + CN_PASS_LEN {
        return 0 - 1;
    }
    var k: i64 = 0;

    while k < n {
        if cn_in_alphabet(out[k]) == 0 {
            return 0 - 1;
        }
        k = k + 1;
    }
    return n;
}

export fn cn_normalize_code(key: const u8[:], out: u8[:]) -> i64 {
    return cn_normalize_body(key, out);
}

// format_share_code 47-58: blocks of eight digits joined by one space
fn cn_format_body(code: const u8[:], out: u8[:]) -> i64 {
    var p: i64 = 0;
    var i: i64 = 0;

    while i < code.len {
        if i > 0 && i % 8 == 0 {
            p = cn_putc(out, p, 32);
        }
        p = cn_putc(out, p, code[i]);
        i = i + 1;
    }
    return p;
}

export fn cn_format_code(code: const u8[:], out: u8[:]) -> i64 {
    return cn_format_body(code, out);
}

// resolve_code 190-192: 0 the given code is not a share code, 1 generate one (the
// caller owns the random source), 2 the normalised code in `out`
export fn cn_resolve_code(has_code: i64, code: const u8[:], out: u8[:]) -> i64 {
    if has_code == 0 {
        return 1;
    }
    let n = cn_normalize_body(code, out);

    if n < 0 {
        return 0;
    }
    return 2;
}

// ── the dial address (upstream_host 111-129) ───────────────────────────────────────────────────────

// The child dials over TCP: a wildcard bind is reached on loopback, a specific
// address on itself, a unix socket has no address at all.
fn cn_upstream_body(n: i64, hosts: const u8[:], off: i64[:], len: i64[:], out: u8[:]) -> i64 {
    var i: i64 = 0;

    while i < n {
        let h = off[i];
        let l = len[i];

        if cn_ends_with(hosts, h, l, ".sock") == 0 {
            if cn_eq_lit(hosts, h, l, "0.0.0.0") {
                return cn_puts(out, 0, "127.0.0.1");
            }
            if cn_eq_lit(hosts, h, l, "::") {
                return cn_puts(out, 0, "[::1]");
            }
            // llama-connect builds http://host:port, an IPv6 literal needs brackets
            let colon = cn_find_range(hosts, h, h + l, 58);

            if colon < h + l && hosts[h] != 91 {
                var p = cn_putc(out, 0, 91);
                p = cn_put_range(out, p, hosts, h, l);
                p = cn_putc(out, p, 93);
                return p;
            }
            return cn_put_range(out, 0, hosts, h, l);
        }
        i = i + 1;
    }
    return 0;
}

export fn cn_upstream_host(n: i64, hosts: const u8[:], off: i64[:], len: i64[:], out: u8[:]) -> i64 {
    return cn_upstream_body(n, hosts, off, len, out);
}

// ── why the tunnel cannot start (unavailable_reason 167-188) ───────────────

// out[0] reason: 0 none, 1 no subprocess support, 2 bad share code, 3 no TCP
//        address, 4 no llama-connect binary. The text is written to `out[1]`.
// `has_code` is an explicit --connect-code; `binary_found` comes from
// cn_find_binary.
fn cn_unavailable_body(subproc_supported: i64, has_code: i64, code_valid: i64, has_tcp: i64, binary_found: i64, out: u8[:]) -> i64 {
    if subproc_supported == 0 {
        let p = cn_puts(out, 0, "this build has subprocess support disabled, rebuild with -DLLAMA_SUBPROCESS=ON");
        return p;
    }
    if has_code != 0 && code_valid == 0 {
        var p = cn_puts(out, 0, "--connect-code must be ");
        p = cn_put_i(out, p, CN_ROOM_LEN + CN_PASS_LEN);
        p = cn_puts(out, p, " characters from '");
        p = cn_put_range(out, p, CN_CHARS, 0, CN_CHARS.len);
        p = cn_putc(out, p, 39);
        return p;
    }
    if has_tcp == 0 {
        return cn_puts(out, 0, "llama-connect reaches the server over TCP, add a TCP address to --host");
    }
    if binary_found == 0 {
        var p = cn_puts(out, 0, "could not find 'llama-connect' next to llama-server or in PATH.");
        p = cn_putc(out, p, 10);
        p = cn_puts(out, p, "    it is a separate binary: download it from https://github.com/ggml-org/llama-connect/releases");
        p = cn_putc(out, p, 10);
        p = cn_puts(out, p, "    or build llama.cpp with -DLLAMA_CONNECT=ON to have it fetched automatically");
        return p;
    }
    return 0;
}

export fn cn_unavailable(subproc_supported: i64, has_code: i64, code_valid: i64, has_tcp: i64, binary_found: i64, out: u8[:]) -> i64 {
    let n = cn_unavailable_body(subproc_supported, has_code, code_valid, has_tcp, binary_found, out);

    if n == 0 {
        return 0;
    }
    if subproc_supported == 0 {
        return 1;
    }
    if has_code != 0 && code_valid == 0 {
        return 2;
    }
    if has_tcp == 0 {
        return 3;
    }
    return 4;
}

// find_binary 136-165: the copy shipped next to llama-server first, then
// every PATH entry in order, an empty entry skipped. -1 when nothing exists,
// 0 for the sibling, 1+k for the k-th PATH entry.
export fn cn_find_binary(sibling: i64, ndirs: i64, exists: i64[:]) -> i64 {
    if sibling != 0 {
        return 0;
    }
    var i: i64 = 0;

    while i < ndirs {
        if exists[i] != 0 {
            return i + 1;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// ── child log forwarding (forward_child_log 80-109) ────────────────────────

// out[0] level: 0 error, 1 warning, 2 debug, 3 info. Returns the offset of the
// text after the tag; an untagged line is the startup banner, shown as info.
fn cn_log_body(line: const u8[:], out: i64[:]) -> i64 {
    var level: i64 = 3;
    var text: i64 = 0;

    if cn_starts_with(line, 0, line.len, "[ERROR] ") != 0 {
        level = 0;
        text = 8;
    } else if cn_starts_with(line, 0, line.len, "[WARN] ") != 0 {
        level = 1;
        text = 7;
    } else if cn_starts_with(line, 0, line.len, "[INFO] ") != 0 {
        level = 3;
        text = 7;
    } else if cn_starts_with(line, 0, line.len, "[DEBUG] ") != 0 {
        level = 2;
        text = 8;
    } else if cn_starts_with(line, 0, line.len, "[TRACE] ") != 0 {
        level = 2;
        text = 8;
    }
    out[0] = level;
    out[1] = text;
    return text;
}

export fn cn_log_tag(line: const u8[:], out: i64[:]) -> i64 {
    return cn_log_body(line, out);
}

// ── the model source ───────────────────────────────────────────────────────

// server-models.cpp 692-698 model_hf_repo: a preset names its own repo, a cached
// model is listed under
// its name, a local file or a models dir entry has none.
export fn cn_model_hf_repo(has_opt: i64, opt_value: const u8[:], source: i64, name: const u8[:], out: u8[:]) -> i64 {
    if has_opt != 0 && opt_value.len > 0 {
        return cn_puts(out, 0, opt_value);
    }
    if source == CN_SRC_CACHE {
        return cn_puts(out, 0, name);
    }
    return 0;
}

// sidecar_kind index of a name, -1 when no download can pull it
fn cn_kind_of(kind: const u8[:], from: i64, len: i64) -> i64 {
    if cn_eq(kind, from, len, "mmproj", 0, 6) {
        return 0;
    }
    if cn_eq(kind, from, len, "mtp", 0, 3) {
        return 1;
    }
    if cn_eq(kind, from, len, "dflash", 0, 6) {
        return 2;
    }
    if cn_eq(kind, from, len, "eagle3", 0, 6) {
        return 3;
    }
    if cn_eq(kind, from, len, "dspark", 0, 6) {
        return 4;
    }
    return 0 - 1;
}

fn cn_kind_name(k: i64, out: u8[:], at: i64) -> i64 {
    if k == 0 {
        return cn_puts(out, at, "mmproj");
    }
    if k == 1 {
        return cn_puts(out, at, "mtp");
    }
    if k == 2 {
        return cn_puts(out, at, "dflash");
    }
    if k == 3 {
        return cn_puts(out, at, "eagle3");
    }
    return cn_puts(out, at, "dspark");
}

// server-models.cpp 655-678: a draft head resolves at the tag even without a
// model, so it is not part of
// the default set; mmproj is.
fn cn_kind_draft(k: i64) -> i64 {
    if k == 0 {
        return 0;
    }
    return 1;
}

// parse_sidecars: an absent option takes the default set, every named kind is
// checked against the one list. On an unknown kind the message goes to `out`
// and the call returns -1. Otherwise the accepted kinds are joined by ',' in
// the order the request listed them, the call returns the text length and
// meta[0] holds how many kinds were accepted.
export fn cn_sidecars(absent: i64, list: const u8[:], out: u8[:], meta: i64[:]) -> i64 {
    if absent != 0 {
        // defaults: every kind -hf pulls without a draft type
        var p: i64 = 0;
        var k: i64 = 0;
        var n: i64 = 0;

        while k < 5 {
            if cn_kind_draft(k) == 0 {
                if p > 0 {
                    p = cn_putc(out, p, 44);
                }
                p = cn_kind_name(k, out, p);
                n = n + 1;
            }
            k = k + 1;
        }
        meta[0] = n;
        return p;
    }
    meta[0] = 0;

    let end = list.len;
    var pos: i64 = 0;
    var p: i64 = 0;
    var n: i64 = 0;

    while pos <= end {
        var e = pos;

        while e < end && list[e] != 44 {
            e = e + 1;
        }
        let k = cn_kind_of(list, pos, e - pos);

        if k < 0 {
            var q = cn_puts(out, 0, "unknown sidecar '");
            q = cn_put_range(out, q, list, pos, e - pos);
            q = cn_puts(out, q, "', expected one of mmproj, mtp, dflash, eagle3, dspark");
            return 0 - 1;
        }
        if p > 0 {
            p = cn_putc(out, p, 44);
        }
        p = cn_put_range(out, p, list, pos, e - pos);
        n = n + 1;
        pos = e + 1;
    }
    meta[0] = n;
    return p;
}

// server-models.cpp 1431-1444: the environment a download child gets for its
// sidecar kinds: a draft head
// becomes "draft-<name>" in LLAMA_ARG_SPEC_TYPE, in the order of the one sidecar
// list, and mmproj turns LLAMA_ARG_MMPROJ_AUTO on. `sel` holds one flag per kind
// in that same order. Returns the length of the SPEC_TYPE value, which is
// "none" when no draft head is selected; out[0] is the MMPROJ_AUTO value.
export fn cn_sidecar_env(sel: i64[:], spec: u8[:], out: i64[:]) -> i64 {
    var mmproj: i64 = 0;
    var p: i64 = 0;
    var k: i64 = 0;

    while k < 5 {
        if sel[k] != 0 {
            if cn_kind_draft(k) == 0 {
                mmproj = 1;
            } else {
                if p > 0 {
                    p = cn_putc(spec, p, 44);
                }
                p = cn_puts(spec, p, "draft-");
                p = cn_kind_name(k, spec, p);
            }
        }
        k = k + 1;
    }
    out[0] = mmproj;
    if p == 0 {
        return cn_puts(spec, 0, "none");
    }
    return p;
}
