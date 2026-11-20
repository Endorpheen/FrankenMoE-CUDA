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

fn sort_ordered(a: f32, b: f32, order: i64) -> bool {
    return if order == 0 { a <= b } else { a >= b };
}
// order: 0 = ASC, 1 = DESC.

fn bitstep(x: u64, dr: u64, ncols: i64, col: i64, k: i64, j: i64, order: i64, stable: bool) {
    let ixj: i64 = col ^ j;

    if ixj <= col as i64 {
        return;
    }
    let a: i64 = (sload_i32(dr + ( (col * 4 as i64) as u64))) as i64;
    let b: i64 = (sload_i32(dr + ( (ixj * 4 as i64) as u64))) as i64;
    var doswap: i64 = 0 as i64;

    if stable {
        let low = if (col & k) == 0 { a } else { b };
        let high = if (col & k) == 0 { b } else { a };
        if low >= ncols || high >= ncols {
            if low > high { doswap = 1; }
        } else {
            let left = ld_f32(x, (low * 4) as u64);
            let right = ld_f32(x, (high * 4) as u64);
            if !sort_ordered(left, right, order) || (left == right && low > high) { doswap = 1; }
        }
    } else if ( (col & k) == (0 as i64)) {
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

fn argsort_row(x: u64, dst: u64, ncols: i64, ncols_pad: i64, nout: i64, order: i64,
               blk: i64, tid: i64, dr: u64, nan_flag: u64) {
    ensure(ncols > 0 as i64 && ncols_pad >= ncols as i64, 20);
    ensure(blk > 0 as i64 && tid >= 0 as i64, 21);
    let stable = nan_flag != 0;
    var col: i64 = tid;
    var found_nan: i32 = 0;

    while col < ncols_pad as i64 {
        sstore_i32(dr + ( (col * 4 as i64) as u64), col as i32);
        if stable && col < ncols {
            let value = ld_f32(x, (4 * col) as u64);
            if value != value { found_nan = 1; }
        }
        col = col + blk;
    }
    if stable { sstore_i32(nan_flag + ((4 * tid) as u64), found_nan); }
    sync();
    if stable {
        if tid == 0 {
            var found: i32 = 0;
            for i in blk { found |= sload_i32(nan_flag + ((4 * i) as u64)); }
            sstore_i32(nan_flag, found);
        }
        sync();
        found_nan = sload_i32(nan_flag);
    }
    if stable && found_nan != 0 {
        // Insertion order is observable for unordered values in the CPU graph.
        if tid == 0 {
            for i in ncols {
                var j = i;
                while j > 0 {
                    let at = dr + ((4 * j) as u64);
                    let a = sload_i32(at - 4);
                    let b = sload_i32(at);
                    if sort_ordered(ld_f32(x, (4 * (a as i64)) as u64),
                                    ld_f32(x, (4 * (b as i64)) as u64), order) { break; }
                    sstore_i32(at - 4, b);
                    sstore_i32(at, a);
                    j -= 1;
                }
            }
        }
        sync();
    } else {
        var k: i64 = 2;
        while k <= ncols_pad {
            var j = k / 2;
            while j > 0 {
                col = tid;
                while col < ncols_pad {
                    bitstep(x, dr, ncols, col, k, j, order, stable);
                    col += blk;
                }
                sync();
                j /= 2;
            }
            k *= 2;
        }
    }
    col = tid;

    while col < nout {
        let v: i32 = sload_i32(dr + ( (col * 4 as i64) as u64));
        st_i32(dst, (4 * col) as u64, v);
        col = col + blk;
    }
}

fn argsort(x: u64, dst: u64, ncols: i64, ncols_pad: i64, order: i64, blk: i64, tid: i64, row: i64) {
    let base = (row * ncols * 4) as u64;
    argsort_row(x + base, dst + base, ncols, ncols_pad, ncols, order, blk, tid, smem_base(), 0);
}
// Retained entry; the parent never executes main.

fn main() {
    argsort(0, 0, 1 as i64, 1 as i64, 0 as i64, 1 as i64, 0 as i64, 0 as i64);
}

