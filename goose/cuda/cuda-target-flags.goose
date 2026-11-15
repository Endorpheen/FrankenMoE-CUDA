// P10-06 S2: CUDA compile flags, preprocessor defines and link set (tgt_).
//
// Native source at base commit 7bc37253, copied byte-identical into
// tests/native/ggml-cuda-CMakeLists.txt (hashes in results/native-sha256.txt):
//
//   lines 131-156  the add_compile_definitions() table
//   lines 157-193  the static/shared link selection, VMM and NCCL guards
//   lines 195-266  the CUDA_FLAGS list and the host-compiler join
//
// Every export is a pure decision: it returns the CMake list the native text
// would build, joined with ';' in native order. The gate configures a real
// CMake project with the extracted native blocks and reads the resulting
// COMPILE_OPTIONS, COMPILE_DEFINITIONS and LINK_LIBRARIES back.
//
// Not owned here: the contents of ggml_get_flags() (the caller passes the host
// flag text it produced), the CUDA toolkit itself, and any linking. GGML_CUDA_CUB_3DOT2
// is carried as an input bit because the native text gates CCCL::CCCL on it.
//
// Conventions: 0/1 answers are 0 = false/no, 1 = true/yes.

import std;

const TGT_SEMI: u8 = 59;

fn tgt_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < s.len && at + i < out.len {
        out[at + i] = s[i];
        i = i + 1;
    }
    return at + i;
}

fn tgt_putc(out: u8[:], at: i64, c: u8) -> i64 {
    if at < out.len {
        out[at] = c;
    }
    return at + 1;
}

fn tgt_ver_ge(major: i64, minor: i64, want_major: i64, want_minor: i64) -> bool {
    if major > want_major {
        return true;
    }
    if major < want_major {
        return false;
    }
    return minor >= want_minor;
}

// CMakeLists.txt:196-214 — CUDA_FLAGS before any -Xcompiler element.
fn tgt_flags_impl(major: i64, minor: i64, debug: i64, fatal: i64,
                  mode: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = tgt_put(out, at, "-use_fast_math");
    at = tgt_putc(out, at, TGT_SEMI);
    at = tgt_put(out, at, "-extended-lambda");
    if debug == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "-lineinfo");
    }
    if tgt_ver_ge(major, minor, 12, 8) {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "-compress-mode=");
        at = tgt_put(out, at, mode);
    }
    if fatal == 1 {
        // list(APPEND CUDA_FLAGS -Werror all-warnings) is two list elements
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "-Werror");
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "all-warnings");
    }
    return at;
}

// CMakeLists.txt:195-266 — the final CUDA_FLAGS including the single joined
// -Xcompiler argument. `host_flags` is what ggml_get_flags() produced, already
// joined with spaces; it is added only when GGML_ALL_WARNINGS is on and the
// host compiler is not MSVC.
fn tgt_final_impl(major: i64, minor: i64, debug: i64, fatal: i64, mode: const u8[:],
                  msvc: i64, all_warn: i64, host_flags: const u8[:], out: u8[:]) -> i64 {
    var at: i64 = tgt_flags_impl(major, minor, debug, fatal, mode, out);
    // CUDA_CXX_FLAGS: [host flags], then -Wno-pedantic or /Zc:preprocessor
    var warn: const u8[:] = "-Wno-pedantic";
    if msvc == 1 {
        warn = "/Zc:preprocessor";
    }
    var prepend: i64 = 0;
    if all_warn == 1 && msvc == 0 && host_flags.len > 0 {
        prepend = 1;
    }
    var joined: i64 = 1;
    if prepend == 0 && warn.len == 0 {
        joined = 0;
    }
    if joined == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "-Xcompiler");
        at = tgt_putc(out, at, TGT_SEMI);
        if prepend == 1 {
            at = tgt_put(out, at, host_flags);
            if warn.len > 0 {
                at = tgt_putc(out, at, 32);
            }
        }
        if warn.len > 0 {
            at = tgt_put(out, at, warn);
        }
    }
    return at;
}

// CMakeLists.txt:131-156 plus the NCCL define (185-188) and the debug define
// (198-201) — the directory-level define list in native order.
fn tgt_defs_impl(peer: const u8[:], graphs: i64, mmq: i64, cublas: i64, novmm: i64,
                 fa: i64, nopeer: i64, nccl: i64, nccl_found: i64, debug: i64,
                 out: u8[:]) -> i64 {
    var at: i64 = 0;
    at = tgt_put(out, at, "GGML_CUDA_PEER_MAX_BATCH_SIZE=");
    at = tgt_put(out, at, peer);
    if graphs == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_USE_GRAPHS");
    }
    if mmq == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_FORCE_MMQ");
    }
    if cublas == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_FORCE_CUBLAS");
    }
    if novmm == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_NO_VMM");
    }
    if fa == 0 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_NO_FA");
    }
    if nopeer == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_NO_PEER_COPY");
    }
    if nccl == 1 && nccl_found == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_USE_NCCL");
    }
    if debug == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "GGML_CUDA_DEBUG");
    }
    return at;
}

// CMakeLists.txt:157-193 — the link set in native order.
fn tgt_link_impl(is_static: i64, win: i64, major: i64, minor: i64, novmm: i64,
                 nccl_found: i64, cub: i64, out: u8[:]) -> i64 {
    var at: i64 = 0;
    var first: i64 = 1;
    if is_static == 1 && win == 1 {
        at = tgt_put(out, at, "CUDA::cudart_static");
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "CUDA::cublas");
        first = 0;
    } else {
        if cub == 1 {
            at = tgt_put(out, at, "CCCL::CCCL");
            at = tgt_putc(out, at, TGT_SEMI);
            first = 0;
        }
        if is_static == 1 {
            at = tgt_put(out, at, "CUDA::cudart_static");
            at = tgt_putc(out, at, TGT_SEMI);
            if tgt_ver_ge(major, minor, 10, 1) {
                at = tgt_put(out, at, "CUDA::cublas_static");
                at = tgt_putc(out, at, TGT_SEMI);
                at = tgt_put(out, at, "CUDA::cublasLt_static");
            } else {
                at = tgt_put(out, at, "CUDA::cublas_static");
            }
        } else {
            at = tgt_put(out, at, "CUDA::cudart");
            at = tgt_putc(out, at, TGT_SEMI);
            at = tgt_put(out, at, "CUDA::cublas");
        }
        first = 0;
    }
    if novmm == 0 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "CUDA::cuda_driver");
    }
    if nccl_found == 1 {
        at = tgt_putc(out, at, TGT_SEMI);
        at = tgt_put(out, at, "NCCL::NCCL");
    }
    if first == 1 {
        return 0;
    }
    return at;
}

export fn tgt_flags(major: i64, minor: i64, debug: i64, fatal: i64,
                    mode: const u8[:], out: u8[:]) -> i64 {
    return tgt_flags_impl(major, minor, debug, fatal, mode, out);
}

export fn tgt_final(major: i64, minor: i64, debug: i64, fatal: i64, mode: const u8[:],
                    msvc: i64, all_warn: i64, host_flags: const u8[:], out: u8[:]) -> i64 {
    return tgt_final_impl(major, minor, debug, fatal, mode, msvc, all_warn, host_flags, out);
}

export fn tgt_defs(peer: const u8[:], graphs: i64, mmq: i64, cublas: i64, novmm: i64,
                   fa: i64, nopeer: i64, nccl: i64, nccl_found: i64, debug: i64,
                   out: u8[:]) -> i64 {
    return tgt_defs_impl(peer, graphs, mmq, cublas, novmm, fa, nopeer, nccl, nccl_found,
                         debug, out);
}

export fn tgt_link(is_static: i64, win: i64, major: i64, minor: i64, novmm: i64,
                   nccl_found: i64, cub: i64, out: u8[:]) -> i64 {
    return tgt_link_impl(is_static, win, major, minor, novmm, nccl_found, cub, out);
}
