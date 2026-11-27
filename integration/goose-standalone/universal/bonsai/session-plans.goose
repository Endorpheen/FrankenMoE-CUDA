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

extern fn bonsai_verify_emit_gdn(context: u64, args: const u64[:]) -> i32;

export fn bonsai_verify_conv_args(input: const u64[:], prefix: const u64[:], rows: u32, output: u64[:]) -> i32 {
    if input.len != 8 || prefix.len != 8 || output.len != 8 || prefix[0] == 0 || rows < 3 || rows > 5 { return -1; }
    for i in 4 {
        output[i] = input[i];
        output[i + 4] = prefix[2 * i];
    }
    return (rows + 3) as! i32;
}

export fn bonsai_verify_gdn_rows(context: u64, input: const u64[:], prefix: const u64[:],
                                 state: u64, rows: u32) -> i32 {
    if input.len != 8 || prefix.len != 8 || rows < 3 || rows > 5 || prefix[1] == 0 || state != input[5] { return -1; }
    var middle: u64[6] = [state, prefix[1], prefix[3], prefix[5], prefix[7], state];
    middle[rows] = state;
    for i in rows as! i64 {
        let token = i as! u64;
        let args: u64[8] = [
            input[0] + token * 4096 * 4, input[1] + token * 4096 * 4,
            input[2] + token * 10240 * 4, input[3] + token * 48 * 4,
            input[4] + token * 48 * 4, middle[i], input[6] + token * 6144 * 4, middle[i + 1],
        ];
        if bonsai_verify_emit_gdn(context, args) != 0 { return -1; }
    }
    return 0;
}

export fn bonsai_verify_commit(states: i64[:], prefix: u64[:], layers: const u32[:],
        start: u32, rows: u32, kept: u32) -> i64 {
    if kept == 0 || kept > rows || rows > 5 || prefix.len < 8 * layers.len { return -1; }
    if kept < rows {
        for i in layers.len {
            if 2 * (layers[i] as! i64) + 1 >= states.len { return -1; }
        }
        for i in layers.len {
            let layer = (layers[i] as! i64) * 2;
            let selected = i * 8 + ((kept - 1) as! i64) * 2;
            for plane in 2 {
                let old = states[layer + plane];
                states[layer + plane] = prefix[selected + plane] as! i64;
                prefix[selected + plane] = old as! u64;
            }
        }
    }
    return (start as! i64) + (kept as! i64);
}

export fn bonsai_verify_position_packets(position: u64, count: i64, packets: u64[:]) -> bool {
    if count < 3 || count > 5 || packets.len != 24 * count { return false; }
    for i in count {
        let pos = position + (i as! u64);
        let keys = pos + 1;
        let tiles = (keys + 255) / 256;
        let base = 24 * i;
        packets[base] = pos | (pos << 32);
        packets[base + 1] = pos;
        let attention: u64[22] = [
            256, 256, 1, 24, 4, 4, keys, 24576, 1024, 576, 144, 576, 144,
            0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys,
        ];
        for j in 22 { packets[base + 2 + j] = attention[j]; }
    }
    return true;
}

export fn bonsai_verify_row(op: i32, index: u64, input: const u64[:], row: u64[:]) -> bool {
    if input.len != 8 || row.len != 8 || index >= 5 { return false; }
    for i in 8 { row[i] = input[i]; }
    let bytes = index * 4;
    if op == 2 {
        if input[4] != 1024 { return false; }
        row[1] += bytes * input[3];
        row[2] += bytes * input[4];
    } else if op == 4 {
        row[0] += bytes * 5120;
        row[5] += bytes * 48;
        row[6] += bytes * 48;
    } else if op == 5 {
        row[0] += bytes * 10240;
        row[3] += bytes * 10240;
    } else if op == 6 {
        row[0] += bytes * 4096;
        row[1] += bytes * 4096;
        row[2] += bytes * 10240;
        row[3] += bytes * 48;
        row[4] += bytes * 48;
        row[6] += bytes * 6144;
        if index % 2 != 0 {
            row[5] = input[7];
            row[7] = input[5];
        }
    } else if op == 8 {
        row[0] += bytes * input[2] * 256;
        row[1] += bytes * input[2] * 256;
    } else if op == 10 {
        row[0] += bytes * 6144;
        row[3] += bytes * 6144;
    } else { return false; }
    return true;
}

extern fn bonsai_verify_attn_launch(context: u64, role: u32, x: u32, y: u32, z: u32,
                                   threads: u32, args: u64) -> i32;

export fn bonsai_verify_attention(context: u64, pointers: const u64[:], rows: u32,
        first_keys: u32, max_tiles: u32, tail: bool, fused_merge: bool) -> i32 {
    if pointers.len != 6 || rows < 3 || rows > 5 { return -1; }
    let q = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[0]));
    let k = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[1]));
    let v = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[2]));
    let dst = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[3]));
    let partial = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(pointers[4]));
    let keys = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(first_keys));
    let tiles = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(max_tiles));
    let scale: f32 = 0.0625 as! f32;
    let scale_address = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(scale));
    let attention_role: u32 = if rows == 4 { 2 } else if rows == 5 { 3 } else if tail { 1 } else { 0 };
    let groups: u32 = if rows == 4 { 8 } else { 12 };
    let panels: u32 = if rows >= 4 { 2 } else { 1 };
    let attention_args: u64[7] = [q, k, v, partial, keys, tiles, scale_address];
    if bonsai_verify_attn_launch(context, attention_role, groups, max_tiles, panels,
                                 32, bonsai_flash_args(attention_args)) != 0 { return -1; }
    if fused_merge {
        let args: u64[4] = [partial, dst, keys, tiles];
        return bonsai_verify_attn_launch(context, 4, rows * 24, 1, 1, 128, bonsai_flash_args(args));
    }
    let partial_bytes = 24 * (max_tiles as! u64) * 258 * 4;
    let start: i32 = 0;
    let start_address = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(start));
    for i in rows as! i64 {
        let row = i as! u64;
        let row_partial = pointers[4] + row * partial_bytes;
        let row_output = pointers[3] + row * 6144 * 4;
        let config = pointers[5] + row * 192 + 16;
        let args: u64[4] = [
            bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row_partial)),
            bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row_output)),
            bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(config)), start_address,
        ];
        if bonsai_verify_attn_launch(context, 5, 24, 1, 1, 32, bonsai_flash_args(args)) != 0 { return -1; }
    }
    return 0;
}

export fn bonsai_verify_launch_rows(context: u64, operation: i32, input: const u64[:],
                                    config: const u64[:]) -> i32 {
    if input.len != 8 || config.len != 8 || config[1] < 3 || config[1] > 5 { return -1; }
    let position = config[0];
    let rows = config[1] as! u32;
    let flags = config[2];
    let partial = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(config[4]));
    let first_keys = (position + 1) as! u32;
    let max_tiles = ((position + config[1] + 255) / 256) as! u32;
    let grouped = (flags & 2) != 0;
    let decode_mma = (flags & 4) != 0;
    let shared_bytes = config[1] * 24 * (max_tiles as! u64) * 258 * 4;
    if operation == 10 && !decode_mma && grouped && (flags & 64) != 0 && shared_bytes <= config[3] {
        let buffers: u64[6] = [input[0], input[1], input[2], input[3], config[4], config[5]];
        return bonsai_verify_attention(context, buffers, rows, first_keys, max_tiles,
                                       (flags & 128) != 0, (flags & 256) != 0);
    }

    let start: i32 = 0;
    let zero: u64 = 0;
    let scale: f32 = 0.0625 as! f32;
    let no_pointer = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(zero));
    let start_arg = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(start));
    let scale_arg = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(scale));
    for token in rows as! i64 {
        var row: u64[8];
        if !bonsai_verify_row(operation, token as! u64, input, row) { return -1; }
        let a = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[0]));
        let b = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[1]));
        let c = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[2]));
        let d = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[3]));
        var status: i32 = -1;
        if operation == 2 {
            let width = input[3] as! u32;
            let output = input[4] as! u32;
            let args: u64[5] = [a, b, c,
                bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(width)),
                bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(output))];
            status = bonsai_verify_attn_launch(context, 6, (output + 3) / 4, 1, 1, 128, bonsai_flash_args(args));
        } else if operation == 4 {
            let staged = (flags & 1) != 0;
            let role: u32 = if staged { 7 } else { 8 };
            let threads: u32 = if staged { 256 } else { 32 };
            let args: u64[7] = [a, b, c, d,
                bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[4])),
                bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[5])),
                bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(row[6]))];
            status = bonsai_verify_attn_launch(context, role, 48, 1, 1, threads, bonsai_flash_args(args));
        } else if operation == 5 {
            let args: u64[4] = [a, b, c, d];
            status = bonsai_verify_attn_launch(context, 9, 40, 1, 1, 256, bonsai_flash_args(args));
        } else if operation == 6 {
            status = bonsai_verify_emit_gdn(context, row);
        } else if operation == 8 {
            let heads = input[2] as! u32;
            let rope = if heads == 24 { config[6] } else { config[7] };
            let positions = config[5] + (token as! u64) * 192;
            let args: u64[5] = [a, bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(positions)),
                no_pointer, b, bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(rope))];
            status = bonsai_verify_attn_launch(context, 10, heads, 1, 1, 128, bonsai_flash_args(args));
        } else if operation == 10 {
            let keys = first_keys + (token as! u32);
            let tiles = (keys + 255) / 256;
            let key_arg = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(keys));
            let tile_arg = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(tiles));
            let config_pointer = config[5] + (token as! u64) * 192 + 16;
            let config_arg = bonsai_flash_address(unsafe_c_address<c_ptr<const void>>(config_pointer));
            let direct = grouped && decode_mma && (keys >= 32768 || (flags & 8) != 0);
            if direct {
                let role: u32 = if (flags & 16) != 0 { 11 } else { 12 };
                let args: u64[6] = [a, b, c, partial, key_arg, tile_arg];
                status = bonsai_verify_attn_launch(context, role, 4, tiles, 1, 128, bonsai_flash_args(args));
            } else if grouped {
                let wide = keys >= 32768;
                let role: u32 = if wide { 13 } else { 14 };
                let groups: u32 = if wide { 8 } else { 12 };
                let args: u64[7] = [a, b, c, partial, key_arg, tile_arg, scale_arg];
                status = bonsai_verify_attn_launch(context, role, groups, tiles, 1, 32, bonsai_flash_args(args));
            } else {
                let args: u64[8] = [a, b, c, no_pointer, partial, config_arg, scale_arg, start_arg];
                status = bonsai_verify_attn_launch(context, 15, 24, tiles, 1, 32, bonsai_flash_args(args));
            }
            if status != 0 { return -1; }
            if grouped && (flags & 32) != 0 && keys >= 32768 {
                let args: u64[4] = [partial, d, key_arg, tile_arg];
                status = bonsai_verify_attn_launch(context, 16, 24, 1, 1, 128, bonsai_flash_args(args));
            } else {
                let args: u64[4] = [partial, d, config_arg, start_arg];
                status = bonsai_verify_attn_launch(context, 5, 24, 1, 1, 32, bonsai_flash_args(args));
            }
        }
        if status != 0 { return status; }
    }
    return 0;
}

const BONSAI_WORKSPACE_LAYERS: i64 = 64;
const BONSAI_WORKSPACE_HISTORY: u64 = 3 * 10240 * 4;
const BONSAI_WORKSPACE_GDN: u64 = 48 * 128 * 128 * 4;
const BONSAI_WORKSPACE_ATTENTION: u64 = 32 * 1024 * 1024;

fn bonsai_workspace_take(cursor: u64&, bytes: u64) -> u64 {
    let start = cursor;
    cursor += (bytes + 255) & ~255;
    return start;
}

export fn bonsai_workspace_plan(context: u64, batch: u64, rows: u64, flags: u64,
        bound: const i64[:], states: i64[:], scratch: i64[:], prefix: u64[:],
        layer_ids: u32[:], layout: u64[:]) -> bool {
    if context == 0 || context > 262144 || batch == 0 || batch > 1024 || rows > 5 ||
        bound.len != 15 * BONSAI_WORKSPACE_LAYERS || states.len != 2 * BONSAI_WORKSPACE_LAYERS ||
        scratch.len != 24 || prefix.len != 8 * BONSAI_WORKSPACE_LAYERS ||
        layer_ids.len != BONSAI_WORKSPACE_LAYERS || layout.len != 17 { return false; }
    let commit = (flags & 4) != 0;
    if commit && rows < 3 { return false; }
    let widths: u64[23] = [5120, 5120, 5120, 12288, 6144, 1024, 6144, 10240, 6144,
        6144, 6144, 6144, 5120, 5120, 17408, 17408, 17408, 17408, 5120, 248320, 5120, 6144, 17408];
    let cache = context * 576;
    let all_tasks = 24 * batch * ((context + 255) / 256) * 258 * 4;
    let partial = if (flags & 1) == 0 && all_tasks < BONSAI_WORKSPACE_ATTENTION { all_tasks }
        else { BONSAI_WORKSPACE_ATTENTION };
    var cursor: u64 = 0;
    for layer in BONSAI_WORKSPACE_LAYERS {
        let attention = bound[layer * 15] == 0;
        states[2 * layer] = bonsai_workspace_take(&cursor, if attention { cache } else { BONSAI_WORKSPACE_HISTORY }) as! i64;
        states[2 * layer + 1] = bonsai_workspace_take(&cursor, if attention { cache } else { BONSAI_WORKSPACE_GDN }) as! i64;
    }
    for i in widths.len {
        let multiplier = if i < 19 { batch } else if i == 19 && (flags & 2) != 0 { rows } else { 1 };
        scratch[i] = bonsai_workspace_take(&cursor, widths[i] * 4 * multiplier) as! i64;
    }
    scratch[23] = bonsai_workspace_take(&cursor, BONSAI_WORKSPACE_GDN) as! i64;
    var layers: i64 = 0;
    if commit {
        for layer in BONSAI_WORKSPACE_LAYERS {
            if bound[layer * 15] != 1 { continue; }
            layer_ids[layers] = layer as! u32;
            for i in (rows - 1) as! i64 {
                prefix[8 * layers + 2 * i] = bonsai_workspace_take(&cursor, BONSAI_WORKSPACE_HISTORY);
                prefix[8 * layers + 2 * i + 1] = bonsai_workspace_take(&cursor, BONSAI_WORKSPACE_GDN);
            }
            layers += 1;
        }
    }
    layout[1] = partial;
    layout[2] = layers as! u64;
    layout[3] = bonsai_workspace_take(&cursor, 16 * batch);
    layout[4] = bonsai_workspace_take(&cursor, 1024 * batch);
    layout[5] = bonsai_workspace_take(&cursor, 4 * batch);
    layout[6] = bonsai_workspace_take(&cursor, 2 * 17408 * batch);
    layout[7] = bonsai_workspace_take(&cursor, 27 * 8);
    layout[8] = bonsai_workspace_take(&cursor, 27 * 8);
    layout[9] = bonsai_workspace_take(&cursor, 22 * 8);
    layout[10] = bonsai_workspace_take(&cursor, partial);
    if (flags & 8) != 0 {
        layout[11] = bonsai_workspace_take(&cursor, 27 * 8);
        layout[12] = bonsai_workspace_take(&cursor, 27 * 8);
        layout[13] = bonsai_workspace_take(&cursor, rows * 192);
    }
    if (flags & 16) != 0 {
        let limit = if context < 36864 { context } else { 36864 };
        let chunks = (limit + 31) / 32;
        layout[14] = chunks;
        layout[15] = bonsai_workspace_take(&cursor, chunks * 4 * 16384);
        layout[16] = bonsai_workspace_take(&cursor, chunks * 4 * 16384);
    }
    layout[0] = cursor;
    return true;
}

export fn bonsai_workspace_rebase(base: u64, rows: u64, flags: u64, states: i64[:],
                                  scratch: i64[:], prefix: u64[:], layout: u64[:]) {
    for i in states.len { states[i] += base as! i64; }
    for i in scratch.len { scratch[i] += base as! i64; }
    for i in 3 .. 11 { layout[i] += base; }
    if (flags & 4) != 0 {
        for i in layout[2] as! i64 {
            for j in (2 * (rows - 1)) as! i64 { prefix[8 * i + j] += base; }
        }
    }
    if (flags & 8) != 0 { for i in 11 .. 14 { layout[i] += base; } }
    if (flags & 16) != 0 { for i in 15 .. 17 { layout[i] += base; } }
}
