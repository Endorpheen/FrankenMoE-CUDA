// IQ1_M decode in Goose, from ggml-quants.c: dequantize_row_iq1_m.
//
// block_iq1_m is 56 bytes in this fork, verified with offsetof: qs[32] 0, qh[16] 32, scales[8] 48.
// The scale is itself an fp16 assembled bit by bit from the top nibbles of four 16-bit scale words.
// Eight groups of 32 outputs; within a group qs[0..3] feed four grid indices widened by qh bits, the
// first two use dl1 and the last two dl2, and each of the four has its own delta sign taken from qh.
extern "fg326q_ld_u8" fn ld_u8(a: u64) -> i64;
extern "fg326q_ld_u16" fn ld_u16(a: u64) -> i64;
extern "fg326q_half_f32" fn half_f32(v: u64) -> f32;
extern "fg326q_st_f32" fn st_f32(a: u64, v: f32);
extern "fg326q_grid" fn grid(code: i64) -> i64;
extern "fg326q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg326q_delta(bit: i64) -> f32 {
    return if bit != (0 as i64) {
        0 as f32 - (0.125 as f32)
    } else {
        0.125 as f32
    }
    ;
}

fn fg326q_emit(gi: i64, dl: f32, dt: f32, y: u64) {
    var j: i64 = 0 as i64;

    while j < (8 as i64) {
        let raw: i64 = (gi >> (j * (8 as i64))) & (255 as i64);
        let sv: i64 = if raw >= (128 as i64) {
            raw - (256 as i64)
        } else {
            raw
        }
        ;
        st_f32(y + (j as u64) * (4 as u64), dl * ( (sv as f32) + dt));
        j = j + (1 as i64);
    }
}

fn fg326q_block(src: u64, dst: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let base: u64 = src + ui * (56 as u64);
    let qs: u64 = base;
    let qh: u64 = base + (32 as u64);
    let scp: u64 = base + (48 as u64);
    let sc0: i64 = ld_u16(scp);
    let sc1: i64 = ld_u16(scp + (2 as u64));
    let sc2: i64 = ld_u16(scp + (4 as u64));
    let sc3: i64 = ld_u16(scp + (6 as u64));
    let bits: i64 = (sc0 >> (12 as i64)) | ( (sc1 >> (8 as i64)) & (0x00f0 as i64)) | ( (sc2 >> (4 as i64)) & (0x0f00 as i64)) | (sc3 & (0xf000 as i64));
    let d: f32 = half_f32(bits as u64);
    let y0: u64 = dst + ui * (1024 as u64);
    var ib: i64 = 0 as i64;

    while ib < (8 as i64) {
        let hf: i64 = ib / (2 as i64);
        let scw: i64 = if hf == (0 as i64) {
            sc0
        } else {
            if hf == (1 as i64) {
                sc1
            } else {
                if hf == (2 as i64) {
                    sc2
                } else {
                    sc3
                }
            }
        }
        ;
        let sh: i64 = ( (ib % (2 as i64)) * (6 as i64));
        let b1: i64 = (scw >> sh) & (7 as i64);
        let b2: i64 = (scw >> (sh + (3 as i64))) & (7 as i64);
        let dl1: f32 = d * ( ( (2 as i64) * b1 + (1 as i64)) as f32);
        let dl2: f32 = d * ( ( (2 as i64) * b2 + (1 as i64)) as f32);
        let h0: i64 = ld_u8(qh + (ib as u64) * (2 as u64));
        let h1: i64 = ld_u8(qh + (ib as u64) * (2 as u64) + (1 as u64));
        let q0: i64 = ld_u8(qs + (ib as u64) * (4 as u64));
        let q1: i64 = ld_u8(qs + (ib as u64) * (4 as u64) + (1 as u64));
        let q2: i64 = ld_u8(qs + (ib as u64) * (4 as u64) + (2 as u64));
        let q3: i64 = ld_u8(qs + (ib as u64) * (4 as u64) + (3 as u64));
        let i0: i64 = q0 | ( (h0 << (8 as i64)) & (0x700 as i64));
        let i1: i64 = q1 | ( (h0 << (4 as i64)) & (0x700 as i64));
        let i2: i64 = q2 | ( (h1 << (8 as i64)) & (0x700 as i64));
        let i3: i64 = q3 | ( (h1 << (4 as i64)) & (0x700 as i64));
        let yb: u64 = y0 + (ib as u64) * (128 as u64);
        fg326q_emit(grid(i0), dl1, fg326q_delta(h0 & (8 as i64)), yb);
        fg326q_emit(grid(i1), dl1, fg326q_delta(h0 & (0x80 as i64)), yb + (32 as u64));
        fg326q_emit(grid(i2), dl2, fg326q_delta(h1 & (8 as i64)), yb + (64 as u64));
        fg326q_emit(grid(i3), dl2, fg326q_delta(h1 & (0x80 as i64)), yb + (96 as u64));
        ib = ib + (1 as i64);
    }
    return 1 as i64;
}

fn fg326q_decode(src: u64, dst: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 1900 as i64);
    ensure(k % (256 as i64) == (0 as i64), 1901 as i64);
    var nb: i64 = k / (256 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg326q_block(src, dst, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg326q_decode(0 as u64, 0 as u64, 0 as i64);
}

