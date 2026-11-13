// Small KV-cache scanning helpers in Goose (APX: P5).
//
// Three functions that look trivial individually and are therefore easy to get subtly wrong:
//
//   get_has_shift   result |= cells[s].get_has_shift()      over all streams  -- OR, not overwrite
//   seq_pos_min     min position present for a sequence, or -1 when the sequence is absent
//   used_max_p1     largest used slot plus one, or 0 when nothing is used
//
// The trap in each: an OR loop that assigns instead of accumulating reports only the last stream; a min
// that starts at 0 reports 0 for every sequence; and `used_max_p1` returning the largest index rather than
// index+1 is off by one, which changes how many rows get serialized.
//
// seq_pos_min is model-specific in the native code (the position set is maintained incrementally), so the
// bridge hands over the sorted position array and this takes its first element, which is what
// `seq_pos[seq_id].begin()->first` means.
extern "fg233c_fail" fn fail(code: i64);
extern "fg233c_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn has_shift(n_streams: i64, flags: u64) -> i64 {
    ensure(n_streams >= 0, 1200);
    var r: i64 = 0;
    var s: i64 = 0;

    while s < n_streams {
        if ld_i32(flags, s) != 0 as i64 {
            r = 1 as i64;
        }
        s = s + 1 as i64;
    }
    r
}
// pos_min returns the first entry of the per-sequence position set, or -1 when it is empty.

fn pos_min(n_pos: i64, positions: u64) -> i64 {
    ensure(n_pos >= 0, 1201);

    if n_pos == 0 as i64 {
        return 0 as i64 - 1 as i64;
    }
    return ld_i32(positions, 0 as i64);
}
// used_max_p1 returns the largest used slot index plus one, or 0 when there is none.

fn used_max_p1(n_used: i64, used: u64) -> i64 {
    ensure(n_used >= 0, 1202);

    if n_used == 0 as i64 {
        return 0 as i64;
    }
    var mx: i64 = 0;
    var i: i64 = 0;

    while i < n_used {
        let v: i64 = ld_i32(used, i);
        ensure(v >= 0, 1203);

        if mx < v {
            mx = v;
        }
        i = i + 1 as i64;
    }
    mx + 1 as i64
}

fn main() {
    has_shift(0 as i64, 0 as u64);
    has_shift(2 as i64, 8 as u64);
    pos_min(0 as i64, 16 as u64);
    pos_min(3 as i64, 24 as u64);
    used_max_p1(0 as i64, 32 as u64);
    used_max_p1(4 as i64, 40 as u64);
}

