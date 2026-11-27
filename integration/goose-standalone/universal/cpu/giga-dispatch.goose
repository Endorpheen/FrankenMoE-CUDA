export fn giga_cuda_mla_validate(config: const i64[:], lengths: const i64[:]) -> i32 {
    if config.len != 23 || config[22] != 1 { return 0; }
    if lengths.len != 5 || config[17] <= 0 || config[17] > 65535 ||
       config[18] <= 0 || config[18] > 2147483647 ||
       config[19] <= 0 || config[19] > config[18] || config[20] <= 0 ||
       config[20] % config[19] != 0 ||
       lengths[0] <= 0 || lengths[1] <= 0 || lengths[2] <= 0 ||
       lengths[3] < 0 || lengths[4] <= 0 { return -1; }
    return 1;
}

export fn giga_cuda_mla_plan(config: const i64[:], capacity: i32,
        pair_kernel: bool, batch_pair: bool, mma_kernel: bool, plan: i64[:]) {
    let positions = config[2];
    let tasks = config[18];
    let common = config[0] == 576 && config[1] == 512 && config[4] == 1 && config[5] == 1;
    let pair = pair_kernel && common && positions > 0 && positions <= 1073741823 &&
        tasks % (2 * positions) == 0 && (positions == 1 || (batch_pair && capacity >= 2));
    let mma = mma_kernel && common && positions >= 8 && positions % 8 == 0 &&
        config[6] % 16 == 0 && config[16] == 256 && tasks % 8 == 0 && capacity >= 8;
    let modulus = if mma { 8 } else { if pair && capacity >= 2 { 2 } else { 1 } };
    plan[0] = (capacity - capacity % modulus) as i64;
    plan[1] = if pair { 1 } else { 0 };
    plan[2] = if mma { 1 } else { 0 };
    plan[3] = if mma && positions <= 4096 && config[6] >= 8192 && config[3] == 32 { 1 } else { 0 };
}

export fn giga_cuda_mla_indexed_capacity(counts: const i32[:], task_bytes: u64,
        capacity: i32, tasks: i32) -> i32 {
    for row in counts.len { if counts[row] != 2048 { return 0; } }
    let available = task_bytes * (capacity as u64) / (8 * 514 * 4);
    let cap = if available < (tasks as u64) { available as i32 } else { tasks };
    return cap - cap % 8;
}

extern "giga_mla_launch_wave" fn giga_cuda_launch_mla_wave(context: u64,
        start: i32, count: i32, tile: i32, merge: i32, blocks: i32, tile_rows: i32, threads: i32) -> i32;

export fn giga_cuda_mla_dispatch(context: u64, tasks: i32, capacity: i32, tiles: i32,
        positions: i64, paired_shape: bool, mma: bool, indexed: bool, visible: bool) -> i32 {
    if capacity <= 0 { return -1; }
    var start: i32 = 0;
    while start < tasks {
        let remaining = tasks - start;
        let count = if remaining < capacity { remaining } else { capacity };
        let pair = !mma && paired_shape && count % 2 == 0 && start % 2 == 0;
        let tile: i32 = if indexed { 0 } else { if visible { 1 } else { if mma { 2 } else {
            if pair { if positions == 1 { 3 } else { 4 } } else { 5 }
        } } };
        let merge: i32 = if indexed { 0 } else { if pair && positions > 1 { 1 } else { 2 } };
        let width: i32 = if mma { 8 } else { if pair { 2 } else { 1 } };
        let error = giga_cuda_launch_mla_wave(context, start, count, tile, merge, count / width,
            if indexed { 8 } else { tiles }, if mma { 256 } else { 32 });
        if error != 0 { return error; }
        start += count;
    }
    return 0;
}

export fn giga_cuda_dsa_mask_plan(cells: i64, count: i64, rows: i64, prefix: i64,
        mask_bytes: i64, picks_bytes: i64, output_bytes: i64, plan: i64[:]) -> bool {
    if cells < 1 || cells > 1048576 || count < 1 || count > cells ||
       rows < 1 || rows > 4096 || prefix < 0 || prefix >= rows { return false; }
    let items = cells * rows;
    let kept = count * (rows - prefix);
    if mask_bytes != items * 2 || picks_bytes != kept * 4 || output_bytes != mask_bytes { return false; }
    plan[0] = items;
    plan[1] = kept;
    plan[2] = cells * prefix;
    plan[3] = (items + 255) / 256;
    plan[4] = (kept + 255) / 256;
    return true;
}

export fn giga_cuda_dsa_top_valid(cells: i64, rows: i64, scores: i64, ids: i64) -> bool {
    return cells > 4096 && cells <= 1048576 && rows >= 1 && rows <= 4096 &&
        scores == cells * rows * 4 && ids == rows * 2048 * 4;
}

export fn giga_cuda_dsa_score_valid(config: const i64[:], lengths: const i64[:]) -> bool {
    if config.len != 6 || config[0] < 1 || config[0] > 1048576 ||
       config[1] < 1 || config[1] > 4096 || lengths.len != 5 { return false; }
    for i in lengths.len { if lengths[i] <= 0 { return false; } }
    return true;
}

export fn giga_cuda_dsa_score_columns(cells: i64, shared: bool) -> i32 {
    let width = if shared { 16 } else { 4 };
    return ((cells + width - 1) / width) as i32;
}
