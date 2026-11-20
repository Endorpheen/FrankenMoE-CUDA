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
