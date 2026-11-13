// Apex CUDA timestep embedding in Goose, from ggml-cuda/tsembd.cu.
//
// Device side: the odd-dim centre zero, the half bound, freq = exp(-log(max_period)*j/half),
// arg = timestep*freq, and the cos/sin pair.
// Host side: grid = (ceil(((dim+1)/2)/256), ne00, 1), block 256, and input validation.
//
// All control flow and arithmetic is Goose. Externs are a float load/store and
// the four scalar math functions, compiled in the same translation unit with the
// same fast-math flags as the native kernel.
extern "fg276d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg276d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg276d_expf" fn expf(x: f32) -> f32;
extern "fg276d_logf" fn logf(x: f32) -> f32;
extern "fg276d_cosf" fn cosf(x: f32) -> f32;
extern "fg276d_sinf" fn sinf(x: f32) -> f32;
extern "fg276d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn tsembd(dst: u64, timesteps: u64, nb1: i64, dim: i64, max_period: i64, i: i64, j: i64) {
    ensure(dim > (0 as i64) && max_period > (0 as i64), 20);
    let half: i64 = dim / 2;
    let base: i64 = i * nb1;

    if (dim % 2) != (0 as i64) {
        if j == half {
            st_f32(dst, ( (base + (2 * half) * 4 as i64) as u64), 0.0 as f32);
        }
    }

    if j >= half as i64 {
        return;
    }
    let timestep: f32 = ld_f32(timesteps, ( (i * 4 as i64) as u64));
    let lg: f32 = logf( (max_period as i64) as f32);
    let freq: f32 = expf( ( (0.0 as f32) - lg) * ( (j) as f32) / ( (half) as f32));
    let arg: f32 = timestep * freq;
    st_f32(dst, ( (base + (j * 4 as i64)) as u64), cosf(arg));
    st_f32(dst, ( (base + ( (j + half) * 4 as i64)) as u64), sinf(arg));
}
// Retained entry; the parent never executes main.

fn main() {
    tsembd(0, 0, 0 as i64, 0 as i64, 0 as i64, 0 as i64, 0 as i64);
}

