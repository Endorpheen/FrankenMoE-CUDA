// Apex CUDA snake and softcap in Goose, from ggml-cuda/snake.cu and softcap.cu.
//
// snake: y = x + sin^2(a[c]*x) * inv_b[c] with c = idx / T, for F32, F16 and
// BF16 data with F32 compute. The native uses fastdiv for the channel index; the
// fastdiv result equals the exact integer quotient, so Goose uses the division
// directly and keeps the same value.
// softcap: y = tanh(scale*x) * softcap.
//
// All arithmetic is Goose. Externs are typed loads/stores and the scalar sinf and
// tanhf, compiled with the same fast-math flags as the native kernels.
extern "fg286d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg286d_ld_half" fn ld_half(x: u64, o: u64) -> f32;
extern "fg286d_ld_bf16" fn ld_bf16(x: u64, o: u64) -> f32;
extern "fg286d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg286d_st_half" fn st_half(dst: u64, o: u64, v: f32);
extern "fg286d_st_bf16" fn st_bf16(dst: u64, o: u64, v: f32);
extern "fg286d_sinf" fn sinf(x: f32) -> f32;
extern "fg286d_tanhf" fn tanhf(x: f32) -> f32;
extern "fg286d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn snake_f32(x: u64, a: u64, invb: u64, dst: u64, total: i64, tlen: i64, idx: i64) {
    if idx >= total as i64 {
        return;
    }
    ensure(tlen > 0 as i64, 20);
    let c: i64 = idx / tlen;
    let xi: f32 = ld_f32(x, ( (idx * 4 as i64) as u64));
    let s: f32 = sinf(ld_f32(a, ( (c * 4 as i64) as u64)) * xi);
    st_f32(dst, ( (idx * 4 as i64) as u64), xi + s * s * ld_f32(invb, ( (c * 4 as i64) as u64)));
}

fn snake_f16(x: u64, a: u64, invb: u64, dst: u64, total: i64, tlen: i64, idx: i64) {
    if idx >= total as i64 {
        return;
    }
    ensure(tlen > 0 as i64, 20);
    let c: i64 = idx / tlen;
    let xi: f32 = ld_half(x, ( (idx * 2 as i64) as u64));
    let s: f32 = sinf(ld_f32(a, ( (c * 4 as i64) as u64)) * xi);
    st_half(dst, ( (idx * 2 as i64) as u64), xi + s * s * ld_f32(invb, ( (c * 4 as i64) as u64)));
}

fn snake_bf16(x: u64, a: u64, invb: u64, dst: u64, total: i64, tlen: i64, idx: i64) {
    if idx >= total as i64 {
        return;
    }
    ensure(tlen > 0 as i64, 20);
    let c: i64 = idx / tlen;
    let xi: f32 = ld_bf16(x, ( (idx * 2 as i64) as u64));
    let s: f32 = sinf(ld_f32(a, ( (c * 4 as i64) as u64)) * xi);
    st_bf16(dst, ( (idx * 2 as i64) as u64), xi + s * s * ld_f32(invb, ( (c * 4 as i64) as u64)));
}

fn softcap_f32(x: u64, dst: u64, scale: f32, cap: f32, k: i64, idx: i64) {
    if idx >= k as i64 {
        return;
    }
    st_f32(dst, ( (idx * 4 as i64) as u64), tanhf(scale * ld_f32(x, ( (idx * 4 as i64) as u64))) * cap);
}

fn main() {
    snake_f32(0, 0, 0, 0, 0 as i64, 1 as i64, 0 as i64);
    snake_f16(0, 0, 0, 0, 0 as i64, 1 as i64, 0 as i64);
    snake_bf16(0, 0, 0, 0, 0 as i64, 1 as i64, 0 as i64);
    softcap_f32(0, 0, 0.0 as f32, 0.0 as f32, 0 as i64, 0 as i64);
}

