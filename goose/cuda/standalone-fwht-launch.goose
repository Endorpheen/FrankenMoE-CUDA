export fn fwht_launch(src: u64, dst: u64, width: i32, rows: i64, scale: f32,
    block_index: i32, thread: i32) {
    fg183f_row(src, dst, width, rows, scale, block_index, 4, thread / 32, thread % 32, 32);
}

export fn fwht_launch_q4(src: u64, dst: u64, width: i32, rows: i64, scale: f32,
    block_index: i64, thread: i64, scratch: u64) {
    let warp = thread / 32;
    let workspace = scratch + 4 * (warp as u64) * (width as! u64);
    fwht_q4_write(src, dst, workspace, width, rows, scale, 4 * block_index + warp, (thread % 32) as i32);
}
