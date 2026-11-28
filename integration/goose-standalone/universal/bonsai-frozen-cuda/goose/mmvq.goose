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

// At most two quant blocks fit in the first warp of the original row kernel.
export fn fg_mmvq_short_row(weights: u64, packed: u64, out: u64,
                            extent: i64, kind: i64, lane: i64) {
    let partial = if kind == 12 { mmvq_q4_partial(weights, packed, 0, 0, extent, 8, lane) }
        else { mmvq_q6_partial(weights, packed, 0, 0, extent, 8, lane) };
    // The other three warps of the original kernel each contribute +0.
    let sum = mmvq_warp_sum(partial + 0.0);
    if lane == 0 { mmvq_store(out, sum); }
}

extern "mmvq_job_word" fn bonsai_mmvq_job(jobs: u64, index: u64) -> u64;

export fn bonsai_mmvq_short(weights: u64, packed: u64, output: u64, jobs: u64,
    stride: u64, extent: i64, kind: i64, words: u64, block_index: u64, route: u64, thread: u64) {
    let row = block_index * 4 + thread / 32;
    let job = route * words;
    let plane = if words == 3 { bonsai_mmvq_job(jobs, job + 2) } else { 0 };
    fg_mmvq_short_row(weights + plane + row * stride, packed + route * (extent as u64) * 288,
        output + bonsai_mmvq_job(jobs, job + 1) + row * 4, extent, kind, (thread % 32) as i64);
}

export fn bonsai_mmvq_dense(weights: u64, packed: u64, output: u64,
    stride: u64, extent: i64, kind: i64, row: u64, thread: i64, shared: u64) {
    fg_mmvq_row(weights + row * stride, packed, output + row * 4, shared, extent, kind, thread);
}
