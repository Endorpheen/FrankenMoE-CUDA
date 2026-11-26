extern "ggml_rms_norm" fn giga_rms(ctx: u64, x: u64, epsilon: f32) -> u64;
extern "ggml_mul" fn giga_mul(ctx: u64, a: u64, b: u64) -> u64;
extern "ggml_mul_mat" fn giga_mat(ctx: u64, w: u64, x: u64) -> u64;
extern "ggml_silu" fn giga_silu(ctx: u64, x: u64) -> u64;
extern "ggml_sigmoid" fn giga_sigmoid(ctx: u64, x: u64) -> u64;
extern "ggml_scale" fn giga_scale(ctx: u64, x: u64, scale: f32) -> u64;

export fn giga_gated_norm(ctx: u64, x: u64, weight: u64,
                          up: u64, down: u64, epsilon: f32) -> u64 {
    if ctx == 0 || x == 0 || weight == 0 || up == 0 || down == 0 || !(epsilon > 0.0) { return 0; }
    let normalized = giga_mul(ctx, giga_rms(ctx, x, epsilon), weight);
    let hidden = giga_silu(ctx, giga_mat(ctx, up, normalized));
    let gate = giga_sigmoid(ctx, giga_mat(ctx, down, hidden));
    return giga_scale(ctx, giga_mul(ctx, normalized, gate), 2.0);
}
