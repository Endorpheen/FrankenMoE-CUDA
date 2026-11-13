// IQ2_S decode in Goose, from ggml-quants.c: dequantize_row_iq2_s.
//
// block_iq2_s is 82 bytes in this fork, verified with offsetof: d (f16) 0, qs[64] 2, qh[8] 66, scales[8] 74.
// The qs array splits in half: codes at qs[0..31] and sign bytes at qs[32..63] (signs = qs + QK_K/8). Per group
// four code bytes, four sign bytes, one scale byte (low nibble for l<2, high for l>=2), and the eight-bit code is
// widened by two bits sliced out of qh[ib] with a per-l shift.
extern "fg323q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg323q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg323q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg323q_grid" fn grid(code: i64) -> i64;
extern "fg323q_kmask" fn kmask(j: i64) -> i64;
extern "fg323q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg323q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (82 as u64);
    let d: f32 = ld_f16(base);
    let qs: u64 = base + (2 as u64);
    let qh: u64 = base + (66 as u64);
    let scp: u64 = base + (74 as u64);
    let y0: u64 = dst + ui * (1024 as u64);
    var ib: i64 = 0 as i64;

    while ib < (8 as i64) {
        let sc: i64 = ld_u8(scp + (ib as u64));
        let db0: f32 = d * (0.5 as f32 + ( (sc & (15 as i64)) as f32)) * (0.25 as f32);
        let db1: f32 = d * (0.5 as f32 + ( (sc >> (4 as i64)) as f32)) * (0.25 as f32);
        let hb: i64 = ld_u8(qh + (ib as u64));
        var l: i64 = 0 as i64;

        while l < (4 as i64) {
            let off: u64 = ( (ib * (4 as i64) + l) as u64);
            let code: i64 = (ld_u8(qs + off)) | ( (hb << ( (8 as i64) - (2 as i64) * (l as i64))) & (0x300 as i64));
            let sg: i64 = ld_u8(qs + off + (32 as u64));
            let g: i64 = grid(code);
            let db: f32 = if l < (2 as i64) {
                db0
            } else {
                db1
            }
            ;
            let y: u64 = y0 + (ib as u64) * (128 as u64) + (l as u64) * (32 as u64);
            var j: i64 = 0 as i64;

            while j < (8 as i64) {
                let gv: i64 = (g >> (j * (8 as i64))) & (255 as i64);
                let neg: bool = (sg & kmask(j)) != (0 as i64);
                let s: f32 = if neg {
                    0 as f32 - (1 as f32)
                } else {
                    1 as f32
                }
                ;
                st_f32(y + (j as u64) * (4 as u64), db * (gv as f32) * s);
                j = j + (1 as i64);
            }
            l = l + (1 as i64);
        }
        ib = ib + (1 as i64);
    }
    return 1 as i64;
}

fn fg323q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 1600 as i64);
    ensure(k % (256 as i64) == (0 as i64), 1601 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg323q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg323q_decode(0 as u64, 0 as u64, 0 as i64);
}

