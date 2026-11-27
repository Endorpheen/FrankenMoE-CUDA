// CUDA transposed CPY tile in Goose (F32), from ggml-cuda/cpy.cu
// cpy_scalar_transpose<float>.
//
// A 32x32 tile is loaded from a (ne00 x ne01) matrix, staged through a
// double-buffered shared float[32][33] array, then written transposed. The
// wrapper owns the shared array; Goose owns every index and the loop over eight
// matrices per block.
extern "fg434c_ld_f32" fn ld_f32(p: u64, o: u64) -> f32;
extern "fg434c_st_f32" fn st_f32(p: u64, o: u64, v: f32);
extern "fg434c_sync" fn sync();
extern "fg434c_fail" fn fail(c: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

// Stage one 32-by-32 tile with a padded shared stride for the transpose.
fn transpose_tile(src: u64, dst: u64, columns: i64, rows: i64,
    src_pitch: i64, dst_pitch: i64, bx: i64, by: i64, tx: i64, ty: i64, scratch: u64) {
    let sx = bx * 32 + tx;
    let sy = by * 32 + ty;
    let dx = by * 32 + tx;
    let dy = bx * 32 + ty;
    for group in 4 {
        let row = sy + 8 * group;
        if sx < columns && row < rows {
            let tile = ((ty + 8 * group) * 33 + tx) as u64;
            let offset = (4 * (row * src_pitch + sx)) as u64;
            st_f32(scratch, 4 * tile, ld_f32(src, offset));
        }
    }
    sync();
    for group in 4 {
        let row = dy + 8 * group;
        if dx < rows && row < columns {
            let tile = (tx * 33 + ty + 8 * group) as u64;
            let offset = (4 * (row * dst_pitch + dx)) as u64;
            st_f32(dst, offset, ld_f32(scratch, 4 * tile));
        }
    }
}

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
