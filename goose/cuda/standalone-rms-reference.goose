extern "fg_vector_word" fn rms_word(cfg: u64, index: i64) -> i64;
extern "fg_rms_shared_load" fn rms_shared_load(address: u64, offset: u64) -> f32;
extern "fg_rms_shared_store" fn rms_shared_store(address: u64, offset: u64, value: f32);

export fn fg_reference_rms(x: u64, weights: u64, out: u64, cfg: u64, eps: f32, shared: u64,
    row: i64, thread: i32, lanes: i32, vectors: i32) {
    let width = rms_word(cfg, 2);
    let pos = row * width;
    let start = rms_offset(cfg, pos, 6);
    let dst = out + ((4 * pos) as u64);
    let weighted = weights != 0;
    let m = if weighted { weights + (rms_offset(cfg, pos, 14) as u64) } else { 0 };
    let cols = if weighted { rms_word(cfg, 14) as i32 } else { 1 };
    rms_row(x + (start as u64), dst, shared, m, 0, width as i32, thread, lanes,
        vectors, vectors > 0, weighted, false, false, cols, 1, eps, 1.0);
}
