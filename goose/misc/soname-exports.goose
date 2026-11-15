// P10-08 S6: SONAME, shared-object versioning and the export surface.
//
// Native sources and artifacts at the base commit:
//   ggml/CMakeLists.txt:6-8, 31        GGML_VERSION_MAJOR/MINOR/PATCH, base text
//   ggml/src/CMakeLists.txt:212-214    VERSION ${GGML_VERSION}, SOVERSION major
//   CMakeLists.txt:6-20                LLAMA_VERSION_*, -dev suffix rule
//   src/CMakeLists.txt:49-52           VERSION base, SOVERSION major
//   common/CMakeLists.txt:126-128      llama-common inherits the llama version
//   tools/mtmd/CMakeLists.txt:77-79    mtmd inherits the llama version
//   ggml/include/ggml.h:170-188        GGML_API visibility macro
//   ggml/include/ggml-backend.h:6-18   GGML_BACKEND_API visibility macro
//   include/llama.h:15-27              LLAMA_API visibility macro
//   docs/goose-migration/abi-ledger.json   the reconstructed runtime ledger
//   build/exp054-dflash/bin/*.so.*     the reference shared objects (LP64)
//
// The rule every consumer of these libraries depends on: a SONAME is
// "lib<stem>.so.<soversion>", the symlink chain is ".so -> .so.<soversion> ->
// .so.<version>", and the soversion is the version's major component. A build
// that changes any of the three renames the library for the dynamic loader even
// though the ABI did not move.
//
// The export surface is frozen as a reconciliation, not as a symbol list:
//   - every function declared with the public macro of a header must be
//     exported by at least one of the libraries that header belongs to;
//   - one declaration in the reference build is newer than the shared object
//     that carries it (ggml_backend_op_alloc_size_may_expand is defined in the
//     source and absent from the reference .so), so it is an explicit,
//     enumerated allowance instead of a silent hole;
//   - six llama_* symbols are exported with no declaration in llama.h
//     (the session/state compatibility set). They are listed, because a
//     consumer can bind to them and they therefore cannot disappear unnoticed;
//   - the ledger describes a differently named library
//     (libllama-exp371.so.0.7.0 against upstream libllama.so.0), so its counts
//     are frozen as ledger facts and checked against the reference objects.
//
// The prefix is son_.

import std;

const SON_NONE: i64 = 0 - 1;
const SON_DIGIT: const u8[:] = "0123456789";

// ── shared objects ─────────────────────────────────────────────────────────
const SON_LIB_N: i64 = 7;

const SON_LIB_STEM: const u8[:][7] = [
    "llama",
    "llama-common",
    "mtmd",
    "ggml",
    "ggml-base",
    "ggml-cpu",
    "ggml-cuda"
];

const SON_LIB_VER: const u8[:][7] = [
    "0.3.0",
    "0.3.0",
    "0.3.0",
    "0.22.0",
    "0.22.0",
    "0.22.0",
    "0.22.0"
];

const SON_LIB_SOV: i64[7] = [0, 0, 0, 0, 0, 0, 0];

// Required DT_NEEDED entries, one call per index, -1 when exhausted.
// libllama does not link a device backend: backends are loaded at run time,
// so a direct dependency on libggml-cuda would freeze the GPU choice into the
// link line.
const SON_LIB_NEEDED_N: i64[7] = [2, 2, 1, 1, 0, 1, 1];

const SON_LIB_NEEDED: const u8[:][8] = [
    "libggml.so.0",
    "libggml-base.so.0",
    "libllama.so.0",
    "libggml.so.0",
    "libllama.so.0",
    "libggml-base.so.0",
    "libggml-base.so.0",
    "libggml-base.so.0"
];

const SON_LIB_FORBID: const u8[:][7] = [
    "libggml-cuda.so.0",
    "libggml-cuda.so.0",
    "libggml-cuda.so.0",
    "libllama.so.0",
    "libllama.so.0",
    "libllama.so.0",
    "libllama.so.0"
];

// ── public headers and the libraries that must export their declarations ────
// Library bit i corresponds to SON_LIB_STEM[i].
const SON_HDR_N: i64 = 5;

const SON_HDR_NAME: const u8[:][5] = [
    "llama.h",
    "gguf.h",
    "ggml-backend.h",
    "ggml-opt.h",
    "ggml-alloc.h"
];

const SON_HDR_LIBS: i64[5] = [
    1,
    16,
    24,
    16,
    16
];

// ── the enumerated allowance and the undeclared exports ────────────────────
const SON_ALLOW_N: i64 = 1;
// Defined at ggml/src/ggml-backend.cpp:2114 in the header canon and absent from
// the reference .so; a vintage difference, not a missing implementation.
const SON_ALLOW_NAME: const u8[:][1] = [
    "ggml_backend_op_alloc_size_may_expand"
];

const SON_LEGACY_N: i64 = 6;
const SON_LEGACY_NAME: const u8[:][6] = [
    "llama_copy_state_data",
    "llama_get_expert_cache_stats",
    "llama_get_state_size",
    "llama_load_session_file",
    "llama_save_session_file",
    "llama_set_state_data"
];

// ── the reconstructed-runtime ledger (abi-ledger.json) ─────────────────────
const SON_LEDGER_EXPORTS: i64 = 307;
const SON_LEDGER_DECLS: i64 = 165;
const SON_LEDGER_IMPORTS: i64 = 948;
const SON_LEDGER_STRUCTS: i64 = 0;
const SON_LEDGER_LIB: const u8[:] = "libllama-exp371.so.0.7.0";

// ── export macros ──────────────────────────────────────────────────────────
// Codes: 0 plain extern, 1 default-visibility extern, 2 plain declaration,
// 3 default-visibility declaration. Native expansion text decides which.
const SON_MACRO_N: i64 = 3;
const SON_MACRO_NAME: const u8[:][3] = [
    "GGML_API",
    "GGML_BACKEND_API",
    "LLAMA_API"
];

fn son_copy(out: u8[:], s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && n < out.len {
        out[n] = s[n];
        n = n + 1;
    }
    return n;
}

fn son_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var n: i64 = 0;
    while n < s.len && at + n < out.len {
        out[at + n] = s[n];
        n = n + 1;
    }
    return at + n;
}

fn son_macro_base(a: i64) -> i64 {
    return 10 * a;
}

// ---- exports --------------------------------------------------------------

export fn son_lib_count() -> i64 {
    return SON_LIB_N;
}

export fn son_lib_stem(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    return son_copy(out, SON_LIB_STEM[id]);
}

export fn son_lib_version(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    return son_copy(out, SON_LIB_VER[id]);
}

export fn son_lib_soversion(id: i64) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    return SON_LIB_SOV[id];
}

// "lib<stem>.so.<soversion>"
export fn son_soname(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    var at: i64 = son_put(out, 0, "lib");
    at = son_put(out, at, SON_LIB_STEM[id]);
    at = son_put(out, at, ".so.");
    var digits: u8[:] = out;
    // soversion is a single decimal digit in this vintage; written by hand so
    // the function does not need a number formatter.
    if at < digits.len {
        digits[at] = SON_DIGIT[SON_LIB_SOV[id]];
        at = at + 1;
    }
    return at;
}

// "lib<stem>.so -> lib<stem>.so.<sov> -> lib<stem>.so.<version>"
export fn son_chain(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    var at: i64 = son_put(out, 0, "lib");
    at = son_put(out, at, SON_LIB_STEM[id]);
    at = son_put(out, at, ".so -> lib");
    at = son_put(out, at, SON_LIB_STEM[id]);
    at = son_put(out, at, ".so.");
    if at < out.len {
        out[at] = SON_DIGIT[SON_LIB_SOV[id]];
        at = at + 1;
    }
    at = son_put(out, at, " -> lib");
    at = son_put(out, at, SON_LIB_STEM[id]);
    at = son_put(out, at, ".so.");
    at = son_put(out, at, SON_LIB_VER[id]);
    return at;
}

export fn son_needed_count(id: i64) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    return SON_LIB_NEEDED_N[id];
}

export fn son_needed_at(id: i64, i: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_LIB_N || i < 0 || i >= SON_LIB_NEEDED_N[id] {
        return SON_NONE;
    }
    var base: i64 = 0;
    var k: i64 = 0;
    while k < id {
        base = base + SON_LIB_NEEDED_N[k];
        k = k + 1;
    }
    return son_copy(out, SON_LIB_NEEDED[base + i]);
}

export fn son_forbidden_needed(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_LIB_N {
        return SON_NONE;
    }
    return son_copy(out, SON_LIB_FORBID[id]);
}

export fn son_header_count() -> i64 {
    return SON_HDR_N;
}

export fn son_header_name(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_HDR_N {
        return SON_NONE;
    }
    return son_copy(out, SON_HDR_NAME[id]);
}

// Library bitmask that must export the header's declarations.
export fn son_header_libs(id: i64) -> i64 {
    if id < 0 || id >= SON_HDR_N {
        return SON_NONE;
    }
    return SON_HDR_LIBS[id];
}

export fn son_allowance_count() -> i64 {
    return SON_ALLOW_N;
}

export fn son_allowance_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= SON_ALLOW_N {
        return SON_NONE;
    }
    return son_copy(out, SON_ALLOW_NAME[i]);
}

export fn son_legacy_count() -> i64 {
    return SON_LEGACY_N;
}

export fn son_legacy_name(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= SON_LEGACY_N {
        return SON_NONE;
    }
    return son_copy(out, SON_LEGACY_NAME[i]);
}

export fn son_ledger_exports() -> i64 {
    return SON_LEDGER_EXPORTS;
}

export fn son_ledger_declarations() -> i64 {
    return SON_LEDGER_DECLS;
}

export fn son_ledger_imports() -> i64 {
    return SON_LEDGER_IMPORTS;
}

export fn son_ledger_structs() -> i64 {
    return SON_LEDGER_STRUCTS;
}

export fn son_ledger_library(out: u8[:]) -> i64 {
    return son_copy(out, SON_LEDGER_LIB);
}

export fn son_macro_count() -> i64 {
    return SON_MACRO_N;
}

export fn son_macro_name(id: i64, out: u8[:]) -> i64 {
    if id < 0 || id >= SON_MACRO_N {
        return SON_NONE;
    }
    return son_copy(out, SON_MACRO_NAME[id]);
}

// Export-macro outcome for one macro and one shared/static configuration.
export fn son_macro_outcome(id: i64, shared: i64) -> i64 {
    if id < 0 || id >= SON_MACRO_N {
        return SON_NONE;
    }
    var base: i64 = 0;
    if id == 2 {
        base = 2;
    }
    if shared != 0 {
        base = base + 1;
    }
    return base;
}

// Split a shared-object file name: stem, version and soversion, newline
// separated. Returns the number of bytes written, or -1 when the name is not
// of the form "lib<stem>.so.<version>".
export fn son_split_soname(name: const u8[:], out: u8[:]) -> i64 {
    if name.len < 8 {
        return SON_NONE;
    }
    if name[0] != 108 || name[1] != 105 || name[2] != 98 {
        return SON_NONE;
    }
    // the last ".so." in the name
    var pos: i64 = SON_NONE;
    var i: i64 = 0;
    while i + 4 <= name.len {
        if name[i] == 46 && name[i + 1] == 115 && name[i + 2] == 111 && name[i + 3] == 46 {
            pos = i;
        }
        i = i + 1;
    }
    if pos <= 3 || pos + 4 >= name.len {
        return SON_NONE;
    }
    var at: i64 = 0;
    // stem: bytes 3 .. pos-1
    i = 3;
    while i < pos && at < out.len {
        out[at] = name[i];
        at = at + 1;
        i = i + 1;
    }
    if at < out.len {
        out[at] = 10;
        at = at + 1;
    }
    // version: bytes pos+4 .. end
    var sov_end: i64 = pos + 4;
    while sov_end < name.len && name[sov_end] != 46 {
        sov_end = sov_end + 1;
    }
    i = pos + 4;
    while i < name.len && at < out.len {
        out[at] = name[i];
        at = at + 1;
        i = i + 1;
    }
    if at < out.len {
        out[at] = 10;
        at = at + 1;
    }
    // soversion: the major component
    i = pos + 4;
    while i < sov_end && at < out.len {
        out[at] = name[i];
        at = at + 1;
        i = i + 1;
    }
    return at;
}

// 1 when the name is the upstream llama SONAME a consumer links against.
export fn son_identity_matches(name: const u8[:]) -> i64 {
    if name.len != 12 {
        return 0;
    }
    var want: const u8[:] = "libllama.so.0";
    var i: i64 = 0;
    while i < 12 {
        if name[i] != want[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}
