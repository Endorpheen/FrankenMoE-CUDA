extern "fg_fwht_quantize_shared" fn quantize_shared(src: u64, sb: u64, dst: u64, db: u64);
extern "fg_fwht_warp_sync" fn warp_sync();

export fn fwht_q4_write(src: u64, dst: u64, scratch: u64, width: i32, rows: i64,
    scale: f32, row: i64, lane: i32) {
    if row >= rows { return; }
    fg183f_row(src + ((4 * row * width) as u64), scratch, width, 1, scale, 0, 1, 0, lane, 32);
    warp_sync();
    let blocks = width / 32;
    if lane < blocks {
        quantize_shared(scratch, (128 * lane) as u64, dst, (18 * (row * blocks + (lane as i64))) as u64);
    }
}
