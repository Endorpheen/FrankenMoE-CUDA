// Apex KV state-read header decisions on the working line (llama_kv_cache::state_read_data).
//
// The KV read starts with eight checks, in this order: layer count, capacity, V layout, key rotation,
// value rotation, and then per layer the key type and row size and the value type and row size.
// Unlike the recurrent variant there is no separate PLE section here, and the layout flag is
// `v_trans`, not `s_trans` -- the two files differ and must not share a module.
//
// Output: the first rejection code, or zero with two per-layer read masks (keys, values), so the
// caller performs exactly the transfers native would. The transfers themselves stay native.
//
// Packet:
//   k=0  0 n_layer_ref, 1 n_layer_cur, 2 cell_count, 3 size, 4 v_trans_ref, 5 v_trans_cur,
//        6 n_rot_k_ref, 7 n_rot_k, 8 n_rot_v_ref, 9 n_rot_v, 10 k_type_ok[], 11 k_row_ok[],
//        12 v_type_ok[], 13 v_row_ok[]
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (0 ok, negative rejection code)
//
// Records: 0 mask (a = 0 keys / 1 values, b = per-layer bitmap)
extern "fg366k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg366k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg366k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg366k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg366k_fail" fn fail(code: i64);

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
    let nl_ref: i64 = get(packet, 0, 0) as i64;
    let nl: i64 = get(packet, 0, 1) as i64;
    let cc: i64 = get(packet, 0, 2) as i64;
    let size: i64 = get(packet, 0, 3) as i64;
    let tr_ref: i64 = get(packet, 0, 4) as i64;
    let tr: i64 = get(packet, 0, 5) as i64;
    let rk_ref: i64 = get(packet, 0, 6) as i64;
    let rk: i64 = get(packet, 0, 7) as i64;
    let rv_ref: i64 = get(packet, 0, 8) as i64;
    let rv: i64 = get(packet, 0, 9) as i64;
    let ktp: u64 = get(packet, 0, 10);
    let krp: u64 = get(packet, 0, 11);
    let vtp: u64 = get(packet, 0, 12);
    let vrp: u64 = get(packet, 0, 13);
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 1, 1);
    ensure(nl >= (0 as i64), 60);

    if nl_ref != nl {
        done(packet, cntp, 1 as i64);
        return true;
    }

    if cc > size {
        done(packet, cntp, 2 as i64);
        return true;
    }

    if tr_ref != tr {
        done(packet, cntp, 3 as i64);
        return true;
    }

    if rk_ref != rk {
        done(packet, cntp, 4 as i64);
        return true;
    }

    if rv_ref != rv {
        done(packet, cntp, 5 as i64);
        return true;
    }
    // per layer, in native order: key type, key row, value type, value row
    var il: i64 = 0;

    while il < nl {
        if at(ktp, il) == (0 as i64) {
            done(packet, cntp, 6 as i64);
            return true;
        }

        if at(krp, il) == (0 as i64) {
            done(packet, cntp, 7 as i64);
            return true;
        }

        if at(vtp, il) == (0 as i64) {
            done(packet, cntp, 8 as i64);
            return true;
        }

        if at(vrp, il) == (0 as i64) {
            done(packet, cntp, 9 as i64);
            return true;
        }
        il = il + 1 as i64;
    }
    var k_mask: i64 = 0 as i64;

    if cc > (0 as i64) {
        k_mask = ( (1 as i64) << nl) - (1 as i64);
    }
    emit(out, cntp, 0 as i64, 0 as i64, k_mask);
    emit(out, cntp, 0 as i64, 1 as i64, k_mask);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, 0 as i64 as u64);
    return true;
}

fn main() {
    run(0);
}

