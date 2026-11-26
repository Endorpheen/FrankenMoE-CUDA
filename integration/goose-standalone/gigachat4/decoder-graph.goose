extern "ggml_tanh" fn giga_tanh(ctx: u64, x: u64) -> u64;

// Batch fields: token ids, positions, cache rows, attention mask, output ids.
// Shape fields: tokens, live cache cells, cache capacity, streams, output tokens, original context.
// Tuning fields: RMS epsilon, attention scale, router normalization epsilon and scale, logit softcap.
export fn giga_decoder_graph(ctx: u64, graph: u64, weights: u64[:], offsets: i64[:],
                             kinds: i64[:], state: u64[:], batch: u64[:], shape: i64[:],
                             strides: u64[:], tuning: f32[:], routed_limits: f32[:],
                             shared_limits: f32[:], factors: u64[:], rope: f32[:],
                             index_rope: f32[:], normalize: bool, out: u64[:]) -> i64 {
    if ctx == 0 || graph == 0 || weights.len < 1036 || offsets.len < 34 ||
       kinds.len < 33 || state.len < 128 || batch.len < 5 || shape.len < 6 ||
       strides.len < 2 || tuning.len < 5 || routed_limits.len < 32 ||
       shared_limits.len < 32 || factors.len < 32 || out.len < 2 ||
       batch[0] == 0 || weights[3] == 0 || offsets[0] != 5 || offsets[33] != 1036 ||
       shape[0] < 1 || shape[4] < 0 || shape[4] > shape[0] { return -1; }

    var hidden = giga_get_rows(ctx, weights[3], batch[0]);
    for layer in 32 {
        let start = offsets[layer];
        let end = offsets[layer + 1];
        let kind = kinds[layer];
        if start < 5 || end < start || end > weights.len || (kind != 0 && kind != 1) { return -2; }
        let w = weights[start .. end];
        let local = state[(layer * 4) .. (layer * 4 + 4)];
        var attention: u64 = 0;
        var ffn_offset: i64 = 21;
        if kind == 0 {
            attention = giga_decoder_gdn(ctx, graph, hidden, w, local, shape[0], shape[3], tuning[0]);
        } else {
            ffn_offset = 29;
            attention = giga_decoder_mla(ctx, graph, hidden, w, local,
                batch[1], batch[2], batch[3], shape[0], shape[1], shape[2], shape[3],
                strides[0], strides[1], tuning[0], tuning[1], shape[5] as i32,
                factors[layer], rope, index_rope, tuning.len > 5 && tuning[5] != 0.0);
        }
        hidden = giga_decoder_ffn(ctx, graph, hidden, attention, w, ffn_offset,
            0, shape[0], layer == 0, tuning[0], normalize, tuning[2], tuning[3],
            routed_limits[layer], shared_limits[layer]);
        if hidden == 0 { return -(layer + 3); }
    }

    if shape[4] == 0 {
        var state = hidden;
        if shape.len > 6 && shape[6] != 0 {
            state = giga_norm_at(ctx, hidden, weights, 0, tuning[0]);
            if state == 0 { return -1; }
        }
        giga_expand(graph, state);
        out[0] = state;
        out[1] = 0;
        return 0;
    }
    return giga_finish_logits(ctx, graph, hidden, weights, 0, weights[4], batch[4],
                              tuning[0], tuning[4], out);
}

fn giga_finish_logits(ctx: u64, graph: u64, hidden: u64, w: u64[:], norm_offset: i64,
                       head: u64, ids: u64, epsilon: f32, softcap: f32, out: u64[:]) -> i64 {
    if head == 0 || out.len < 2 { return -1; }
    let normalized = giga_norm_at(ctx, hidden, w, norm_offset, epsilon);
    if normalized == 0 { return -1; }
    var selected = normalized;
    if ids != 0 { selected = giga_get_rows(ctx, normalized, ids); }
    var logits = giga_mat(ctx, head, selected);
    if softcap > 0.0 {
        logits = giga_scale(ctx, giga_tanh(ctx, giga_scale(ctx, logits, 1.0 / softcap)), softcap);
    }
    giga_expand(graph, logits);
    out[0] = normalized;
    out[1] = logits;
    return 0;
}
