// CUDA row reduction in Goose, from ggml-cuda/reduce_rows.cuh.
//
// The native kernel has two association-sensitive layers and both are
// reproduced exactly: eight per-thread accumulators are summed in index order,
// then a five-step __shfl_xor_sync tree reduces each warp, and blocks wider
// than one warp stage their warp sums in shared memory and run the same tree
// again. Changing either order changes the float result, which the fixture
// compares bit for bit.
//
// Pointers arrive as base plus byte offset. Only loads, stores, the shuffle,
// the barrier and the failure trap are extern primitives; every index,
// arithmetic operation and the norm decision are Goose.
extern "fg414d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg414d_st_f32" fn st_f32(x: u64, o: u64, v: f32);
extern "fg414d_shfl_xor_f32" fn shfl_xor_f32(v: f32, m: i64) -> f32;
extern "fg414d_sync" fn sync();
extern "fg414d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn warp_sum(v: f32) -> f32 {
    var x: f32 = v;
    x = x + shfl_xor_f32(x, 16 as i64);
    x = x + shfl_xor_f32(x, 8 as i64);
    x = x + shfl_xor_f32(x, 4 as i64);
    x = x + shfl_xor_f32(x, 2 as i64);
    x = x + shfl_xor_f32(x, 1 as i64);
    return x;
}

fn block_sum(v: f32, tx: i64, bdx: i64, shared: u64) -> f32 {
    var x: f32 = warp_sum(v);

    if bdx > (32 as i64) {
        let warp: i64 = tx / (32 as i64);
        let lane: i64 = tx % (32 as i64);

        if lane == (0 as i64) {
            st_f32(shared, (warp * (4 as i64)) as u64, x);
        }
        sync();
        var y: f32 = 0.0;

        if lane < (bdx / (32 as i64)) {
            y = ld_f32(shared, (lane * (4 as i64)) as u64);
        }
        x = warp_sum(y);
    }
    return x;
}

fn reduce_row(x: u64, dst: u64, row: i64, ncols: i64, norm: i64, tx: i64, bdx: i64, shared: u64) -> i64 {
    ensure(ncols > (0 as i64), 20);
    var i: i64 = tx;
    var a0: f32 = 0.0;
    var a1: f32 = 0.0;
    var a2: f32 = 0.0;
    var a3: f32 = 0.0;
    var a4: f32 = 0.0;
    var a5: f32 = 0.0;
    var a6: f32 = 0.0;
    var a7: f32 = 0.0;

    while i < ncols {
        var t0: f32 = 0.0;
        var t1: f32 = 0.0;
        var t2: f32 = 0.0;
        var t3: f32 = 0.0;
        var t4: f32 = 0.0;
        var t5: f32 = 0.0;
        var t6: f32 = 0.0;
        var t7: f32 = 0.0;

        if i < ncols {
            t0 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t1 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t2 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t3 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t4 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t5 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t6 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;

        if i < ncols {
            t7 = ld_f32(x, (i * (4 as i64)) as u64);
        }
        i = i + bdx;
        a0 = a0 + t0;
        a1 = a1 + t1;
        a2 = a2 + t2;
        a3 = a3 + t3;
        a4 = a4 + t4;
        a5 = a5 + t5;
        a6 = a6 + t6;
        a7 = a7 + t7;
    }
    var s: f32 = a0 + a1;
    s = s + a2;
    s = s + a3;
    s = s + a4;
    s = s + a5;
    s = s + a6;
    s = s + a7;
    s = block_sum(s, tx, bdx, shared);

    if tx == (0 as i64) {
        if norm != (0 as i64) {
            st_f32(dst, (row * (4 as i64)) as u64, s / (ncols as f32));
        } else {
            st_f32(dst, (row * (4 as i64)) as u64, s);
        }
    }
    return 1 as i64;
}

fn reduce_entry(
    base: u64,
    dst: u64,
    row: i64,
    ncols: i64,
    norm: i64,
    strided: i64,
    ne1: i64,
    ne2: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    tx: i64,
    bdx: i64,
    shared: u64) -> i64 {
    ensure(ncols > (0 as i64), 21);
    ensure(ne1 > (0 as i64) && ne2 > (0 as i64), 22);
    var x: u64 = base;

    if strided != (0 as i64) {
        let i1: i64 = row % ne1;
        let i2: i64 = (row / ne1) % ne2;
        let i3: i64 = row / (ne1 * ne2);
        x = base + ( (i3 * nb3 + i2 * nb2 + i1 * nb1) as u64);
    } else {
        x = base + ( (row * ncols * (4 as i64)) as u64);
    }
    return reduce_row(x, dst, row, ncols, norm, tx, bdx, shared);
}

fn main() {
    reduce_entry(
        0 as u64,
        0 as u64,
        0 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        1 as i64,
        1 as i64,
        4 as i64,
        4 as i64,
        4 as i64,
        0 as i64,
        1 as i64,
        0 as u64);
}

