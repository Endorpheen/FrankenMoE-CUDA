// Q3_K decode in Goose, from ggml-quants.c: dequantize_row_q3_K.
//
// block_q3_K is 110 bytes in this fork, verified with offsetof: hmask[32] 0, qs[64] 32, scales[12] 96, d (f16) 108.
// The 12 scale bytes pack sixteen 6-bit scales; they are unpacked into the four little-endian words the
// reference builds with its aux[] shuffles. The sign comes from hmask with a mask that starts at 1 and
// shifts once per 16-element run, eight runs per block, so it is not reset between the two halves.
extern "fg314q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg314q_ld_u32" fn ld_u32(a: u64) -> i64;
extern "fg314q_ld_f16" fn ld_f16(a: u64) -> f32;
extern "fg314q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg314q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// signed byte i (0..15) of the unpacked scale words

fn fg314q_sc(a0: i64, a1: i64, a2: i64, a3: i64, i: i64) -> i64 {
    let w: i64 = if i < (4 as i64) {
        a0
    } else {
        if i < (8 as i64) {
            a1
        } else {
            if i < (12 as i64) {
                a2
            } else {
                a3
            }
        }
    }
    ;
    let sh: i64 = (i % (4 as i64)) * (8 as i64);
    let b: i64 = (w >> sh) & (255 as i64);
    return if b >= (128 as i64) {
        b - (256 as i64)
    } else {
        b
    }
    ;
}

fn fg314q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (110 as u64);
    let hm: u64 = base;
    let q0: u64 = base + (32 as u64);
    let scp: u64 = base + (96 as u64);
    let d: f32 = ld_f16(base + (108 as u64));
    // unpack the 6-bit scales exactly as the reference
    let w0: i64 = ld_u32(scp);
    let w1: i64 = ld_u32(scp + (4 as u64));
    let w2: i64 = ld_u32(scp + (8 as u64));
    let mask2: i64 = 0x0f0f0f0f as i64;
    let mask1: i64 = 0x03030303 as i64;
    let a2: i64 = ( (w0 >> (4 as i64)) & mask2) | ( ( ( (w2 >> (4 as i64)) & mask1) << (4 as i64)));
    let a3: i64 = ( (w1 >> (4 as i64)) & mask2) | ( ( ( (w2 >> (6 as i64)) & mask1) << (4 as i64)));
    let a0: i64 = (w0 & mask2) | ( ( ( (w2 >> (0 as i64)) & mask1) << (4 as i64)));
    let a1: i64 = (w1 & mask2) | ( ( ( (w2 >> (2 as i64)) & mask1) << (4 as i64)));
    var q: u64 = q0;
    var y: u64 = dst + ui * (1024 as u64);
    var m: i64 = 1 as i64;
    var is: i64 = 0 as i64;
    var half: i64 = 0 as i64;

    while half < (2 as i64) {
        var shift: i64 = 0 as i64;
        var jj: i64 = 0 as i64;

        while jj < (4 as i64) {
            let dl1: f32 = d * ( (fg314q_sc(a0, a1, a2, a3, is) - (32 as i64)) as f32);
            is = is + (1 as i64);
            var l: i64 = 0 as i64;

            while l < (16 as i64) {
                let ul: u64 = l as u64;
                let neg: i64 = if (ld_u8(hm + ul) & m) != (0 as i64) {
                    0 as i64
                } else {
                    4 as i64
                }
                ;
                st_f32(y, dl1 * ( ( ( ( (ld_u8(q + ul) >> shift) & (3 as i64)) - neg)) as f32));
                y = y + (4 as u64);
                l = l + (1 as i64);
            }
            let dl2: f32 = d * ( (fg314q_sc(a0, a1, a2, a3, is) - (32 as i64)) as f32);
            is = is + (1 as i64);
            l = 0 as i64;

            while l < (16 as i64) {
                let ul: u64 = (l + (16 as i64)) as u64;
                let neg: i64 = if (ld_u8(hm + ul) & m) != (0 as i64) {
                    0 as i64
                } else {
                    4 as i64
                }
                ;
                st_f32(y, dl2 * ( ( ( ( (ld_u8(q + ul) >> shift) & (3 as i64)) - neg)) as f32));
                y = y + (4 as u64);
                l = l + (1 as i64);
            }
            shift = shift + (2 as i64);
            m = m << (1 as i64);
            jj = jj + (1 as i64);
        }
        q = q + (32 as u64);
        half = half + (1 as i64);
    }
    return 1 as i64;
}

fn fg314q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 700 as i64);
    ensure(k % (256 as i64) == (0 as i64), 701 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg314q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg314q_decode(0 as u64, 0 as u64, 0 as i64);
}

