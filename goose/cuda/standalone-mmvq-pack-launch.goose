extern "mmvq_job_word" fn mmvq_pack_job(jobs: u64, index: u64) -> u64;

export fn mmvq_launch_quantize(input: u64, packed: u64, jobs: u64, extent: i32,
    words: u64, block_index: i32, route: u64, thread: i32) {
    let width = (extent as i64) * 256;
    let divisor = MmvqFastDiv { 0 as u32, 0 as u32, 1 as u32 };
    let packed_offset = route * (extent as u64) * 288;
    fg178q_quant(input + mmvq_pack_job(jobs, route * words), packed + packed_offset,
        width, 0, 0, 0, width, 1, 1, divisor, block_index, 0, 0, thread, 128);
}
