// Apex KV state clearing plan in Goose (llama_kv_cache::state_clear, working line).
//
// Clearing a failed restore zeroes the tensors of the cells the restore touched. The shape is decided
// once and then applied per layer:
//
//   contiguous cells   one call per tensor whose size covers the whole block;
//   scattered cells    one call per cell, at that cell's row.
//
// Keys are cleared for every layer. Values only for layers that have a value tensor, and a transposed
// value tensor is cleared per embedding row at `(head + j * cells) * element_size` — the same address
// the read path would have written.
//
// Packet:
//   k=0  0 n_idx, 1 idxs, 2 head, 3 cells_size, 4 n_layer, 5 k_row, 6 v_present, 7 v_row,
//        8 v_trans, 9 v_el, 10 contig
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (number of clears)
//
// Records, four slots: kind (0 = key, 1 = value), layer, offset, size.
extern "fg367k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg367k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg367k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg367k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg367k_fail" fn fail(code: i64);

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

fn emit4(out: u64, cntp: u64, kind: i64, il: i64, off: i64, size: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (4 as i64), kind);
    setat(out, n * (4 as i64) + 1 as i64, il);
    setat(out, n * (4 as i64) + 2 as i64, off);
    setat(out, n * (4 as i64) + 3 as i64, size);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    let n_idx: i64 = get(packet, 0, 0) as i64;
    let idxs: u64 = get(packet, 0, 1);
    let head: i64 = get(packet, 0, 2) as i64;
    let cells_size: i64 = get(packet, 0, 3) as i64;
    let n_layer: i64 = get(packet, 0, 4) as i64;
    let k_row: u64 = get(packet, 0, 5);
    let v_present: u64 = get(packet, 0, 6);
    let v_row: u64 = get(packet, 0, 7);
    let v_trans: i64 = get(packet, 0, 8) as i64;
    let v_el: i64 = get(packet, 0, 9) as i64;
    let contig: i64 = get(packet, 0, 10) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    ensure(n_idx >= (0 as i64) && n_layer >= (0 as i64), 60);
    ensure(cells_size > (0 as i64) && v_el > (0 as i64), 61);
    var n: i64 = 0 as i64;
    var il: i64 = 0;

    while il < n_layer {
        let row: i64 = at(k_row, il);

        if contig != (0 as i64) {
            emit4(out, cntp, 0 as i64, il, head * row, n_idx * row);
            n = n + (1 as i64);
        } else {
            var i: i64 = 0;

            while i < n_idx {
                emit4(out, cntp, 0 as i64, il, at(idxs, i) * row, row);
                n = n + (1 as i64);
                i = i + 1 as i64;
            }
        }
        il = il + 1 as i64;
    }
    il = 0;

    while il < n_layer {
        if at(v_present, il) != (0 as i64) {
            let row2: i64 = at(v_row, il);

            if v_trans == (0 as i64) {
                if contig != (0 as i64) {
                    emit4(out, cntp, 1 as i64, il, head * row2, n_idx * row2);
                    n = n + (1 as i64);
                } else {
                    var i2: i64 = 0;

                    while i2 < n_idx {
                        emit4(out, cntp, 1 as i64, il, at(idxs, i2) * row2, row2);
                        n = n + (1 as i64);
                        i2 = i2 + 1 as i64;
                    }
                }
            } else {
                if contig != (0 as i64) {
                    var j: i64 = 0;

                    while j < row2 {
                        emit4(out, cntp, 1 as i64, il, (head + j * cells_size) * v_el, n_idx * v_el);
                        n = n + (1 as i64);
                        j = j + 1 as i64;
                    }
                } else {
                    var j2: i64 = 0;

                    while j2 < row2 {
                        var i3: i64 = 0;

                        while i3 < n_idx {
                            emit4(out, cntp, 1 as i64, il, (at(idxs, i3) + j2 * cells_size) * v_el, v_el);
                            n = n + (1 as i64);
                            i3 = i3 + 1 as i64;
                        }
                        j2 = j2 + 1 as i64;
                    }
                }
            }
        }
        il = il + 1 as i64;
    }
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, n as u64);
    return true;
}

fn main() {
    run(0);
}

