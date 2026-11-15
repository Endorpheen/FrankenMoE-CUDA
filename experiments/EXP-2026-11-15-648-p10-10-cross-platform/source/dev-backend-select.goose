// Backend and device selection policy, ported from ggml/src/ggml-backend-reg.cpp.
//
// Native sources (upstream/llama.cpp at the task base commit):
//   ggml-backend-reg.cpp:206-224   register_device duplicate rule
//   ggml-backend-reg.cpp:186-205   register_backend duplicate rule
//   ggml-backend-reg.cpp:307-315   striequals
//   ggml-backend-reg.cpp:318-333   ggml_backend_reg_by_name
//   ggml-backend-reg.cpp:344-352   ggml_backend_dev_by_name
//   ggml-backend-reg.cpp:354-363   ggml_backend_dev_by_type
//   ggml-backend-reg.cpp:381-390   ggml_backend_init_best
//   ggml-backend-reg.cpp:404-466   get_executable_path
//   ggml-backend-reg.cpp:468-482   backend_filename_prefix / _extension
//   ggml-backend-reg.cpp:484-560   ggml_backend_load_best
//
// Owned decisions:
//   - the name match: ASCII case-insensitive equality, first registration wins,
//     used identically for a backend and for a device;
//   - the type match: first device of that type in registration order, which is
//     what makes the order of the registry a policy and not an accident;
//   - init_best: GPU, then IGPU, then CPU, and no device at all when none of the
//     three exists (an ACCEL or META device is never chosen by this path);
//   - the dynamic-load score rule: a candidate wins only with a score strictly
//     greater than the best so far, so the first of equal scores stays, and a
//     best score of zero sends the loader to the base-name fallback;
//   - the file-name rule: the `libggml-<name>-` prefix (or `ggml-` on Windows)
//     and the exact `.so` / `.dll` extension, so `libggml-cpu.so.0` is not a
//     candidate;
//   - the search-path order: the user path alone, or GGML_BACKEND_DIR, the
//     executable directory and the current directory, with a failed
//     current-directory lookup dropping that entry;
//   - the executable prefix: the directory of the executable with a trailing
//     separator, "." on a failed Linux readlink and an empty prefix when the
//     platform has no implementation;
//   - the registration order: a duplicate backend registration is ignored
//     together with all of its devices, and a duplicate device is ignored
//     individually.
//
// Not owned: directory iteration, dlopen, the score function itself, the API
// version check, environment variables (the caller resolves GGML_BACKEND_DIR)
// and every device capability query. Everything here is a decision over values
// the caller supplies, so the module opens no library and touches no device.
//
// Platform codes match the other platform slices:
//   PLAT_LINUX = 0, PLAT_BSD = 1, PLAT_APPLE = 2, PLAT_WIN = 3.
//
// Device type codes are the native enum order:
//   0 CPU, 1 GPU, 2 IGPU, 3 ACCEL, 4 META.
//
// Every string export writes into a caller-sized buffer and returns the number
// of bytes written; a negative return means "no value" (-1 name not found,
// -1 no candidate), and nothing is written then.

const DEV_LINUX: i64 = 0;
const DEV_BSD: i64 = 1;
const DEV_APPLE: i64 = 2;
const DEV_WIN: i64 = 3;

const DEV_CPU: i64 = 0;
const DEV_GPU: i64 = 1;
const DEV_IGPU: i64 = 2;
const DEV_ACCEL: i64 = 3;
const DEV_META: i64 = 4;

fn d_put(dst: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        dst[p] = s[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

fn d_lower(c: u8) -> u8 {
    if c >= 65 && c <= 90 {
        return c + 32;
    }
    return c;
}

// striequals: ASCII case-insensitive equality of two NUL-free views.
fn d_eq_ci(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if d_lower(a[i]) != d_lower(b[i]) {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// First entry whose name matches, or -1. blob carries every name back to back,
// off and len locate entry i.
fn d_name_index(blob: const u8[:], off: i64[:], len: i64[:], n: i64, name: const u8[:]) -> i64 {
    var i: i64 = 0;

    while i < n {
        let s = blob[off[i] .. off[i] + len[i]];

        if d_eq_ci(s, name) {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// First device of the requested type, or -1.
fn d_type_index(types: i64[:], n: i64, want: i64) -> i64 {
    var i: i64 = 0;

    while i < n {
        if types[i] == want {
            return i;
        }
        i = i + 1;
    }
    return 0 - 1;
}

// ggml_backend_init_best: GPU, then IGPU, then CPU.
fn d_best_index(types: i64[:], n: i64) -> i64 {
    var i = d_type_index(types, n, DEV_GPU);

    if i >= 0 {
        return i;
    }
    i = d_type_index(types, n, DEV_IGPU);
    if i >= 0 {
        return i;
    }
    return d_type_index(types, n, DEV_CPU);
}

// ggml_backend_load_best's selection loop: strictly greater score wins, the
// running best starts at zero, and a zero best means "no candidate".
fn d_best_score(has_score: i64[:], scores: i64[:], n: i64) -> i64 {
    var best: i64 = 0;
    var best_i: i64 = 0 - 1;
    var i: i64 = 0;

    while i < n {
        if has_score[i] != 0 && scores[i] > best {
            best = scores[i];
            best_i = i;
        }
        i = i + 1;
    }
    return best_i;
}

// The last dot of the file name, with std::filesystem::path::extension
// semantics for ".", ".." and a leading dot.
fn d_extension_at(filename: const u8[:]) -> i64 {
    if filename.len == 0 {
        return 0 - 1;
    }
    if filename.len == 1 && filename[0] == 46 {
        return 0 - 1;
    }
    if filename.len == 2 && filename[0] == 46 && filename[1] == 46 {
        return 0 - 1;
    }
    var i: i64 = filename.len - 1;

    while i > 0 {
        if filename[i] == 46 {
            return i;
        }
        i = i - 1;
    }
    return 0 - 1;
}

fn d_starts(s: const u8[:], pre: const u8[:]) -> bool {
    if pre.len > s.len {
        return false;
    }
    var i: i64 = 0;

    while i < pre.len {
        if s[i] != pre[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn d_filename(platform: i64, name: const u8[:], out: u8[:]) -> i64 {
    var n: i64 = 0;

    if platform == DEV_WIN {
        n = d_put(out, 0, "ggml-");
    } else {
        n = d_put(out, 0, "libggml-");
    }
    n = d_put(out, n, name);
    if platform == DEV_WIN {
        return d_put(out, n, ".dll");
    }
    return d_put(out, n, ".so");
}

// The `filename.native().find(file_prefix) == 0 && ext == file_extension` test:
// the prefix is "[lib]ggml-<name>-" and the extension must be exactly the
// platform's shared-library suffix. The trailing dash is part of the prefix, so
// the base library "libggml-cpu.so" is not a variant candidate at all: it is
// what the base-name fallback loop loads when no variant scored above zero.
// "libggml-cpu-armv8.so" is a candidate, "libggml-cpu-armv8.so.1" is not.
fn d_match(platform: i64, name: const u8[:], filename: const u8[:]) -> i64 {
    var at: i64 = 0;

    if platform == DEV_WIN {
        if !d_starts(filename, "ggml-") {
            return 0;
        }
        at = 5;
    } else {
        if !d_starts(filename, "libggml-") {
            return 0;
        }
        at = 8;
    }
    if !d_starts(filename[at .. filename.len], name) {
        return 0;
    }
    at = at + name.len;
    if at >= filename.len || filename[at] != 45 {
        return 0;
    }
    let ext_at = d_extension_at(filename);

    if ext_at < 0 {
        return 0;
    }
    let ext = filename[ext_at .. filename.len];

    if platform == DEV_WIN {
        if ext.len == 4 && ext[0] == 46 && ext[1] == 100 && ext[2] == 108 && ext[3] == 108 {
            return 1;
        }
        return 0;
    }
    if ext.len == 3 && ext[0] == 46 && ext[1] == 115 && ext[2] == 111 {
        return 1;
    }
    return 0;
}

// get_executable_path: the executable name removed at the last separator and a
// trailing separator appended. resolved == 0 models a failed readlink or
// GetModuleFileName; Linux keeps its "." default there, Windows returns an
// empty prefix, and a path without a separator gets the separator appended.
fn d_exe_prefix(platform: i64, path: const u8[:], resolved: i64, out: u8[:]) -> i64 {
    if platform != DEV_WIN && platform != DEV_APPLE && platform != DEV_LINUX && platform != DEV_BSD {
        return 0;
    }
    if resolved == 0 {
        if platform == DEV_LINUX {
            return d_put(out, 0, "./");
        }
        return 0;
    }

    let sep: u8 = 92;
    var use_sep: u8 = 47;

    if platform == DEV_WIN {
        use_sep = sep;
    }
    var at: i64 = 0 - 1;
    var i: i64 = path.len - 1;

    while i >= 0 {
        if path[i] == use_sep {
            at = i;
            i = 0 - 1;
        } else {
            i = i - 1;
        }
    }
    var n: i64 = 0;

    if at >= 0 {
        n = d_put(out, 0, path[0 .. at]);
    } else {
        n = d_put(out, 0, path);
        if n > 0 {
            n = n;
        }
    }
    out[n] = use_sep;
    return n + 1;
}

// The search-path list ggml_backend_load_best walks, in order. Each entry lands
// in blob at off[i] with length len[i]; the return value is the entry count.
// A non-empty user_path is the only entry, as in the native function.
fn d_search_paths(
    user_path: const u8[:],
    has_dir: i64,
    dir: const u8[:],
    exe_dir: const u8[:],
    cwd: const u8[:],
    cwd_ok: i64,
    blob: u8[:],
    off: i64[:],
    len: i64[:]) -> i64 {
    var count: i64 = 0;
    var p: i64 = 0;

    if user_path.len > 0 {
        off[0] = 0;
        len[0] = user_path.len;
        d_put(blob, 0, user_path);
        return 1;
    }
    if has_dir != 0 {
        off[count] = p;
        len[count] = dir.len;
        p = d_put(blob, p, dir);
        count = count + 1;
    }
    off[count] = p;
    len[count] = exe_dir.len;
    p = d_put(blob, p, exe_dir);
    count = count + 1;
    if cwd_ok != 0 {
        off[count] = p;
        len[count] = cwd.len;
        p = d_put(blob, p, cwd);
        count = count + 1;
    }
    return count;
}

// Registration order: a repeated backend id is skipped with all of its devices,
// a repeated device id is skipped on its own.
fn d_registered_ids(
    reg_ids: i64[:],
    n_reg: i64,
    dev_reg: i64[:],
    dev_id: i64[:],
    n_dev: i64,
    seen_reg: i64[:],
    out: i64[:]) -> i64 {
    var n_seen: i64 = 0;
    var p: i64 = 0;
    var r: i64 = 0;

    while r < n_reg {
        let id = reg_ids[r];
        var dup: bool = id < 0;

        var j: i64 = 0;

        while j < n_seen {
            if seen_reg[j] == id {
                dup = true;
            }
            j = j + 1;
        }
        if !dup {
            seen_reg[n_seen] = id;
            n_seen = n_seen + 1;
            var k: i64 = 0;

            while k < n_dev {
                if dev_reg[k] == r {
                    var have: bool = false;
                    var q: i64 = 0;

                    while q < p {
                        if out[q] == dev_id[k] {
                            have = true;
                        }
                        q = q + 1;
                    }
                    if !have {
                        out[p] = dev_id[k];
                        p = p + 1;
                    }
                }
                k = k + 1;
            }
        }
        r = r + 1;
    }
    return p;
}

// ---- exports --------------------------------------------------------------

export fn dev_by_name(blob: const u8[:], off: i64[:], len: i64[:], n: i64, name: const u8[:]) -> i64 {
    return d_name_index(blob, off, len, n, name);
}

export fn dev_reg_by_name(blob: const u8[:], off: i64[:], len: i64[:], n: i64, name: const u8[:]) -> i64 {
    return d_name_index(blob, off, len, n, name);
}

export fn dev_by_type(types: i64[:], n: i64, want: i64) -> i64 {
    return d_type_index(types, n, want);
}

export fn dev_init_best(types: i64[:], n: i64) -> i64 {
    return d_best_index(types, n);
}

export fn dev_best_score(has_score: i64[:], scores: i64[:], n: i64) -> i64 {
    return d_best_score(has_score, scores, n);
}

export fn dev_backend_filename(platform: i64, name: const u8[:], out: u8[:]) -> i64 {
    return d_filename(platform, name, out);
}

export fn dev_match_candidate(platform: i64, name: const u8[:], filename: const u8[:]) -> i64 {
    return d_match(platform, name, filename);
}

export fn dev_exe_prefix(platform: i64, path: const u8[:], resolved: i64, out: u8[:]) -> i64 {
    return d_exe_prefix(platform, path, resolved, out);
}

export fn dev_search_paths(
    user_path: const u8[:],
    has_dir: i64,
    dir: const u8[:],
    exe_dir: const u8[:],
    cwd: const u8[:],
    cwd_ok: i64,
    blob: u8[:],
    off: i64[:],
    len: i64[:]) -> i64 {
    return d_search_paths(user_path, has_dir, dir, exe_dir, cwd, cwd_ok, blob, off, len);
}

export fn dev_registered_ids(
    reg_ids: i64[:],
    n_reg: i64,
    dev_reg: i64[:],
    dev_id: i64[:],
    n_dev: i64,
    seen_reg: i64[:],
    out: i64[:]) -> i64 {
    return d_registered_ids(reg_ids, n_reg, dev_reg, dev_id, n_dev, seen_reg, out);
}
