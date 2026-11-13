// Apex KV rotation matrices in Goose: the Walsh-Hadamard generator and the rotation-size choice.
//
// The matrix is Sylvester's: starting from the top-left corner, each doubling step copies the block
// into the right and bottom quadrants and negates the bottom-right one, so the entries are ±1 and the
// norm comes from scaling the corner by 1/sqrt(n). The gate compares the integer signs and the scale
// separately, because that is what the recursion produces before normalisation.
//
// The K rotation size starts at 64 and doubles while the whole key width is still a multiple of twice
// the size; the V rotation is fixed at 64 and only applies to a quantised V whose head width is a
// multiple of 64. The K rotation itself is enabled for the lightning-indexer architectures when the
// full key width equals the indexer head size.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n, 3 embd_k_all, 4 embd_v_all, 5 arch_indexer, 6 k_full_eq_idx,
//        7 rot_disable, 8 type_v_quantized, 9 embd_head_v
//   k=3  0 done, 1 ret
extern "fg320k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg320k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg320k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg320k_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "fg320k_fail" fn fail(code: i64);

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

fn is_pow2(n: i64) -> bool {
    if n <= (0 as i64) {
        return false;
    }
    var x: i64 = n;

    while x > (1 as i64) {
        if x % 2 as i64 != (0 as i64) {
            return false;
        }
        x = x / 2 as i64;
    }
    return true;
}

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let out: u64 = get(packet, 0, 1);
    let n: i64 = get(packet, 0, 2) as i64;
    let embd_k: i64 = get(packet, 0, 3) as i64;
    let embd_v: i64 = get(packet, 0, 4) as i64;
    let arch_idx: i64 = get(packet, 0, 5) as i64;
    let k_eq_idx: i64 = get(packet, 0, 6) as i64;
    let rot_disable: i64 = get(packet, 0, 7) as i64;
    let v_quant: i64 = get(packet, 0, 8) as i64;
    let head_v: i64 = get(packet, 0, 9) as i64;
    var ret: i64 = 0;

    if op == (0 as i64) {
        ensure(is_pow2(n), 60);
        ensure(n >= (1 as i64), 61);
        // integer signs, row-major
        setat(out, 0 as i64, 1 as i64);
        var s: i64 = 1;

        while s < n {
            var i: i64 = 0;

            while i < s {
                var j: i64 = 0;

                while j < s {
                    let val: i64 = at(out, i * n + j);
                    setat(out, (i + s) * n + j, val);
                    setat(out, i * n + (j + s), val);
                    setat(out, (i + s) * n + (j + s), (0 as i64) - val);
                    j = j + 1 as i64;
                }
                i = i + 1 as i64;
            }
            s = s * 2 as i64;
        }
        ret = n * n;
    } else {
        if op == (1 as i64) {
            // rotation sizes
            var rot_k: i64 = 0;
            var n_rot_k: i64 = 0;
            var rot_v: i64 = 0;
            var n_rot_v: i64 = 0;

            if arch_idx != (0 as i64) {
                if k_eq_idx != (0 as i64) {
                    rot_k = 1 as i64;
                }
            }

            if rot_k != (0 as i64) && embd_k > (0 as i64) {
                n_rot_k = 64 as i64;

                while embd_k % (2 as i64 * n_rot_k) == (0 as i64) {
                    n_rot_k = n_rot_k * 2 as i64;
                }
            }

            if rot_disable == (0 as i64) {
                if embd_v > (0 as i64) {
                    if v_quant != (0 as i64) {
                        if head_v % 64 as i64 == (0 as i64) {
                            rot_v = 1 as i64;
                            n_rot_v = 64 as i64;
                        }
                    }
                }
            }
            setat(out, 0 as i64, rot_k);
            setat(out, 1 as i64, n_rot_k);
            setat(out, 2 as i64, rot_v);
            setat(out, 3 as i64, n_rot_v);
            ret = 1 as i64;
        } else {
            ensure(false, 62);
        }
    }
    put(packet, 3, 0, 1 as u64);
    put(packet, 3, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

