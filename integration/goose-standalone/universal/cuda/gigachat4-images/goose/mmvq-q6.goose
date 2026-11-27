extern "fg186q_ld_i32" fn ld_i32(src: u64, byte: u64) -> i32;
extern "fg186q_ld_i16" fn ld_i16(src: u64, byte: u64) -> u32;
extern "fg186q_ld_i8" fn ld_i8(src: u64, byte: u64) -> i32;
extern "fg186q_ld_half" fn ld_half(src: u64, byte: u64) -> f32;
extern "fg186q_dp4a" fn dp4a(a: i32, b: i32, c: i32) -> i32;
fn ld_b2(src: u64, byte: i64) -> i32 {
    let lo: u32 = ld_i16(src, (byte as u64));
    let hi: u32 = ld_i16(src, ( (byte + (2 as i64)) as u64));
    return ((hi << 16) | (lo & 65535 as u32)) as !i32;
}
fn impl_mmvq(
    vl0: i32,
    vl1: i32,
    vh0: i32,
    vh1: i32,
    u0: i32,
    u1: i32,
    u2: i32,
    u3: i32,
    sc0: i32,
    sc1: i32,
    d: f32,
    d8_0: f32,
    d8_1: f32) -> f32 {
    var sum: f32 = 0.0;
    // i = 0: nibbles 0..3, q8 words u0 and u1
    let l0: i32 = (vl0 >> 0) & (0x0F0F0F0F as i32);
    let h0: i32 = ( (vh0 >> 0) << 4) & (0x30303030 as i32);
    var s0: i32 = dp4a(l0 | h0, u0, 0 as i32);
    let offset = 0xE0E0E0E0 as !i32;
    s0 = dp4a(offset, u0, s0);
    let l1: i32 = (vl1 >> 0) & (0x0F0F0F0F as i32);
    let h1: i32 = ( (vh1 >> 0) << 4) & (0x30303030 as i32);
    s0 = dp4a(l1 | h1, u1, s0);
    s0 = dp4a(offset, u1, s0);
    sum = sum + d8_0 * ( (s0 * sc0) as f32);
    // i = 1: nibbles 4..7, q8 words u2 and u3
    let l2: i32 = (vl0 >> 4) & (0x0F0F0F0F as i32);
    let h2: i32 = ( (vh0 >> 4) << 4) & (0x30303030 as i32);
    var s1: i32 = dp4a(l2 | h2, u2, 0 as i32);
    s1 = dp4a(offset, u2, s1);
    let l3: i32 = (vl1 >> 4) & (0x0F0F0F0F as i32);
    let h3: i32 = ( (vh1 >> 4) << 4) & (0x30303030 as i32);
    s1 = dp4a(l3 | h3, u3, s1);
    s1 = dp4a(offset, u3, s1);
    sum = sum + d8_1 * ( (s1 * sc1) as f32);
    return d * sum;
}
fn mmvq_q6_dot_row(vx: u64, vy: u64, ybase: i64, kbx_offset: i64, kbx: i64, kby: i64, iqs: i64) -> f32 {
    let xb: i64 = (kbx_offset + kbx) * (210 as i64);
    // Every divisor here is a power of two, so the divisions are written as shifts and
    // masks. Goose lowers `/` and `%` to gg_div/gg_mod, each of which is emitted with a
    // zero-divisor test, an overflow test and a trap branch; with 13 such calls the Q6_K
    // body carried 79 ISETP+SEL against 7 in native. Shifts have no runtime helper at all,
    // so the guards disappear without changing any result.
    //   h  = iqs/16      r  = iqs%16
    //   bq8 = 4h + r/8   scale_off = 8h + r/4   vh_shift = 2*(r/8)
    let iq: i32 = iqs as i32;
    let h: i32 = iq >> 4 as i32;
    let r: i32 = iq & 15 as i32;
    let bq8: i64 = (4 * h + (r >> 3 as i32)) as i64;
    let scale_off: i64 = (8 * h + (r >> 2 as i32)) as i64;
    let vh_shift: i32 = 2 * (r >> 3 as i32);
    // vl[j] = get_int_b2(ql, iqs+j) -> 32-bit word at byte 4*(iqs+j)
    let vl0: i32 = ld_b2(vx, xb + 4 * (iqs + 0 as i64));
    let vl1: i32 = ld_b2(vx, xb + 4 * (iqs + 1 as i64));
    // vh[j] = get_int_b2(qh, 4*h + iqs%4 + j) >> vh_shift, qh at xb+128
    let qhb: i64 = xb + (128 as i64);
    let vh0: i32 = ld_b2(vx, qhb + 4 * ( (8 as i64) * h + (iqs & 7 as i64) + 0)) >> (vh_shift);
    let vh1: i32 = ld_b2(vx, qhb + 4 * ( (8 as i64) * h + (iqs & 7 as i64) + 1)) >> (vh_shift);
    // int8 scales[16] at xb+192; the two used entries are scale_off and scale_off+4
    let scb: i64 = xb + (192 as i64) + scale_off;
    // scales is int8_t[16], so the byte must be sign-extended before the multiply.
    let sc0: i32 = (ld_i8(vx, (scb as u64)) as i32);
    let sc1: i32 = (ld_i8(vx, ( (scb + (4 as i64)) as u64)) as i32);
    // u words: q8 block bq8+2*i, word offset (iqs%QI8_1) + j with QI8_1 = 8
    let uoff: i64 = iqs & 7 as i64;
    let yb0: i64 = (ybase + kby + bq8) * (36 as i64);
    let yb1: i64 = (ybase + kby + bq8 + (2 as i64)) * (36 as i64);
    let d8_0: f32 = ld_half(vy, (yb0 as u64));
    let d8_1: f32 = ld_half(vy, (yb1 as u64));
    let u0: i32 = ld_i32(vy, ( (yb0 + (4 as i64) + 4 * uoff) as u64));
    let u1: i32 = ld_i32(vy, ( (yb0 + (4 as i64) + 4 * (uoff + 1 as i64)) as u64));
    let u2: i32 = ld_i32(vy, ( (yb1 + (4 as i64) + 4 * uoff) as u64));
    let u3: i32 = ld_i32(vy, ( (yb1 + (4 as i64) + 4 * (uoff + 1 as i64)) as u64));
    let d: f32 = ld_half(vx, ( (xb + (208 as i64)) as u64));
    return impl_mmvq(vl0, vl1, vh0, vh1, u0, u1, u2, u3, sc0, sc1, d, d8_0, d8_1);
}
export fn fg186q_partial(vx: u64, vy: u64, ybase: i64, row_block: i64,
    blocks_per_row: i64, blocks_per_iter: i64, tid: i64) -> f32 {
    var sum: f32 = 0.0;
    var kbx = tid >> 4 as i64;
    let iqs = 2 * (tid & 15);
    while kbx < blocks_per_row {
        sum += mmvq_q6_dot_row(vx, vy, ybase, row_block, kbx, 8 * kbx, iqs);
        kbx += blocks_per_iter;
    }
    return sum;
}
