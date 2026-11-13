// CPU Q8_K activation quantization in Goose, from ggml-quants.c:
// quantize_row_q8_K_ref, reached through quantize_row_q8_K in libggml-cpu.
//
// This one is worth transferring because it is measurably scalar: the built
// libggml-cpu.so contains zero SIMD instructions in quantize_row_q8_K, so the code that
// runs in the Apex CPU MoE path is exactly this algorithm, not an AVX2 sibling. The
// companion dot products do have SIMD siblings, which is why they are out of reach.
//
// Everything algorithmic is Goose: the per-super-block abs-max scan, the zero-block
// early out, the -127/max inverse scale, the rounding and clamp to int8, the sixteen
// per-group sums, and the 1/iscale delta. Extern symbols are only the raw float load,
// the int8/int16 stores and the rounding/abs primitives. No GGML routine is called.
//
// Layout: block_q8_K is 292 bytes (float d at 0, int8 qs[256] at 4, int16 bsums[16] at
// 260). QK_K = 256.
extern "fg190q_ld_f32" fn ld_f32(a: u64) -> f32;
extern "fg190q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg190q_st_i8" fn st_i8(a: u64, v: i32);
extern "fg190q_st_i16" fn st_i16(a: u64, v: i32);
extern "fg190q_ld_i8" fn ld_i8(a: u64) -> i32;
extern "fg190q_round" fn rnd(v: f32) -> i32;
extern "fg190q_abs" fn fabsf32(v: f32) -> f32;
extern "fg190q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// One 256-element super-block. Returns 1 if it was written, 0 if it hit the all-zero
// early out (which still zeroes the quant bytes).

fn fg190q_block(src: u64, dst: u64, i: i64) -> i64 {
    // src is float[256] per super-block, so element j sits at byte 4*j
    let so: i64 = i * (256 as i64) * (4 as i64);
    var amax: f32 = 0.0;
    var maxv: f32 = 0.0;
    var j: i64 = 0;

    while j < 256 as i64 {
        let xv: f32 = ld_f32(src + ( (so + 4 * j) as u64));
        let ax: f32 = fabsf32(xv);

        if ax > amax {
            amax = ax;
            maxv = xv;
        }
        j += 1;
    }
    let yb: i64 = i * 292 as i64;

    if amax == 0.0 {
        st_f32(dst + ( (yb) as u64), 0.0);
        var z: i64 = 0;

        while z < 256 as i64 {
            st_i8(dst + ( (yb + (4 as i64) + z) as u64), 0);
            z += 1;
        }
        return 0 as i64;
    }
    // iscale = -127/max; the sign flip is deliberate, matching the native comment about
    // IQ2_XXS needing it for the AVX sibling.
    let iscale: f32 = (0.0 - 127.0 as f32) / maxv;
    j = 0;

    while j < 256 as i64 {
        let xv: f32 = ld_f32(src + ( (so + 4 * j) as u64));
        var v: i32 = rnd(iscale * xv);

        if v > 127 as i32 {
            v = 127 as i32;
        }
        st_i8(dst + ( (yb + (4 as i64) + j) as u64), v);
        j += 1;
    }
    var g: i64 = 0;

    while g < 16 as i64 {
        var sum: i32 = 0;
        var ii: i64 = 0;

        while ii < 16 as i64 {
            sum = sum + ld_i8(dst + ( (yb + (4 as i64) + g * (16 as i64) + ii) as u64));
            ii += 1;
        }
        st_i16(dst + ( (yb + (260 as i64) + 2 * g) as u64), sum);
        g += 1;
    }
    st_f32(dst + ( (yb) as u64), (1.0 as f32) / iscale);
    return 1 as i64;
}
// Row entry: nb super-blocks of 256 elements.

fn fg190q_row(k: i64, src: u64, dst: u64) {
    ensure(k > 0 && k % 256 == 0, 10);
    let nb: i64 = k / 256 as i64;
    var i: i64 = 0;

    while i < nb {
        fg190q_block(src, dst, i);
        i += 1;
    }
}

fn main() {
    fg190q_row(0, 0, 0);
}

