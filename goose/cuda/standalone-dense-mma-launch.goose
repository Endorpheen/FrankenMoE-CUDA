export fn dense_mma_launch(weights: u64, input: u64, output: u64, jobs: u64,
    stride: i64, extent: i64, rows: i64, count: i64, row_block: i64, token_block: i64,
    thread: i64, partitions: i64) {
    let row = 64 * row_block + 16 * (thread / 32);
    let first = 8 * token_block;
    let remaining = count - first;
    if row < rows {
        let active = if remaining < 8 { remaining } else { 8 };
        fg_device_dense_mma(weights, input, output, jobs + (16 * first) as u64,
            stride, extent, row, rows, active, thread % 32, partitions);
    }
}
