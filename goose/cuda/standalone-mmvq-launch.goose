extern "mmvq_job_word" fn mmvq_launch_job(jobs: u64, index: u64) -> u64;

export fn mmvq_launch_matvec(weights: u64, packed: u64, output: u64, jobs: u64,
    stride: u64, extent: i64, kind: i64, words: u64, row: u64, route: u64, thread: i64, shared: u64) {
    let job = route * words;
    let plane = if words == 3 { mmvq_launch_job(jobs, job + 2) } else { 0 };
    fg_mmvq_row(weights + plane + row * stride, packed + route * (extent as u64) * 288,
        output + mmvq_launch_job(jobs, job + 1) + row * 4, shared, extent, kind, thread);
}
