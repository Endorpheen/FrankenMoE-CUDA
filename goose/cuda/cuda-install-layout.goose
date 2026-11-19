// P10-06 S3: install and dist layout of the CUDA backend artefact (inst_).
//
// Native source at base commit 7bc37253, copied byte-identical into
// tests/native/ (hashes in results/native-sha256.txt):
//
//   ggml/src/CMakeLists.txt:379-424  ggml_add_backend_library(): library kind,
//                                    install destination, GGML_BACKEND_DL /
//                                    GGML_BACKEND_BUILD / GGML_BACKEND_SHARED,
//                                    the VERSION/SOVERSION exception, the
//                                    GGML_AVAILABLE_BACKENDS bookkeeping
//   ggml/src/CMakeLists.txt:366-371  GGML_BACKEND_DIR requires GGML_BACKEND_DL
//   ggml/CMakeLists.txt:6-9,31       GGML_VERSION and GGML_VERSION_MAJOR
//
// Every export is a pure decision over the caller's configure values. The gate
// builds and installs a synthetic project that contains the native function
// text and compares the real installed file list with these exports.
//
// Boundaries: the file system, the compiler, the installer and the macOS
// dylib naming stay outside; inst_files describes the Linux layout only, and
// inst_versioned covers the macOS MODULE exception as a predicate.
//
// Conventions: 0/1 answers are 0 = false/no, 1 = true/yes. `dir` is the
// install destination as the native text resolves it ("lib", "bin" or the
// GGML_BACKEND_DIR value).

import std;

const INS_SEMI: u8 = 59;
const INS_SLASH: u8 = 47;

fn ins_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len && at + i < out.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + i;
}

fn ins_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
    }
    return at + 1;
}

fn ins_num(out: u8[:], at: i64, v: i64) -> i64 {
    var pos: i64 = at;
    if v == 0 {
        return ins_putc(out, pos, 48);
    }
    var d: i64 = 0;
    var t: i64 = v;
    while t > 0 {
        d = d + 1;
        t = t / 10;
    }
    var k: i64 = d;
    while k > 0 {
        var p: i64 = 1;
        var j: i64 = 1;
        while j < k {
            p = p * 10;
            j = j + 1;
        }
        pos = ins_putc(out, pos, (48 + (v / p) % 10) as u8);
        k = k - 1;
    }
    return pos;
}

// CMakeLists.txt:380-395 — 0 = STATIC, 1 = SHARED, 2 = MODULE.
fn ins_kind_impl(dl: i64, shared: i64) -> i64 {
    if dl == 1 {
        return 2;
    }
    if shared == 1 {
        return 1;
    }
    return 0;
}

// CMakeLists.txt:386-394 — 0 = CMAKE_INSTALL_LIBDIR default, 1 = BINDIR,
// 2 = GGML_BACKEND_DIR.
fn ins_dest_impl(dl: i64, dir_set: i64) -> i64 {
    if dl == 0 {
        return 0;
    }
    if dir_set == 1 {
        return 2;
    }
    return 1;
}

// CMakeLists.txt:405-412 — VERSION/SOVERSION are skipped only for an Apple
// dlopen module.
fn ins_versioned_impl(dl: i64, apple: i64) -> i64 {
    if dl == 1 && apple == 1 {
        return 0;
    }
    return 1;
}

// CMakeLists.txt:379-395 plus the VERSION/SOVERSION properties — the relative
// paths the installer writes on Linux, in link-name order.
fn ins_files_impl(dl: i64, shared: i64, dir: const u8[:], name: const u8[:],
                   version: const u8[:], soversion: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var kind: i64 = ins_kind_impl(dl, shared);
    at = ins_put(out, at, dir);
    at = ins_putc(out, at, INS_SLASH);
    at = ins_put(out, at, "lib");
    at = ins_put(out, at, name);
    if kind == 0 {
        at = ins_put(out, at, ".a");
        return at;
    }
    if kind == 2 {
        at = ins_put(out, at, ".so");
        return at;
    }
    var base: i64 = at;
    at = ins_put(out, at, ".so.");
    at = ins_put(out, at, version);
    at = ins_putc(out, at, INS_SEMI);
    var i: i64 = 0;
    while i < base {
        at = ins_putc(out, at, out[i]);
        i = i + 1;
    }
    at = ins_put(out, at, ".so.");
    at = ins_put(out, at, soversion);
    at = ins_putc(out, at, INS_SEMI);
    i = 0;
    while i < base {
        at = ins_putc(out, at, out[i]);
        i = i + 1;
    }
    at = ins_put(out, at, ".so");
    return at;
}

// CMakeLists.txt:384,400-403 — the target's own define list in native order.
// GGML_BACKEND_DL and GGML_BACKEND_BUILD are PRIVATE, GGML_BACKEND_SHARED is
// PUBLIC, so a PUBLIC definition also lands in the target's own list.
fn ins_defs_impl(dl: i64, shared: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    var any: i64 = 0;
    if dl == 1 {
        at = ins_put(out, at, "GGML_BACKEND_DL");
        any = 1;
    }
    if shared == 1 {
        if any == 1 {
            at = ins_putc(out, at, INS_SEMI);
        }
        at = ins_put(out, at, "GGML_BACKEND_BUILD");
        at = ins_putc(out, at, INS_SEMI);
        at = ins_put(out, at, "GGML_BACKEND_SHARED");
        any = 1;
    }
    if any == 0 {
        return 0;
    }
    return at;
}

// CMakeLists.txt:402 — the one public define.
fn ins_public_defs_impl(shared: i64, out: u8[:]) -> i64 {
    if shared == 0 {
        return 0;
    }
    return ins_put(out, 0, "GGML_BACKEND_SHARED");
}

// CMakeLists.txt:414-423 — append the backend to GGML_AVAILABLE_BACKENDS once.
fn ins_avail_impl(existing: const u8[:], backend: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = ins_put(out, 0, existing);
    if existing.len == 0 {
        return ins_put(out, 0, backend);
    }
    var pos_from: i64 = 0;
    var found: i64 = 0;
    var done: i64 = 0;
    while done == 0 {
        var end: i64 = pos_from;
        while end < existing.len && existing[end] != INS_SEMI {
            end = end + 1;
        }
        if end - pos_from == backend.len {
            var same: i64 = 1;
            var i: i64 = 0;
            while i < backend.len {
                if existing[pos_from + i] != backend[i] {
                    same = 0;
                }
                i = i + 1;
            }
            if same == 1 {
                found = 1;
            }
        }
        if end >= existing.len {
            done = 1;
        } else {
            pos_from = end + 1;
        }
    }
    if found == 1 {
        return at;
    }
    at = ins_putc(out, at, INS_SEMI);
    return ins_put(out, at, backend);
}

// ggml/CMakeLists.txt:6-9,31 — "major.minor.patch".
fn ins_version_impl(major: i64, minor: i64, patch: i64, out: u8[:]) -> i64 {
    var at: i64 = ins_num(out, 0, major);
    at = ins_putc(out, at, 46);
    at = ins_num(out, at, minor);
    at = ins_putc(out, at, 46);
    return ins_num(out, at, patch);
}

// CMakeLists.txt:366-371 — GGML_BACKEND_DIR without GGML_BACKEND_DL is fatal.
fn ins_guard_impl(dl: i64, dir_set: i64) -> i64 {
    if dl == 0 && dir_set == 1 {
        return 1;
    }
    return 0;
}

export fn inst_kind(dl: i64, shared: i64) -> i64 {
    return ins_kind_impl(dl, shared);
}

export fn inst_dest(dl: i64, dir_set: i64) -> i64 {
    return ins_dest_impl(dl, dir_set);
}

export fn inst_versioned(dl: i64, apple: i64) -> i64 {
    return ins_versioned_impl(dl, apple);
}

export fn inst_files(dl: i64, shared: i64, dir: const u8[:], name: const u8[:],
                     version: const u8[:], soversion: const u8[:], out: u8[:]) -> i64 {
    return ins_files_impl(dl, shared, dir, name, version, soversion, out);
}

export fn inst_defs(dl: i64, shared: i64, out: u8[:]) -> i64 {
    return ins_defs_impl(dl, shared, out);
}

export fn inst_public_defs(shared: i64, out: u8[:]) -> i64 {
    return ins_public_defs_impl(shared, out);
}

export fn inst_avail(existing: const u8[:], backend: const u8[:], out: u8[:]) -> i64 {
    return ins_avail_impl(existing, backend, out);
}

export fn inst_version(major: i64, minor: i64, patch: i64, out: u8[:]) -> i64 {
    return ins_version_impl(major, minor, patch, out);
}

// The SOVERSION value the same VERSION/SOVERSION rule installs: the major
// number in decimal.
export fn inst_soversion(major: i64, out: u8[:]) -> i64 {
    return ins_num(out, 0, major);
}

export fn inst_guard(dl: i64, dir_set: i64) -> i64 {
    return ins_guard_impl(dl, dir_set);
}
