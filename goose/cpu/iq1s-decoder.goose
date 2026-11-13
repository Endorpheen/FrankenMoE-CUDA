// IQ1_S decode in Goose, from ggml-quants.c: dequantize_row_iq1_s.
//
// block_iq1_s is 50 bytes in this fork, verified with offsetof: d (f16) 0, qs[32] 2, qh[8] (uint16) 34.
// Eight groups of 32 outputs: one qh word per group gives the grid's top three bits per l, a 3-bit scale
// (bits 12..14) and the delta sign (bit 15). Grid values are SIGNED bytes and the delta is added to the
// integer value before scaling, not after -- the two details that separate this from the other IQ formats.
extern "fg325q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg325q_ld_u16" fn ld_u16(a: u64) -> i64;
extern "fg325q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg325q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg325q_grid" fn grid(code: i64) -> i64;
extern "fg325q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg325q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (50 as u64);
    let d: f32 = ld_f16(base);
    let qs: u64 = base + (2 as u64);
    let qph: u64 = base + (34 as u64);
    let y0: u64 = dst + ui * (1024 as u64);
    var ib: i64 = 0 as i64;

    while ib < (8 as i64) {
        let qhb: i64 = ld_u16(qph + (ib as u64) * (2 as u64));
        let dl: f32 = d * ( (2 as i64) * ( (qhb >> (12 as i64)) & (7 as i64)) + (1 as i64)) as f32;
        let delta: f32 = if (qhb & (0x8000 as i64)) != (0 as i64) {
            0 as f32 - (0.125 as f32)
        } else {
            0.125 as f32
        }
        ;
        var l: i64 = 0 as i64;

        while l < (4 as i64) {
            let off: u64 = ( (ib * (4 as i64) + l) as u64);
            let code: i64 = (ld_u8(qs + off)) | ( ( (qhb >> (l * (3 as i64))) & (7 as i64)) << (8 as i64));
            let g: i64 = grid(code);
            let y: u64 = y0 + (ib as u64) * (128 as u64) + (l as u64) * (32 as u64);
            var j: i64 = 0 as i64;

            while j < (8 as i64) {
                let raw: i64 = (g >> (j * (8 as i64))) & (255 as i64);
                let sv: i64 = if raw >= (128 as i64) {
                    raw - (256 as i64)
                } else {
                    raw
                }
                ;
                st_f32(y + (j as u64) * (4 as u64), dl * ( (sv as f32) + delta));
                j = j + (1 as i64);
            }
            l = l + (1 as i64);
        }
        ib = ib + (1 as i64);
    }
    return 1 as i64;
}

fn fg325q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 1800 as i64);
    ensure(k % (256 as i64) == (0 as i64), 1801 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg325q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg325q_decode(0 as u64, 0 as u64, 0 as i64);
}

