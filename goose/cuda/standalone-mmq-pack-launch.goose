extern "fg_mmq_index" fn mmq_launch_job(jobs: u64, index: i64) -> u64;

export fn mmq_pack_launch(input: u64, jobs: u64, packed: u64, extent: i32, layout: i32,
    first: i64, words: i64, group: i32, job: i32, lane: i32, columns: i32, input_stride: i64) {
    let index = if jobs != 0 { mmq_launch_job(jobs, (first + job) * words) } else { ((job as i64) * input_stride) as u64 };
    let width = 256 * (extent as i64);
    if columns != 0 {
        fg180q_mmq(input + index, 0, packed, width, 0, 0, 0, width, columns, 1,
            1, layout, 0, job, group, 0, lane, 32, columns, 2 * extent);
    } else {
        let output = packed + (((job as i64) * 288 * extent) as u64);
        fg180q_mmq(input + index, 0, output, width, 0, 0, 0, width, 1, 1,
            1, layout, 0, 0, group, 0, lane, 32, 1, 2 * extent);
    }
}

export fn mmq_pack_glu_launch(gate: u64, up: u64, jobs: u64, packed: u64,
    extent: i64, layout: i32, first: i64, words: i64, group: i64, job: i64, lane: i64) {
    let index = if jobs != 0 { mmq_launch_job(jobs, (first + job) * words) } else { (job * extent * 1024) as u64 };
    fg_mmq_glu_pack(gate + index, up + index, packed + ((job * 288 * extent) as u64), layout, group, lane);
}
