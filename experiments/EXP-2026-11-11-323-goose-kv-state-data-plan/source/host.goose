// Apex KV state data plan in Goose (llama_kv_cache::state_write_data, P5-08).
//
// The header carries the V layout flag, the layer count and the two rotation sizes. Then keys: per
// layer the type and row size, followed by one transfer per cell range at `range.first * row`. For a
// plain V the values follow the same shape. For a transposed V the element size and the GQA width are
// written instead of a row size, and the transfers walk the embedding dimension outermost: for every
// embedding row j and every cell range, the offset is `(first + j * kv_size) * element_size`.
//
// Operation records, four slots each: kind, a, b, c.
//   kind 0: scalar(a)                    a <= 0: v_trans, n_layer, n_rot_k, n_rot_v
//                                        a >  0: k_type, k_row_size
//   kind 1: v scalar a                   a = v_type / v_size_el / v_embd_gqa
//   kind 2: transfer(off, size)          a = offset, b = size
//
// Packet, int64 slots:
//   k=0  0 v_trans, 1 n_layer, 2 n_rot_k, 3 n_rot_v, 4 kv_size, 5 n_embd_v_gqa, 6 n_embd_v_gqa_max,
//        7 n_ranges, 8 ranges, 9 k_type, 10 k_row, 11 v_type, 12 v_row, 13 v_el
//   k=1  0 out
//   k=3  0 done, 1 ret (record count)
extern "fg323k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg323k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg323k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg323k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg323k_fail" fn fail(code: i64);

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

fn emit(out: u64, cntp: u64, kind: i64, a: i64, b: i64, c: i64) {
    let n: i64 = at(cntp, 0 as i64);
    setat(out, n * (4 as i64), kind);
    setat(out, n * (4 as i64) + 1 as i64, a);
    setat(out, n * (4 as i64) + 2 as i64, b);
    setat(out, n * (4 as i64) + 3 as i64, c);
    setat(cntp, 0 as i64, n + (1 as i64));
}

fn run(packet: i64) -> bool {
    let v_trans: i64 = get(packet, 0, 0) as i64;
    let n_layer: i64 = get(packet, 0, 1) as i64;
    let n_rot_k: i64 = get(packet, 0, 2) as i64;
    let n_rot_v: i64 = get(packet, 0, 3) as i64;
    let kv_size: i64 = get(packet, 0, 4) as i64;
    let gqa: i64 = get(packet, 0, 5) as i64;
    let gqa_max: i64 = get(packet, 0, 6) as i64;
    let n_ranges: i64 = get(packet, 0, 7) as i64;
    let ranges: u64 = get(packet, 0, 8);
    let k_type: i64 = get(packet, 0, 9) as i64;
    let k_row: i64 = get(packet, 0, 10) as i64;
    let v_type: i64 = get(packet, 0, 11) as i64;
    let v_row: i64 = get(packet, 0, 12) as i64;
    let v_el: i64 = get(packet, 0, 13) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    ensure(n_layer >= (0 as i64) && kv_size > (0 as i64) && n_ranges >= (0 as i64), 60);
    ensure(k_row >= (0 as i64) && v_el > (0 as i64), 61);
    // header scalars
    emit(out, cntp, 0 as i64, 0 as i64, v_trans, 0 as i64);
    emit(out, cntp, 0 as i64, 1 as i64, n_layer, 0 as i64);
    emit(out, cntp, 0 as i64, 2 as i64, n_rot_k, 0 as i64);
    emit(out, cntp, 0 as i64, 3 as i64, n_rot_v, 0 as i64);
    var il: i64 = 0;

    while il < n_layer {
        emit(out, cntp, 0 as i64, 4 as i64, k_type, 0 as i64);
        emit(out, cntp, 0 as i64, 5 as i64, k_row, 0 as i64);
        var r: i64 = 0;

        while r < n_ranges {
            let lo: i64 = at(ranges, (2 as i64 * r));
            let hi: i64 = at(ranges, (2 as i64 * r + 1 as i64));
            emit(out, cntp, 2 as i64, lo * k_row, (hi - lo) * k_row, 0 as i64);
            r = r + 1 as i64;
        }
        il = il + 1 as i64;
    }

    if v_trans == (0 as i64) {
        il = 0;

        while il < n_layer {
            emit(out, cntp, 1 as i64, 0 as i64, v_type, 0 as i64);
            emit(out, cntp, 1 as i64, 1 as i64, v_row, 0 as i64);
            var r: i64 = 0;

            while r < n_ranges {
                let lo: i64 = at(ranges, (2 as i64 * r));
                let hi: i64 = at(ranges, (2 as i64 * r + 1 as i64));
                emit(out, cntp, 2 as i64, lo * v_row, (hi - lo) * v_row, 0 as i64);
                r = r + 1 as i64;
            }
            il = il + 1 as i64;
        }
    } else {
        il = 0;

        while il < n_layer {
            emit(out, cntp, 1 as i64, 0 as i64, v_type, 0 as i64);
            emit(out, cntp, 1 as i64, 2 as i64, v_el, 0 as i64);
            emit(out, cntp, 1 as i64, 3 as i64, gqa, 0 as i64);
            var j: i64 = 0;

            while j < gqa {
                var r: i64 = 0;

                while r < n_ranges {
                    let lo: i64 = at(ranges, (2 as i64 * r));
                    let hi: i64 = at(ranges, (2 as i64 * r + 1 as i64));
                    emit(out, cntp, 2 as i64, (lo + j * kv_size) * v_el, (hi - lo) * v_el, 0 as i64);
                    r = r + 1 as i64;
                }
                j = j + 1 as i64;
            }
            il = il + 1 as i64;
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, at(cntp, 0 as i64) as u64);
    return true;
}

fn main() {
    run(0);
}

