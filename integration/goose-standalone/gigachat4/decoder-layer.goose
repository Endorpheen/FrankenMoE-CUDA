extern "ggml_cpy" fn giga_copy(ctx: u64, source: u64, target: u64) -> u64;

fn giga_norm_at(ctx: u64, x: u64, w: u64[:], offset: i64, epsilon: f32) -> u64 {
    return giga_gated_norm(ctx, x, w[offset], w[offset + 1], w[offset + 2], epsilon);
}

// State order for GDN is convolution history, recurrent state, and their destinations.
export fn giga_decoder_gdn(ctx: u64, graph: u64, input: u64, w: u64[:],
                           state: u64[:], tokens: i64, streams: i64, epsilon: f32) -> u64 {
    if ctx == 0 || graph == 0 || input == 0 || w.len < 21 || state.len < 4 ||
       tokens < 1 || streams < 1 || tokens % streams != 0 { return 0; }
    for i in 4 { if state[i] == 0 { return 0; } }
    let norm = giga_norm_at(ctx, input, w, 0, epsilon);
    if norm == 0 { return 0; }
    giga_expand(graph, norm);
    var projected: u64[4] = [0; 4];
    var result: u64[3] = [0; 3];
    let per_stream = tokens / streams;
    if giga_gdn_projections(ctx, norm, w[13], w[12], w[18], w[17], w[16], w[15],
                            per_stream, streams, projected) != 0 { return 0; }
    if giga_gdn_layer(ctx, projected, state[0], state[1], w[14], w[19], w[20],
                      per_stream, streams, epsilon, state[1] == state[3], result) != 0 { return 0; }
    giga_expand(graph, giga_copy(ctx, result[1], state[2]));
    giga_expand(graph, giga_copy(ctx, result[2], state[3]));
    return result[0];
}

// State order for MLA is its cache, the index cache, and the optional index rotation.
export fn giga_decoder_mla(ctx: u64, graph: u64, input: u64, w: u64[:], state: u64[:],
                           pos: u64, rows: u64, mask: u64,
                           tokens: i64, cells: i64, capacity: i64, streams: i64,
                           kv_stride: u64, idx_stride: u64, epsilon: f32, scale: f32,
                           original_ctx: i32, factors: u64, rope: f32[:], index_rope: f32[:]) -> u64 {
    if ctx == 0 || graph == 0 || input == 0 || w.len < 29 || state.len < 3 { return 0; }
    let norm = giga_norm_at(ctx, input, w, 0, epsilon);
    if norm == 0 { return 0; }
    giga_expand(graph, norm);
    var weights: u64[13] = [0; 13];
    for i in 13 { weights[i] = w[12 + i]; }
    var projected: u64[4] = [0; 4];
    var index: u64[3] = [0; 3];
    if giga_mla_project(ctx, norm, pos, weights, tokens, epsilon,
                        original_ctx, factors, rope, projected) != 0 { return 0; }
    if giga_dsa_index_project(ctx, norm, projected[2], pos, w[25], w[26], w[27], w[28],
                              tokens, 64, original_ctx, index_rope, index) != 0 { return 0; }
    return giga_mla_layer(ctx, graph, projected, index, state[0], state[1], state[2], rows, mask,
                          tokens, cells, capacity, streams, kv_stride, idx_stride, scale, w[17], w[18]);
}

export fn giga_decoder_ffn(ctx: u64, graph: u64, input: u64, attention: u64, w: u64[:],
                           ffn_offset: i64, output_ids: u64, output_tokens: i64,
                           dense: bool, epsilon: f32, normalize: bool, norm_eps: f32,
                           routing_scale: f32, routed_limit: f32, shared_limit: f32) -> u64 {
    if ctx == 0 || graph == 0 || input == 0 || attention == 0 ||
       ffn_offset < 12 || ffn_offset > w.len || output_tokens < 1 { return 0; }
    var residual = input;
    var attended = attention;
    if output_ids != 0 {
        residual = giga_get_rows(ctx, residual, output_ids);
        attended = giga_get_rows(ctx, attended, output_ids);
    }
    let post_attention = giga_norm_at(ctx, attended, w, 3, epsilon);
    if post_attention == 0 { return 0; }
    let combined = giga_add(ctx, residual, post_attention);
    let normalized = giga_norm_at(ctx, combined, w, 6, epsilon);
    if normalized == 0 { return 0; }

    var transformed: u64 = 0;
    if dense {
        if w.len - ffn_offset < 3 { return 0; }
        transformed = giga_dense_ffn(ctx, normalized, w[ffn_offset],
                                      w[ffn_offset + 1], w[ffn_offset + 2], shared_limit);
    } else {
        if w.len - ffn_offset < 8 { return 0; }
        var weights: u64[8] = [0; 8];
        for i in 8 { weights[i] = w[ffn_offset + i]; }
        transformed = giga_moe_ffn(ctx, graph, normalized, weights, output_tokens,
                                   normalize, norm_eps, routing_scale, routed_limit, shared_limit);
    }
    let post_ffn = giga_norm_at(ctx, transformed, w, 9, epsilon);
    if post_ffn == 0 { return 0; }
    return giga_add(ctx, combined, post_ffn);
}
