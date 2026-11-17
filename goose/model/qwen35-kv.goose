// Single-stream contiguous-prefix session state:
// [914, capacity, valid_prefix, native_padding, pending_tokens, generation].
fn qk_state_ok(state: i64[:]) -> bool {
    if state.len < 6 { return false; }
    return state[0] == 914 && state[1] > 0 && state[1] <= 2147483647 &&
           state[2] >= 0 && state[2] <= state[1] && state[3] > 0 && state[3] <= state[1] &&
           (state[3] & (state[3] - 1)) == 0 && state[1] % state[3] == 0 &&
           state[4] >= 0 && state[4] <= state[1] - state[2] && state[5] >= 0;
}

export fn q35_kv_init(capacity: i64, padding: i64, state: i64[:]) -> i64 {
    if state.len < 6 || capacity <= 0 || capacity > 2147483647 || padding <= 0 || padding > capacity ||
       (padding & (padding - 1)) != 0 || capacity % padding != 0 {
        return -1;
    }
    state[0] = 914;
    state[1] = capacity;
    state[2] = 0;
    state[3] = padding;
    state[4] = 0;
    state[5] = 0;
    return 0;
}

export fn q35_kv_read_rows(state: i64[:], added: i64) -> i64 {
    if !qk_state_ok(state) || (state[4] != 0 && state[4] != added) || added <= 0 || added > state[1] - state[2] { return -1; }
    let step: i64 = if state[3] > 256 { state[3] } else { 256 };
    let used: i64 = state[2] + added;
    let padded: i64 = ((used + step - 1) / step) * step;
    let result: i64 = if padded > step { padded } else { step };
    return if result < state[1] { result } else { state[1] };
}

// One ticket for the complete model step, shared by every layer's cache build.
export fn q35_kv_begin(state: i64[:], added: i64) -> i64 {
    if !qk_state_ok(state) || state[4] != 0 || q35_kv_read_rows(state, added) < 0 || state[5] == 9223372036854775807 { return -1; }
    state[4] = added;
    state[5] += 1;
    return state[5];
}

export fn q35_kv_indices(state: i64[:], added: i64, out: i64[:]) -> i64 {
    if q35_kv_read_rows(state, added) < 0 || out.len < added { return -1; }
    var i: i64 = 0;
    while i < added { out[i] = state[2] + i; i += 1; }
    return added;
}

export fn q35_kv_positions(state: i64[:], added: i64, out: i64[:]) -> i64 {
    if q35_kv_read_rows(state, added) < 0 || out.len < added * 4 { return -1; }
    var s: i64 = 0;
    while s < 4 {
        var i: i64 = 0;
        while i < added {
            out[s * added + i] = if s < 3 { state[2] + i } else { 0 };
            i += 1;
        }
        s += 1;
    }
    return added * 4;
}

// Caller uploads/converts this host mask through its backend input primitive.
export fn q35_kv_mask(state: i64[:], added: i64, out: f32[:]) -> i64 {
    let rows: i64 = q35_kv_read_rows(state, added);
    if rows < 0 || out.len < rows * added { return -1; }
    let negative_infinity: f32 = float_bits(4286578688);
    var i: i64 = 0;
    while i < added {
        var j: i64 = 0;
        while j < rows {
            out[i * rows + j] = if j <= state[2] + i { 0.0 } else { negative_infinity };
            j += 1;
        }
        i += 1;
    }
    return rows;
}

export fn q35_kv_commit(state: i64[:], ticket: i64) -> i64 {
    if !qk_state_ok(state) || state[4] <= 0 || state[5] != ticket { return -1; }
    state[2] += state[4];
    state[4] = 0;
    return state[2];
}

export fn q35_kv_abort(state: i64[:], ticket: i64) -> i64 {
    if !qk_state_ok(state) || state[4] <= 0 || state[5] != ticket || state[5] == 9223372036854775807 { return -1; }
    state[4] = 0;
    state[5] += 1;
    return state[2];
}

export fn q35_kv_rewind(state: i64[:], prefix: i64) -> i64 {
    if !qk_state_ok(state) || state[4] != 0 || prefix < 0 || prefix > state[2] || state[5] == 9223372036854775807 { return -1; }
    state[2] = prefix;
    state[5] += 1;
    return prefix;
}

fn qk_row_bytes(width: i64) -> i64 {
    if width <= 0 || width > 2147483647 || width % 32 != 0 { return -1; }
    return (width / 32) * 18;
}

// Host byte slices, never raw device pointers. Snapshot metadata:
// [914, capacity, prefix, padding, width, Q4_0, generation, reserved0].
export fn q35_kv_host_save(state: i64[:], width: i64, keys: u8[:], values: u8[:], meta: i64[:], bytes: u8[:]) -> i64 {
    let row: i64 = qk_row_bytes(width);
    if !qk_state_ok(state) || state[4] != 0 || row < 0 || meta.len < 8 { return -1; }
    let n: i64 = row * state[2];
    if keys.len < n || values.len < n || bytes.len < n * 2 { return -1; }
    var i: i64 = 0;
    while i < n { bytes[i] = keys[i]; bytes[n + i] = values[i]; i += 1; }
    meta[0] = 914; meta[1] = state[1]; meta[2] = state[2]; meta[3] = state[3];
    meta[4] = width; meta[5] = 2; meta[6] = state[5]; meta[7] = 0;
    return n * 2;
}

export fn q35_kv_host_restore(state: i64[:], width: i64, keys: u8[:], values: u8[:], meta: i64[:], bytes: u8[:]) -> i64 {
    let row: i64 = qk_row_bytes(width);
    if !qk_state_ok(state) || state[4] != 0 || row < 0 || meta.len < 8 || state[5] == 9223372036854775807 { return -1; }
    if meta[0] != 914 || meta[1] != state[1] || meta[2] < 0 || meta[2] > state[1] || meta[3] != state[3] ||
       meta[4] != width || meta[5] != 2 || meta[6] < 0 || meta[7] != 0 { return -1; }
    let n: i64 = row * meta[2];
    if keys.len < n || values.len < n || bytes.len < n * 2 { return -1; }
    var i: i64 = 0;
    while i < n { keys[i] = bytes[i]; values[i] = bytes[n + i]; i += 1; }
    state[2] = meta[2]; state[4] = 0; state[5] += 1;
    return state[2];
}

// Explicit SET_ROWS dependencies replace reliance on host graph-expand order.
export fn q35_attn_cached(c: i64, x: i64, positions: i64, mask: i64, weights: i64[:], cfg: i64[:], sections: i64,
                          cache_k: i64, cache_v: i64, indices: i64, state: i64[:], ticket: i64, work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || x == 0 || mask == 0 || cache_k == 0 || cache_v == 0 || indices == 0 || cfg.len < 14 ||
       weights.len < 8 || work.len < 16 || nodes.len < 35 || !qk_state_ok(state) || state[4] <= 0 || state[5] != ticket { return 0; }
    let count: i64 = ne(x, 1);
    let read_rows: i64 = q35_kv_read_rows(state, count);
    if read_rows < 0 || cfg[1] <= 0 || state[4] != count { return 0; }
    let width: i64 = ne(cache_k, 0);
    if qk_row_bytes(width) < 0 || prop(cache_k, 0) != 2 || prop(cache_v, 0) != 2 ||
       ne(cache_v, 0) != width || ne(cache_k, 1) != state[1] || ne(cache_v, 1) != state[1] ||
       ne(cache_k, 2) != 1 || ne(cache_k, 3) != 1 || ne(cache_v, 2) != 1 || ne(cache_v, 3) != 1 ||
       pred(0, cache_k, 0) == 0 || pred(0, cache_v, 0) == 0 ||
       width % cfg[1] != 0 || (width / cfg[1]) % 32 != 0 || prop(indices, 0) != 27 || ne(indices, 0) != count ||
       ne(indices, 1) != 1 || ne(indices, 2) != 1 || ne(indices, 3) != 1 || pred(0, indices, 0) == 0 ||
       prop(mask, 0) != 1 || ne(mask, 0) != read_rows || ne(mask, 1) < count ||
       ne(mask, 2) != 1 || ne(mask, 3) != 1 || pred(0, mask, 0) == 0 { return 0; }
    if weights[2] == 0 || ne(weights[2], 1) != width { return 0; }
    if q35_attn_project(c, x, positions, weights, cfg, sections, work, nodes) == 0 { return 0; }
    if ne(nodes[14], 0) * ne(nodes[14], 1) != width { return 0; }
    let k_stride: i64 = qa_get(nodes[14], 1, 2) as! i64;
    let v_stride: i64 = qa_get(nodes[8], 1, 2) as! i64;
    nodes[29] = view4(c, nodes[14], width, count, 1, 1, k_stride, k_stride * count, k_stride * count, 0);
    nodes[30] = view4(c, nodes[8], width, count, 1, 1, v_stride, v_stride * count, v_stride * count, 0);
    qa_begin(work, 19, c, cache_k, nodes[29], indices, 0);
    nodes[31] = gb_common_args(work);
    qa_begin(work, 19, c, cache_v, nodes[30], indices, 0);
    nodes[32] = gb_common_args(work);
    let head: i64 = width / cfg[1];
    let head_bytes: i64 = qk_row_bytes(head);
    let row_bytes: i64 = qk_row_bytes(width);
    let stream_bytes: i64 = row_bytes * state[1];
    nodes[33] = view4(c, nodes[31], head, cfg[1], read_rows, 1, head_bytes, row_bytes, stream_bytes, 0);
    nodes[34] = view4(c, nodes[32], head, cfg[1], read_rows, 1, head_bytes, row_bytes, stream_bytes, 0);
    let result: i64 = q35_attn_finish(c, x, nodes[13], nodes[33], nodes[34], nodes[15], mask, weights, cfg, work, nodes);
    if result == 0 { return 0; }
    return result;
}
