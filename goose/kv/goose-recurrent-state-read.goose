// Apex recurrent state restore in Goose (llama_memory_recurrent::state_read_meta / state_read_data).
//
// Metadata, one sequence: the section must fit, the destination is removed first, and a zero-length
// section stops right there — no record is read at all. Each record must carry **zero** ids here (the
// single-sequence layout stores one id for the whole section, unlike the attention cache where every
// record carries one). The slot search must succeed; afterwards the restored cells claim their own
// source, so the recurrent states are not overwritten by a later copy.
//
// Metadata, whole cache: the section must fit and the cache is cleared **before** any record is read,
// so a failure part-way leaves a cleared cache, not the previous one. Ids are range-checked, and a
// sequence may have only one tail cell — the tail lives in the cell whose index equals the sequence id.
// Then head is zero, used is the section length, and every cell claims itself as source.
//
// Data: header first — layer count, capacity, transposition. The transposed value layout is rejected
// before any write, which is why the clearing path in state_clear does not handle it. Then, per layer,
// the recurrent rows (skip null layers) with their optional PLE rows, then the state rows. A null
// tensor skips its layer entirely: no type and no row size is read for it.
//
// Packet:
//   k=0  0 op, 1 cell_count, 2 size, 3 dest, 4 n_seq_max, 5 rec_pos, 6 rec_nseq, 7 rec_ids,
//        8 find_ok, 9 head, 10 tail, 11 nl_ref, 12 nl, 13 s_trans, 14 r_present, 15 r_rows
//   k=1  0 out, 1 cnt, 2 p_present, 3 p_rows, 4 s_present, 5 s_rows, 6 r_type_ok, 7 r_row_ok,
//        8 p_row_ok, 9 s_type_ok, 10 s_row_ok
//   k=2  0 done, 1 ret (0 ok, negative rejection code)
//
// Records, three slots: 0 sequence removed, 1 record read (a = index, b = n_seq_id), 2 cache cleared,
// 3 id inserted (a = cell, b = id), 4 tail set (a = cell, b = cell), 5 head written (a = value),
// 6 used written (a = value), 7 source claimed (a = cell), 8 tensor read (a = offset, b = bytes).
extern "fg338k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg338k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg338k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg338k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg338k_fail" fn fail(code: i64);

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

fn done(packet: i64, cntp: u64, code: i64) {
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, ( (0 as i64) - code as i64) as u64);
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let cell_count: i64 = get(packet, 0, 1) as i64;
    let size: i64 = get(packet, 0, 2) as i64;
    let dest: i64 = get(packet, 0, 3) as i64;
    let n_seq_max: i64 = get(packet, 0, 4) as i64;
    let rec_pos: u64 = get(packet, 0, 5);
    let rec_nseq: u64 = get(packet, 0, 6);
    let rec_ids: u64 = get(packet, 0, 7);
    let find_ok: i64 = get(packet, 0, 8) as i64;
    let headp: u64 = get(packet, 0, 9);
    let tail: u64 = get(packet, 0, 10);
    let nl_ref: i64 = get(packet, 0, 11) as i64;
    let nl: i64 = get(packet, 0, 12) as i64;
    let s_trans: i64 = get(packet, 0, 13) as i64;
    let r_present: u64 = get(packet, 0, 14);
    let r_rows: u64 = get(packet, 0, 15);
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    let p_present: u64 = get(packet, 1, 2);
    let p_rows: u64 = get(packet, 1, 3);
    let s_present: u64 = get(packet, 1, 4);
    let s_rows: u64 = get(packet, 1, 5);
    let r_type_ok: i64 = get(packet, 1, 6) as i64;
    let r_row_ok: i64 = get(packet, 1, 7) as i64;
    let p_row_ok: i64 = get(packet, 1, 8) as i64;
    let s_type_ok: i64 = get(packet, 1, 9) as i64;
    let s_row_ok: i64 = get(packet, 1, 10) as i64;
    var ret: i64 = 0;

    if op == (0 as i64) {
        // metadata, single sequence

        if cell_count > size {
            done(packet, cntp, 1 as i64);
            return true;
        }
        emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);

        if cell_count == (0 as i64) {
            done(packet, cntp, 0 as i64);
            return true;
        }
        var i: i64 = 0;

        while i < cell_count {
            let nseq: i64 = at(rec_nseq, i);

            if nseq != (0 as i64) {
                done(packet, cntp, 2 as i64);
                return true;
            }
            emit(out, cntp, 1 as i64, i, nseq);
            i = i + 1 as i64;
        }

        if find_ok == (0 as i64) {
            done(packet, cntp, 3 as i64);
            return true;
        }
        var h: i64 = at(headp, 0 as i64);
        var k: i64 = 0;

        while k < cell_count {
            emit(out, cntp, 7 as i64, h + k, 0 as i64);
            k = k + 1 as i64;
        }
        ret = 0 as i64;
    } else {
        if op == (1 as i64) {
            // metadata, whole cache

            if cell_count > size {
                done(packet, cntp, 1 as i64);
                return true;
            }
            emit(out, cntp, 2 as i64, 0 as i64, 0 as i64);
            var i2: i64 = 0;

            while i2 < cell_count {
                let nseq: i64 = at(rec_nseq, i2);
                emit(out, cntp, 1 as i64, i2, nseq);
                var j: i64 = 0;

                while j < nseq {
                    let id: i64 = at(rec_ids, i2 * (8 as i64) + j);

                    if id < (0 as i64) || id >= n_seq_max {
                        done(packet, cntp, 4 as i64);
                        return true;
                    }
                    emit(out, cntp, 3 as i64, i2, id);

                    if at(tail, id) != (0 as i64) - 1 as i64 {
                        done(packet, cntp, 5 as i64);
                        return true;
                    }
                    setat(tail, id, i2);
                    emit(out, cntp, 4 as i64, i2, i2);
                    j = j + 1 as i64;
                }
                i2 = i2 + 1 as i64;
            }
            emit(out, cntp, 5 as i64, 0 as i64, 0 as i64);
            emit(out, cntp, 6 as i64, cell_count, 0 as i64);
            var k2: i64 = 0;

            while k2 < cell_count {
                emit(out, cntp, 7 as i64, k2, 0 as i64);
                k2 = k2 + 1 as i64;
            }
            ret = 0 as i64;
        } else {
            if op == (2 as i64) {
                // data header

                if nl_ref != nl {
                    done(packet, cntp, 1 as i64);
                    return true;
                }

                if cell_count > size {
                    done(packet, cntp, 2 as i64);
                    return true;
                }

                if s_trans != (0 as i64) {
                    done(packet, cntp, 3 as i64);
                    return true;
                }
                var h2: i64 = at(headp, 0 as i64);
                var il: i64 = 0;

                while il < nl {
                    if at(r_present, il) != (0 as i64) {
                        if r_type_ok == (0 as i64) {
                            done(packet, cntp, 4 as i64);
                            return true;
                        }

                        if r_row_ok == (0 as i64) {
                            done(packet, cntp, 5 as i64);
                            return true;
                        }
                        let rr: i64 = at(r_rows, il);

                        if cell_count > (0 as i64) {
                            emit(out, cntp, 8 as i64, h2 * rr, cell_count * rr);
                        }

                        if at(p_present, il) != (0 as i64) {
                            if p_row_ok == (0 as i64) {
                                done(packet, cntp, 6 as i64);
                                return true;
                            }
                            let pr: i64 = at(p_rows, il);

                            if cell_count > (0 as i64) {
                                emit(out, cntp, 8 as i64, h2 * pr, cell_count * pr);
                            }
                        }
                    }
                    il = il + 1 as i64;
                }
                il = 0;

                while il < nl {
                    if at(s_present, il) != (0 as i64) {
                        if s_type_ok == (0 as i64) {
                            done(packet, cntp, 7 as i64);
                            return true;
                        }

                        if s_row_ok == (0 as i64) {
                            done(packet, cntp, 8 as i64);
                            return true;
                        }
                        let sr: i64 = at(s_rows, il);

                        if cell_count > (0 as i64) {
                            emit(out, cntp, 8 as i64, h2 * sr, cell_count * sr);
                        }
                    }
                    il = il + 1 as i64;
                }
                ret = 0 as i64;
            } else {
                ensure(false, 60);
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

