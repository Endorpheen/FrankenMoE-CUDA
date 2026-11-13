// NVFP4 decode in Goose, from ggml-quants.c: dequantize_row_nvfp4.
//
// block_nvfp4 is 36 bytes in this fork, verified with offsetof: d[4] (u8) 0, qs[32] 4. QK_NVFP4 is 64 in
// four sub-blocks of 16, each with its own UE4M3 scale byte and eight quant bytes whose low nibbles are the
// first eight values and high nibbles the last eight. Values come from the same doubled e2m1 table as MXFP4.
extern "fg327q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg327q_ue4m3" fn ue4m3(v: u64) -> f32;
extern "fg327q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg327q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg327q_val(i: i64) -> i64 {
    if i == (0 as i64) {
        return 0 as i64;
    }

    if i == (1 as i64) {
        return 1 as i64;
    }

    if i == (2 as i64) {
        return 2 as i64;
    }

    if i == (3 as i64) {
        return 3 as i64;
    }

    if i == (4 as i64) {
        return 4 as i64;
    }

    if i == (5 as i64) {
        return 6 as i64;
    }

    if i == (6 as i64) {
        return 8 as i64;
    }

    if i == (7 as i64) {
        return 12 as i64;
    }

    if i == (8 as i64) {
        return 0 as i64;
    }

    if i == (9 as i64) {
        return 0 as i64 - (1 as i64);
    }

    if i == (10 as i64) {
        return 0 as i64 - (2 as i64);
    }

    if i == (11 as i64) {
        return 0 as i64 - (3 as i64);
    }

    if i == (12 as i64) {
        return 0 as i64 - (4 as i64);
    }

    if i == (13 as i64) {
        return 0 as i64 - (6 as i64);
    }

    if i == (14 as i64) {
        return 0 as i64 - (8 as i64);
    }
    return 0 as i64 - (12 as i64);
}

fn fg327q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (36 as u64);
    let qs: u64 = base + (4 as u64);
    let y0: u64 = dst + ui * (256 as u64);
    var s: i64 = 0 as i64;

    while s < (4 as i64) {
        let d: f32 = ue4m3(ld_u8(base + (s as u64)) as u64);
        let qb: u64 = qs + (s as u64) * (8 as u64);
        let y: u64 = y0 + (s as u64) * (64 as u64);
        var j: i64 = 0 as i64;

        while j < (8 as i64) {
            let uj: u64 = j as u64;
            let b: i64 = ld_u8(qb + uj);
            st_f32(y + uj * (4 as u64), d * (fg327q_val(b & (15 as i64)) as f32));
            st_f32(y + (uj + (8 as u64)) * (4 as u64), d * (fg327q_val(b >> (4 as i64)) as f32));
            j = j + (1 as i64);
        }
        s = s + (1 as i64);
    }
    return 1 as i64;
}

fn fg327q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 2000 as i64);
    ensure(k % (64 as i64) == (0 as i64), 2001 as i64);
    var nb: i64 = k / (64 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg327q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg327q_decode(0 as u64, 0 as u64, 0 as i64);
}

