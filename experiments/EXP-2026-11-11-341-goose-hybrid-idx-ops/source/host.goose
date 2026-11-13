// Apex hybrid indexed operations in Goose (llama_memory_hybrid_idx delegates and state halves).
//
// Staleness first, because every operation leans on it: a position is clamped at zero, a negative
// sequence id applies to all sequences, and the stored value keeps the **minimum** edited position.
// `stale_pos` returns zero — meaning "regroup the whole sequence" — when the edit is at or below the
// sequence's first position, so an edit that moves the first position invalidates everything.
//
// seq_rm: recurrent first because it can refuse; a refusal stops the operation before the indexer or
// the attention cache see anything. On success the indexer is told the edit position first, then
// removed, then told the position again — the first call reads the sequence's current minimum, the
// second records it, so the order of those two matters.
//
// seq_cp asserts a whole-sequence copy (p0 <= 0, p1 < 0) and marks the destination from zero: the
// destination takes the source's pools, rows included.
//
// seq_keep marks **every** sequence from zero, because every other sequence loses its cells.
//
// seq_add marks at `p0 + shift` when the shift is negative — the moved cells end below p0 — and at p0
// otherwise.
//
// seq_div marks the sequence from zero.
//
// state_write appends the indexer section **last**, so it is a pure suffix an old reader can stop
// before. state_read mirrors that order and reuses the attention cache's slot layout instead of
// searching: two searches agree only while both caches see the same occupancy, which a restore cannot
// promise. Any throw drops the sequence from all three caches — a half-restored trio is the one state
// the indexer cannot repair by itself.
//
// state_drop deliberately does not go through seq_rm: recurrent may refuse, and then only the other two
// are cleared, which is still a state the three agree on.
//
// Packet:
//   k=0  0 op, 1 out, 2 seq_id, 3 p0, 4 p1, 5 shift, 6 d, 7 recr_ok, 8 has_idx, 9 seq_min,
//        10 stale_cur, 11 sinfos_empty
//   k=1  0 cnt
//   k=2  0 done, 1 ret
//
// Records, three slots: 0 delegate ran (a = 0 recr / 1 idx / 2 attn), 1 stale written (a = seq or -1,
// b = value), 2 layout copied (a = count), 3 dropped from (a = 0 recr / 1 idx / 2 attn), 4 all cleared,
// 5 returned (a = value).
extern "fg341k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg341k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg341k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg341k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg341k_fail" fn fail(code: i64);

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

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), kind);
    setat(out, n * (3 as i64) + 1 as i64, a);
    setat(out, n * (3 as i64) + 2 as i64, b);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn stale_pos(seq_id: i64, p0: i64, seq_min: i64) -> i64 {
    // an edit at or below the first position moves the minimum, which regroups the whole sequence

    if seq_id < (0 as i64) || p0 <= seq_min {
        return 0 as i64;
    }
    return p0;
}

fn stale_set(out: u64, cntp: u64, seq_id: i64, p0: i64, stale: u64, n_seq: i64) {
    var v: i64 = p0;

    if v < (0 as i64) {
        v = 0 as i64;
    }

    if seq_id < (0 as i64) {
        // every sequence regroups at the same position
        var q: i64 = 0;

        while q < n_seq {
            if v < at(stale, q) {
                setat(stale, q, v);
            }
            q = q + 1 as i64;
        }
        emit(out, cntp, 1 as i64, (0 as i64) - 1 as i64, v);
    } else {
        if v < at(stale, seq_id) {
            setat(stale, seq_id, v);
        }
        emit(out, cntp, 1 as i64, seq_id, at(stale, seq_id));
    }
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let seq_id: i64 = get(packet, 0, 2) as i64;
    let p0: i64 = get(packet, 0, 3) as i64;
    let p1: i64 = get(packet, 0, 4) as i64;
    let shift: i64 = get(packet, 0, 5) as i64;
    let d: i64 = get(packet, 0, 6) as i64;
    let recr_ok: i64 = get(packet, 0, 7) as i64;
    let has_idx: i64 = get(packet, 0, 8) as i64;
    let seq_min: i64 = get(packet, 0, 9) as i64;
    let stale: u64 = get(packet, 0, 10);
    let sinfos_empty: i64 = get(packet, 0, 11) as i64;
    let n_seq: i64 = get(packet, 0, 12) as i64;
    let cntp: u64 = get(packet, 1, 0);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // seq_rm
        emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

        if recr_ok == (0 as i64) {
            ret = 0 as i64;
        } else {
            if has_idx != (0 as i64) {
                let st: i64 = stale_pos(seq_id, p0, seq_min);
                emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
                stale_set(out, cntp, seq_id, st, stale, n_seq);
            }
            emit(out, cntp, 0 as i64, 2 as i64, 0 as i64);
            ret = 1 as i64;
        }
    } else {
        if op == (1 as i64) {
            // seq_cp: whole sequences only
            ensure(p0 <= (0 as i64) && p1 < (0 as i64), 60);
            emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
            emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

            if has_idx != (0 as i64) {
                emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
                stale_set(out, cntp, seq_id, 0 as i64, stale, n_seq);
            }
            ret = 0 as i64;
        } else {
            if op == (2 as i64) {
                // seq_keep: every sequence regroups
                emit(out, cntp, 0 as i64, 2 as i64, 0 as i64);
                emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

                if has_idx != (0 as i64) {
                    emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
                    stale_set(out, cntp, (0 as i64) - 1 as i64, 0 as i64, stale, n_seq);
                }
                ret = 0 as i64;
            } else {
                if op == (3 as i64) {
                    // seq_add
                    emit(out, cntp, 0 as i64, 2 as i64, 0 as i64);
                    emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

                    if has_idx != (0 as i64) {
                        var src: i64 = p0;

                        if shift < (0 as i64) {
                            src = p0 + shift;
                        }
                        let st2: i64 = stale_pos(seq_id, src, seq_min);
                        emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
                        stale_set(out, cntp, seq_id, st2, stale, n_seq);
                    }
                    ret = 0 as i64;
                } else {
                    if op == (4 as i64) {
                        // seq_div
                        emit(out, cntp, 0 as i64, 2 as i64, 0 as i64);
                        emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

                        if has_idx != (0 as i64) {
                            emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
                            stale_set(out, cntp, seq_id, 0 as i64, stale, n_seq);
                        }
                        ret = 0 as i64;
                    } else {
                        if op == (5 as i64) {
                            // state_write: the indexer section is a pure suffix
                            emit(out, cntp, 0 as i64, 2 as i64, 0 as i64);
                            emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

                            if has_idx != (0 as i64) {
                                emit(out, cntp, 0 as i64, 1 as i64, 0 as i64);
                            }
                            ret = 0 as i64;
                        } else {
                            if op == (6 as i64) {
                                // state_read: attention layout reused by the indexer, then a stale mark if cells were rewritten
                                emit(out, cntp, 0 as i64, 2 as i64, 0 as i64);
                                ret = 0 as i64;

                                if recr_ok == (0 as i64) {
                                    // recurrent threw: drop from all three and rethrow
                                    emit(out, cntp, 3 as i64, 2 as i64, 0 as i64);
                                    emit(out, cntp, 3 as i64, 0 as i64, 0 as i64);

                                    if has_idx != (0 as i64) {
                                        emit(out, cntp, 3 as i64, 1 as i64, 0 as i64);
                                        stale_set(out, cntp, seq_id, 0 as i64, stale, n_seq);
                                    }
                                    ret = (0 as i64) - 1 as i64;
                                } else {
                                    emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

                                    if has_idx != (0 as i64) {
                                        emit(out, cntp, 3 as i64, 1 as i64, 0 as i64);

                                        if sinfos_empty == (0 as i64) {
                                            emit(out, cntp, 2 as i64, 1 as i64, 0 as i64);
                                        }

                                        if sinfos_empty == (0 as i64) {
                                            stale_set(out, cntp, seq_id, 0 as i64, stale, n_seq);
                                        }
                                    }
                                }
                            } else {
                                if op == (7 as i64) {
                                    // state_drop: does not go through seq_rm

                                    if seq_id < (0 as i64) {
                                        emit(out, cntp, 4 as i64, 0 as i64, 0 as i64);
                                        ret = 0 as i64;
                                    } else {
                                        emit(out, cntp, 3 as i64, 2 as i64, 0 as i64);
                                        emit(out, cntp, 3 as i64, 0 as i64, 0 as i64);

                                        if has_idx != (0 as i64) {
                                            emit(out, cntp, 3 as i64, 1 as i64, 0 as i64);
                                            stale_set(out, cntp, seq_id, 0 as i64, stale, n_seq);
                                        }
                                        ret = 0 as i64;
                                    }
                                } else {
                                    ensure(false, 61);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

