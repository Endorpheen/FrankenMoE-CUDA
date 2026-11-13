// Apex hybrid indexed memory: which pools an ubatch must re-pool
// (llama_memory_hybrid_idx_context::kpool_build_state).
//
// A pool is marked once per representative row: sequences that share cells share their pools, and
// the single pooled row is read by all of them, so scatter rows stay unique. Marks are generation
// stamps, so the state is reused without clearing the arrays each step.
//
// Two passes mark pools: every pool ending after a stale position (a sequence edit regroups from
// there on), then every pool the ubatch's own tokens touch. In order mode a token's cell gives its
// rank and the rank its pool, because positions repeat for images; otherwise the pool is found by
// position and the token must still lie inside it.
//
// The graph size is padded to a bound that does not move with the batch, capped by what the cache
// can hold.
//
// Packet, int64 slots:
//   k=0  0 kpool, 1 kv_size, 2 n_stream, 3 n_seq_max, 4 n_pool_real, 5 by_order, 6 n_tokens,
//        7 n_seqs_unq, 8 i_cur, 9 pos, 10 ub_seq, 11 width, 12 stale_batch, 13 tok_cell
//   k=1  0 pools_off, 1 pools_cnt, 2 cells_off, 3 cells_pos, 4 cells_cell, 5 strm, 6 is_new,
//        7 rep_gen, 8 gen_ptr, 9 n_new_ptr, 10 n_new_g_ptr, 11 n_seq
//   k=3  0 done, 1 ret
extern "fg312k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg312k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg312k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg312k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg312k_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn at(arr: u64, i: i64) -> i64 {
    return ld(arr, i * (8 as i64));
}

fn setat(arr: u64, i: i64, v: i64) {
    st(arr, i * (8 as i64), v);
}

fn pad64(x: i64) -> i64 {
    let q: i64 = (x + (63 as i64)) / (64 as i64);
    return q * (64 as i64);
}

fn kpool_pad(n_pool: i64) -> i64 {
    let p: i64 = pad64(n_pool + (1 as i64));

    if p < (64 as i64) {
        return 64 as i64;
    }
    return p;
}

fn rank(off: i64, cnt: i64, pos: i64, cell: i64, cpos: u64, ccell: u64) -> i64 {
    var lo: i64 = 0;
    var hi: i64 = cnt;

    while lo < hi {
        let mid: i64 = (lo + hi) / (2 as i64);
        let mp: i64 = at(cpos, off + mid);
        let mc: i64 = at(ccell, off + mid);
        var less: bool = mp < pos;

        if mp == pos {
            if mc < cell {
                less = true;
            }
        }

        if less {
            lo = mid + (1 as i64);
        } else {
            hi = mid;
        }
    }

    if lo < cnt {
        if at(cpos, off + lo) == pos {
            if at(ccell, off + lo) == cell {
                return lo;
            }
        }
    }
    return (0 as i64) - 1 as i64;
}

fn run(packet: i64) -> bool {
    let kpool: i64 = get(packet, 0, 0) as i64;
    let kv_size: i64 = get(packet, 0, 1) as i64;
    let n_stream: i64 = get(packet, 0, 2) as i64;
    let n_seq_max: i64 = get(packet, 0, 3) as i64;
    let n_pool_real: i64 = get(packet, 0, 4) as i64;
    let by_order: bool = get(packet, 0, 5) != (0 as u64);
    let n_tokens: i64 = get(packet, 0, 6) as i64;
    let n_seqs_unq: i64 = get(packet, 0, 7) as i64;
    let i_cur: i64 = get(packet, 0, 8) as i64;
    let posarr: u64 = get(packet, 0, 9);
    let ub_seq: u64 = get(packet, 0, 10);
    let width: i64 = get(packet, 0, 11) as i64;
    let stale_batch: u64 = get(packet, 0, 12);
    let tok_cell: u64 = get(packet, 0, 13);
    let tok_nsid: u64 = get(packet, 0, 14);
    let pools_off: u64 = get(packet, 1, 0);
    let pools_cnt: u64 = get(packet, 1, 1);
    let cells_off: u64 = get(packet, 1, 2);
    let cpos: u64 = get(packet, 1, 3);
    let ccell: u64 = get(packet, 1, 4);
    let strm: u64 = get(packet, 1, 5);
    let is_new: u64 = get(packet, 1, 6);
    let rep_gen: u64 = get(packet, 1, 7);
    let genp: u64 = get(packet, 1, 8);
    let n_new_p: u64 = get(packet, 1, 9);
    let n_new_g_p: u64 = get(packet, 1, 10);
    let n_seq: i64 = get(packet, 1, 11) as i64;
    let cells_cnt: u64 = get(packet, 1, 12);
    let pools_all: u64 = get(packet, 1, 13);
    ensure(kpool >= (1 as i64) && kv_size > (0 as i64) && n_stream > (0 as i64), 60);
    ensure(width >= (1 as i64) && n_seq >= (0 as i64), 61);
    var gen: i64 = at(genp, 0) + (1 as i64);

    if gen == (0 as i64) {
        var z: i64 = 0;

        while z < n_pool_real {
            setat(is_new, z, 0 as i64);
            z = z + 1 as i64;
        }
        var z2: i64 = 0;
        let rn: i64 = kv_size * n_stream;

        while z2 < rn {
            setat(rep_gen, z2, 0 as i64);
            z2 = z2 + 1 as i64;
        }
        gen = 1 as i64;
    }
    setat(genp, 0 as i64, gen);
    // pool_start[s] lets is_new be indexed globally
    var n_new: i64 = 0;
    var ip: i64 = 0;
    var s: i64 = 0;

    while s < n_seq {
        let cnt: i64 = at(pools_cnt, s);
        let base: i64 = at(cells_off, s);
        let st_s: i64 = at(strm, s);
        // POS_CLEAN is the maximum position, not a negative sentinel: an edit at or after it never happens
        var stale_from: i64 = 2147483647 as i64;

        if i_cur == (0 as i64) {
            stale_from = at(stale_batch, s);
        }

        if stale_from != (2147483647 as i64) {
            // first pool whose last cell ends at or after stale_from
            var lo: i64 = 0;
            var hi: i64 = cnt;

            while lo < hi {
                let mid: i64 = (lo + hi) / (2 as i64);
                let j: i64 = at(pools_all, ip + mid);
                let lastp: i64 = at(cpos, base + j + kpool - (1 as i64));

                if lastp < stale_from {
                    lo = mid + (1 as i64);
                } else {
                    hi = mid;
                }
            }
            var k: i64 = lo;

            while k < cnt {
                let j: i64 = at(pools_all, ip + k);
                let rep: i64 = st_s * kv_size + at(ccell, base + j + kpool - (1 as i64));

                if at(rep_gen, rep) != gen {
                    setat(rep_gen, rep, gen);
                    setat(is_new, ip + k, gen);
                    n_new = n_new + 1 as i64;
                }
                k = k + 1 as i64;
            }
        }
        ip = ip + cnt;
        s = s + 1 as i64;
    }
    // second pass: the ubatch's own tokens
    var i: i64 = 0;

    while i < n_tokens {
        let pp: i64 = at(posarr, i);
        let nsid: i64 = at(tok_nsid, i);
        var t: i64 = 0;

        while t < nsid {
            let sid: i64 = at(ub_seq, i * width + t);
            // global pool base of this sequence
            var ip_s: i64 = 0;
            var z: i64 = 0;

            while z < sid {
                ip_s = ip_s + at(pools_cnt, z);
                z = z + 1 as i64;
            }
            let cnt: i64 = at(pools_cnt, sid);
            let base: i64 = at(cells_off, sid);
            let st_s: i64 = at(strm, sid);
            var found: i64 = (0 as i64) - 1 as i64;

            if by_order {
                let cell: i64 = at(tok_cell, i);
                let r: i64 = rank(base, at(cells_cnt, sid), pp, cell, cpos, ccell);

                if r >= (0 as i64) {
                    let kk: i64 = r / kpool;

                    if kk < cnt {
                        found = kk;
                    }
                }
            } else {
                var lo: i64 = 0;
                var hi: i64 = cnt;

                while lo < hi {
                    let mid: i64 = (lo + hi) / (2 as i64);
                    let j: i64 = at(pools_all, ip_s + mid);
                    let fp: i64 = at(cpos, base + j);

                    if fp <= pp {
                        lo = mid + (1 as i64);
                    } else {
                        hi = mid;
                    }
                }

                if lo > (0 as i64) {
                    let kk: i64 = lo - (1 as i64);
                    let j: i64 = at(pools_all, ip_s + kk);
                    let lastp: i64 = at(cpos, base + j + kpool - (1 as i64));

                    if pp <= lastp {
                        found = kk;
                    }
                }
            }

            if found >= (0 as i64) {
                let j: i64 = at(pools_all, ip_s + found);
                let rep: i64 = st_s * kv_size + at(ccell, base + j + kpool - (1 as i64));

                if at(rep_gen, rep) != gen {
                    setat(rep_gen, rep, gen);
                    setat(is_new, ip_s + found, gen);
                    n_new = n_new + 1 as i64;
                }
            }
            t = t + 1 as i64;
        }
        i = i + 1 as i64;
    }
    // graph size bound
    let bound: i64 = n_tokens / kpool + n_seqs_unq;
    let n_pool_max: i64 = (kv_size / kpool) * n_seq_max;
    var cap: i64 = kpool_pad(n_pool_real) - (1 as i64);

    if cap > n_pool_max {
        cap = n_pool_max;
    }
    // max(n_new, 1, min(bound, cap))
    var m: i64 = bound;

    if cap < m {
        m = cap;
    }
    var n_newg: i64 = n_new;

    if n_newg < (1 as i64) {
        n_newg = 1 as i64;
    }

    if m > n_newg {
        n_newg = m;
    }
    setat(n_new_p, 0 as i64, n_new);
    setat(n_new_g_p, 0 as i64, n_newg);
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, n_new as u64);
    return true;
}

fn main() {
    run(0);
}

