// Q2_K decode in Goose, from ggml-quants.c: dequantize_row_q2_K.
//
// block_q2_K is 84 bytes in this fork, verified with offsetof: scales[16] 0, qs[64] 16, d (f16) 80,
// dmin (f16) 82. QK_K = 256 in two halves of 128; within a half, four shifts and eight scale
// bytes: two 16-element runs per shift, the second from q[l+16].
extern "fg313q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg313q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg313q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg313q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg313q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (84 as u64);
    let d: f32 = ld_f16(base + (80 as u64));
    let mn: f32 = ld_f16(base + (82 as u64));
    let sc: u64 = base;
    var q: u64 = base + (16 as u64);
    var y: u64 = dst + ui * (1024 as u64);
    var is: i64 = 0 as i64;
    var n: i64 = 0 as i64;

    while n < (256 as i64) {
        var shift: i64 = 0 as i64;
        var jj: i64 = 0 as i64;

        while jj < (4 as i64) {
            let s1: i64 = ld_u8(sc + (is as u64));
            let dl1: f32 = d * ( (s1 & (15 as i64)) as f32);
            let ml1: f32 = mn * ( (s1 >> (4 as i64)) as f32);
            is = is + (1 as i64);
            let s2: i64 = ld_u8(sc + (is as u64));
            let dl2: f32 = d * ( (s2 & (15 as i64)) as f32);
            let ml2: f32 = mn * ( (s2 >> (4 as i64)) as f32);
            is = is + (1 as i64);
            var l: i64 = 0 as i64;

            while l < (16 as i64) {
                st_f32(y, dl1 * ( ( ( (ld_u8(q + (l as u64)) >> shift) & (3 as i64))) as f32) - ml1);
                y = y + (4 as u64);
                l = l + (1 as i64);
            }
            l = 0 as i64;

            while l < (16 as i64) {
                st_f32(y, dl2 * ( ( ( (ld_u8(q + ( (l + (16 as i64)) as u64)) >> shift) & (3 as i64))) as f32) - ml2);
                y = y + (4 as u64);
                l = l + (1 as i64);
            }
            shift = shift + (2 as i64);
            jj = jj + (1 as i64);
        }
        q = q + (32 as u64);
        n = n + (128 as i64);
    }
    return 1 as i64;
}

fn fg313q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 600 as i64);
    ensure(k % (256 as i64) == (0 as i64), 601 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg313q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg313q_decode(0 as u64, 0 as u64, 0 as i64);
}

