// Apex k-pool context sizes in Goose (llama_memory_hybrid_idx_context, sizing paths).
//
// Three places produce k-pool sizes and they are not the same:
//
//   full context  kpool_build_sizes only fills `n_pool_real` from the layout; `n_new`, `n_new_g` and
//                 the generation stay zero. It exists so get_n_kpool() works while the graph is being
//                 reserved, before any batch has been prepared.
//   batch context the constructor walks the layout, then raises `n_pool_real` to the worst case the
//                 cache can hold — `(size / kpool) * n_seq_max` pools — because the reserved graph must
//                 not grow on the first decode. `n_new` takes that value and `n_new_g` is at least one:
//                 the graph always pools one entry, so a zero-token new list still has a row.
//                 The worst case must fit the padded count without overflowing the 32-bit index space.
//   accessor      get_n_kpool() pads n_pool_real; get_n_kpool_new() returns n_new_g, which is that same
//                 stable bound, never the count of pools actually built this step.
//
// Packet:
//   k=0  0 op, 1 out, 2 idx_size, 3 kpool, 4 n_seq_max, 5 lay_pool_real, 6 track, 7 n_pool_real,
//        8 n_new, 9 n_new_g
//   k=1  0 cnt
//   k=2  0 done, 1 ret
//
// Records: 0 size written (a = field 0..2, b = value), 1 worst case (a = value), 2 track skipped,
// 3 accessor (a = 0 padded / 1 new, b = value).
extern "fg343k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg343k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg343k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg343k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg343k_fail" fn fail(code: i64);

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

fn pad64(n: i64) -> i64 {
    var v: i64 = ( (n + (1 as i64)) + (63 as i64)) / (64 as i64) * (64 as i64);

    if v < (64 as i64) {
        v = 64 as i64;
    }
    return v;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let idx_size: i64 = get(packet, 0, 2) as i64;
    let kpool: i64 = get(packet, 0, 3) as i64;
    let n_seq_max: i64 = get(packet, 0, 4) as i64;
    let lay: i64 = get(packet, 0, 5) as i64;
    let track: i64 = get(packet, 0, 6) as i64;
    let n_pool_real: i64 = get(packet, 0, 7) as i64;
    let n_new: i64 = get(packet, 0, 8) as i64;
    let n_new_g: i64 = get(packet, 0, 9) as i64;
    let cntp: u64 = get(packet, 1, 0);
    var ret: i64 = 0;

    if op == (0 as i64) {
        // full context: sizes only
        ensure(lay >= (0 as i64), 60);
        emit(out, cntp, 0 as i64, 0 as i64, lay);
        ret = lay;
    } else {
        if op == (1 as i64) {
            // batch context

            if track == (0 as i64) {
                emit(out, cntp, 2 as i64, 0 as i64, 0 as i64);
                ret = (0 as i64) - 1 as i64;
            } else {
                ensure(kpool > (0 as i64) && n_seq_max >= (0 as i64), 61);
                let worst: i64 = (idx_size / kpool) * n_seq_max;
                emit(out, cntp, 1 as i64, worst, 0 as i64);
                // the padded count must fit the 32-bit index space
                ensure(worst <= (4294967295 as i64) - (64 as i64), 62);
                var npr: i64 = lay;

                if worst > npr {
                    npr = worst;
                }
                emit(out, cntp, 0 as i64, 0 as i64, npr);
                emit(out, cntp, 0 as i64, 1 as i64, npr);
                var g: i64 = npr;

                if g < (1 as i64) {
                    g = 1 as i64;
                }
                emit(out, cntp, 0 as i64, 2 as i64, g);
                ret = npr;
            }
        } else {
            if op == (2 as i64) {
                // accessors
                let padded: i64 = pad64(n_pool_real);
                emit(out, cntp, 3 as i64, 0 as i64, padded);
                emit(out, cntp, 3 as i64, 1 as i64, n_new_g);
                ret = padded;
            } else {
                ensure(false, 63);
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

