// IQ3_XXS decode in Goose, from ggml-quants.c: dequantize_row_iq3_xxs.
//
// block_iq3_xxs is 98 bytes in this fork, verified with offsetof: d (f16) 0, qs[96] 2. The first 64 qs bytes
// carry two 8-bit grid codes per l (eight bytes per group); the last 32 are scales_and_signs, four bytes per
// group where the top nibble scales and the remaining 28 bits give four 7-bit sign indices.
// Note the group factor here is 0.5, unlike IQ2_XXS's 0.25.
extern "fg322q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg322q_ld_u32" fn ld_u32(a: u64) -> i64;
extern "fg322q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg322q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg322q_grid" fn grid(code: i64) -> i64;
extern "fg322q_signs" fn signs(idx: i64) -> i64;
extern "fg322q_kmask" fn kmask(j: i64) -> i64;
extern "fg322q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg322q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (98 as u64);
    let d: f32 = ld_f16(base);
    let qs: u64 = base + (2 as u64);
    let sas: u64 = qs + (64 as u64);
    let y0: u64 = dst + ui * (1024 as u64);
    var ib: i64 = 0 as i64;

    while ib < (8 as i64) {
        let aux: i64 = ld_u32(sas + (ib as u64) * (4 as u64));
        let db: f32 = d * (0.5 as f32 + ( (aux >> (28 as i64)) as f32)) * (0.5 as f32);
        var l: i64 = 0 as i64;

        while l < (4 as i64) {
            let sg: i64 = signs( (aux >> (l * (7 as i64))) & (127 as i64));
            let qb: u64 = qs + (ib as u64) * (8 as u64);
            let c1: i64 = ld_u8(qb + ( (l * (2 as i64)) as u64));
            let c2: i64 = ld_u8(qb + ( (l * (2 as i64) + (1 as i64)) as u64));
            let g1: i64 = grid(c1);
            let g2: i64 = grid(c2);
            let y: u64 = y0 + (ib as u64) * (128 as u64) + (l as u64) * (32 as u64);
            var j: i64 = 0 as i64;

            while j < (4 as i64) {
                let v1: i64 = (g1 >> (j * (8 as i64))) & (255 as i64);
                let v2: i64 = (g2 >> (j * (8 as i64))) & (255 as i64);
                let n1: bool = (sg & kmask(j)) != (0 as i64);
                let n2: bool = (sg & kmask(j + (4 as i64))) != (0 as i64);
                let s1: f32 = if n1 {
                    0 as f32 - (1 as f32)
                } else {
                    1 as f32
                }
                ;
                let s2: f32 = if n2 {
                    0 as f32 - (1 as f32)
                } else {
                    1 as f32
                }
                ;
                st_f32(y + (j as u64) * (4 as u64), db * (v1 as f32) * s1);
                st_f32(y + (j as u64) * (4 as u64) + (16 as u64), db * (v2 as f32) * s2);
                j = j + (1 as i64);
            }
            l = l + (1 as i64);
        }
        ib = ib + (1 as i64);
    }
    return 1 as i64;
}

fn fg322q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 1500 as i64);
    ensure(k % (256 as i64) == (0 as i64), 1501 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg322q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg322q_decode(0 as u64, 0 as u64, 0 as i64);
}

