// P9-01 S3: the CLI's configuration resolution layer in Goose.
//
// Native source: runtime/cli/main.cpp — env_int (42-45), the special-value flag
// branches of the parse loop (694-704 --cache-mb auto, 726-742 --dense-weights
// and its deprecated aliases, 659-668 --mtp/--ngram), the env override block
// (792-802) and the cache_auto default (804-810). Those are the decisions that
// turn flags, environment and defaults into one RunConfig; everything after
// them is the engine.
//
// Why this is separable: config.h says the library never reads the environment
// and the CLI resolves overrides before the struct is built. This module is
// that resolution, with no I/O: the caller passes whether a flag was typed,
// whether the variable exists, and the raw value. getenv and std::atoi are
// native; their DECISIONS are here.
//
// Flat ABI (field ids are the native env table, in its order):
//   cc_env_name(field, out)     -> length; "BMOE_*" for field 0..10, -1 otherwise
//   cc_flag_name(field, out)    -> length; the flag the env value defers to
//   cc_env_default(field)       -> the default literal main.cpp passes to env_int
//   cc_env_int(seen, flag_val, has_env, raw, dflt)  -> effective int
//   cc_env_bool(seen, flag_val, has_env, raw, dflt) -> effective int (0/1)
//   cc_cache_auto(moe_on, cache_auto, cache_mb_seen, env_cache_present) -> 0/1
//   cc_cache_mb_kind(raw)       -> 1 for the literal "auto", 0 otherwise
//   cc_dense_mode(raw)          -> 0 mmap, 1 warm, 2 anon, 3 ahwb, -1 invalid
//   cc_draft_pick(cur, want)    -> resulting source, or -1 for the native exit 2
//   cc_atoi(raw)                -> atoi-compatible prefix parse
//
// The precedence rule is the whole point: a flag WINS even when it repeats its
// default, so the test is "was the flag typed", not "does the value differ".
// An EMPTY environment variable is absent (env_int checks `v && *v`).

const CC_FIELDS: i64 = 11;
const CC_BAD: i64 = -1;

// The eleven fields main.cpp lets the environment override, in the native order.
fn cc_field_env(field: i64) -> const u8[:] {
    if field == 0 {
        return "BMOE_CACHE_MB";
    }
    if field == 1 {
        return "BMOE_IO_THREADS";
    }
    if field == 2 {
        return "BMOE_PROGRESS";
    }
    if field == 3 {
        return "BMOE_OVERLAP";
    }
    if field == 4 {
        return "BMOE_PREFETCH";
    }
    if field == 5 {
        return "BMOE_N_EXPERT_USED";
    }
    if field == 6 {
        return "BMOE_GPU_LAYERS";
    }
    if field == 7 {
        return "BMOE_VRAM_CACHE_MB";
    }
    if field == 8 {
        return "BMOE_EXPERT_HOT_SLOTS";
    }
    if field == 9 {
        return "BMOE_PREDICT_LOG";
    }
    if field == 10 {
        return "BMOE_PREDICT_PREFETCH";
    }
    return "";
}

fn cc_field_flag(field: i64) -> const u8[:] {
    if field == 0 {
        return "--cache-mb";
    }
    if field == 1 {
        return "--io-threads";
    }
    if field == 2 {
        return "--progress";
    }
    if field == 3 {
        return "--overlap";
    }
    if field == 4 {
        return "--prefetch";
    }
    if field == 5 {
        return "--n-expert-used";
    }
    if field == 6 {
        return "--gpu-layers";
    }
    if field == 7 {
        return "--vram-cache-mb";
    }
    if field == 8 {
        return "--expert-hot-slots";
    }
    if field == 9 {
        return "--predict-log";
    }
    if field == 10 {
        return "--predict-prefetch";
    }
    return "";
}

// The literal defaults in main.cpp:792-802. io_threads is the only non-zero one.
fn cc_field_default(field: i64) -> i64 {
    if field == 1 {
        return 4;
    }
    return 0;
}

fn cc_put(out: u8[:], s: const u8[:]) -> i64 {
    var k: i64 = 0;
    while k < s.len {
        if k < out.len {
            out[k] = s[k];
        }
        k += 1;
    }
    return s.len;
}

fn cc_at(s: const u8[:], at: i64) -> i64 {
    if at < 0 || at >= s.len {
        return 0;
    }
    return s[at];
}

fn cc_isdigit(c: i64) -> i64 {
    if c >= 48 && c <= 57 {
        return 1;
    }
    return 0;
}

// std::atoi: skip leading whitespace, take one optional sign, consume digits,
// stop at the first other byte. No digits is 0. Overflow is undefined in the
// native call; the port accumulates in i64 and the gate stays in range.
//
// The body is a private function because this toolchain cannot instantiate an
// exported const-slice signature for an internal call (same limitation the
// accepted cli-protocol slice records); the export below is the ABI wrapper.
fn cc_atoi_parse(s: const u8[:]) -> i64 {
    var at: i64 = 0;
    while cc_at(s, at) == 32 || cc_at(s, at) == 9 || cc_at(s, at) == 10 ||
          cc_at(s, at) == 11 || cc_at(s, at) == 12 || cc_at(s, at) == 13 {
        at += 1;
    }
    var neg: i64 = 0;
    if cc_at(s, at) == 43 || cc_at(s, at) == 45 {
        if cc_at(s, at) == 45 {
            neg = 1;
        }
        at += 1;
    }
    var v: i64 = 0;
    while cc_isdigit(cc_at(s, at)) != 0 {
        v = v * 10 + (cc_at(s, at) - 48);
        at += 1;
    }
    if neg != 0 {
        return 0 - v;
    }
    return v;
}

export fn cc_env_name(field: i64, out: u8[:]) -> i64 {
    if field < 0 || field >= CC_FIELDS {
        return CC_BAD;
    }
    return cc_put(out, cc_field_env(field));
}

export fn cc_flag_name(field: i64, out: u8[:]) -> i64 {
    if field < 0 || field >= CC_FIELDS {
        return CC_BAD;
    }
    return cc_put(out, cc_field_flag(field));
}

export fn cc_env_default(field: i64) -> i64 {
    if field < 0 || field >= CC_FIELDS {
        return CC_BAD;
    }
    return cc_field_default(field);
}

// main.cpp:42-45 — env_int: `(v && *v) ? atoi(v) : dflt`, applied only when the
// flag was not typed. has_env is the getenv() result; an empty raw value is
// treated as absent, exactly like `*v == 0`.
fn cc_env_int_impl(seen: i64, flag_val: i64, has_env: i64, raw: const u8[:], dflt: i64) -> i64 {
    if seen != 0 {
        return flag_val;
    }
    if has_env == 0 {
        return dflt;
    }
    if raw.len == 0 {
        return dflt;
    }
    return cc_atoi_parse(raw);
}

export fn cc_atoi(s: const u8[:]) -> i64 {
    return cc_atoi_parse(s);
}

export fn cc_env_int(seen: i64, flag_val: i64, has_env: i64, raw: const u8[:], dflt: i64) -> i64 {
    return cc_env_int_impl(seen, flag_val, has_env, raw, dflt);
}

// The same for the three boolean fields (progress, overlap, predict-log,
// predict-prefetch): the native call is `env_int(...) != 0`.
export fn cc_env_bool(seen: i64, flag_val: i64, has_env: i64, raw: const u8[:], dflt: i64) -> i64 {
    let v: i64 = cc_env_int_impl(seen, flag_val, has_env, raw, dflt);
    if v != 0 {
        return 1;
    }
    return 0;
}

// main.cpp:804-810 — with streaming on and no cache budget explicitly chosen,
// the CLI turns auto-sizing on. An explicit --cache-mb, a typed --cache-mb 0,
// a --cache-mb auto and any BMOE_CACHE_MB (even an empty one) all suppress it.
export fn cc_cache_auto(moe_on: i64, cache_auto: i64, cache_mb_seen: i64, env_cache_present: i64) -> i64 {
    if cache_auto != 0 {
        return 1;
    }
    if moe_on == 0 {
        return 0;
    }
    if cache_mb_seen != 0 {
        return 0;
    }
    if env_cache_present != 0 {
        return 0;
    }
    return 1;
}

// main.cpp:695-699 — the value is compared to the literal "auto" BEFORE atoi.
export fn cc_cache_mb_kind(raw: const u8[:]) -> i64 {
    if raw.len == 4 && raw[0] == 97 && raw[1] == 117 && raw[2] == 116 && raw[3] == 111 {
        return 1;
    }
    return 0;
}

// main.cpp:726-741 — mmap | warm | anon | ahwb. The deprecated aliases
// --dense-odirect (anon) and --no-warm-dense (mmap) are the caller's mapping;
// this is the vocabulary the value must be in.
export fn cc_dense_mode(raw: const u8[:]) -> i64 {
    if raw.len == 4 && raw[0] == 109 && raw[1] == 109 && raw[2] == 97 && raw[3] == 112 {
        return 0;
    }
    if raw.len == 4 && raw[0] == 119 && raw[1] == 97 && raw[2] == 114 && raw[3] == 109 {
        return 1;
    }
    if raw.len == 4 && raw[0] == 97 && raw[1] == 110 && raw[2] == 111 && raw[3] == 110 {
        return 2;
    }
    if raw.len == 4 && raw[0] == 97 && raw[1] == 104 && raw[2] == 119 && raw[3] == 98 {
        return 3;
    }
    return CC_BAD;
}

// main.cpp:659-668 — --mtp and --ngram are two sources for one verify loop.
// Asking for the other one after a source is already set is a contradiction:
// the native prints a message and exits 2. The port reports the conflict as -1
// so the caller keeps the exit code and the text.
export fn cc_draft_pick(cur: i64, want: i64) -> i64 {
    if cur != 0 && cur != want {
        return CC_BAD;
    }
    return want;
}
