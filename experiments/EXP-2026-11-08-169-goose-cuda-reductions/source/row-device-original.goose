// fg169r row-reductions device module: regular F32 row-sum/row-mean backend migrated from
// row-reductions/inputs/reduce_rows.cuh (reduce_rows_f32<norm> and reduce_rows_f32_strided<norm>).
// Native effects are limited to F32 load/store, F32 shuffle, barrier and PDL grid sync; all arithmetic,
// indexing, the eight-accumulator reduction and the mean division run in Goose on i64 indices over
// raw u64 element pointers. The native CUB device-wide mean branch is NOT migrated (see contract.json).
extern "fg169r_load" fn load(p: u64, i: u64) -> f32;
extern "fg169r_store" fn store(p: u64, i: u64, v: f32);
extern "fg169r_shuffle" fn shuffle(v: f32, offset: i32) -> f32;
extern "fg169r_barrier" fn barrier();
extern "fg169r_sync" fn sync();
// Warp phase of native block_reduce<block_reduce_method::SUM,float>: butterfly over offsets 16,8,4,2,1.

fn rr_warp(v: f32) -> f32 {
    var value = v;
    var shift: i32 = 16;

    while shift > 0 {
        value += shuffle(value, shift);
        shift >>= 1;
    }
    return value;
}
// Native block_reduce order: warp phase, lane0 of each warp writes shared_vals[warp_id], barrier,
// sentinel 0.0f, lanes < bs/32 reload partials, second warp phase. shared holds native shared_vals[32];
// only the first bs/32 entries are touched and no caller reuses shared before that phase completes.

fn rr_block_sum(v: f32, shared: u64, tid: i32, bs: i32) -> f32 {
    var value = rr_warp(v);

    if bs > 32 {
        let lane = tid % 32;

        if lane == 0 {
            store(shared, (tid / 32) as u64, value);
        }
        barrier();
        value = 0.0;

        if lane < bs / 32 {
            value = load(shared, lane as u64);
        }
        value = rr_warp(value);
    }
    return value;
}
// Per-row reduction with the native eight independent accumulators: local f32[8] temp/sum_temp POD
// arrays (no heap, no slices), guarded strided gather (i<ncols else exact 0.0), per-accumulator adds
// in ascending j after each full gather pass, final fold sum=0.0; sum+=sum_temp[j] for j=0..7.
// fg169r_sync fires once before the first load, exactly like ggml_cuda_pdl_sync() in reduce_row_f32.

fn rr_reduce_row(x: u64, ncols: i64, tid: i32, bs: i32, shared: u64) -> f32 {
    sync();
    var temp: f32[8] = default < f32[8] > ();
    var sum_temp: f32[8] = default < f32[8] > ();
    var i: i64 = tid as i64;

    while i < ncols {
        var j: i32 = 0;

        while j < 8 {
            temp[j] = if i < ncols {
                load(x, i as u64)
            } else {
                0.0
            }
            ;
            i += bs as i64;
            j += 1;
        }
        j = 0;

        while j < 8 {
            sum_temp[j] = sum_temp[j] + temp[j];
            j += 1;
        }
    }
    var sum: f32 = 0.0;
    var k: i32 = 0;

    while k < 8 {
        sum += sum_temp[k];
        k += 1;
    }
    sum = rr_block_sum(sum, shared, tid, bs);
    return sum;
}
// Contiguous row-sum (reduce_rows_f32<false>): one block per row, row base = x + row*ncols in i64,
// only lane 0 stores dst[row].

fn rr_sum_rows_f32(x: u64, dst: u64, ncols: i64, row: i64, tid: i32, bs: i32, shared: u64) {
    let base = x + ( (row * ncols) as u64);
    let sum = rr_reduce_row(base, ncols, tid, bs, shared);

    if tid != 0 {
        return;
    }
    store(dst, row as u64, sum);
}
// Contiguous row-mean (reduce_rows_f32<true>): single f32 division by ncols after the full reduction.

fn rr_mean_rows_f32(x: u64, dst: u64, ncols: i64, row: i64, tid: i32, bs: i32, shared: u64) {
    let base = x + ( (row * ncols) as u64);
    let sum = rr_reduce_row(base, ncols, tid, bs, shared);

    if tid != 0 {
        return;
    }
    store(dst, row as u64, sum / (ncols as f32));
}
// Higher-strided row addressing mirroring reduce_rows_f32_strided: row -> (i1,i2,i3) decomposition in
// i64 with byte strides nb1..nb3. Row byte offsets must be divisible by 4 (native reinterprets char*
// to float* under the same requirement); the port divides by 4 for raw element-indexed load/store.

fn rr_strided_row_base(x: u64, row: i64, ne1: i64, ne2: i64, nb1: i64, nb2: i64, nb3: i64) -> u64 {
    let i1 = row % ne1;
    let i2 = (row / ne1) % ne2;
    let i3 = row / (ne1 * ne2);
    let bytes = (i1 * nb1 + i2 * nb2 + i3 * nb3) as u64;
    return x + (bytes / 4);
}

fn rr_sum_rows_f32_strided(
    x: u64,
    dst: u64,
    ncols: i64,
    ne1: i64,
    ne2: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    row: i64,
    tid: i32,
    bs: i32,
    shared: u64) {
    let base = rr_strided_row_base(x, row, ne1, ne2, nb1, nb2, nb3);
    let sum = rr_reduce_row(base, ncols, tid, bs, shared);

    if tid != 0 {
        return;
    }
    store(dst, row as u64, sum);
}

fn rr_mean_rows_f32_strided(
    x: u64,
    dst: u64,
    ncols: i64,
    ne1: i64,
    ne2: i64,
    nb1: i64,
    nb2: i64,
    nb3: i64,
    row: i64,
    tid: i32,
    bs: i32,
    shared: u64) {
    let base = rr_strided_row_base(x, row, ne1, ne2, nb1, nb2, nb3);
    let sum = rr_reduce_row(base, ncols, tid, bs, shared);

    if tid != 0 {
        return;
    }
    store(dst, row as u64, sum / (ncols as f32));
}
// Compile-time reachability stub calling every entry with zero/dummy scalars; never executed on host.

fn main() {
    rr_sum_rows_f32(0 as u64, 0 as u64, 0, 0, 0, 32, 0 as u64);
    rr_mean_rows_f32(0 as u64, 0 as u64, 0, 0, 0, 32, 0 as u64);
    rr_sum_rows_f32_strided(0 as u64, 0 as u64, 0, 1, 1, 0, 0, 0, 0, 0, 32, 0 as u64);
    rr_mean_rows_f32_strided(0 as u64, 0 as u64, 0, 1, 1, 0, 0, 0, 0, 0, 32, 0 as u64);
}

