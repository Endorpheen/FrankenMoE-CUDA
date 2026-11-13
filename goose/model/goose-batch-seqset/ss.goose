// Sequence-set construction in Goose (APX: P6-07).
//
// Native (llama-batch.cpp:271-285) builds, for every token, the SET of sequence ids it belongs to, and the
// union over the whole batch:
//
//     for i in tokens:
//         cur = {}                                  // seq_set_t, a bitset<LLAMA_MAX_SEQ>
//         for s in seq_id[i]: cur.set(seq_id[i][s]); unq.set(seq_id[i][s])
//         seq_set.push_back(cur); seq_set_map[cur].push_back(i)
//
// The sets are later used as the key of a MAP (`seq_set_map[cur]`), so two tokens with the same set must
// produce the same bits, and the map's iteration order depends on that equality. Getting the bit packing
// wrong does not fail any token-level check; it silently changes how tokens are grouped when the batch is
// split, which changes order and performance.
//
// `LLAMA_MAX_SEQ` is up to 64 here, so one 64-bit word per set is enough; the module keeps the shape
// two-word so a larger value costs nothing to add later.
//
// Negative or out-of-range seq ids are the caller's error, not this function's: native would index out of
// the bitset, so the guard here traps instead of silently shifting by 64.
extern "fg240s_fail" fn fail(code: i64);
extern "fg240s_ld_i32" fn ld_i32(base: u64, idx: i64) -> i64;
extern "fg240s_st_i64" fn st_i64(base: u64, idx: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// build_sets fills out_lo[i] with the bitmask of sequence ids for token i and returns the union mask.
// seq_id_flat holds the ids back to back; off[i] is where token i's list starts, off[n_tok] its end.

fn build_sets(n_tok: i64, seq_id_flat: u64, off: u64, out_lo: u64, n_max: i64) -> i64 {
    ensure(n_tok >= 0 as i64, 2000);
    ensure(n_max > 0 as i64 && n_max <= 64 as i64, 2001);
    var unq: i64 = 0;
    var i: i64 = 0;

    while i < n_tok {
        let b: i64 = ld_i32(off, i);
        let e: i64 = ld_i32(off, i + 1 as i64);
        ensure(e >= b, 2002);
        var cur: i64 = 0;
        var k: i64 = b;

        while k < e {
            let sid: i64 = ld_i32(seq_id_flat, k);
            ensure(sid >= 0 as i64 && sid < n_max, 2003);
            cur = cur | ( (1 as i64) << sid);
            unq = unq | ( (1 as i64) << sid);
            k = k + 1 as i64;
        }
        st_i64(out_lo, i, cur);
        i = i + 1 as i64;
    }
    unq
}

fn main() {
    build_sets(0 as i64, 0 as u64, 0 as u64, 0 as u64, 64 as i64);
    build_sets(2 as i64, 8 as u64, 16 as u64, 24 as u64, 64 as i64);
    build_sets(3 as i64, 32 as u64, 40 as u64, 48 as u64, 64 as i64);
}

