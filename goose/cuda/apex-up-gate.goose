extern "fg_device_shared_load" fn read_shared(address: u64) -> f32;
extern "fg_device_shared_store" fn write_shared(address: u64, value: f32);
extern "fg_device_global_store" fn write_output(address: u64, value: f32);
extern "fg_device_barrier" fn barrier();
extern "fg_vector_swiglu_reference_g" fn swiglu(gate: f32, up: f32) -> f32;
extern "apex_up_gate_word" fn job_word(jobs: u64, index: i64) -> u64;

export fn apex_up_gate_reduce(up: u64, gate: u64, input: u64, output: u64, extent: i64, thread: i64, sums: u64) {
    let lane = thread % 32;
    let warp = thread / 32;
    var u = fg185q_partial(up, input, 0, 0, extent, 8, thread);
    var g = fg185q_partial(gate, input, 0, 0, extent, 8, thread);
    if warp != 0 {
        let offset = ((32 * (warp - 1) + lane) * 4) as! u64;
        write_shared(sums + offset, u);
        write_shared(sums + 384 + offset, g);
    }
    barrier();
    if warp == 0 {
        for index in 3 {
            let offset = ((32 * index + lane) * 4) as! u64;
            u += read_shared(sums + offset);
            g += read_shared(sums + 384 + offset);
        }
        u = warp_sum(u);
        g = warp_sum(g);
        if lane == 0 { write_output(output, swiglu(g, u)); }
    }
}

export fn apex_up_gate_launch(weights: u64, packed: u64, output: u64, up_jobs: u64, gate_jobs: u64,
    stride: u64, extent: i64, row: i64, route: i64, thread: i64, sums: u64) {
    let row_offset = (row as! u64) * stride;
    let input = packed + ((route * extent * 288) as! u64);
    let job = 3 * route;
    apex_up_gate_reduce(weights + job_word(up_jobs, job + 2) + row_offset,
        weights + job_word(gate_jobs, job + 2) + row_offset, input,
        output + job_word(up_jobs, job + 1) + ((4 * row) as! u64), extent, thread, sums);
}

export fn apex_up_gate_shared(up: u64, gate: u64, packed: u64, output: u64, stride: u64,
    extent: i64, row: u64, thread: i64, sums: u64) {
    let offset = row * stride;
    apex_up_gate_reduce(up + offset, gate + offset, packed, output + row * 4, extent, thread, sums);
}
