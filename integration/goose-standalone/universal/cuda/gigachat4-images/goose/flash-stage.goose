struct F2 { x: f32, y: f32 }

extern "fg_flash_unpack" fn flash_unpack(address: u64, pair: i64) -> F2;
extern "fg_flash_half_store" fn flash_half_store(address: u64, value: f32);
extern "fg_flash_mask_half" fn mask_half(dst: u64, bits: u32);

export fn fg_flash_stage(src: u64, dst: u64, keys: i64, padded: i64, row_stride: i64, head_stride: i64,
    row: i64, head: i64, lane: i64) {
    let group = lane / 16;
    let pair = lane % 16;
    let value = if row < keys {
        flash_unpack(src + ((head * head_stride + row * row_stride + group * 18) as u64), pair)
    } else { F2 { 0.0, 0.0 } };
    let base = dst + ((512 * (head * padded + row) + 2 * (group * 32 + pair)) as u64);
    flash_half_store(base, value.x);
    flash_half_store(base + 32, value.y);
}

// Same [query][32 keys + 8 padding] shared tile as the dense mask loader.
export fn fg_flash_load_causal(first_keys: u64, dst: u64, key0: i64, query0: i64,
    queries: i64, cols: i32, lane: i32, warp: i32) {
    let query = warp * 8 + lane / 4;
    if query >= cols { return; }
    let count = (first_keys as i64) + (query0 + (query as i64)) % queries;
    let first = 8 * (lane % 4);
    for j in 8 {
        let bits: u32 = if key0 + (first as i64) + j < count { 0 } else { 0xfc00 };
        mask_half(dst + ((80 * (query as i64) + 2 * (first as i64) + 2 * j) as u64), bits);
    }
}
