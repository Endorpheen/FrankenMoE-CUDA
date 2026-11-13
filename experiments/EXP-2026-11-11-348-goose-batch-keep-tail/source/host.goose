// Apex batch split tail policy in Goose (llama_batch_allocr::split_equal, n_keep_tail block).
//
// The expansion loop already chose how many tokens each sequence set contributes. This block then
// decides which sequence sets survive, so the trailing tokens of a sequence are never split across
// ubatches — that matters for the recurrent rollback snapshots.
//
// Rules, in native order:
//   keep the longest prefix of sets where every set is either finished (remaining == 0) or has at least
//   n_keep_tail tokens left;
//   when the first set alone violates it (n_keep == 0), resolve that set directly: extend it to
//   completion if everything fits, otherwise truncate it from the back until exactly n_keep_tail tokens
//   remain — truncation gives tokens **back** to the pool, so `used` and the counter both decrease;
//   every deferred set returns all of its tokens to the pool.
//
// Packet:
//   k=0  0 n_seqs, 1 sizes, 2 cur_idx, 3 idxs, 4 idx_off, 5 used, 6 n_used, 7 n_ubatch, 8 n_keep_tail,
//        9 used_len
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (n_seqs kept, -1 when the block is skipped)
//
// Records, three slots: 0 extended (a = seq, b = index), 1 truncated (a = seq, b = index),
// 2 deferred release (a = seq, b = index), 3 final (a = seq, b = index), 4 kept count (a = n_seqs).
extern "fg348k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg348k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg348k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg348k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg348k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    stw(arr, i * (8 as i64), v);
}

fn setbit(arr: u64, i: i64) {
    stw(arr, i * (8 as i64), 1 as i64);
}

fn clrbit(arr: u64, i: i64) {
    stw(arr, i * (8 as i64), 0 as i64);
}

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), kind);
    setat(out, n * (3 as i64) + 1 as i64, a);
    setat(out, n * (3 as i64) + 2 as i64, b);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    var n_seqs: i64 = get(packet, 0, 0) as i64;
    let sizes: u64 = get(packet, 0, 1);
    let cur: u64 = get(packet, 0, 2);
    let idxs: u64 = get(packet, 0, 3);
    let off: u64 = get(packet, 0, 4);
    let used: u64 = get(packet, 0, 5);
    var n_used: i64 = get(packet, 0, 6) as i64;
    let n_ubatch: i64 = get(packet, 0, 7) as i64;
    let n_keep_tail: i64 = get(packet, 0, 8) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    ensure(n_seqs >= (0 as i64) && n_keep_tail >= (0 as i64), 60);

    if n_keep_tail <= (0 as i64) {
        put(packet, 2, 0, 1 as u64);
        put(packet, 2, 1, ( (0 as i64) - 1 as i64) as u64);
        return true;
    }
    ensure(n_ubatch > n_keep_tail, 61);
    // remaining tokens of a set
    // keep the longest prefix that satisfies the constraint
    var n_keep: i64 = 0 as i64;
    var stop: i64 = 0 as i64;

    while n_keep < n_seqs && stop == (0 as i64) {
        let rem: i64 = (at(sizes, n_keep) - at(cur, n_keep));

        if rem != (0 as i64) && rem < n_keep_tail {
            stop = 1 as i64;
        } else {
            n_keep = n_keep + (1 as i64);
        }
    }

    if n_keep == (0 as i64) {
        // the first set alone violates it: resolve it directly
        let rem0: i64 = at(sizes, 0 as i64) - at(cur, 0 as i64);
        var have: i64 = at(cur, 0 as i64);

        if have + rem0 <= n_ubatch {
            // extend the set to completion
            var rr: i64 = rem0;

            while rr > (0 as i64) {
                let idx: i64 = at(idxs, at(off, 0 as i64) + at(cur, 0 as i64));
                emit(out, cntp, 0 as i64, 0 as i64, idx);
                setbit(used, idx);
                n_used = n_used + (1 as i64);
                setat(cur, 0 as i64, at(cur, 0 as i64) + (1 as i64));
                rr = rr - (1 as i64);
            }
        } else {
            // truncate until exactly n_keep_tail tokens remain
            var rr2: i64 = at(sizes, 0 as i64) - at(cur, 0 as i64);

            while rr2 < n_keep_tail {
                let last: i64 = at(idxs, at(off, 0 as i64) + have - (1 as i64));
                emit(out, cntp, 1 as i64, 0 as i64, last);
                clrbit(used, last);
                n_used = n_used - (1 as i64);
                have = have - (1 as i64);
                setat(cur, 0 as i64, at(cur, 0 as i64) - (1 as i64));
                rr2 = rr2 + (1 as i64);
            }
        }
        n_keep = 1 as i64;
    }
    // deferred sets return the tokens they had already taken in this ubatch -- not their whole lists
    var s: i64 = n_keep;

    while s < n_seqs {
        var k: i64 = 0;
        let cnt: i64 = at(cur, s);

        while k < cnt {
            let idx: i64 = at(idxs, at(off, s) + k);
            emit(out, cntp, 2 as i64, s, idx);
            clrbit(used, idx);
            n_used = n_used - (1 as i64);
            k = k + 1 as i64;
        }
        s = s + 1 as i64;
    }
    // concat the surviving per-set lists
    // the concatenation is over the taken lists, exactly as the deferred release was
    var t: i64 = 0;

    while t < n_keep {
        var k2: i64 = 0;
        let cnt2: i64 = at(cur, t);

        while k2 < cnt2 {
            emit(out, cntp, 3 as i64, t, at(idxs, at(off, t) + k2));
            k2 = k2 + 1 as i64;
        }
        t = t + 1 as i64;
    }
    emit(out, cntp, 4 as i64, n_keep, 0 as i64);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, n_keep as u64);
    return true;
}

fn main() {
    run(0);
}

