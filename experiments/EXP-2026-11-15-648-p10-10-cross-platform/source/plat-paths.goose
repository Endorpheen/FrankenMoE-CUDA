// Platform paths, environment writes, process priority and TTY colour policy,
// ported from common/common.cpp.
//
// Native sources (upstream/llama.cpp at the task base commit):
//   common/common.h:25-29              DIRECTORY_SEPARATOR
//   common/common.cpp:1007-1017        common_set_env
//   common/common.cpp:1019-1071        fs_get_cache_directory
//   common/common.cpp:1073-1116        fs_get_config_directory
//   common/common.cpp:1118-1127        fs_get_cache_file
//   common/common.cpp:1185-1202        tty_can_use_colors
//   common/common.cpp:232-283          set_process_priority (both branches)
//
// Owned decisions:
//   - the separator per platform ('\\' on Windows, '/' everywhere else);
//   - the cache-root resolution order: LLAMA_CACHE first (returned as given,
//     with a trailing separator), then XDG_CACHE_HOME / $HOME/.cache/
//     getpwuid fallback on the Linux family, $HOME/Library/Caches on Apple,
//     %LOCALAPPDATA% on Windows, an abort on Emscripten, and the "llama.cpp"
//     suffix that is appended only when the root was not taken from the
//     environment;
//   - the config-root resolution order, where Apple shares the XDG branch and
//     the getpwuid fallback is Linux-only;
//   - the cache-file rule: the file name must not contain a directory
//     separator (the native GGML_ASSERT), so a caller cannot escape the root;
//   - the environment write rule: an empty value unsets on POSIX and still goes
//     through _putenv_s on Windows;
//   - the priority mapping and its NORMAL short-circuit, which returns success
//     without touching the process;
//   - the colour rule: a non-empty NO_COLOR wins over everything, TERM=dumb is
//     second, and the two TTY answers are OR-ed.
//
// Not owned: getenv itself, getpwuid, creating the cache directory, the actual
// setpriority/SetPriorityClass call, isatty and every filesystem side effect.
// The caller passes every environment value, so the module reads no globals.
//
// Platform codes, used by every export:
//   PLAT_LINUX = 0, PLAT_BSD = 1 (FreeBSD/AIX/OpenBSD/NetBSD), PLAT_APPLE = 2,
//   PLAT_WIN = 3, PLAT_EMSCRIPTEN = 4.
//
// Return convention for the directory and file exports: a non-negative value is
// the number of bytes written to `out`; -1 is "failed to find $HOME" (or
// %LOCALAPPDATA%), -2 is "not implemented on this platform", -3 is the
// separator assertion in fs_get_cache_file. A negative return writes nothing.
//
// Every output buffer is sized by the caller and must hold the result plus no
// trailing NUL; nothing here writes past the assembled length.

const PLAT_LINUX: i64 = 0;
const PLAT_BSD: i64 = 1;
const PLAT_APPLE: i64 = 2;
const PLAT_WIN: i64 = 3;
const PLAT_EMSCRIPTEN: i64 = 4;

// enum ggml_sched_priority
const P_LOW: i64 = 0;
const P_NORMAL: i64 = 1;
const P_MEDIUM: i64 = 2;
const P_HIGH: i64 = 3;
const P_REALTIME: i64 = 4;

// SetPriorityClass constants (winbase.h).
const P_WIN_BELOW_NORMAL: i64 = 16384; // 0x00004000
const P_WIN_NORMAL: i64 = 32;          // 0x00000020
const P_WIN_ABOVE_NORMAL: i64 = 32768; // 0x00008000
const P_WIN_HIGH: i64 = 128;           // 0x00000080
const P_WIN_REALTIME: i64 = 256;       // 0x00000100

fn p_sep(platform: i64) -> u8 {
    if platform == PLAT_WIN {
        return 92;
    }
    return 47;
}

fn p_put(out: u8[:], at: i64, s: const u8[:]) -> i64 {
    var p = at;
    var i: i64 = 0;

    while i < s.len {
        out[p] = s[i];
        p = p + 1;
        i = i + 1;
    }
    return p;
}

// ensure_trailing_slash: empty or a different last byte gets one appended.
fn p_ensure(out: u8[:], n: i64, sep: u8) -> i64 {
    if n == 0 || out[n - 1] != sep {
        out[n] = sep;
        return n + 1;
    }
    return n;
}

fn p_has(s: const u8[:], c: u8) -> bool {
    var i: i64 = 0;

    while i < s.len {
        if s[i] == c {
            return true;
        }
        i = i + 1;
    }
    return false;
}

fn p_eq(a: const u8[:], b: const u8[:]) -> bool {
    if a.len != b.len {
        return false;
    }
    var i: i64 = 0;

    while i < a.len {
        if a[i] != b[i] {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// ---- fs_get_cache_directory ----------------------------------------------

fn p_cache_dir(
    platform: i64,
    llama_cache: const u8[:],
    xdg_cache: const u8[:],
    home: const u8[:],
    pw_dir: const u8[:],
    localappdata: const u8[:],
    out: u8[:]) -> i64 {
    let sep = p_sep(platform);

    if llama_cache.len > 0 {
        var n = p_put(out, 0, llama_cache);
        return p_ensure(out, n, sep);
    }

    var n: i64 = 0;

    if platform == PLAT_LINUX || platform == PLAT_BSD {
        if xdg_cache.len > 0 {
            n = p_put(out, 0, xdg_cache);
        } else if home.len > 0 {
            n = p_put(out, 0, home);
            n = p_put(out, n, "/.cache/");
        } else if platform == PLAT_LINUX {
            if pw_dir.len == 0 {
                return 0 - 1;
            }
            n = p_put(out, 0, pw_dir);
            n = p_put(out, n, "/.cache/");
        } else {
            return 0 - 1;
        }
    } else if platform == PLAT_APPLE {
        if home.len == 0 {
            return 0 - 1;
        }
        n = p_put(out, 0, home);
        n = p_put(out, n, "/Library/Caches/");
    } else if platform == PLAT_WIN {
        if localappdata.len == 0 {
            return 0 - 1;
        }
        n = p_put(out, 0, localappdata);
    } else {
        return 0 - 2;
    }

    n = p_ensure(out, n, sep);
    n = p_put(out, n, "llama.cpp");
    return p_ensure(out, n, sep);
}

// ---- fs_get_config_directory ---------------------------------------------

fn p_config_dir(
    platform: i64,
    xdg_config: const u8[:],
    home: const u8[:],
    pw_dir: const u8[:],
    appdata: const u8[:],
    out: u8[:]) -> i64 {
    let sep = p_sep(platform);
    var n: i64 = 0;

    if platform == PLAT_LINUX || platform == PLAT_BSD || platform == PLAT_APPLE {
        if xdg_config.len > 0 {
            n = p_put(out, 0, xdg_config);
        } else if home.len > 0 {
            n = p_put(out, 0, home);
            n = p_put(out, n, "/.config/");
        } else if platform == PLAT_LINUX {
            if pw_dir.len == 0 {
                return 0 - 1;
            }
            n = p_put(out, 0, pw_dir);
            n = p_put(out, n, "/.config/");
        } else {
            return 0 - 1;
        }
    } else if platform == PLAT_WIN {
        if appdata.len == 0 {
            return 0 - 1;
        }
        n = p_put(out, 0, appdata);
    } else {
        return 0 - 2;
    }

    n = p_ensure(out, n, sep);
    n = p_put(out, n, "llama.cpp");
    return p_ensure(out, n, sep);
}

// ---- fs_get_cache_file ----------------------------------------------------

// The cache root followed by the file name. Creating the directory is the
// caller's job; only the name rule is owned here.
fn p_cache_file(
    platform: i64,
    llama_cache: const u8[:],
    xdg_cache: const u8[:],
    home: const u8[:],
    pw_dir: const u8[:],
    localappdata: const u8[:],
    filename: const u8[:],
    out: u8[:]) -> i64 {
    let sep = p_sep(platform);

    if p_has(filename, sep) {
        return 0 - 3;
    }
    let n = p_cache_dir(platform, llama_cache, xdg_cache, home, pw_dir, localappdata, out);
    if n < 0 {
        return n;
    }
    return p_put(out, n, filename);
}

// ---- common_set_env -------------------------------------------------------

// 0 the POSIX unsetenv branch, 1 the POSIX setenv branch, 2 the Windows
// _putenv_s branch (taken for an empty value too).
fn p_set_env(platform: i64, value_empty: i64) -> i64 {
    if platform == PLAT_WIN {
        return 2;
    }
    if value_empty != 0 {
        return 0;
    }
    return 1;
}

// ---- set_process_priority -------------------------------------------------

// 1 when the native function returns true without a system call (NORMAL).
fn p_priority_noop(prio: i64) -> i64 {
    if prio == P_NORMAL {
        return 1;
    }
    return 0;
}

// The value handed to setpriority (POSIX nice) or SetPriorityClass (Windows).
fn p_priority(platform: i64, prio: i64) -> i64 {
    if platform == PLAT_WIN {
        if prio == P_LOW {
            return P_WIN_BELOW_NORMAL;
        }
        if prio == P_NORMAL {
            return P_WIN_NORMAL;
        }
        if prio == P_MEDIUM {
            return P_WIN_ABOVE_NORMAL;
        }
        if prio == P_HIGH {
            return P_WIN_HIGH;
        }
        if prio == P_REALTIME {
            return P_WIN_REALTIME;
        }
        return P_WIN_NORMAL;
    }
    if prio == P_LOW {
        return 5;
    }
    if prio == P_NORMAL {
        return 0;
    }
    if prio == P_MEDIUM {
        return 0 - 5;
    }
    if prio == P_HIGH {
        return 0 - 10;
    }
    if prio == P_REALTIME {
        return 0 - 20;
    }
    return 0;
}

// ---- tty_can_use_colors ---------------------------------------------------

fn p_tty_colors(no_color: const u8[:], term: const u8[:], out_tty: i64, err_tty: i64) -> i64 {
    if no_color.len > 0 {
        return 0;
    }
    if p_eq(term, "dumb") {
        return 0;
    }
    if out_tty != 0 || err_tty != 0 {
        return 1;
    }
    return 0;
}

// ---- exports --------------------------------------------------------------

export fn plat_sep(platform: i64) -> i64 {
    return p_sep(platform) as i64;
}

export fn plat_cache_dir(
    platform: i64,
    llama_cache: const u8[:],
    xdg_cache: const u8[:],
    home: const u8[:],
    pw_dir: const u8[:],
    localappdata: const u8[:],
    out: u8[:]) -> i64 {
    return p_cache_dir(platform, llama_cache, xdg_cache, home, pw_dir, localappdata, out);
}

export fn plat_config_dir(
    platform: i64,
    xdg_config: const u8[:],
    home: const u8[:],
    pw_dir: const u8[:],
    appdata: const u8[:],
    out: u8[:]) -> i64 {
    return p_config_dir(platform, xdg_config, home, pw_dir, appdata, out);
}

export fn plat_cache_file(
    platform: i64,
    llama_cache: const u8[:],
    xdg_cache: const u8[:],
    home: const u8[:],
    pw_dir: const u8[:],
    localappdata: const u8[:],
    filename: const u8[:],
    out: u8[:]) -> i64 {
    return p_cache_file(platform, llama_cache, xdg_cache, home, pw_dir, localappdata, filename, out);
}

export fn plat_set_env(platform: i64, value_empty: i64) -> i64 {
    return p_set_env(platform, value_empty);
}

export fn plat_priority_noop(prio: i64) -> i64 {
    return p_priority_noop(prio);
}

export fn plat_priority(platform: i64, prio: i64) -> i64 {
    return p_priority(platform, prio);
}

export fn plat_tty_colors(no_color: const u8[:], term: const u8[:], out_tty: i64, err_tty: i64) -> i64 {
    return p_tty_colors(no_color, term, out_tty, err_tty);
}
