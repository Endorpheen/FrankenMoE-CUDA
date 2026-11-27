extern "fg185q_partial_g" fn mmvq_q4_partial(weights: u64, packed: u64, ybase: i64, row_block: i64,
    blocks_per_row: i64, blocks_per_iter: i64, tid: i64) -> f32;
extern "fg186q_partial_g" fn mmvq_q6_partial(weights: u64, packed: u64, ybase: i64, row_block: i64,
    blocks_per_row: i64, blocks_per_iter: i64, tid: i64) -> f32;
extern "warp_sum_g" fn mmvq_warp_sum(value: f32) -> f32;
extern "fg_device_shared_load" fn mmvq_shared_load(address: u64) -> f32;
extern "fg_device_shared_store" fn mmvq_shared_store(address: u64, value: f32);
extern "fg_device_global_store" fn mmvq_store(address: u64, value: f32);
extern "fg_device_barrier" fn mmvq_barrier();

export fn fg_mmvq_row(weights: u64, packed: u64, out: u64, shared: u64, extent: i64, kind: i64, tid: i64) {
    let lane = tid % 32;
    let warp = tid / 32;
    var sum = if kind == 12 { mmvq_q4_partial(weights, packed, 0, 0, extent, 8, tid) }
        else { mmvq_q6_partial(weights, packed, 0, 0, extent, 8, tid) };
    if warp > 0 {
        mmvq_shared_store(shared + ((4 * ((warp - 1) * 32 + lane)) as u64), sum);
    }
    mmvq_barrier();
    if warp == 0 {
        for i in 3 {
            sum += mmvq_shared_load(shared + ((4 * (i * 32 + lane)) as u64));
        }
        sum = mmvq_warp_sum(sum);
        if lane == 0 { mmvq_store(out, sum); }
    }
}
