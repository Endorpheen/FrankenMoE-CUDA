// Apex KV state data restore in Goose (llama_kv_cache::state_read_data, P5-08).
//
// Destination indices are batched into contiguous runs: a run is a maximal walk where each index is
// the previous plus one, so a contiguous block reads as a single transfer.
//
// The header is validated in native order (layer count, capacity, V layout, key rotation, value
// rotation). Then the layer loop is outermost everywhere: keys per layer, then values per layer — as
// row-size transfers for a plain V, or, for a transposed V, with the embedding row between the layer
// and the runs, at offset `(run.from + j * cells) * element_size`.
//
// Rejection codes: 1 layer count, 2 capacity, 3 V layout, 4 key rotation, 5 value rotation.
//
// Transfer records, three slots: offset, size, embedding row (0 for keys and plain values).
//
// Packet, int64 slots:
//   k=0  0 v_trans_ref, 1 n_layer_ref, 2 n_rot_k_ref, 3 n_rot_v_ref, 4 cell_count, 5 cells_size,
//        6 v_trans, 7 n_layer, 8 n_rot_k, 9 n_rot_v, 10 idxs, 11 k_row, 12 v_row (GQA width),
//        13 v_el
//   k=1  0 out
//   k=2  0 count ptr
//   k=3  0 done, 1 ret
extern "fg325k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg325k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg325k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg325k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg325k_fail" fn fail(code: i64);

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

fn emit(out: u64, cntp: u64, off: i64, size: i64, h: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (3 as i64), off);
    setat(out, n * (3 as i64) + 1 as i64, size);
    setat(out, n * (3 as i64) + 2 as i64, h);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn reject(packet: i64, code: i64) {
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ( (0 as i64) - code as i64) as u64);
}

fn run(packet: i64) -> bool {
    let vtr_ref: i64 = get(packet, 0, 0) as i64;
    let nl_ref: i64 = get(packet, 0, 1) as i64;
    let nrk_ref: i64 = get(packet, 0, 2) as i64;
    let nrv_ref: i64 = get(packet, 0, 3) as i64;
    let cell_count: i64 = get(packet, 0, 4) as i64;
    let cells_size: i64 = get(packet, 0, 5) as i64;
    let vtr: i64 = get(packet, 0, 6) as i64;
    let nl: i64 = get(packet, 0, 7) as i64;
    let nrk: i64 = get(packet, 0, 8) as i64;
    let nrv: i64 = get(packet, 0, 9) as i64;
    let idxs: u64 = get(packet, 0, 10);
    let k_row: i64 = get(packet, 0, 11) as i64;
    let v_row: i64 = get(packet, 0, 12) as i64;
    let v_el: i64 = get(packet, 0, 13) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(cell_count >= (0 as i64) && cells_size > (0 as i64), 60);
    ensure(k_row >= (0 as i64) && v_row >= (0 as i64) && v_el > (0 as i64), 61);

    if nl_ref != nl {
        reject(packet, 1 as i64);
        return true;
    }

    if cell_count > cells_size {
        reject(packet, 2 as i64);
        return true;
    }

    if vtr_ref != vtr {
        reject(packet, 3 as i64);
        return true;
    }

    if nrk_ref != nrk {
        reject(packet, 4 as i64);
        return true;
    }

    if nrv_ref != nrv {
        reject(packet, 5 as i64);
        return true;
    }
    // keys: layer outermost, runs inside
    var il: i64 = 0;

    while il < nl {
        var i0: i64 = 0;

        while i0 < cell_count {
            var i1: i64 = i0 + (1 as i64);
            var stop1: bool = false;

            while i1 < cell_count && !stop1 {
                if at(idxs, i1) == at(idxs, i1 - (1 as i64)) + (1 as i64) {
                    i1 = i1 + 1 as i64;
                } else {
                    stop1 = true;
                }
            }
            let from: i64 = at(idxs, i0);
            let to: i64 = at(idxs, i1 - (1 as i64)) + (1 as i64);
            emit(out, cntp, from * k_row, (to - from) * k_row, 0 as i64);
            i0 = i1;
        }
        il = il + 1 as i64;
    }

    if vtr == (0 as i64) {
        il = 0;

        while il < nl {
            var i2: i64 = 0;

            while i2 < cell_count {
                var i3: i64 = i2 + (1 as i64);
                var stop2: bool = false;

                while i3 < cell_count && !stop2 {
                    if at(idxs, i3) == at(idxs, i3 - (1 as i64)) + (1 as i64) {
                        i3 = i3 + 1 as i64;
                    } else {
                        stop2 = true;
                    }
                }
                let f2: i64 = at(idxs, i2);
                let t2: i64 = at(idxs, i3 - (1 as i64)) + (1 as i64);
                emit(out, cntp, f2 * v_row, (t2 - f2) * v_row, 0 as i64);
                i2 = i3;
            }
            il = il + 1 as i64;
        }
    } else {
        il = 0;

        while il < nl {
            var j: i64 = 0;

            while j < v_row {
                var i4: i64 = 0;

                while i4 < cell_count {
                    var i5: i64 = i4 + (1 as i64);
                    var stop4: bool = false;

                    while i5 < cell_count && !stop4 {
                        if at(idxs, i5) == at(idxs, i5 - (1 as i64)) + (1 as i64) {
                            i5 = i5 + 1 as i64;
                        } else {
                            stop4 = true;
                        }
                    }
                    let f4: i64 = at(idxs, i4);
                    let t4: i64 = at(idxs, i5 - (1 as i64)) + (1 as i64);
                    emit(out, cntp, (f4 + j * cells_size) * v_el, (t4 - f4) * v_el, j);
                    i4 = i5;
                }
                j = j + 1 as i64;
            }
            il = il + 1 as i64;
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, 0 as i64 as u64);
    return true;
}

fn main() {
    run(0);
}

