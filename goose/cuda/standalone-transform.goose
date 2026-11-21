extern "fg_attn_word" fn blit_word(cfg: u64, index: i64) -> i64;
extern "fg_device_copy_u32" fn blit_copy(src: u64, dst: u64);

export fn fg_tensor_blit(x: u64, y: u64, dst: u64, cfg: u64, index: i64) {
    if index >= blit_word(cfg, 2) { return; }
    let axis = blit_word(cfg, 3);
    let left = blit_word(cfg, 4);
    var input = x;
    var stride_word: i64 = 9;
    if y != 0 {
        var axis_span: i64 = 1;
        for i in axis { axis_span *= blit_word(cfg, 5 + i); }
        if (index / axis_span) % blit_word(cfg, 5 + axis) >= left {
            input = y;
            stride_word = 13;
        }
    }
    var rest = index;
    var offset: i64 = 0;
    for i in 4 {
        let extent = blit_word(cfg, 5 + i);
        var coord = rest % extent;
        rest /= extent;
        if stride_word == 13 && i == axis { coord -= left; }
        offset += coord * blit_word(cfg, stride_word + i);
    }
    blit_copy(input + (offset as u64), dst + ((4 * index) as u64));
}

export fn fg_tensor_transpose_concat(history: u64, projection: u64, dst: u64, cfg: u64,
    bx: i64, by: i64, lane: i64, warp: i64, scratch: u64) {
    let prefix = blit_word(cfg, 4);
    let width = blit_word(cfg, 5);
    let channels = blit_word(cfg, 6);
    let input_pitch = blit_word(cfg, 13) / 4;
    transpose_tile(projection, dst + ((4 * prefix) as u64), channels, width - prefix,
        input_pitch, width, bx, by, lane, warp, scratch);
    let channel = bx * 32 + lane;
    if by == 0 && warp < prefix && channel < channels {
        let from = (channel * blit_word(cfg, 10) + 4 * warp) as u64;
        let into = (4 * (channel * width + warp)) as u64;
        blit_copy(history + from, dst + into);
    }
}
