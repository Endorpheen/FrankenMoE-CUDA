export fn fg_mla_launch_batch_pair(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64,
    scale: f32, query_base: i64, task: i64, tile: i64, lane: i64) {
    let positions = attn_word(cfg, 2);
    let pair = query_base / 2 + task;
    let first_query = (pair / positions) * 2 * positions + pair % positions;
    fg_mla_attn_batch_pair(q, k, v, mask, scratch, cfg, scale, 2 * task, first_query, tile, lane);
}

export fn fg_mla_launch_batch_merge(scratch: u64, output: u64, cfg: u64, query_base: i64, task: i64, lane: i64) {
    let positions = attn_word(cfg, 2);
    let slot = query_base + task;
    let pair = slot / 2;
    let query = (pair / positions * 2 + slot % 2) * positions + pair % positions;
    fg_mla_attn_merge(scratch, output, cfg, task, query, lane);
}
