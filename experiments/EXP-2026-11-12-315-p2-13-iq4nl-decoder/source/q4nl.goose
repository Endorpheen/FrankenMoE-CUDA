// IQ4_NL decode in Goose, from ggml-quants.c: dequantize_row_iq4_nl.
//
// block_iq4_nl is 18 bytes in this fork, verified with offsetof: d (f16) 0, qs[16] 2. QK4_NL = 32.
// Unlike the K-quants the two nibble halves are not interleaved: the sixteen low nibbles fill outputs
// 0..15 and the sixteen high nibbles fill 16..31. The 16-entry value table is embedded from
// ggml-common.h (kvalues_iq4nl), not loaded from a pointer.
extern "fg315q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg315q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg315q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg315q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg315q_val(i: i64) -> i64 {
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

fn fg315q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (18 as u64);
    let d: f32 = ld_f16(base);
    let qs: u64 = base + (2 as u64);
    let y: u64 = dst + ui * (128 as u64);
    var j: i64 = 0 as i64;

    while j < (16 as i64) {
        let uj: u64 = j as u64;
        let b: i64 = ld_u8(qs + uj);
        st_f32(y + uj * (4 as u64), d * (fg315q_val(b & (15 as i64)) as f32));
        st_f32(y + (uj + (16 as u64)) * (4 as u64), d * (fg315q_val(b >> (4 as i64)) as f32));
        j = j + (1 as i64);
    }
    return 1 as i64;
}

fn fg315q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 800 as i64);
    ensure(k % (32 as i64) == (0 as i64), 801 as i64);
    var nb: i64 = k / (32 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg315q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg315q_decode(0 as u64, 0 as u64, 0 as i64);
}

