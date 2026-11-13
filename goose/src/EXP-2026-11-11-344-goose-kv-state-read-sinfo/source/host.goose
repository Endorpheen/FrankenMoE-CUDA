// Apex KV state read orchestration and clearing in Goose
// (llama_kv_cache::state_read_sinfo, state_clear(seq_id), state_clear(seq_id, strm, sinfo)).
//
// state_read_sinfo is the transaction around the meta and data halves:
//
//   - a mirrored cache does nothing; a partial-only restore of a non-SWA cache does nothing either,
//     silently — that combination has no partial state to read;
//   - the mirrored layout must have exactly one entry per stream, checked before anything is read;
//   - the stream count in the section must match, checked before anything is written;
//   - a whole-context read clears the cache **once up front**: clearing per stream inside the loop
//     would leave only the last stream cleared;
//   - a zero-cell stream requires an empty mirrored layout, or the two no longer agree;
//   - a failed meta or a throwing data half clears the sequence through the same stream and layout,
//     then throws. The clear comes before the throw, so a half-restored cache never survives.
//
// state_clear(seq_id) collects the cells that only this sequence holds — a cell still shared keeps its
// data — and delegates. The shared body removes the sequence first, then zeroes the tensors: one
// contiguous range when the layout allows, per-cell otherwise; keys by layer, then values by layer, and
// a transposed value cache is cleared per embedding row.
//
// Packet:
//   k=0  0 op, 1 other, 2 partial, 3 swa_none, 4 seq_id, 5 n_stream, 6 mirror, 7 mirror_size,
//        8 n_stream_cur, 9 cell_count, 10 mirror_empty, 11 meta_ok, 12 data_throw, 13 out
//   k=1  0 cnt, 1 s2s, 2 idxs, 3 n_idx, 4 n_layer, 5 k_row, 6 v_present, 7 v_row, 8 v_trans,
//        9 v_el, 10 cells_size, 11 head, 12 seq_has, 13 seq_count
//   k=2  0 done, 1 ret (0 ok, negative rejection)
//
// Records: 0 whole cache cleared, 1 stream processed (a = index), 2 meta read (a = stream),
// 3 data read (a = stream), 4 sequence cleared (a = stream), 5 sequence removed, 6 tensor zeroed
// (a = offset, b = bytes), 7 layout rejected, 8 stream skipped (a = index).
extern "fg344k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg344k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg344k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg344k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg344k_fail" fn fail(code: i64);

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

fn contiguous(idxs: u64, n: i64) -> bool {
    if n <= (1 as i64) {
        return true;
    }
    var i: i64 = 1;

    while i < n {
        if at(idxs, i) != at(idxs, i - (1 as i64)) + (1 as i64) {
            return false;
        }
        i = i + 1 as i64;
    }
    return true;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let other: i64 = get(packet, 0, 1) as i64;
    let partial: i64 = get(packet, 0, 2) as i64;
    let swa_none: i64 = get(packet, 0, 3) as i64;
    let seq_id: i64 = get(packet, 0, 4) as i64;
    let n_stream: i64 = get(packet, 0, 5) as i64;
    let mirror: i64 = get(packet, 0, 6) as i64;
    let mirror_size: i64 = get(packet, 0, 7) as i64;
    let n_stream_cur: i64 = get(packet, 0, 8) as i64;
    let cell_count: u64 = get(packet, 0, 9);
    let mirror_empty: u64 = get(packet, 0, 10);
    let meta_ok: u64 = get(packet, 0, 11);
    let data_throw: u64 = get(packet, 0, 12);
    let out: u64 = get(packet, 0, 13);
    let cntp: u64 = get(packet, 1, 0);
    let s2s: u64 = get(packet, 1, 1);
    let idxs: u64 = get(packet, 1, 2);
    let n_idx: i64 = get(packet, 1, 3) as i64;
    let n_layer: i64 = get(packet, 1, 4) as i64;
    let k_row: u64 = get(packet, 1, 5);
    let v_present: u64 = get(packet, 1, 6);
    let v_row: u64 = get(packet, 1, 7);
    let v_trans: i64 = get(packet, 1, 8) as i64;
    let v_el: i64 = get(packet, 1, 9) as i64;
    let cells_size: i64 = get(packet, 1, 10) as i64;
    let head: i64 = get(packet, 1, 11) as i64;
    let seq_has: u64 = get(packet, 1, 12);
    let seq_count: u64 = get(packet, 1, 13);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // state_read_sinfo

        if other != (0 as i64) {
            done(packet, cntp, 0 as i64);
            return true;
        }

        if partial != (0 as i64) && swa_none != (0 as i64) {
            done(packet, cntp, 0 as i64);
            return true;
        }

        if mirror != (0 as i64) && mirror_size != n_stream {
            done(packet, cntp, 1 as i64);
            return true;
        }

        if n_stream_cur != n_stream {
            done(packet, cntp, 2 as i64);
            return true;
        }

        if seq_id == (0 as i64) - 1 as i64 {
            // once, before the loop
            emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);
        }
        var s: i64 = 0;

        while s < n_stream {
            let cc: i64 = at(cell_count, s);

            if cc == (0 as i64) {
                if mirror != (0 as i64) {
                    if at(mirror_empty, s) == (0 as i64) {
                        done(packet, cntp, 3 as i64);
                        return true;
                    }
                }
                emit(out, cntp, 8 as i64, s, 0 as i64);
                s = s + 1 as i64;
            } else {
                var strm: i64 = s;

                if seq_id != (0 as i64) - 1 as i64 {
                    strm = at(s2s, seq_id);
                }
                emit(out, cntp, 1 as i64, s, 0 as i64);
                var ok: i64 = at(meta_ok, s);

                if ok != (0 as i64) {
                    emit(out, cntp, 2 as i64, strm, 0 as i64);
                }
                emit(out, cntp, 3 as i64, strm, 0 as i64);

                if at(data_throw, s) != (0 as i64) {
                    ok = 0 as i64;
                }

                if ok == (0 as i64) {
                    emit(out, cntp, 4 as i64, strm, 0 as i64);
                    done(packet, cntp, 4 as i64);
                    return true;
                }
                s = s + 1 as i64;
            }
        }
        ret = 0 as i64;
    } else {
        if op == (1 as i64) {
            // state_clear(seq_id, strm, sinfo)

            if seq_id == (0 as i64) - 1 as i64 {
                emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);
                ret = 0 as i64;
            } else {
                emit(out, cntp, 5 as i64, seq_id, 0 as i64);

                if n_idx <= (0 as i64) {
                    done(packet, cntp, 0 as i64);
                    return true;
                }
                let is_contig: bool = contiguous(idxs, n_idx);
                var il: i64 = 0;

                while il < n_layer {
                    if is_contig {
                        let off: i64 = head * at(k_row, il);
                        emit(out, cntp, 6 as i64, off, n_idx * at(k_row, il));
                    } else {
                        var i: i64 = 0;

                        while i < n_idx {
                            emit(out, cntp, 6 as i64, at(idxs, i) * at(k_row, il), at(k_row, il));
                            i = i + 1 as i64;
                        }
                    }
                    il = il + 1 as i64;
                }
                il = 0;

                while il < n_layer {
                    if at(v_present, il) != (0 as i64) {
                        if v_trans == (0 as i64) {
                            if is_contig {
                                let off2: i64 = head * at(v_row, il);
                                emit(out, cntp, 6 as i64, off2, n_idx * at(v_row, il));
                            } else {
                                var i2: i64 = 0;

                                while i2 < n_idx {
                                    emit(out, cntp, 6 as i64, at(idxs, i2) * at(v_row, il), at(v_row, il));
                                    i2 = i2 + 1 as i64;
                                }
                            }
                        } else {
                            if is_contig {
                                var j: i64 = 0;

                                while j < at(v_row, il) {
                                    emit(out, cntp, 6 as i64, (head + j * cells_size) * v_el, n_idx * v_el);
                                    j = j + 1 as i64;
                                }
                            } else {
                                var j2: i64 = 0;

                                while j2 < at(v_row, il) {
                                    var i3: i64 = 0;

                                    while i3 < n_idx {
                                        emit(out, cntp, 6 as i64, (at(idxs, i3) + j2 * cells_size) * v_el, v_el);
                                        i3 = i3 + 1 as i64;
                                    }
                                    j2 = j2 + 1 as i64;
                                }
                            }
                        }
                    }
                    il = il + 1 as i64;
                }
                ret = 0 as i64;
            }
        } else {
            if op == (2 as i64) {
                // state_clear(seq_id): cells only this sequence holds

                if seq_id == (0 as i64) - 1 as i64 {
                    emit(out, cntp, 0 as i64, 0 as i64, 0 as i64);
                    ret = 0 as i64;
                } else {
                    var count: i64 = 0;
                    var c: i64 = 0;

                    while c < cells_size {
                        if at(seq_has, c) != (0 as i64) {
                            if at(seq_count, c) == (1 as i64) {
                                emit(out, cntp, 5 as i64, c, 0 as i64);
                                count = count + 1 as i64;
                            }
                        }
                        c = c + 1 as i64;
                    }
                    ret = count;
                }
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

