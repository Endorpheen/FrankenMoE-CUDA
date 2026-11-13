// Apex CUDA argsort (bitonic) in Goose, from ggml-cuda/argsort.cu.
//
// Device side: shared index initialization, the bitonic (k, j) stages with a
// barrier per stage, the bitonic_step compare/swap with the padding rule
// (index >= ncols sorts to the end), the strict comparisons that leave ties in
// place, and the final copy without padding.
// Host side: ncols_pad = next_power_of_2(ncols), block = min(ncols_pad, 1024),
// grid = nrows, shared = ncols_pad*4, with a shared-memory limit check.
//
// Shared memory and barriers are explicit primitives; every comparison, index
// and swap decision is Goose.
extern "fg283d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg283d_st_i32" fn st_i32(dst: u64, o: u64, v: i32);
extern "fg283d_smem" fn smem_base() -> u64;
extern "fg283d_sload_i32" fn sload_i32(a: u64) -> i32;
extern "fg283d_sstore_i32" fn sstore_i32(a: u64, v: i32);
extern "fg283d_sync" fn sync();
extern "fg283d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// order: 0 = ASC, 1 = DESC.

fn bitstep(x: u64, dr: u64, ncols: i64, col: i64, k: i64, j: i64, order: i64) {
    let ixj: i64 = col ^ j;

    if ixj <= col as i64 {
        return;
    }
    let a: i64 = (sload_i32(dr + ( (col * 4 as i64) as u64))) as i64;
    let b: i64 = (sload_i32(dr + ( (ixj * 4 as i64) as u64))) as i64;
    var doswap: i64 = 0 as i64;

    if ( (col & k) == (0 as i64)) {
        if a >= ncols as i64 {
            doswap = 1 as i64;
        } else {
            if b < ncols as i64 {
                if order == (0 as i64) {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) > ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                } else {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) < ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                }
            }
        }
    } else {
        if b >= ncols as i64 {
            doswap = 1 as i64;
        } else {
            if a < ncols as i64 {
                if order == (0 as i64) {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) < ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                } else {
                    if ld_f32(x, ( (a * 4 as i64) as u64)) > ld_f32(x, ( (b * 4 as i64) as u64)) {
                        doswap = 1 as i64;
                    }
                }
            }
        }
    }

    if doswap != (0 as i64) {
        sstore_i32(dr + ( (col * 4 as i64) as u64), b as i32);
        sstore_i32(dr + ( (ixj * 4 as i64) as u64), a as i32);
    }
}

fn argsort(x: u64, dst: u64, ncols: i64, ncols_pad: i64, order: i64, blk: i64, tid: i64, row: i64) {
    ensure(ncols > 0 as i64 && ncols_pad >= ncols as i64, 20);
    ensure(blk > 0 as i64 && tid >= 0 as i64, 21);
    let dr: u64 = smem_base();
    var col: i64 = tid;

    while col < ncols_pad as i64 {
        sstore_i32(dr + ( (col * 4 as i64) as u64), col as i32);
        col = col + blk;
    }
    sync();
    var k: i64 = 2 as i64;

    while k <= ncols_pad as i64 {
        var j: i64 = k / 2 as i64;

        while j > 0 as i64 {
            col = tid;
            let xrow: u64 = x + ( ( (row * ncols) * 4 as i64) as u64);

            while col < ncols_pad as i64 {
                bitstep(xrow, dr, ncols, col, k, j, order);
                col = col + blk;
            }
            sync();
            j = j / 2 as i64;
        }
        k = k * 2 as i64;
    }
    col = tid;

    while col < ncols as i64 {
        let v: i32 = sload_i32(dr + ( (col * 4 as i64) as u64));
        st_i32(dst, ( ( (row * ncols + col) * 4 as i64) as u64), v);
        col = col + blk;
    }
}
// Retained entry; the parent never executes main.

fn main() {
    argsort(0, 0, 1 as i64, 1 as i64, 0 as i64, 1 as i64, 0 as i64, 0 as i64);
}

