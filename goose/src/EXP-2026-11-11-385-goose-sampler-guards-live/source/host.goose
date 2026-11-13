// Apex sampler early-exit guards in Goose (min_p, typical, top_p, top_k).
//
// Each filter is skipped when its parameter makes it a no-op:
//   min_p    p <= 0, or an empty candidate list
//   typical  p >= 1
//   top_p    p >= 1
//   top_k    k <= 0
//
// Packet:
//   k=0  0 op, 1 a (p scaled by 1e6, or k), 2 b (size)
//   k=1  0 out
//   k=2  0 done, 1 ret
//
// Records: 0 decide (a = op, b = ret)
extern "fg385k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg385k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg385k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg385k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg385k_fail" fn fail(code: i64);

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

fn run(packet: i64) -> bool {
    let op: i64 = get(packet, 0, 0) as i64;
    let a: i64 = get(packet, 0, 1) as i64;
    let b: i64 = get(packet, 0, 2) as i64;
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    var ret: i64 = 0 as i64;

    if op == (0 as i64) {
        // min_p: a is p * 1e6, b is the candidate count
        ret = if a <= (0 as i64) || b == (0 as i64) {
            1 as i64
        } else {
            0 as i64
        }
        ;
    } else {
        if op == (1 as i64) {
            // typical: skipped at p >= 1
            ret = if a >= (1000000 as i64) {
                1 as i64
            } else {
                0 as i64
            }
            ;
        } else {
            if op == (2 as i64) {
                // top_p: skipped at p >= 1
                ret = if a >= (1000000 as i64) {
                    1 as i64
                } else {
                    0 as i64
                }
                ;
            } else {
                if op == (3 as i64) {
                    // top_k: skipped at k <= 0
                    ret = if a <= (0 as i64) {
                        1 as i64
                    } else {
                        0 as i64
                    }
                    ;
                } else {
                    ensure(false, 60);
                }
            }
        }
    }
    emit(out, cntp, 0 as i64, op, ret);
    put(packet, 2, 0, 1 as u64);
    put(packet, 2, 1, ret as u64);
    return true;
}

fn main() {
    run(0);
}

