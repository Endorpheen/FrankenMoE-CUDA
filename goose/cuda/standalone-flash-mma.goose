extern "fg_attn_word" fn flash_word(config: u64, index: i64) -> i64;

fn flash_causal_part(q: u64, k: u64, v: u64, out: u64, fix: u64, cfg: u64, scale: f32,
    tile: i32, group: i32, start: i32, stop: i32, needs_fixup: bool, is_fixup: bool,
    worker: i32, workers: i32, lane: i32, warp: i32, shared: u64) {
    let queries = flash_word(cfg, 2);
    let heads = flash_word(cfg, 3);
    let keys = flash_word(cfg, 6);
    let prefix = flash_word(cfg, 21);
    let head = (group as i64) * 8;
    let kv_offset = ((group as i64) * ((keys + 31) / 32) * 16384) as u64;
    let columns: i32 = if queries == 4 { 4 } else { 8 };
    let value_partitions: i32 = if columns == 4 { 2 } else { 1 };
    process_tile(q + ((head * flash_word(cfg, 8)) as u64), k + kv_offset, v + kv_offset,
        prefix as u64, 0, out + ((head * 1024) as u64), fix, scale, 1.0, queries, heads as i32, 8,
        flash_word(cfg, 7) / 8, flash_word(cfg, 8) / 8, 128, 128, 0, tile, 0, start, stop,
        columns, value_partitions, needs_fixup, is_fixup, worker, workers, lane, warp, 0, 0, 0, 0, shared);
}

// One block handles eight causal queries for eight heads sharing the same KV head.
export fn fg_flash_causal_mma(q: u64, k: u64, v: u64, out: u64, cfg: u64, scale: f32,
    tile: i32, group: i32, lane: i32, warp: i32, shared: u64) {
    let queries = flash_word(cfg, 2);
    let prefix = flash_word(cfg, 21);
    let end = ((tile as i64) + 1) * 8;
    let visible = prefix + (if end < queries { end } else { queries }) - 1;
    flash_causal_part(q, k, v, out, 0, cfg, scale, tile, group, 0, ((visible + 31) / 32) as i32,
        false, false, 0, 1, lane, warp, shared);
}

// Partition the flattened KV tiles among workers. A split writes its
// trailing partial to the existing fixup layout. Its final worker owns out.
export fn fg_flash_stream_mma(q: u64, k: u64, v: u64, out: u64, fix: u64, cfg: u64, scale: f32,
    worker: i32, workers: i32, lane: i32, warp: i32, shared: u64) {
    let queries = flash_word(cfg, 2);
    let tiles_per_head = (queries + 7) / 8;
    let key_tiles = (flash_word(cfg, 6) + 31) / 32;
    let work = tiles_per_head * flash_word(cfg, 4) * key_tiles;
    var cursor = (worker as i64) * work / (workers as i64);
    let limit = ((worker as i64) + 1) * work / (workers as i64);
    while cursor < limit {
        let output = cursor / key_tiles;
        let tile = output % tiles_per_head;
        let start = cursor % key_tiles;
        let end = key_tiles - start;
        let remaining = limit - cursor;
        let length = if remaining < end { remaining } else { end };
        let stop = start + length;
        let partial = stop < key_tiles;
        let tile_end = (tile + 1) * 8;
        let visible = flash_word(cfg, 21) + (if tile_end < queries { tile_end } else { queries }) - 1;
        let bound = (visible + 31) / 32;
        // The tile preloads one KV block. For a wholly masked range one
        // valid, fully masked block emits the neutral partial for fixup.
        let capped = if stop < bound { stop } else { bound };
        let kv_end = if capped <= start { start + 1 } else { capped };
        flash_causal_part(q, k, v, out, fix, cfg, scale, tile as i32, (output / tiles_per_head) as i32,
            start as i32, kv_end as i32, !partial && start != 0, partial, worker, workers, lane, warp, shared);
        cursor += length;
        if cursor < limit { barrier(); }
    }
}
