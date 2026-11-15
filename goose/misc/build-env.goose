// P10-05 S2: build-environment preflight for the owned build system.
//
// Native sources at the base commit 792293ad, copied byte-identical into
// tests/native/:
//
//   scripts/build.sh:16-30            BUILD_DIR and JOBS defaults, the exact
//                                     cmake argument vector and its order
//   scripts/prepare_upstreams.sh:20-27,42-73,76-80
//                                     pinned upstream table, the three-way
//                                     integration-patch state, the ten
//                                     integration markers, the symlink guard
//   runtime/CMakeLists.txt:28-36,44-72
//                                     build-type default, option defaults and
//                                     the subdirectory composition
//
// These are the decisions a build makes before any compiler runs, so the slice
// is fully offline and model-free: no cmake build, no submodule fetch, no git.
// The gate executes the native text — bash for the shell rules, `cmake -P` for
// the CMake rules — and compares it with the exports.
//
// Output convention: text is written to caller-sized buffers and the returned
// length is exact. env_cmake_args emits one argument per line so a caller can
// rebuild argv without a quoting layer; the native script has no argument that
// contains a newline.

import std;

const EV_TAB: u8 = 9;
const EV_NL: u8 = 10;
const EV_SPACE: u8 = 32;
const EV_DASH: u8 = 45;
const EV_QUOTE: u8 = 34;
const EV_DQUOTE: u8 = 34;

fn ev_ws(c: u8) -> bool {
    return c == EV_SPACE || c == EV_TAB || c == 11 || c == 12 || c == 13;
}

fn ev_at(s: const u8[:], p0: i64, lit: const u8[:]) -> bool {
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

fn ev_digit(c: u8) -> bool {
    return c >= 48 && c <= 57;
}

fn ev_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn ev_put(out: u8[:], at0: i64, s: const u8[:]) -> i64 {
    var at: i64 = at0;
    var i: i64 = 0;
    while i < s.len {
        at = ev_putc(out, at, s[i]);
        i = i + 1;
    }
    return at;
}

fn ev_put_int(out: u8[:], at0: i64, v0: i64) -> i64 {
    var at: i64 = at0;
    var v: i64 = v0;
    if v < 0 {
        at = ev_putc(out, at, EV_DASH);
        if v == (0 - 9223372036854775807 - 1) {
            return ev_put(out, at, "9223372036854775808");
        }
        v = 0 - v;
    }
    if v == 0 {
        return ev_putc(out, at, 48);
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
        at = ev_putc(out, at, 48 + (buf[i] as u8));
    }
    return at;
}

// bash ${VAR:-default} on an unset name and on an empty value: both take the
// default. build.sh:9-10 uses exactly that form for BUILD_DIR and JOBS.
fn ev_use_default(set: i64, val: const u8[:]) -> i64 {
    if set == 0 || val.len == 0 {
        return 1;
    }
    return 0;
}

// The same rule as an export, for callers that only hold the pair.
export fn env_use_default(set: i64, val: const u8[:]) -> i64 {
    return ev_use_default(set, val);
}

// build.sh:9 — BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}"
export fn env_build_dir(root: const u8[:], set: i64, val: const u8[:], out: u8[:]) -> i64 {
    if ev_use_default(set, val) == 0 {
        return ev_put(out, 0, val);
    }
    var at: i64 = ev_put(out, 0, root);
    at = ev_putc(out, at, 47);
    return ev_put(out, at, "build/franken-cuda");
}

// build.sh:10 — JOBS="${JOBS:-8}", passed on as `-j "$JOBS"`. The value is
// text: the script performs no numeric validation, `cmake --build -j` owns that.
export fn env_jobs(set: i64, val: const u8[:], out: u8[:]) -> i64 {
    if ev_use_default(set, val) != 0 {
        return ev_put(out, 0, "8");
    }
    return ev_put(out, 0, val);
}

// build.sh:16-30 — the configure argument vector, in the script's order:
//   -S <root>/runtime -B <BUILD_DIR> -G Ninja -DCMAKE_BUILD_TYPE=Release
//   -DCMAKE_C_COMPILER=<CC> -DCMAKE_CXX_COMPILER=<CXX>
//   -DCMAKE_CUDA_COMPILER=<CUDACXX> -DCMAKE_CUDA_ARCHITECTURES=<CUDA_ARCH>
//   -DGGML_CUDA=ON -DBMOE_BUILD_CLI=ON -DBMOE_BUILD_TESTS=ON
// Empty overrides fall back: gcc-13, g++-13, /usr/bin/nvcc, 89.
export fn env_cmake_args(root: const u8[:], build_dir: const u8[:],
                         cc_set: i64, cc: const u8[:], cxx_set: i64, cxx: const u8[:],
                         cuda_set: i64, cuda: const u8[:], arch_set: i64, arch: const u8[:],
                         out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = ev_put(out, at, "-S\n");
    at = ev_put(out, at, root);
    at = ev_put(out, at, "/runtime\n-B\n");
    at = ev_put(out, at, build_dir);
    at = ev_put(out, at, "\n-G\nNinja\n-DCMAKE_BUILD_TYPE=Release\n-DCMAKE_C_COMPILER=");
    if ev_use_default(cc_set, cc) != 0 {
        at = ev_put(out, at, "gcc-13");
    } else {
        at = ev_put(out, at, cc);
    }
    at = ev_put(out, at, "\n-DCMAKE_CXX_COMPILER=");
    if ev_use_default(cxx_set, cxx) != 0 {
        at = ev_put(out, at, "g++-13");
    } else {
        at = ev_put(out, at, cxx);
    }
    at = ev_put(out, at, "\n-DCMAKE_CUDA_COMPILER=");
    if ev_use_default(cuda_set, cuda) != 0 {
        at = ev_put(out, at, "/usr/bin/nvcc");
    } else {
        at = ev_put(out, at, cuda);
    }
    at = ev_put(out, at, "\n-DCMAKE_CUDA_ARCHITECTURES=");
    if ev_use_default(arch_set, arch) != 0 {
        at = ev_put(out, at, "89");
    } else {
        at = ev_put(out, at, arch);
    }
    at = ev_put(out, at, "\n-DGGML_CUDA=ON\n-DBMOE_BUILD_CLI=ON\n-DBMOE_BUILD_TESTS=ON\n");
    return at;
}

// runtime/CMakeLists.txt:28-30 — the default build type is "Release" only when
// the caller set none and the generator is single-config; a multi-config
// generator keeps the cache variable empty. Writes the effective value.
export fn env_build_type(set: i64, val: const u8[:], multi: i64, out: u8[:]) -> i64 {
    if (set == 0 || val.len == 0) && multi == 0 {
        return ev_put(out, 0, "Release");
    }
    return ev_put(out, 0, val);
}

// runtime/CMakeLists.txt:44-72 — which subdirectories enter the build.
// bit 0 core (always), 1 cli, 2 tests, 3 tools.
export fn env_subdirs(llama: i64, cli: i64, tests: i64, tools: i64) -> i64 {
    var m: i64 = 1;
    if cli != 0 && llama != 0 {
        m = m + 2;
    }
    if tests != 0 && llama != 0 {
        m = m + 4;
    }
    if tools != 0 {
        m = m + 8;
    }
    return m;
}

// runtime/CMakeLists.txt:32-36 — option(NAME "description" DEFAULT) parsed into
// "NAME<TAB>DEFAULT" lines, in file order, so the table is generated from the
// canonical file instead of being repeated here.
export fn env_option_table(text: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    while i < text.len {
        var j: i64 = i;
        while j < text.len && text[j] != EV_NL {
            j = j + 1;
        }
        var p: i64 = i;
        while p < j && ev_ws(text[p]) {
            p = p + 1;
        }
        if ev_at(text, p, "option(") {
            var name_at: i64 = p + 7;
            var name_end: i64 = name_at;
            while name_end < j && !ev_ws(text[name_end]) {
                name_end = name_end + 1;
            }
            if name_end > name_at {
                var q: i64 = name_at;
                while q < name_end {
                    at = ev_putc(out, at, text[q]);
                    q = q + 1;
                }
                at = ev_putc(out, at, EV_TAB);
                // The default is the token after the closing quote of the
                // description; CMake requires the description to be quoted.
                var last_q: i64 = 0 - 1;
                q = name_end;
                while q < j {
                    if text[q] == EV_QUOTE {
                        last_q = q;
                    }
                    q = q + 1;
                }
                var d0: i64 = name_end;
                if last_q >= 0 {
                    d0 = last_q + 1;
                }
                while d0 < j && ev_ws(text[d0]) {
                    d0 = d0 + 1;
                }
                var d1: i64 = d0;
                while d1 < j && text[d1] != 41 && !ev_ws(text[d1]) {
                    d1 = d1 + 1;
                }
                q = d0;
                while q < d1 {
                    at = ev_putc(out, at, text[q]);
                    q = q + 1;
                }
                at = ev_putc(out, at, EV_NL);
            }
        }
        i = j + 1;
    }
    return at;
}

// prepare_upstreams.sh:27 — [[ "$(git -C "$dir" rev-parse HEAD)" == "$sha" ]].
export fn env_pin_ok(expected: const u8[:], actual: const u8[:]) -> i64 {
    if expected.len != actual.len {
        return 0;
    }
    var i: i64 = 0;
    while i < expected.len {
        if expected[i] != actual[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

// prepare_upstreams.sh:63-70 — three-way state of the integration patch, in the
// native order: 1 already applied, 2 present with preserved local changes,
// 3 apply now.
export fn env_integration_state(reverse_ok: i64, markers: i64) -> i64 {
    if reverse_ok != 0 {
        return 1;
    }
    if markers != 0 {
        return 2;
    }
    return 3;
}

// prepare_upstreams.sh:76-80 — the symlink guard: an existing non-symlink path
// is an error, everything else may be replaced.
export fn env_link_guard(exists: i64, is_symlink: i64) -> i64 {
    if exists != 0 && is_symlink == 0 {
        return 1;
    }
    return 0;
}

// prepare_upstreams.sh:42-61 — integration_present() requires every marker:
// grep -Fq for each fixed substring. Snippets arrive as one blob plus offsets.
export fn env_markers_all(text: const u8[:], snip: const u8[:], off: const i64[:],
                          len: const i64[:], n: i64) -> i64 {
    var k: i64 = 0;
    while k < n {
        var found: i64 = 0;
        var p: i64 = 0;
        while p + len[k] <= text.len {
            var i: i64 = 0;
            var ok: i64 = 1;
            while i < len[k] {
                if text[p + i] != snip[off[k] + i] {
                    ok = 0;
                }
                i = i + 1;
            }
            if ok != 0 {
                found = 1;
                p = text.len;
            } else {
                p = p + 1;
            }
        }
        if found == 0 {
            return 0;
        }
        k = k + 1;
    }
    return 1;
}

// prepare_upstreams.sh:38-40 — the pinned upstream table, generated from the
// script text: every `clone_at <url> <dir> <sha> <branch>` line becomes
// "url<TAB>dir<TAB>sha<TAB>branch", in file order.
export fn env_upstream_pins(text: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    var rows: i64 = 0;
    while i < text.len {
        var j: i64 = i;
        while j < text.len && text[j] != EV_NL {
            j = j + 1;
        }
        if ev_at(text, i, "clone_at ") {
            var f: i64 = 0;
            var p: i64 = i + 9;
            var fields: i64[4] = [0, 0, 0, 0];
            var ends: i64[4] = [0, 0, 0, 0];
            while f < 4 && p < j {
                while p < j && ev_ws(text[p]) {
                    p = p + 1;
                }
                if p < j {
                    fields[f] = p;
                    while p < j && !ev_ws(text[p]) {
                        p = p + 1;
                    }
                    ends[f] = p;
                    f = f + 1;
                }
            }
            if f == 4 {
                if rows > 0 {
                    at = ev_putc(out, at, EV_NL);
                }
                var k: i64 = 0;
                while k < 4 {
                    if k > 0 {
                        at = ev_putc(out, at, EV_TAB);
                    }
                    var q: i64 = fields[k];
                    while q < ends[k] {
                        at = ev_putc(out, at, text[q]);
                        q = q + 1;
                    }
                    k = k + 1;
                }
                rows = rows + 1;
            }
        }
        i = j + 1;
    }
    if rows > 0 {
        at = ev_putc(out, at, EV_NL);
    }
    return at;
}
