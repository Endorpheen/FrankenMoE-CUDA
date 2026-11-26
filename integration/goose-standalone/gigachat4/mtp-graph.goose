// Inputs are the draft token ids and normalized hidden states from the preceding pass.
// The cache tensors belong to the draft context.
export fn giga_mtp_graph(ctx: u64, graph: u64, hidden: u64, w: u64[:],
                         state: u64[:], batch: u64[:], shape: i64[:], strides: u64[:],
                         tuning: f32[:], dense_limit: f32, factors: u64,
                         rope: f32[:], index_rope: f32[:], out: u64[:]) -> i64 {
    if ctx == 0 || graph == 0 || hidden == 0 || w.len < 44 ||
       state.len < 3 || batch.len < 5 || shape.len < 6 ||
       strides.len < 2 || tuning.len < 5 || out.len < 2 ||
       batch[0] == 0 || w[41] == 0 || w[42] == 0 || w[43] == 0 ||
       shape[0] < 1 || shape[4] < 0 || shape[4] > shape[0] { return -1; }

    let embedded = giga_get_rows(ctx, w[42], batch[0]);
    let e = giga_norm_at(ctx, embedded, w, 32, tuning[0]);
    let h = giga_norm_at(ctx, hidden, w, 35, tuning[0]);
    if e == 0 || h == 0 { return -2; }
    let input = giga_mat(ctx, w[41], giga_concat(ctx, e, h, 0));
    let attended = giga_decoder_mla(ctx, graph, input, w, state, batch[1], batch[2], batch[3],
        shape[0], shape[1], shape[2], shape[3], strides[0], strides[1], tuning[0], tuning[1],
        shape[5] as i32, factors, rope, index_rope);
    let next = giga_decoder_ffn(ctx, graph, input, attended, w, 29, 0, shape[0],
        true, tuning[0], false, 0.0, 1.0, 0.0, dense_limit);
    if next == 0 { return -3; }
    if shape[4] == 0 {
        let normalized = giga_norm_at(ctx, next, w, 38, tuning[0]);
        if normalized == 0 { return -3; }
        giga_expand(graph, normalized);
        out[0] = normalized;
        out[1] = 0;
        return 0;
    }
    return giga_finish_logits(ctx, graph, next, w, 38, w[43], batch[4],
                              tuning[0], tuning[4], out);
}
