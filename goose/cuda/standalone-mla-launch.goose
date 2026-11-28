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

export fn giga_mla_launch_visible(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64,
    scores: u64, scale: f32, query_base: i64, task: i64, tile: i64, thread: i64, visible: u64) {
    let first = 8 * task;
    let query = query_base + first;
    let tiles = attn_word(cfg, 17);
    let index = ((query % attn_word(cfg, 2)) / 8) * tiles + tile;
    if attn_load(visible + ((4 * index) as u64)) == 0.0 {
        giga_mla_tile_empty(scratch, first, tiles, tile, thread);
        return;
    }
    giga_mla_mma_pv_tile(q, k, v, mask, scratch, cfg, scores, scale, first, query, tile, thread / 32, thread % 32);
}

export fn giga_mla_launch_compact(mask: u64, keys: u64, lengths: u64, cfg: u64, group: i64, thread: i64) {
    let query = group * 4 + thread / 32;
    if query < attn_word(cfg, 2) { giga_mla_compact_mask(mask, keys, lengths, cfg, query, thread % 32); }
}

export fn giga_mla_launch_indexed_merge(scratch: u64, output: u64, cfg: u64, query_base: i64, task: i64, lane: i64) {
    let positions = attn_word(cfg, 2);
    let absolute = query_base + task;
    let group = absolute / 8;
    let query = (8 * (group / positions) + absolute % 8) * positions + group % positions;
    giga_mla_indexed_merge(scratch, output, cfg, task, query, lane);
}

export fn giga_mla_mma_tile_entry(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, scores: u64, scale: f32, tile: i64, task: i64, query_base: i64, thread: i64) {
    giga_mla_mma_tile(q, k, v, mask, scratch, cfg, scores, scale, 8 * task, query_base + 8 * task, tile, thread / 32, thread % 32);
}

export fn giga_mla_mma_pv_tile_entry(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, scores: u64, scale: f32, tile: i64, task: i64, query_base: i64, thread: i64) {
    giga_mla_mma_pv_tile(q, k, v, mask, scratch, cfg, scores, scale, 8 * task, query_base + 8 * task, tile, thread / 32, thread % 32);
}

export fn giga_mla_mma_pv_indexed_entry(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, scores: u64, scale: f32, tile: i64, keys: u64, task: i64, query_base: i64, thread: i64) {
    giga_mla_mma_pv_indexed(q, k, v, mask, scratch, cfg, scores, scale, 8 * task, query_base / 8 + task, tile, thread / 32, thread % 32, keys);
}

export fn giga_mask_clear_entry(mask: u64, out: u64, total: i64, prefix: i64, group: i64, lane: i64) {
    giga_device_mask_clear(mask, out, total, prefix, group * 256 + lane);
}

export fn giga_mask_scatter_entry(mask: u64, picks: u64, out: u64, cells: i64, count: i64, total: i64, prefix: i64, group: i64, lane: i64) {
    giga_device_mask_scatter(mask, picks, out, cells, count, total, prefix, group * 256 + lane);
}

export fn mla_attention_tiles_entry(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, scale: f32, task: i64, tile: i64, lane: i64, query_base: i64) {
    fg_mla_attn_tiles(q, k, v, mask, scratch, cfg, scale, task, query_base + task, tile, lane);
}

export fn mla_attention_pair_entry(q: u64, k: u64, v: u64, mask: u64, scratch: u64, cfg: u64, scale: f32, tile: i64, lane: i64, task: i64, query_base: i64) {
    fg_mla_attn_pair(q, k, v, mask, scratch, cfg, scale, 2 * task, query_base + 2 * task, tile, lane);
}

export fn mla_attention_merge_entry(scratch: u64, output: u64, cfg: u64, task: i64, lane: i64, query_base: i64) {
    fg_mla_attn_merge(scratch, output, cfg, task, query_base + task, lane);
}

export fn dsa_score_entry(q: u64, k: u64, weights: u64, mask: u64, out: u64, cfg: u64, token: i64, tile: i64, tid: i64) {
    fg_dsa_score(q, k, weights, mask, out, cfg, 4 * tile + tid / 32, token, tid % 32);
}

export fn giga_dsa_score_shared_q_entry(q: u64, k: u64, weights: u64, mask: u64, out: u64, cfg: u64, token: i64, tile: i64, tid: i64) {
    giga_dsa_score_shared_q(q, k, weights, mask, out, cfg, 16 * tile + 4 * (tid / 32), token, tid % 32);
}
