export fn q4_set_rows_launch(src: u64, dst: u64, stride: u64, blocks: u64, tasks: u64, group: u64, lane: u64) {
    let work = group * 128 + lane;
    if work < tasks {
        fg176r_quant_q4_0(src, (work / blocks) * stride + (work % blocks) * 128, dst, work * 18);
    }
}
