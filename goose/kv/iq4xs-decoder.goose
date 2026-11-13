// IQ4_XS decode in Goose, from ggml-quants.c: dequantize_row_iq4_xs.
//
// block_iq4_xs is 136 bytes in this fork, verified with offsetof: d (f16) 0, scales_h (u16) 2,
// scales_l[4] 4, qs[128] 8. QK_K = 256 in eight groups of 32; each group reads 16 bytes whose low
// nibbles are outputs 0..15 and high nibbles 16..31, scaled by (ls - 32) with ls assembled from a
// 4-bit low part and a 2-bit high part.
extern "fg316q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg316q_ld_u16" fn ld_u16(a: u64) -> i64;
extern "fg316q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg316q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg316q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg316q_val(i: i64) -> i64 {
    if i == (0 as i64) {
        return 0 as i64 - (127 as i64);
    }

    if i == (1 as i64) {
        return 0 as i64 - (104 as i64);
    }

    if i == (2 as i64) {
        return 0 as i64 - (83 as i64);
    }

    if i == (3 as i64) {
        return 0 as i64 - (65 as i64);
    }

    if i == (4 as i64) {
        return 0 as i64 - (49 as i64);
    }

    if i == (5 as i64) {
        return 0 as i64 - (35 as i64);
    }

    if i == (6 as i64) {
        return 0 as i64 - (22 as i64);
    }

    if i == (7 as i64) {
        return 0 as i64 - (10 as i64);
    }

    if i == (8 as i64) {
        return 1 as i64;
    }

    if i == (9 as i64) {
        return 13 as i64;
    }

    if i == (10 as i64) {
        return 25 as i64;
    }

    if i == (11 as i64) {
        return 38 as i64;
    }

    if i == (12 as i64) {
        return 53 as i64;
    }

    if i == (13 as i64) {
        return 69 as i64;
    }

    if i == (14 as i64) {
        return 89 as i64;
    }
    return 113 as i64;
}

fn fg316q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (136 as u64);
    let d: f32 = ld_f16(base);
    let sh: i64 = ld_u16(base + (2 as u64));
    let sl: u64 = base + (4 as u64);
    var qs: u64 = base + (8 as u64);
    var y: u64 = dst + ui * (1024 as u64);
    var ib: i64 = 0 as i64;

    while ib < (8 as i64) {
        let lob: i64 = ld_u8(sl + (ib / (2 as i64)) as u64);
        let lo: i64 = (lob >> ( (ib % (2 as i64)) * (4 as i64))) & (15 as i64);
        let hi: i64 = (sh >> (ib * (2 as i64))) & (3 as i64);
        let ls: i64 = lo | (hi << (4 as i64));
        let dl: f32 = d * ( (ls - (32 as i64)) as f32);
        var j: i64 = 0 as i64;

        while j < (16 as i64) {
            let uj: u64 = j as u64;
            let b: i64 = ld_u8(qs + uj);
            st_f32(y + uj * (4 as u64), dl * (fg316q_val(b & (15 as i64)) as f32));
            st_f32(y + (uj + (16 as u64)) * (4 as u64), dl * (fg316q_val(b >> (4 as i64)) as f32));
            j = j + (1 as i64);
        }
        y = y + (128 as u64);
        qs = qs + (16 as u64);
        ib = ib + (1 as i64);
    }
    return 1 as i64;
}

fn fg316q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 900 as i64);
    ensure(k % (256 as i64) == (0 as i64), 901 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg316q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg316q_decode(0 as u64, 0 as u64, 0 as i64);
}

