// TQ2_0 encode in Goose, from ggml-quants.c: quantize_row_tq2_0_ref.
//
// block_tq2_0 is 66 bytes: qs[64] 0, d (f16) 64. QK_K = 256. The scale is the block absolute max; the
// reciprocal uses the unrounded value while the stored d is its fp16 rounding, so the two are not
// interchangeable. Each 32-byte group covers four 32-element slices, and each value becomes a two-bit code
// (round, then +1 so -1/0/1 map to 0/1/2) shifted by two bits per slice.
extern "fg333q_ld_f32" fn ld_f32(a: u64) -> f32;
extern "fg333q_st_u8" fn st_u8(a: u64, v: i64);
extern "fg333q_st_f16" fn st_f16(a: u64, v: f32);
extern "fg333q_abs" fn absf(v: f32) -> f32;
extern "fg333q_lround" fn lroundf(v: f32) -> i64;
extern "fg333q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn mx(a: f32, b: f32) -> f32 {
    if a > b {
        return a;
    }
    return b;
}

fn fg333q_block(x: u64, y: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let xb: u64 = x + ui * (1024 as u64);
    let yb: u64 = y + ui * (66 as u64);
    var amax: f32 = 0 as f32;
    var j: i64 = 0 as i64;

    while j < (256 as i64) {
        amax = mx(amax, absf(ld_f32(xb + (j as u64) * (4 as u64))));
        j = j + (1 as i64);
    }
    let d: f32 = amax;
    let id: f32 = if d != (0 as f32) {
        (1 as f32) / d
    } else {
        0 as f32
    }
    ;
    st_f16(yb + (64 as u64), d);
    var g: i64 = 0 as i64;

    while g < (2 as i64) {
        let xg: u64 = xb + (g as u64) * (512 as u64);
        var m: i64 = 0 as i64;

        while m < (32 as i64) {
            var q: i64 = 0 as i64;
            var n: i64 = 0 as i64;

            while n < (4 as i64) {
                let xi: i64 = lroundf(ld_f32(xg + ( (m + (n * (32 as i64))) as u64) * (4 as u64)) * id) + (1 as i64);
                q = q + ( (xi & (3 as i64)) << (n * (2 as i64)));
                n = n + (1 as i64);
            }
            st_u8(yb + ( (g * (32 as i64) + m) as u64), q);
            m = m + (1 as i64);
        }
        g = g + (1 as i64);
    }
    return 0 as i64;
}

fn fg333q_encode(x: u64, y: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 2300 as i64);
    ensure(k % (256 as i64) == (0 as i64), 2301 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg333q_block(x, y, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg333q_encode(0 as u64, 0 as u64, 0 as i64);
}

