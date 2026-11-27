extern "ggml_reshape_3d" fn giga_reshape3(ctx: u64, x: u64, a: i64, b: i64, c: i64) -> u64;
extern "ggml_reshape_4d" fn giga_reshape4(ctx: u64, x: u64, a: i64, b: i64, c: i64, d: i64) -> u64;
extern "ggml_add" fn giga_add(ctx: u64, a: u64, b: u64) -> u64;
extern "ggml_softplus" fn giga_softplus(ctx: u64, x: u64) -> u64;

export fn giga_gdn_projections(ctx: u64, input: u64, wqkv: u64, wz: u64,
                               wbeta: u64, walpha: u64, dt: u64, a: u64,
                               tokens: i64, seqs: i64, out: u64[:]) -> i64 {
    if ctx == 0 || input == 0 || wqkv == 0 || wz == 0 || wbeta == 0 ||
       walpha == 0 || dt == 0 || a == 0 || tokens < 1 || seqs < 1 ||
       tokens > 1048576 / seqs || out.len < 4 { return -1; }
    out[0] = giga_reshape3(ctx, giga_mat(ctx, wqkv, input), 8192, tokens, seqs);
    out[1] = giga_reshape4(ctx, giga_mat(ctx, wz, input), 128, 32, tokens, seqs);
    let beta = giga_reshape4(ctx, giga_mat(ctx, wbeta, input), 1, 32, tokens, seqs);
    out[2] = giga_sigmoid(ctx, beta);
    let alpha = giga_reshape3(ctx, giga_mat(ctx, walpha, input), 32, tokens, seqs);
    let decay = giga_mul(ctx, giga_softplus(ctx, giga_add(ctx, alpha, dt)), a);
    out[3] = giga_reshape4(ctx, decay, 1, 32, tokens, seqs);
    return 0;
}
