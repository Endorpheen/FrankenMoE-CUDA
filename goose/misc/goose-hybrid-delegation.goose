// Apex hybrid memory delegation in Goose (llama_memory_hybrid state_write / state_read).
//
// The hybrid cache owns two halves that must be visited in a fixed order:
//   write  attention first, recurrent second, so a partial write is visible as an attention-only state
//   read   attention first as well; if the recurrent half then fails, the attention half is cleared, so
//          a sequence is never left half-restored
//
// Packet:
//   k=0  0 op, 1 a
//   k=1  0 out
//   k=2  0 done, 1 ret
//
// Records: 0 decide (a = op, b = ret)
extern "fg397k_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg397k_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg397k_ld_i64" fn ld(base: u64, byte: i64) -> i64;
extern "fg397k_stw" fn stw(base: u64, byte: i64, v: i64);
extern "fg397k_fail" fn fail(code: i64);

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
    let out: u64 = get(packet, 1, 0);
    let cntp: u64 = get(packet, 2, 0);
    var ret: i64 = 0 as i64;

    if op == (0 as i64) {
        // step 0 is always the attention half, step 1 the recurrent half
        ret = if a == (0 as i64) {
            1 as i64
        } else {
            0 as i64
        }
        ;
    } else {
        if op == (1 as i64) {
            // after a recurrent failure the attention half must be cleared: a = recurrent failed
            ret = if a != (0 as i64) {
                1 as i64
            } else {
                0 as i64
            }
            ;
        } else {
            ensure(false, 60);
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

