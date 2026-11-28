extern "flash_vector_plan_word" fn flash_vector_plan(address: u64, index: i32) -> i64;

export fn flash_vector_launch_pack(query: u64, packed: u64, plan: u64, scale: f32,
    head: i32, lane: i32, shared: u64) {
    let query_offset = (head as i64) * flash_vector_plan(plan, 8);
    let packed_offset = (head as i64) * 320;
    fv_pack_query(query + (query_offset as! u64), packed + (packed_offset as! u64), scale, lane, shared);
}

export fn flash_vector_launch_q4(query: u64, key: u64, value: u64, parts: u64, metadata: u64,
    plan: u64, keys: i32, scale: f32, mode: i32, tile: i32, head: i32, tiles: i32,
    thread: i32, shared: u64) {
    fv_fattn_vec_1(query, key, value, 0, 0, 0, parts, metadata, scale, 0.0, 0.0, 0.0, 0, 0.0,
        256, 1, flash_vector_plan(plan, 3) as! i32, 1, flash_vector_plan(plan, 7), flash_vector_plan(plan, 8), 0,
        256, keys, flash_vector_plan(plan, 4) as! i32, 1,
        flash_vector_plan(plan, 9), flash_vector_plan(plan, 10), 0,
        flash_vector_plan(plan, 11), flash_vector_plan(plan, 12), 0, 1, 1, 1, 0, 0, 0,
        mode, 0, tile, head, 1, tiles, thread % 32, thread / 32, shared);
}
