extern "ggml_transpose" fn giga_transpose(ctx: u64, x: u64) -> u64;
extern "ggml_concat" fn giga_concat(ctx: u64, a: u64, b: u64, dim: i32) -> u64;
extern "ggml_ssm_conv" fn giga_ssm_conv(ctx: u64, x: u64, weights: u64) -> u64;
extern "ggml_view_3d" fn giga_view3(ctx: u64, x: u64, a: i64, b: i64, c: i64,
                                    stride1: u64, stride2: u64, offset: u64) -> u64;
extern "ggml_view_4d" fn giga_view4(ctx: u64, x: u64, a: i64, b: i64, c: i64, d: i64,
                                    stride1: u64, stride2: u64, stride3: u64, offset: u64) -> u64;
extern "ggml_repeat_4d" fn giga_repeat4(ctx: u64, x: u64, a: i64, b: i64, c: i64, d: i64) -> u64;
extern "ggml_gated_delta_net" fn giga_delta(ctx: u64, q: u64, k: u64, v: u64,
                                           gate: u64, beta: u64, state: u64, keep: i64) -> u64;
extern "ggml_reshape_2d" fn giga_reshape2(ctx: u64, x: u64, a: i64, b: i64) -> u64;
extern "sqrtf" fn giga_sqrt(x: f32) -> f32;

fn giga_gdn_heads(ctx: u64, x: u64, tokens: i64, seqs: i64) -> u64 {
    let l2 = giga_scale(ctx, giga_rms(ctx, x, 0.000001 / 128.0), 1.0 / giga_sqrt(128.0));
    let grouped = giga_reshape4(ctx, l2, 128, 1, 16, tokens * seqs);
    let repeated = giga_repeat4(ctx, grouped, 128, 2, 16, tokens * seqs);
    return giga_reshape4(ctx, repeated, 128, 32, tokens, seqs);
}

export fn giga_gdn_layer(ctx: u64, projected: u64[:], conv_history: u64,
                         recurrent: u64, conv_weight: u64, norm_weight: u64,
                         output_weight: u64, tokens: i64, seqs: i64, epsilon: f32,
                         out: u64[:]) -> i64 {
    if ctx == 0 || conv_history == 0 || recurrent == 0 || conv_weight == 0 ||
       norm_weight == 0 || output_weight == 0 || projected.len < 4 || out.len < 3 ||
       tokens < 1 || seqs < 1 || tokens > 1048576 / seqs || !(epsilon > 0.0) { return -1; }
    for i in 4 { if projected[i] == 0 { return -1; } }

    let history = giga_reshape3(ctx, conv_history, 3, 8192, seqs);
    let joined = giga_concat(ctx, history, giga_transpose(ctx, projected[0]), 0);
    let sequence_stride = ((tokens + 3) * 4) as u64;
    let conv_tail = giga_view3(ctx, joined, 3, 8192, seqs,
                              sequence_stride, sequence_stride * 8192, (tokens * 4) as u64);
    let conv = giga_silu(ctx, giga_ssm_conv(ctx, joined, conv_weight));
    let stride: u64 = 8192 * 4;
    let seq_stride = stride * (tokens as u64);
    let q = giga_view4(ctx, conv, 128, 16, tokens, seqs, 512, stride, seq_stride, 0);
    let k = giga_view4(ctx, conv, 128, 16, tokens, seqs, 512, stride, seq_stride, 8192);
    let v = giga_view4(ctx, conv, 128, 32, tokens, seqs, 512, stride, seq_stride, 16384);
    let state = giga_reshape4(ctx, recurrent, 128, 128, 32, seqs);
    let fused = giga_delta(ctx, giga_gdn_heads(ctx, q, tokens, seqs),
                         giga_gdn_heads(ctx, k, tokens, seqs), v, projected[3],
                         projected[2], state, 1);
    let output_bytes = (4096 * tokens * 4) as u64;
    let values = giga_view4(ctx, fused, 128, 32, tokens, seqs, 512, 16384, output_bytes, 0);
    let new_state = giga_view4(ctx, fused, 128, 128, 32, seqs,
                              512, 65536, 2097152, output_bytes * (seqs as u64));
    let norm = giga_mul(ctx, giga_rms(ctx, values, epsilon), norm_weight);
    let gated = giga_scale(ctx, giga_mul(ctx, norm, giga_sigmoid(ctx, projected[1])), 2.0);
    let hidden = giga_reshape3(ctx, gated, 4096, tokens, seqs);
    out[0] = giga_reshape2(ctx, giga_mat(ctx, output_weight, hidden), 2560, tokens * seqs);
    out[1] = conv_tail;
    out[2] = new_state;
    return 0;
}
