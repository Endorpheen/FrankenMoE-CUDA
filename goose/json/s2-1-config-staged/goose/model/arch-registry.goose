// P9-01 S2: the MoE architecture registry and its tensor-name matcher.
//
// Native sources:
//   runtime/core/src/moe/arch_registry.cpp — the nine-row k_recipes table and
//     find_moe_recipe / n_moe_recipes / moe_recipe_at (whole file, 84 lines);
//   runtime/core/include/bmoe/recipe.h — MoeRecipe, max_exps;
//   runtime/core/src/moe/router_hook.cpp:22-39 — match_expert, the only consumer
//     of a recipe's suffix table that decides anything without a graph. It is
//     static in the native file and reads no llama.cpp state, so the gate
//     compares it against a transcription of those lines.
//
// The recipe IS the architecture gate: streaming is refused when the gguf's
// general.architecture is not in this table, and the table's row decides which
// tensors are streamed. Nothing here is model-specific beyond the suffix names.
//
// Flat ABI:
//   ar_count()                      -> the row count (n_moe_recipes)
//   ar_find(arch)                   -> row index, or -1 (find_moe_recipe == nullptr)
//   ar_arch(i, out)                 -> arch length, or -1 when i is out of range
//   ar_suffix(i, slot, out)         -> suffix length, or -1 when the slot is unused
//                                      (the native nullptr) or out of range
//   ar_exps(i)                      -> number of non-null suffixes in the row
//   ar_match(name, row, lay)        -> slot index with lay[0] = layer number, or -1
//                                      (-1 also for a bad row or a short lay buffer)
//
// Lookup is exact std::strcmp: no case folding, no prefix match, no alias.
// That is the native behaviour and it is part of the port.

const AR_COUNT: i64 = 9;
const AR_MAX_EXPS: i64 = 3;
const AR_NONE: i64 = -1;

// k_recipes[i], in the native table order.
fn ar_nth_arch(i: i64) -> const u8[:] {
    if i == 0 {
        return "qwen3moe";
    }
    if i == 1 {
        return "qwen2moe";
    }
    if i == 2 {
        return "qwen35moe";
    }
    if i == 3 {
        return "gemma4";
    }
    if i == 4 {
        return "gpt-oss";
    }
    if i == 5 {
        return "lfm2moe";
    }
    if i == 6 {
        return "deepseek4";
    }
    if i == 7 {
        return "bailingmoe3";
    }
    if i == 8 {
        return "qwen4exp";
    }
    return "";
}

// k_recipes[i].exps_suffix[slot]; "" is the native nullptr and is never a real
// suffix, so ar_suffix reports it as -1 rather than as an empty string.
fn ar_nth_suffix(i: i64, slot: i64) -> const u8[:] {
    if i == 3 {
        // gemma4 fuses gate+up into one tensor and leaves the third slot unused.
        if slot == 0 {
            return "ffn_gate_up_exps";
        }
        if slot == 1 {
            return "ffn_down_exps";
        }
        return "";
    }
    if slot == 0 {
        return "ffn_gate_exps";
    }
    if slot == 1 {
        return "ffn_up_exps";
    }
    if slot == 2 {
        return "ffn_down_exps";
    }
    return "";
}

// Exact byte equality, standing in for std::strcmp(...) == 0.
fn ar_eq(a: const u8[:], b: const u8[:]) -> i64 {
    if a.len != b.len {
        return 0;
    }
    var k: i64 = 0;
    while k < a.len {
        if a[k] != b[k] {
            return 0;
        }
        k += 1;
    }
    return 1;
}

// Byte at `at`, or 0 past the end. The native side reads NUL-terminated C
// strings; mapping "past the end" to 0 keeps a prefix of a shorter name from
// matching the longer literal by accident.
fn ar_at(s: const u8[:], at: i64) -> i64 {
    if at < 0 || at >= s.len {
        return 0;
    }
    return s[at];
}

fn ar_put(out: u8[:], s: const u8[:]) -> i64 {
    var k: i64 = 0;
    while k < s.len {
        if k < out.len {
            out[k] = s[k];
        }
        k += 1;
    }
    return s.len;
}

// arch_registry.cpp:73-81 — find_moe_recipe: nullptr for a null pointer or an
// unknown string. The empty string is unknown too.
export fn ar_find(arch: const u8[:]) -> i64 {
    var i: i64 = 0;
    while i < AR_COUNT {
        if ar_eq(arch, ar_nth_arch(i)) != 0 {
            return i;
        }
        i += 1;
    }
    return AR_NONE;
}

export fn ar_count() -> i64 {
    return AR_COUNT;
}

export fn ar_arch(i: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= AR_COUNT {
        return AR_NONE;
    }
    return ar_put(out, ar_nth_arch(i));
}

export fn ar_suffix(i: i64, slot: i64, out: u8[:]) -> i64 {
    if i < 0 || i >= AR_COUNT || slot < 0 || slot >= AR_MAX_EXPS {
        return AR_NONE;
    }
    let s: const u8[:] = ar_nth_suffix(i, slot);
    if s.len == 0 {
        return AR_NONE; // unused slot: the native exps_suffix[slot] == nullptr
    }
    return ar_put(out, s);
}

// How many expert tensors the row names. The streamer iterates max_exps slots
// and skips the null ones (session.cpp:1019-1026, expert_stream_source.cpp:61-73);
// this is that count in one answer.
export fn ar_exps(i: i64) -> i64 {
    if i < 0 || i >= AR_COUNT {
        return AR_NONE;
    }
    var n: i64 = 0;
    var slot: i64 = 0;
    while slot < AR_MAX_EXPS {
        if ar_nth_suffix(i, slot).len != 0 {
            n += 1;
        }
        slot += 1;
    }
    return n;
}

// router_hook.cpp:22-39 — match_expert("blk.<il>.<suffix>.weight"):
//
//   int il = -1, consumed = 0;
//   if (sscanf(name, "blk.%d.%n", &il, &consumed) != 1 || il < 0) return -1;
//   const char * rest = name + consumed;
//   for (p ...) { if (!suffix) continue;
//                 if (!strncmp(rest, suffix, sl) && !strcmp(rest + sl, ".weight")) return p; }
//   return -1;
//
// %d semantics carried over: leading whitespace is skipped, one optional sign,
// then one or more digits. The exact ".weight" tail is load-bearing — a
// companion "ffn_down_exps.scale" prefix-matches its suffix and must not
// stream. The loop stops at the FIRST matching slot, and the row's unused slots
// are skipped, which is what makes the fused gemma4 layout work.
//
// Integer overflow inside %d is undefined in the native call; the engine only
// ever scans names it generated, so the port accumulates in i64 and the gate
// stays inside the range where both agree.
export fn ar_match(name: const u8[:], row: i64, lay: i64[:]) -> i64 {
    if row < 0 || row >= AR_COUNT || lay.len < 1 {
        return AR_NONE;
    }
    if ar_at(name, 0) != 98 || ar_at(name, 1) != 108 || ar_at(name, 2) != 107 || ar_at(name, 3) != 46 {
        return AR_NONE; // "blk."
    }
    var at: i64 = 4;
    while ar_at(name, at) == 32 || ar_at(name, at) == 9 || ar_at(name, at) == 10 ||
          ar_at(name, at) == 11 || ar_at(name, at) == 12 || ar_at(name, at) == 13 {
        at += 1;
    }
    var neg: i64 = 0;
    if ar_at(name, at) == 43 || ar_at(name, at) == 45 {
        if ar_at(name, at) == 45 {
            neg = 1;
        }
        at += 1;
    }
    var il: i64 = 0;
    var digits: i64 = 0;
    while ar_at(name, at) >= 48 && ar_at(name, at) <= 57 {
        il = il * 10 + (ar_at(name, at) - 48);
        at += 1;
        digits += 1;
    }
    if digits == 0 {
        return AR_NONE; // sscanf reports no conversion
    }
    if neg != 0 {
        il = 0 - il;
    }
    if il < 0 {
        return AR_NONE;
    }
    if ar_at(name, at) != 46 {
        return AR_NONE; // "%d.%n" requires the dot after the number
    }
    at += 1;
    var slot: i64 = 0;
    while slot < AR_MAX_EXPS {
        let suf: const u8[:] = ar_nth_suffix(row, slot);
        if suf.len != 0 {
            var k: i64 = 0;
            var hit: i64 = 1;
            while k < suf.len {
                if ar_at(name, at + k) != ar_at(suf, k) {
                    hit = 0;
                    break;
                }
                k += 1;
            }
            if hit != 0 {
                // rest + sl must equal ".weight" exactly.
                if ar_at(name, at + suf.len) == 46 && ar_at(name, at + suf.len + 1) == 119 &&
                   ar_at(name, at + suf.len + 2) == 101 && ar_at(name, at + suf.len + 3) == 105 &&
                   ar_at(name, at + suf.len + 4) == 103 && ar_at(name, at + suf.len + 5) == 104 &&
                   ar_at(name, at + suf.len + 6) == 116 && ar_at(name, at + suf.len + 7) == 0 {
                    lay[0] = il;
                    return slot;
                }
            }
        }
        slot += 1;
    }
    return AR_NONE;
}
