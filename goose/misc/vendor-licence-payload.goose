// P10-07 S1: the embedded-licence payload that makes a vendored dependency
// shippable.
//
// Native source at the base commit 70de805c, copied byte-identical into
// tests/native/license.cmake (upstream llama.cpp 0b5be7e4):
//
//   cmake/license.cmake:6-20   license_add_file(): IS_ABSOLUTE resolution, the
//                              "License for <NAME>" title, the underline built
//                              by string(REGEX REPLACE "." "=" ...), the
//                              raw-string entry and the missing-file warning
//   cmake/license.cmake:22-45  license_generate(): the LICENSE_TEXT
//                              accumulation and the license.cpp skeleton
//
// The slice decides the exact bytes of every embedded licence entry and of the
// generated license.cpp. Nothing here reads the filesystem: the caller passes
// resolved paths, file contents and presence flags, so the gate is offline.
//
// Not owned: the CMake property store, file(READ), file(WRITE), the TARGET
// check and the build wiring.

import std;

const LIC_LF: u8 = 10;
const LIC_SLASH: u8 = 47;
const LIC_BACKSLASH: u8 = 92;
const LIC_COLON: u8 = 58;

fn lic_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
        return at + 1;
    }
    return at;
}

fn lic_put(out: u8[:], at: i64, lit: const u8[:]) -> i64 {
    var i: i64 = 0;
    var p: i64 = at;
    while i < lit.len {
        p = lic_putc(out, p, lit[i]);
        i = i + 1;
    }
    return p;
}

fn lic_is_alpha(c: u8) -> bool {
    return (c >= 65 && c <= 90) || (c >= 97 && c <= 122);
}

// cmake/license.cmake:7 — if(NOT IS_ABSOLUTE "${FILE}").
// CMake's IS_ABSOLUTE: a leading '/' or '\\', or a drive letter that is
// followed by ':' and a separator. A bare drive letter without a separator is
// not absolute, and "C:/" is.
fn lic_is_abs(path: const u8[:], windows: i64) -> bool {
    if path.len == 0 {
        return false;
    }
    if path[0] == LIC_SLASH {
        return true;
    }
    if windows == 0 {
        // a POSIX host: only a leading slash is absolute, "C:/x" is a relative
        // name that happens to contain a colon
        return false;
    }
    if path[0] == LIC_BACKSLASH {
        return true;
    }
    if path.len >= 3 && lic_is_alpha(path[0]) && path[1] == LIC_COLON &&
        (path[2] == LIC_SLASH || path[2] == LIC_BACKSLASH) {
        return true;
    }
    return false;
}

// The exported form: CMake evaluates the predicate inside an if(), and the
// answer depends on the host, so the platform is an explicit argument.
export fn lic_is_absolute(path: const u8[:], windows: i64) -> i64 {
    if lic_is_abs(path, windows) {
        return 1;
    }
    return 0;
}

// cmake/license.cmake:8 — set(FILE "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}").
// The project directory carries no trailing slash in the canonical build, so
// the join is one separator plus the relative path.
export fn lic_resolve(base: const u8[:], file: const u8[:], windows: i64, out: u8[:]) -> i64 {
    // the native join is reached only when the pre-check said "not absolute";
    // the platform is an argument because a relative name on one host can be an
    // absolute path on the other
    if lic_is_abs(file, windows) {
        return lic_put(out, 0, file);
    }
    var at: i64 = lic_put(out, 0, base);
    at = lic_putc(out, at, LIC_SLASH);
    return lic_put(out, at, file);
}

// cmake/license.cmake:11 — set(TITLE "License for ${NAME}").
export fn lic_title(name: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = lic_put(out, 0, "License for ");
    return lic_put(out, at, name);
}

// cmake/license.cmake:12 — string(REGEX REPLACE "." "=" UNDERLINE "${TITLE}").
// CMake's "." matches every byte, a newline included (verified by the gate), so
// the underline is exactly as long as the title and carries one '=' per byte,
// including each byte of a multi-byte UTF-8 sequence.
export fn lic_underline(title: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    while i < title.len {
        at = lic_putc(out, at, 61);
        i = i + 1;
    }
    return at;
}

// cmake/license.cmake:13 — file(READ "${FILE}" TEXT) reads in text mode: every
// CRLF pair becomes one LF, a lone CR and a lone LF are kept, and one CR that
// ends the file is dropped (a CR at EOF is treated as a line ending). The
// caller passes the raw bytes and this is the text the entry embeds.
export fn lic_file_text(raw: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    var i: i64 = 0;
    while i < raw.len {
        if raw[i] == 13 && i + 1 < raw.len && raw[i + 1] == LIC_LF {
            at = lic_putc(out, at, LIC_LF);
            i = i + 2;
        } else {
            at = lic_putc(out, at, raw[i]);
            i = i + 1;
        }
    }
    if at > 0 && out[at - 1] == 13 {
        return at - 1;
    }
    return at;
}

// cmake/license.cmake:15 — string(APPEND TMP
//   "R\"=L=(${TITLE}\n${UNDERLINE}\n\n${TEXT})=L=\",\n").
// `text` is the file content as file(READ) hands it over: verbatim, without a
// trailing-newline guarantee and without any validation of the delimiter.
fn lic_underline_at(name: const u8[:], out: u8[:], at0: i64) -> i64 {
    // "License for " is twelve bytes and contributes twelve '='
    var at: i64 = lic_put(out, at0, "============");
    var i: i64 = 0;
    while i < name.len {
        at = lic_putc(out, at, 61);
        i = i + 1;
    }
    return at;
}

export fn lic_entry(name: const u8[:], text: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = lic_put(out, 0, "R\"=L=(");
    at = lic_put(out, at, "License for ");
    at = lic_put(out, at, name);
    at = lic_putc(out, at, LIC_LF);
    at = lic_underline_at(name, out, at);
    at = lic_putc(out, at, LIC_LF);
    at = lic_putc(out, at, LIC_LF);
    at = lic_put(out, at, text);
    at = lic_put(out, at, ")=L=\",");
    at = lic_putc(out, at, LIC_LF);
    return at;
}

// cmake/license.cmake:26-30 — the generated license.cpp skeleton.
export fn lic_body(text: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = lic_put(out, 0, "// Generated by CMake\n\n");
    at = lic_put(out, at, "const char* LICENSES[] = {\n");
    at = lic_put(out, at, text);
    at = lic_put(out, at, "nullptr\n");
    at = lic_put(out, at, "};\n");
    return at;
}

// cmake/license.cmake:33-35 — message(FATAL_ERROR "Target '${TARGET_NAME}' does
// not exist"). The generated file is written before this branch, which is why a
// script-mode run of license_generate() always exits non-zero.
export fn lic_missing_target(name: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = lic_put(out, 0, "Target '");
    at = lic_put(out, at, name);
    return lic_put(out, at, "' does not exist");
}

// cmake/license.cmake:18 — message(WARNING "License file '${FILE}' not found").
// The caller passes the already-resolved path, as the native code does.
export fn lic_missing(resolved: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = lic_put(out, 0, "License file '");
    at = lic_put(out, at, resolved);
    at = lic_put(out, at, "' not found");
    return at;
}
