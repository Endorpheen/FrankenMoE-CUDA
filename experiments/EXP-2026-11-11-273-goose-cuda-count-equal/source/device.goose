// Apex CUDA count_equal device computation in Goose, from ggml-cuda/count-equal.cu.
//
// Scope: the per-chunk loop, the equality count, the 32-lane warp reduction and
// the atomic accumulation. The host launch plan (dne, blocks_num) is shared
// launch geometry and stays native; it is not part of this module.
//
// Everything algorithmic is Goose: i0/i1 chunk bounds, the WARP_SIZE-strided
// loop, the equality test, the integer warp reduction ladder and the final
// atomic add. Extern symbols are a 32-bit load, a shuffle, and an atomic add.
extern "fg273d_ld_i32" fn ld_i32(x: u64, o: u64) -> i32;
extern "fg273d_shfl_xor" fn shfl_xor(v: i32, m: i32) -> i32;
extern "fg273d_atomic_add" fn atomic_add(p: u64, v: i32);
extern "fg273d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One block of the native kernel: chunk [blk*dk, min(blk*dk+dk, k)), 32 threads.

fn count_equal(x: u64, y: u64, dst: u64, dk: i64, k: i64, blk: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let i0: i64 = blk * dk;
    var i1: i64 = i0 + dk;

    if i1 > k as i64 {
        i1 = k;
    }
    var ne: i64 = 0;
    var i: i64 = i0 + tid;

    while i < i1 {
        let xi: i32 = ld_i32(x, ( (i * 4 as i64) as u64));
        let yi: i32 = ld_i32(y, ( (i * 4 as i64) as u64));

        if xi == yi {
            ne = ne + 1 as i64;
        }
        i = i + 32 as i64;
    }
    var off: i64 = 16 as i64;

    while off >= 1 as i64 {
        let v: i32 = shfl_xor(ne as i32, off as i32);
        ne = ne + (v as i64);
        off = off / 2 as i64;
    }

    if tid == 0 as i64 {
        atomic_add(dst, ne as i32);
    }
}
// Retained entry; the parent never executes main.

fn main() {
    count_equal(0, 0, 0, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
}

