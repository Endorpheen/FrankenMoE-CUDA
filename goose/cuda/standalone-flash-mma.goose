extern "fg_attn_word" fn flash_word(config: u64, index: i64) -> i64;

// One block handles eight causal queries for eight heads sharing the same KV head.
export fn fg_flash_causal_mma(q: u64, k: u64, v: u64, out: u64, cfg: u64, scale: f32,
    tile: i32, group: i32, lane: i32, warp: i32, shared: u64) {
    let queries = flash_word(cfg, 2);
    let heads = flash_word(cfg, 3);
    let keys = flash_word(cfg, 6);
    let prefix = flash_word(cfg, 21);
    let end = ((tile as i64) + 1) * 8;
    let visible = prefix + (if end < queries { end } else { queries }) - 1;
    let padded = (keys + 31) / 32 * 32;
    let head = (group as i64) * 8;
    let kv_offset = ((group as i64) * padded * 512) as u64;
    process_tile_apex_nc8_np1(q + ((head * flash_word(cfg, 8)) as u64), k + kv_offset, v + kv_offset,
        prefix as u64, 0, out + ((head * 1024) as u64), 0, scale, 1.0, queries, heads as i32, 8,
        flash_word(cfg, 7) / 8, flash_word(cfg, 8) / 8, 128, 128, 0, tile, 0, 0,
        ((visible + 31) / 32) as i32, 8, 1, false, false, 0, 1, lane, warp, 0, 0, 0, 0, shared);
}
