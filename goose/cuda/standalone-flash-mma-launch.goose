export fn flash_launch_uniform(out: u64, fix: u64, queries: i32, heads: i32, workers: i32,
    blocks: i32, tile: i32, query: i32, head: i32, lane: i32) {
    let columns: i32 = if queries == 4 { 4 } else { 8 };
    let per_head = ((queries as i64) + columns - 1) / columns;
    let groups = heads / 8;
    flash_fixup_uniform(out, fix, queries, heads, groups, workers, 8, blocks,
        per_head * groups, per_head, per_head, columns, 8, tile, query, head, lane);
}

export fn flash_launch_general(out: u64, fix: u64, queries: i32, heads: i32, keys: i32,
    worker: i32, query: i32, head: i32, workers: i32, lane: i32) {
    let key_tiles = ((keys as i64) + 31) / 32;
    let columns: i32 = if queries == 4 { 4 } else { 8 };
    let per_head = (((queries as i64) + columns - 1) / columns) * key_tiles;
    let work = per_head * (heads / 8);
    flash_fixup_general(out, fix, queries, heads, 8, work as i32,
        work, per_head, per_head, key_tiles, columns, 8, worker, query, head, workers, lane);
}
