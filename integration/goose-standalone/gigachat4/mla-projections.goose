extern "ggml_view_2d" fn giga_view2(ctx: u64, x: u64, a: i64, b: i64,
                                    stride: u64, offset: u64) -> u64;
extern "ggml_permute" fn giga_permute(ctx: u64, x: u64, a: i32, b: i32, c: i32, d: i32) -> u64;

// Weight order follows the DSA branch of the GGUF tensor bindings.
export fn giga_mla_project(ctx: u64, input: u64, pos: u64, weights: u64[:],
                           tokens: i64, epsilon: f32, original_ctx: i32,
                           factors: u64, rope: f32[:], out: u64[:]) -> i64 {
    if ctx == 0 || input == 0 || pos == 0 || weights.len < 13 ||
       tokens < 1 || !(epsilon > 0.0) || rope.len < 6 || out.len < 4 { return -1; }
    for i in 13 { if weights[i] == 0 { return -1; } }

    let qr = giga_scale(ctx,
        giga_gated_norm(ctx, giga_mat(ctx, weights[1], input),
                        weights[7], weights[8], weights[9], epsilon),
        giga_sqrt(2560.0 / 1536.0));
    let q = giga_mat(ctx, weights[2], qr);
    let q_nope = giga_view3(ctx, q, 128, 32, tokens, 768, 24576, 0);
    let q_pe_raw = giga_view3(ctx, q, 64, 32, tokens, 768, 24576, 512);

    let kv = giga_mat(ctx, weights[3], input);
    let kv_raw = giga_view2(ctx, kv, 512, tokens, 2304, 0);
    let k_pe_raw = giga_view3(ctx, kv, 64, 1, tokens, 2304, 2304, 2048);
    let q_pe = giga_rope(ctx, q_pe_raw, pos, factors, 64, 0, original_ctx,
                         rope[0], rope[1], rope[2], rope[3], rope[4], rope[5]);
    let k_pe = giga_rope(ctx, k_pe_raw, pos, factors, 64, 0, original_ctx,
                         rope[0], rope[1], rope[2], rope[3], rope[4], rope[5]);

    let compressed = giga_scale(ctx,
        giga_gated_norm(ctx, kv_raw, weights[10], weights[11], weights[12], epsilon),
        giga_sqrt(5.0));
    let absorbed = giga_mat(ctx, weights[4], giga_permute(ctx, q_nope, 0, 2, 1, 3));
    out[0] = giga_concat(ctx, giga_permute(ctx, absorbed, 0, 2, 1, 3), q_pe, 0);
    out[1] = giga_concat(ctx, giga_reshape3(ctx, compressed, 512, 1, tokens), k_pe, 0);
    out[2] = qr;
    out[3] = giga_mat(ctx, weights[0], input);
    return 0;
}
