// Apex hybrid indexed memory: the pool staleness rules in Goose
// (llama_memory_hybrid_idx).
//
// A pooled key is only valid while the grouping that produced it holds. Grouping is sequence
// relative, so an edit at p0 leaves every pool that ends before p0 alone: the staleness mark is the
// *minimum* edited position seen, and an edit at or below the sequence's first position regroups the
// whole sequence, which is why that case records 0 instead of p0.
//
// Every sequence operation then decides what to mark:
//   seq_rm   : recurrent cache first (it may refuse), indexer rm, mark stale at stale_pos(p0),
//              attention rm last
//   seq_add  : a negative shift moves cells below p0, so the mark uses p0+shift
//   seq_cp   : whole sequences only; the destination rebuilds from 0
//   seq_keep : every other sequence loses its cells, so all sequences rebuild
//   seq_div  : the whole sequence regroups
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 seq_id, 3 p0, 4 p1, 5 seq_min, 6 n_seq, 7 stale, 8 shift, 9 has_idx
//   k=3  0 done, 1 ret
extern "fg309k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg309k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg309k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg309k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg309k_fail" fn fail(code: i64);

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

fn stale_set(arr: u64, n_seq: i64, seq_id: i64, p0in: i64) {
    var p0: i64 = p0in;

    if p0 < (0 as i64) {
        p0 = 0 as i64;
    }

    if seq_id < (0 as i64) {
        var i: i64 = 0;

        while i < n_seq {
            if at(arr, i) > p0 {
                setat(arr, i, p0);
            }
            i = i + 1 as i64;
        }
        return;
    }
    ensure(seq_id < n_seq, 60);

    if at(arr, seq_id) > p0 {
        setat(arr, seq_id, p0);
    }
}

fn stale_pos(seq_id: i64, p0: i64, seq_min: i64) -> i64 {
    if seq_id < (0 as i64) {
        return 0 as i64;
    }

    if p0 <= seq_min {
        return 0 as i64;
    }
    return p0;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let seq_id: i64 = get(packet, 0, 2) as i64;
    let p0: i64 = get(packet, 0, 3) as i64;
    let p1: i64 = get(packet, 0, 4) as i64;
    let seq_min: i64 = get(packet, 0, 5) as i64;
    let n_seq: i64 = get(packet, 0, 6) as i64;
    let stale: u64 = get(packet, 0, 7);
    let shift: i64 = get(packet, 0, 8) as i64;
    let has_idx: bool = get(packet, 0, 9) != (0 as u64);
    ensure(n_seq > (0 as i64), 61);
    var ret: i64 = 0;

    if op == (0 as i64) {
        stale_set(stale, n_seq, seq_id, p0);
        var i: i64 = 0;

        while i < n_seq {
            setat(out, i, at(stale, i));
            i = i + 1 as i64;
        }
        ret = 1 as i64;
    } else {
        if op == (1 as i64) {
            ret = stale_pos(seq_id, p0, seq_min);
        } else {
            if op == (2 as i64) {
                // seq_rm: recurrent first; a refusal stops before anything else happens
                let recr_ok: i64 = get(packet, 0, 8) as i64;

                if recr_ok == (0 as i64) {
                    ret = 0 as i64;
                } else {
                    if has_idx {
                        let st: i64 = stale_pos(seq_id, p0, seq_min);
                        stale_set(stale, n_seq, seq_id, st);
                    }
                    ret = 1 as i64;
                }
            } else {
                if op == (3 as i64) {
                    // seq_add: mark with the moved start when the shift is negative

                    if has_idx {
                        var base: i64 = p0;

                        if shift < (0 as i64) {
                            base = p0 + shift;
                        }
                        let st: i64 = stale_pos(seq_id, base, seq_min);
                        stale_set(stale, n_seq, seq_id, st);
                    }
                    ret = 1 as i64;
                } else {
                    if op == (4 as i64) {
                        // seq_cp: whole sequences, destination rebuilds from 0; other ops mark their own rule

                        if has_idx {
                            if seq_id == (0 as i64) {
                                stale_set(stale, n_seq, p0, 0 as i64);
                            }
                            // cp: destination

                            if seq_id == (1 as i64) {
                                stale_set(stale, n_seq, (0 as i64) - 1 as i64, 0 as i64);
                            }
                            // keep: all

                            if seq_id == (2 as i64) {
                                stale_set(stale, n_seq, p0, 0 as i64);
                            }
                            // div: the sequence
                        }
                    } else {
                        ensure(false, 62);
                    }
                }
            }
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

