export fn q4_attention_tiles_entry(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, scale: f32, task: i64, tile: i64, lane: i64, query_base: i64) {
    fg_attn_tiles(q, k, v, mask, scratch, cfg, scale, task, query_base + task, tile, lane);
}

export fn q4_attention_merge_entry(scratch: u64, output: u64, cfg: u64, task: i64, lane: i64, query_base: i64) {
    fg_attn_merge(scratch, output, cfg, task, query_base + task, lane);
}
