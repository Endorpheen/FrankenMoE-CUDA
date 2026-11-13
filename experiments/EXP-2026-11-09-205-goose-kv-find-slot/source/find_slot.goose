// KV slot search in Goose (APX: P5-01, the "find" clause).
//
// From llama-kv-cache.cpp:921-1117 llama_kv_cache::find_slot. This is the ring-buffer walk that
// picks the cells an ubatch will occupy. EXP199 already ported the two decisions it calls
// (head_start, can_use_cell) but not the walk itself: the wraparound, the contiguous versus
// per-token testing, and the give-up condition. A wrong walk picks a cell that is still in use,
// which shows up as corrupted attention rather than as a clear error.
//
// The host supplies one predicate result per cell via a byte array, computed from the real cell
// state (empty / single-sequence / SWA-masked) before the call. Goose owns the search order.
extern "fg205f_fail" fn fail(code: i64);
extern "fg205f_ld_i64" fn ld_i64(base: u64, byte: i64) -> i64;
extern "fg205f_st_i64" fn st_i64(base: u64, byte: i64, v: i64);
extern "fg205f_ld_u8" fn ld_u8(base: u64, byte: i64) -> i64;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// head_start: llama-kv-cache.cpp:1026-1029. If enough unused cells sit before the current head,
// start from zero hoping to fill the gap instead of wrapping.

fn head_start(head_cur: i64, used: i64, n_tokens: i64) -> i64 {
    ensure(head_cur >= 0 && used >= 0 && n_tokens >= 0, 10);

    if head_cur > used + 2 as i64 * n_tokens {
        return 0 as i64;
    }
    return head_cur;
}
// find_slots: llma-kv-cache.cpp:1039-1107 for one stream.
//   can_use[j]  1 when cell j may be reused this step (host-computed from cell state)
//   out[n]      receives the chosen cell indices in order
//   cont        1 = the whole ubatch must land contiguously, 0 = per-token search
// Returns the number of cells found, or -1 when no slot exists. The caller treats -1 as an
// empty slot_info, which is what native returns.

fn find_slots(can_use: u64, size: i64, head0: i64, n_tokens: i64, cont: i64, out: u64) -> i64 {
    ensure(size >= 0 && head0 >= 0 && n_tokens >= 0, 20);
    ensure(cont == 0 || cont == 1, 21);

    if n_tokens > size {
        return (0 as i64) - 1 as i64;
    }
    var head: i64 = head0;
    var n_tested: i64 = 0;
    var found: i64 = 0;
    // native: n_test = cont ? n_tokens : 1
    var step: i64 = 1;

    if cont == 1 as i64 {
        step = n_tokens;
    }

    while true {
        // wrap when this pass cannot fit

        if head + step > size {
            n_tested += size - head;
            head = 0;
        } else {
            var i: i64 = 0;

            while i < step {
                let idx: i64 = head;
                head += 1;
                n_tested += 1;

                if ld_u8(can_use, idx) == 1 as i64 {
                    st_i64(out, found * 8 as i64, idx);
                    found += 1;
                } else {
                    if cont == 1 as i64 {
                        i = step;
                        // contiguous: abort this pass
                    }
                }
                i += 1;
            }

            if found == n_tokens {
                return found;
            }

            if cont == 1 as i64 {
                found = 0;
                // contiguous: the run broke, start over
            }

            if n_tested >= size {
                return (0 as i64) - 1 as i64;
            }
        }

        if n_tested >= size {
            return (0 as i64) - 1 as i64;
        }
    }
    return found;
}

fn main() {
    head_start(0 as i64, 0 as i64, 0 as i64);
    find_slots(0 as u64, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as u64);
    find_slots(8 as u64, 4 as i64, 1 as i64, 2 as i64, 1 as i64, 16 as u64);
}

