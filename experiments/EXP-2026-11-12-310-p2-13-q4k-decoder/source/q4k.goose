// Q4_K decode in Goose, from ggml-quants.c: dequantize_row_q4_K.
//
// The executed build is scalar here: base-quants.o contains zero %xmm/%ymm/%zmm instructions in this
// function, so this is the code that runs rather than an AVX2 sibling.
//
// block_q4_K is 144 bytes: d (f16) at 0, dmin (f16) at 2, scales[12] at 4, qs[128] at 16. QK_K = 256.
// Per 64 outputs the low nibbles of 32 bytes come first with one (scale,min) pair, then the high
// nibbles of the same 32 bytes with the next pair, exactly as the reference orders them.
extern "fg310q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg310q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg310q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg310q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// get_scale_min_k4 packed as (m << 8) | sc

fn fg310q_sc(is: i64, q: u64) -> i64 {
    let si: u64 = is as u64;

    if is < (4 as i64) {
        return ( (ld_u8(q + si + (4 as u64)) & (63 as i64)) << (8 as i64)) | (ld_u8(q + si) & (63 as i64));
    }
    let d = (ld_u8(q + si + (4 as u64)) & (15 as i64)) | ( (ld_u8(q + si - (4 as u64)) >> (6 as i64)) << (4 as i64));
    let m = (ld_u8(q + si + (4 as u64)) >> (4 as i64)) | ( (ld_u8(q + si) >> (6 as i64)) << (4 as i64));
    return (m << (8 as i64)) | d;
}

fn fg310q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (144 as u64);
    let d: f32 = ld_f16(base);
    let mn: f32 = ld_f16(base + (2 as u64));
    let sc_base: u64 = base + (4 as u64);
    var q: u64 = base + (16 as u64);
    var is: i64 = 0 as i64;
    var j: i64 = 0 as i64;
    var y: u64 = dst + ui * (1024 as u64);

    while j < (256 as i64) {
        let a: i64 = fg310q_sc(is, sc_base);
        let b: i64 = fg310q_sc(is + (1 as i64), sc_base);
        let d1: f32 = d * ( (a & (255 as i64)) as f32);
        let m1: f32 = mn * ( (a >> (8 as i64)) as f32);
        let d2: f32 = d * ( (b & (255 as i64)) as f32);
        let m2: f32 = mn * ( (b >> (8 as i64)) as f32);
        var l: i64 = 0 as i64;

        while l < (32 as i64) {
            st_f32(y, d1 * ( (ld_u8(q + (l as u64)) & (15 as i64)) as f32) - m1);
            y = y + (4 as u64);
            l = l + (1 as i64);
        }
        l = 0 as i64;

        while l < (32 as i64) {
            st_f32(y, d2 * ( (ld_u8(q + (l as u64)) >> (4 as i64)) as f32) - m2);
            y = y + (4 as u64);
            l = l + (1 as i64);
        }
        q = q + (32 as u64);
        is = is + (2 as i64);
        j = j + (64 as i64);
    }
    return 1 as i64;
}

fn fg310q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 300 as i64);
    ensure(k % (256 as i64) == (0 as i64), 301 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg310q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}
// The compiler requires a root-file entry point; the fixture drives fg310q_decode directly and the
// generated main is renamed out of the way by the bridge.

fn main() {
    fg310q_decode(0 as u64, 0 as u64, 0 as i64);
}

