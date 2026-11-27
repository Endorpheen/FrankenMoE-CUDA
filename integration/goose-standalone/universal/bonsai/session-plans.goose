export fn bonsai_gdn_packet(input: const u64[:], tokens: u32, packet: u64[:]) -> bool {
    if input.len != 8 || packet.len != 57 || tokens == 0 { return false; }
    const state: u64 = 48 * 128 * 128;
    var config: u64[49] = [
        1, 128, 2048, 2048, 1, 128, 2048, 2048, 1, 128, 6144, 6144,
        1, 1, 48, 48, 1, 1, 48, 48, 1, 128, 16384, state,
        128, 48, 1, 1, 0, 0, 1, 0, 0, 0, 8, 32, 4, 32, 1, state,
        0, 0x3db504f3, 1, 4, 16, 1, 0, 1, 6144,
    ];
    if tokens > 1 {
        let count = tokens as u64;
        config[2] = 4096;
        config[6] = 4096;
        config[3] = 4096 * count;
        config[7] = config[3];
        config[10] = 10240;
        config[11] = 10240 * count;
        config[15] = 48 * count;
        config[19] = config[15];
        config[26] = count;
        config[38] = 0;
        config[48] = 6144 * count;
    }
    for i in 8 { packet[i] = input[i]; }
    for i in 49 { packet[8 + i] = config[i]; }
    return true;
}

extern fn bonsai_flash_launch_raw(context: u64, role: u32, x: u32, y: u32,
                                  threads: u32, shared: u32, args: u64) -> i32;
extern "memcpy" fn bonsai_flash_copy(destination: c_ptr<void>, source: c_ptr<const void>,
                                     bytes: u64) -> c_ptr<void>;

fn bonsai_flash_address(pointer: c_ptr<const void>) -> u64 {
    var address: u64 = 0;
    bonsai_flash_copy(unsafe_c_address<c_ptr<void>>(address), unsafe_c_address<c_ptr<const void>>(pointer), 8);
    return address;
}

fn bonsai_flash_args(args: const u64[:]) -> u64 {
    return bonsai_flash_address(unsafe_c_slice_data<c_ptr<const void>>(args));
}

export fn bonsai_flash_schedule(context: u64, position: u64, count: u64,
        partial_bytes: u64, stage_capacity: u64, flags: u64, pointers: const u64[:]) -> i32 {
    if count == 0 || pointers.len != 7 { return 1; }
    let keys = position + count;
    let tiles = (keys + 127) / 128;
    let group_bytes = tiles * 16 * 258 * 4;
    var wave = partial_bytes / group_bytes;
    if wave == 0 { return 1; }
    let chunks = (keys + 31) / 32;
    let staged = (flags & 1) != 0 && chunks <= stage_capacity;
    let ordered = staged && (flags & 2) != 0 && keys >= 2048;
    let compact = staged && (flags & 4) != 0;
    let grouped = compact && (flags & 8) != 0 && (flags & 16) != 0;
    let groups_per_head = if compact { (count * 6 + 15) / 16 } else { (count + 1) / 2 };
    let panel = grouped && (flags & 32) != 0 && keys >= 8192 && wave >= 2 && groups_per_head % 2 == 0;
    if panel { wave &= ~1; }
    let grouping: u64 = if panel { 2 } else if grouped { if keys >= 16384 { 4 } else { 2 } } else { 1 };
    let groups = 4 * groups_per_head;
    let tile_role: u32 = if panel { 6 } else if grouping == 4 { 7 } else if grouping == 2 { 8 }
        else if compact { if (flags & 8) != 0 { 9 } else { 10 } } else { 11 };
    let merge_role: u32 = if compact { if ordered { 12 } else { 13 } } else if ordered { 14 } else { 15 };
    let fused = panel && ordered && (flags & 64) != 0 && count >= 512;

    let count_word = count as! u32;
    let keys_word = keys as! u32;
    let first_word = (position + 1) as! u32;
    let tiles_word = tiles as! u32;
    let chunks_word = chunks as! u32;
    var start_word: u32 = 0;
    let q = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[0]));
    let k = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[1]));
    let v = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[2]));
    let dst = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[3]));
    let stage_k = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[4]));
    let stage_v = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[5]));
    let partial = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[6]));
    let n = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(count_word));
    let end = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(keys_word));
    let first = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(first_word));
    let tile_count = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(tiles_word));
    let chunk_count = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(chunks_word));
    let start = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(start_word));
    let fused_args: u64[10] = [q, stage_k, stage_v, dst, n, end, first, tile_count, start, chunk_count];
    if fused && (flags & 128) != 0 && keys >= 16384 {
        let keys_args: u64[4] = [k, stage_k, end, chunk_count];
        let values_args: u64[4] = [v, stage_v, end, chunk_count];
        if bonsai_flash_launch_raw(context, 0, chunks_word, 4, 128, 0, bonsai_flash_args(keys_args)) != 0 ||
           bonsai_flash_launch_raw(context, 1, chunks_word, 4, 128, 0, bonsai_flash_args(values_args)) != 0 { return -1; }
        return bonsai_flash_launch_raw(context, 2, (groups / 2) as! u32, 1, 256, 41472, bonsai_flash_args(fused_args));
    }
    if staged {
        let stage_args: u64[6] = [k, v, stage_k, stage_v, end, chunk_count];
        if bonsai_flash_launch_raw(context, 3, chunks_word, 4, 128, 0, bonsai_flash_args(stage_args)) != 0 { return -1; }
    }
    if fused {
        return bonsai_flash_launch_raw(context, 4, (groups / 2) as! u32, 1, 256, 50176, bonsai_flash_args(fused_args));
    }

    let stage_tile: u64[10] = [q, stage_k, stage_v, partial, n, end, first, tile_count, start, chunk_count];
    let plain_tile: u64[9] = [q, k, v, partial, n, end, first, tile_count, start];
    let merge: u64[6] = [partial, dst, n, first, tile_count, start];
    let tile_rows = ((tiles + grouping - 1) / grouping) as! u32;
    let tile_threads: u32 = if panel { 256 } else { 128 };
    let merge_threads: u32 = if ordered { 128 } else { 32 };
    let heads: u64 = if compact { 16 } else { 12 };
    var wave_start: u64 = 0;
    while wave_start < groups {
        let remaining = groups - wave_start;
        let active = if remaining < wave { remaining } else { wave };
        start_word = wave_start as! u32;
        let blocks = (if panel { active / 2 } else { active }) as! u32;
        let status = if staged {
            bonsai_flash_launch_raw(context, tile_role, blocks, tile_rows, tile_threads, 0, bonsai_flash_args(stage_tile))
        } else {
            bonsai_flash_launch_raw(context, 5, active as! u32, tiles_word, 128, 0, bonsai_flash_args(plain_tile))
        };
        if status != 0 ||
            bonsai_flash_launch_raw(context, merge_role, (active * heads) as! u32, 1,
                                    merge_threads, 0, bonsai_flash_args(merge)) != 0 { return -1; }
        wave_start += wave;
    }
    return 0;
}
