// P10-06 S1: CUDA architecture list policy (arch_).
//
// Native source at base commit 7bc37253, copied byte-identical into
// tests/native/ggml-cuda-CMakeLists.txt (hashes in results/native-sha256.txt):
//
//   lines   8-57   the default CMAKE_CUDA_ARCHITECTURES list, keyed by
//                  GGML_NATIVE, CUDAToolkit_VERSION and CMAKE_VERSION
//   lines  75-92   the 12X -> 12Xa rewrite over the current and native lists
//   lines  94-99   adoption of CMAKE_CUDA_ARCHITECTURES_NATIVE when "native"
//   line   97      the list-shape regex used for that adoption
//   lines  21-23   the suffix semantics (-real / -virtual / no suffix)
//   lines  10-19   the arch table (50 == Maxwell ... 120 == Blackwell)
//
// Every export is a pure decision over values the caller supplies: no cmake,
// no nvcc, no GPU, no file system. The gate runs the native text itself with
// `cmake -P` and compares the result with these exports.
//
// Conventions: 0/1 answers are 0 = false/no, 1 = true/yes. Lists are the
// CMake list form: entries joined with ';' in native order, no spaces. Every
// out buffer is caller-sized and receives exactly the returned byte count.

import std;

const ARC_SEMI: u8 = 59;
const ARC_DASH: u8 = 45;
const ARC_A: u8 = 97;
const ARC_Z: u8 = 122;
const ARC_0: u8 = 48;
const ARC_9: u8 = 57;

fn arc_eq(s: const u8[:], lit: const u8[:]) -> bool {
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

fn arc_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len && at + i < out.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + i;
}

fn arc_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
    }
    return at + 1;
}

fn arc_digit(c: u8) -> bool {
    return c >= ARC_0 && c <= ARC_9;
}

// Index of the first entry that starts at `from`, or s.len when none.
fn arc_next(s: const u8[:], from: i64) -> i64 {
    var i = from;
    while i < s.len {
        if s[i] == ARC_SEMI {
            return i;
        }
        i = i + 1;
    }
    return s.len;
}

// "11.8" style comparison on the (major, minor) pair the caller extracted.
export fn arch_ver_ge(major: i64, minor: i64, want_major: i64, want_minor: i64) -> i64 {
    if major > want_major {
        return 1;
    }
    if major < want_major {
        return 0;
    }
    if minor >= want_minor {
        return 1;
    }
    return 0;
}

// cmake-cuda CMakeLists.txt:8-57 — the default list for a non-native build,
// before the 12X rewrite. `cmake_major`/`cmake_minor` are CMAKE_VERSION's.
export fn arch_default(cuda_major: i64, cuda_minor: i64, native: i64,
                       cmake_major: i64, cmake_minor: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    if native == 1 && arch_ver_ge(cuda_major, cuda_minor, 11, 6) == 1
        && arch_ver_ge(cmake_major, cmake_minor, 3, 24) == 1 {
        return arc_put(out, at, "native");
    }
    var sep: i64 = 0;
    if arch_ver_ge(cuda_major, cuda_minor, 13, 0) == 0 {
        at = arc_put(out, at, "50-virtual");
        at = arc_putc(out, at, ARC_SEMI);
        at = arc_put(out, at, "61-virtual");
        at = arc_putc(out, at, ARC_SEMI);
        at = arc_put(out, at, "70-virtual");
        sep = 1;
    }
    if sep == 1 {
        at = arc_putc(out, at, ARC_SEMI);
    }
    at = arc_put(out, at, "75-virtual");
    at = arc_putc(out, at, ARC_SEMI);
    at = arc_put(out, at, "80-virtual");
    at = arc_putc(out, at, ARC_SEMI);
    at = arc_put(out, at, "86-real");
    if arch_ver_ge(cuda_major, cuda_minor, 11, 8) == 1 {
        at = arc_putc(out, at, ARC_SEMI);
        at = arc_put(out, at, "89-real");
        at = arc_putc(out, at, ARC_SEMI);
        at = arc_put(out, at, "90-virtual");
    }
    if arch_ver_ge(cuda_major, cuda_minor, 12, 8) == 1 {
        at = arc_putc(out, at, ARC_SEMI);
        at = arc_put(out, at, "120a-real");
    }
    if arch_ver_ge(cuda_major, cuda_minor, 12, 9) == 1 {
        at = arc_putc(out, at, ARC_SEMI);
        at = arc_put(out, at, "121a-real");
    }
    return at;
}

// Leading decimal run of an arch token as an integer, -1 when the token does
// not start with a digit.
fn arc_num_impl(s: const u8[:]) -> i64 {
    if s.len == 0 || arc_digit(s[0]) == false {
        return -1;
    }
    var n: i64 = 0;
    var i: i64 = 0;
    while i < s.len && arc_digit(s[i]) {
        n = n * 10 + (s[i] - ARC_0);
        i = i + 1;
    }
    return n;
}

fn arc_span_eq(s: const u8[:], from: i64, lit: const u8[:]) -> bool {
    if from < 0 || from + lit.len > s.len {
        return false;
    }
    var i: i64 = 0;
    while i < lit.len {
        if s[from + i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

fn arc_ends(s: const u8[:], lit: const u8[:]) -> bool {
    if s.len < lit.len {
        return false;
    }
    var i: i64 = 0;
    while i < lit.len {
        if s[s.len - lit.len + i] != lit[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// Architecture kind: 0 = no suffix (device code and PTX), 1 = -real (device
// code only), 2 = -virtual (PTX only). Native text: CMakeLists.txt:21-23.
fn arc_kind_impl(s: const u8[:]) -> i64 {
    if arc_ends(s, "-virtual") {
        return 2;
    }
    if arc_ends(s, "-real") {
        return 1;
    }
    return 0;
}

// sm number of an arch token: 120a-real -> 120. -1 when there are no digits.
fn arc_sm_impl(s: const u8[:]) -> i64 {
    return arc_num_impl(s);
}

fn arc_token_ok_span(s: const u8[:], from: i64, end: i64) -> i64 {
    if end <= from {
        return 0;
    }
    var i: i64 = from;
    var digits: i64 = 0;
    while i < end && arc_digit(s[i]) {
        digits = digits + 1;
        i = i + 1;
    }
    if digits == 0 {
        return 0;
    }
    if i < end && (s[i] == ARC_A || s[i] == 102) {
        i = i + 1;
    }
    if i == end {
        return 1;
    }
    if s[i] != ARC_DASH {
        return 0;
    }
    var rest: i64 = end - i - 1;
    if rest == 4 && arc_span_eq(s, i + 1, "real") {
        return 1;
    }
    if rest == 7 && arc_span_eq(s, i + 1, "virtual") {
        return 1;
    }
    return 0;
}

// Single-token validity, the element half of CMakeLists.txt:97's pattern
// ^[0-9]+(a|f)?(-real|-virtual)?$
fn arc_token_ok(s: const u8[:]) -> bool {
    return arc_token_ok_span(s, 0, s.len) == 1;
}

// CMakeLists.txt:97 — the list-shape regex, either over a ';'-joined list
// (list_mode = 1) or over a single token (list_mode = 0).
fn arc_valid_impl(s: const u8[:], list_mode: i64) -> i64 {
    if list_mode == 0 {
        if arc_token_ok(s) {
            return 1;
        }
        return 0;
    }
    if s.len == 0 {
        return 0;
    }
    var end0: i64 = arc_next(s, 0);
    if arc_token_ok_span(s, 0, end0) == 0 {
        return 0;
    }
    var i: i64 = end0;
    while i < s.len {
        // s[i] is ';': the element after it may be empty, which the native
        // pattern allows through its `|;` alternative
        i = i + 1;
        var end: i64 = arc_next(s, i);
        if end > i && arc_token_ok_span(s, i, end) == 0 {
            return 0;
        }
        i = end;
    }
    return 1;
}

// ^12[0-9](-real|-virtual)?$ over the token [from, end)
fn arc_is_12x(s: const u8[:], from: i64, end: i64) -> i64 {
    var len: i64 = end - from;
    if len < 3 || len > 11 {
        return 0;
    }
    if s[from] != 49 || s[from + 1] != 50 || arc_digit(s[from + 2]) == false {
        return 0;
    }
    if s[from + 2] > ARC_9 {
        return 0;
    }
    if len == 3 {
        return 1;
    }
    if len == 8 && arc_span_eq(s, from + 3, "-real") {
        return 1;
    }
    if len == 11 && arc_span_eq(s, from + 3, "-virtual") {
        return 1;
    }
    return 0;
}

// CMakeLists.txt:75-92 — every 12X token becomes 12Xa; all other tokens pass
// through unchanged, order preserved.
export fn arch_rewrite(list: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var from: i64 = 0;
    var first: i64 = 1;
    var done: i64 = 0;
    while done == 0 {
        var end: i64 = arc_next(list, from);
        var empty: i64 = 0;
        if end == from {
            empty = 1;
        }
        if empty == 0 {
            if first == 0 {
                at = arc_putc(out, at, ARC_SEMI);
            }
            first = 0;
        }
        if empty == 0 && arc_is_12x(list, from, end) == 1 {
            var i: i64 = from;
            while i < from + 3 {
                at = arc_putc(out, at, list[i]);
                i = i + 1;
            }
            at = arc_putc(out, at, ARC_A);
            i = from + 3;
            while i < end {
                at = arc_putc(out, at, list[i]);
                i = i + 1;
            }
        } else if empty == 0 {
            var i: i64 = from;
            while i < end {
                at = arc_putc(out, at, list[i]);
                i = i + 1;
            }
        }
        if end >= list.len {
            done = 1;
        } else {
            from = end + 1;
        }
    }
    return at;
}

// CMakeLists.txt:94-99 — "native" is replaced by the probed list only when the
// probed list has the expected shape; otherwise the literal stays.
export fn arch_resolve(arch: const u8[:], native_value: const u8[:], out: u8[:]) -> i64 {
    if arc_eq(arch, "native") && arc_valid_impl(native_value, 1) == 1 {
        return arc_put(out, 0, native_value);
    }
    return arc_put(out, 0, arch);
}

// 1 when the arch emits PTX that a JIT may compile (no suffix or -virtual).
export fn arch_has_ptx(s: const u8[:]) -> i64 {
    if arc_kind_impl(s) == 1 {
        return 0;
    }
    return 1;
}

// 1 when the arch emits device code for that sm (no suffix or -real).
export fn arch_has_device(s: const u8[:]) -> i64 {
    if arc_kind_impl(s) == 2 {
        return 0;
    }
    return 1;
}

export fn arch_num(s: const u8[:]) -> i64 {
    return arc_num_impl(s);
}

export fn arch_kind(s: const u8[:]) -> i64 {
    return arc_kind_impl(s);
}

export fn arch_sm(s: const u8[:]) -> i64 {
    return arc_sm_impl(s);
}

export fn arch_valid(s: const u8[:], list_mode: i64) -> i64 {
    return arc_valid_impl(s, list_mode);
}
