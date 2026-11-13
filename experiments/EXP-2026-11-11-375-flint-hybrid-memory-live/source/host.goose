// Apex hybrid memory decisions in Goose (llama_memory_hybrid).
//
// The hybrid cache is two caches that must always agree on a sequence's extent:
//   pos_min  the earliest position either half still holds  = max(attn_min, recr_min)
//   pos_max  the latest position both halves still hold     = min(attn_max, recr_max)
// and a failed read of the recurrent half must empty the attention half, so a sequence is never
// half-restored.
//
// Packet:
//   k=0  0 op, 1 a, 2 b
//   k=1  0 out
//   k=2  0 done, 1 ret
//
// Records: 0 decide (a = op, b = ret)
extern "fg375k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg375k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg375k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg375k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg375k_fail" fn fail(code: i64);

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
        // the earliest position either half holds
        ret = if a > b {
            a
        } else {
            b
        }
        ;
    } else {
        if op == (1 as i64) {
            // the latest position both halves hold
            ret = if a < b {
                a
            } else {
                b
            }
            ;
        } else {
            if op == (2 as i64) {
                // a sequence belongs to both halves or to neither
                let attn: i64 = a;
                let recr: i64 = b;
                let need_clear: i64 = if attn != (0 as i64) && recr == (0 as i64) {
                    1 as i64
                } else {
                    0 as i64
                }
                ;
                ret = need_clear;
            } else {
                ensure(false, 60);
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

