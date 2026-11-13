// Q5_K decode in Goose, from ggml-quants.c: dequantize_row_q5_K.
//
// block_q5_K is 176 bytes in this fork, verified with offsetof: d (f16) 0, dmin (f16) 2,
// scales[12] 4, qh[32] 16, qs[128] 48. QK_K = 256, four groups of 64; the fifth bit for each
// element comes from qh with a per-group mask that shifts left by two every group.
extern "fg312q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg312q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg312q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg312q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg312q_sc(is: i64, q: u64) -> i64 {
    let si: u64 = is as u64;

    if is < (4 as i64) {
        return ( (ld_u8(q + si + (4 as u64)) & (63 as i64)) << (8 as i64)) | (ld_u8(q + si) & (63 as i64));
    }
    let d = (ld_u8(q + si + (4 as u64)) & (15 as i64)) | ( (ld_u8(q + si - (4 as u64)) >> (6 as i64)) << (4 as i64));
    let m = (ld_u8(q + si + (4 as u64)) >> (4 as i64)) | ( (ld_u8(q + si) >> (6 as i64)) << (4 as i64));
    return (m << (8 as i64)) | d;
}

fn fg312q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (176 as u64);
    let d: f32 = ld_f16(base);
    let mn: f32 = ld_f16(base + (2 as u64));
    let sc_base: u64 = base + (4 as u64);
    let qh: u64 = base + (16 as u64);
    var q: u64 = base + (48 as u64);
    var is: i64 = 0 as i64;
    var j: i64 = 0 as i64;
    var y: u64 = dst + ui * (1024 as u64);
    var u1: i64 = 1 as i64;
    var u2: i64 = 2 as i64;

    while j < (256 as i64) {
        let a: i64 = fg312q_sc(is, sc_base);
        let b: i64 = fg312q_sc(is + (1 as i64), sc_base);
        let d1: f32 = d * ( (a & (255 as i64)) as f32);
        let m1: f32 = mn * ( (a >> (8 as i64)) as f32);
        let d2: f32 = d * ( (b & (255 as i64)) as f32);
        let m2: f32 = mn * ( (b >> (8 as i64)) as f32);
        var l: i64 = 0 as i64;

        while l < (32 as i64) {
            let ul: u64 = l as u64;
            let hi1: i64 = if (ld_u8(qh + ul) & u1) != (0 as i64) {
                16 as i64
            } else {
                0 as i64
            }
            ;
            let hi2: i64 = if (ld_u8(qh + ul) & u2) != (0 as i64) {
                16 as i64
            } else {
                0 as i64
            }
            ;
            st_f32(y, d1 * ( ( (ld_u8(q + ul) & (15 as i64)) + hi1) as f32) - m1);
            y = y + (4 as u64);
            l = l + (1 as i64);
        }
        l = 0 as i64;

        while l < (32 as i64) {
            let ul: u64 = l as u64;
            let hi2: i64 = if (ld_u8(qh + ul) & u2) != (0 as i64) {
                16 as i64
            } else {
                0 as i64
            }
            ;
            st_f32(y, d2 * ( ( (ld_u8(q + ul) >> (4 as i64)) + hi2) as f32) - m2);
            y = y + (4 as u64);
            l = l + (1 as i64);
        }
        q = q + (32 as u64);
        is = is + (2 as i64);
        u1 = u1 << (2 as i64);
        u2 = u2 << (2 as i64);
        j = j + (64 as i64);
    }
    return 1 as i64;
}

fn fg312q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 500 as i64);
    ensure(k % (256 as i64) == (0 as i64), 501 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg312q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg312q_decode(0 as u64, 0 as u64, 0 as i64);
}

