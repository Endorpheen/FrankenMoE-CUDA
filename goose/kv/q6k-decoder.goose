// Q6_K decode in Goose, from ggml-quants.c: dequantize_row_q6_K.
//
// block_q6_K is 210 bytes and this fork orders it ql[128], qh[64], scales[16] (int8), d (f16):
// offsets 0, 128, 192, 208 respectively (verified with offsetof, not assumed).
// QK_K = 256, processed as two halves of 128; within a half, 32 indices each produce four outputs
// at +0/+32/+64/+96, using scale entries is+0/2/4/6 where is = l/16.
extern "fg311q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg311q_ld_i8" fn ld_i8(a: u64) -> i64;
extern "fg311q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg311q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg311q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg311q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (210 as u64);
    // this fork orders the block as ql[128], qh[64], scales[16], d -- d sits at byte 208
    let d: f32 = ld_f16(base + (208 as u64));
    let ql: u64 = base;
    let qh: u64 = base + (128 as u64);
    let sc: u64 = base + (192 as u64);
    var half: i64 = 0 as i64;
    var y: u64 = dst + ui * (1024 as u64);

    while half < (2 as i64) {
        var l: i64 = 0 as i64;
        let ql_h: u64 = ql + (half as u64) * (64 as u64);
        let qh_h: u64 = qh + (half as u64) * (32 as u64);
        let sc_h: u64 = sc + (half as u64) * (8 as u64);

        while l < (32 as i64) {
            let ul: u64 = l as u64;
            let q1: i64 = ( (ld_u8(ql_h + ul) & (15 as i64)) | ( ( (ld_u8(qh_h + ul) >> (0 as i64)) & (3 as i64)) << (4 as i64))) - (32 as i64);
            let q2: i64 = ( (ld_u8(ql_h + ul + (32 as u64)) & (15 as i64)) | ( ( (ld_u8(qh_h + ul) >> (2 as i64)) & (3 as i64)) << (4 as i64))) - (32 as i64);
            let q3: i64 = ( (ld_u8(ql_h + ul) >> (4 as i64)) | ( ( (ld_u8(qh_h + ul) >> (4 as i64)) & (3 as i64)) << (4 as i64))) - (32 as i64);
            let q4: i64 = ( (ld_u8(ql_h + ul + (32 as u64)) >> (4 as i64)) | ( ( (ld_u8(qh_h + ul) >> (6 as i64)) & (3 as i64)) << (4 as i64))) - (32 as i64);
            let is: i64 = l / (16 as i64);
            let s1: f32 = (ld_i8(sc_h + (is as u64))) as f32;
            let s2: f32 = (ld_i8(sc_h + ( (is + (2 as i64)) as u64))) as f32;
            let s3: f32 = (ld_i8(sc_h + ( (is + (4 as i64)) as u64))) as f32;
            let s4: f32 = (ld_i8(sc_h + ( (is + (6 as i64)) as u64))) as f32;
            st_f32(y + ul * (4 as u64), d * s1 * ( (q1) as f32));
            st_f32(y + (ul + (32 as u64)) * (4 as u64), d * s2 * ( (q2) as f32));
            st_f32(y + (ul + (64 as u64)) * (4 as u64), d * s3 * ( (q3) as f32));
            st_f32(y + (ul + (96 as u64)) * (4 as u64), d * s4 * ( (q4) as f32));
            l = l + (1 as i64);
        }
        y = y + (512 as u64);
        half = half + (1 as i64);
    }
    return 1 as i64;
}

fn fg311q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 400 as i64);
    ensure(k % (256 as i64) == (0 as i64), 401 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg311q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg311q_decode(0 as u64, 0 as u64, 0 as i64);
}

