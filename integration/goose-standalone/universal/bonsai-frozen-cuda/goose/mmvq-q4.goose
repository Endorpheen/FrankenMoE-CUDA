extern "fg185q_ld_i32" fn ld_i32(src: u64, byte: u64) -> i32;
extern "fg185q_ld_u16" fn ld_u16(src: u64, byte: u64) -> u32;
extern "fg185q_ld_h2" fn ld_h2(src: u64, byte: u64) -> f32;
extern "fg185q_ld_h2b" fn ld_h2b(src: u64, byte: u64) -> f32;
extern "fg185q_dp4a" fn dp4a(a: i32, b: i32, c: i32) -> i32;
extern "fg185q_shfl_xor" fn shfl_xor(v: f32, off: i32, width: i32) -> f32;
export fn warp_sum(v0: f32) -> f32 {
    var v: f32 = v0;
    v = v + shfl_xor(v, 16 as i32, 32 as i32);
    v = v + shfl_xor(v, 8 as i32, 32 as i32);
    v = v + shfl_xor(v, 4 as i32, 32 as i32);
    v = v + shfl_xor(v, 2 as i32, 32 as i32);
    v = v + shfl_xor(v, 1 as i32, 32 as i32);
    return v;
}
fn impl_vmmq(
    v0: i32,
    v1: i32,
    u0: i32,
    u1: i32,
    u2: i32,
    u3: i32,
    sc0: i32,
    sc1: i32,
    m0: i32,
    m1: i32,
    dmx: f32,
    dmy: f32,
    d8_0: f32,
    d8_1: f32) -> f32 {
    var sd: f32 = 0.0;
    var sm: f32 = 0.0;
    // i = 0: nibbles 0..3, q8 words (u0, u1)
    let a0: i32 = (v0 >> 0) & (0x0F0F0F0F as i32);
    let b0: i32 = (v1 >> 0) & (0x0F0F0F0F as i32);
    let d1: i32 = dp4a(b0, u1, dp4a(a0, u0, 0 as i32));
    let s1: i32 = dp4a(0x01010101 as i32, u1, dp4a(0x01010101 as i32, u0, 0 as i32));
    sd = sd + d8_0 * ( (d1 * sc0) as f32);
    sm = sm + d8_0 * ( (s1 * m0) as f32);
    // i = 1: nibbles 4..7, q8 words (u2, u3)
    let a1: i32 = (v0 >> 4) & (0x0F0F0F0F as i32);
    let b1: i32 = (v1 >> 4) & (0x0F0F0F0F as i32);
    let d2: i32 = dp4a(b1, u3, dp4a(a1, u2, 0 as i32));
    let s2: i32 = dp4a(0x01010101 as i32, u3, dp4a(0x01010101 as i32, u2, 0 as i32));
    sd = sd + d8_1 * ( (d2 * sc1) as f32);
    sm = sm + d8_1 * ( (s2 * m1) as f32);
    return dmx * sd - dmy * sm;
}
fn mmvq_q4_dot_row(vx: u64, vy: u64, ybase: i64, kbx_offset: i64, kbx: i64, kby: i64, kqs: i64) -> f32 {
    let xb: i64 = (kbx_offset + kbx) * (144 as i64);
    // bq8_offset = QR4_K*((iqs/2)/(QI8_1/2)) = 2*((iqs/2)/4)
    let bq8: i64 = 2 * ( (kqs / 2) / 4);
    // q4 = qs + 16*bq8_offset + 4*((iqs/2)%4), read as q4[0] and q4[4]
    let qoff: i64 = xb + (16 as i64) + 16 * bq8 + 4 * ( (kqs / 2) % 4);
    let v0: i32 = ld_i32(vx, (qoff as u64));
    let v1: i32 = ld_i32(vx, ( (qoff + (16 as i64)) as u64));
    // scales is uint8[12] at xb+4; native reads u16 words at jm, jm+2, jm+4
    let j: i64 = bq8 / 2;
    let jm: i64 = j & 1;
    let sbase: i64 = xb + (4 as i64);
    let s0: u32 = ld_u16(vx, ( (sbase + 2 * jm) as u64));
    let s2: u32 = ld_u16(vx, ( (sbase + 2 * (jm + 2)) as u64));
    let s4: u32 = ld_u16(vx, ( (sbase + 2 * (jm + 4)) as u64));
    // branchless hi mask: hi = -(j >= 2)
    var hi: u32 = 0 as u32;

    if j >= 2 as i64 {
        hi = 4294967295 as u32;
    }
    let a0: u32 = ( (s0 & (0x3f3f as u32)) & (~ hi)) | ( ( ( (s4 & (0x0f0f as u32)) | ( (s0 & (0xc0c0 as u32)) >> 2)) & hi));
    let a1: u32 = ( (s2 & (0x3f3f as u32)) & (~ hi)) | ( ( ( (s4 >> 4) & (0x0f0f as u32)) | ( (s2 & (0xc0c0 as u32)) >> 2)) & hi);
    // In native `sc` and `m` alias the two u16 words of aux at different byte offsets:
    //   sc = (uint8_t*)aux      -> bytes 0,1 = the two low bytes of aux[0]
    //   m  = sc + 2             -> bytes 2,3 = the two low bytes of aux[1]
    // So the two scales come from aux[0] and the two mins from aux[1]; taking the low
    // byte of each word as a scale and the high byte as a min is wrong.
    let sc0: i32 = (a0 & (255 as u32)) as i32;
    let sc1: i32 = ( (a0 >> 8) & (255 as u32)) as i32;
    let m0: i32 = (a1 & (255 as u32)) as i32;
    let m1: i32 = ( (a1 >> 8) & (255 as u32)) as i32;
    // q8 blocks bq8i = bq8_1 + bq8_offset + i, i in 0..1
    // ybase is a BLOCK index (native advances the block_q8_1 pointer), so it must be
    // scaled to bytes along with kby and bq8. Adding it raw read block 64's data as
    // byte offset 64 of block 0.
    let yb: i64 = (ybase + kby + bq8) * (36 as i64);
    let d8_0: f32 = ld_h2(vy, (yb as u64));
    let d8_1: f32 = ld_h2(vy, ( (yb + (36 as i64)) as u64));
    // u words: q8.qs + 4*((iqs/2)%4), read at +0 and +16 for both q8 blocks
    let ubase: i64 = yb + (4 as i64) + 4 * ( (kqs / 2) % 4);
    let u0: i32 = ld_i32(vy, (ubase as u64));
    let u1: i32 = ld_i32(vy, ( (ubase + (16 as i64)) as u64));
    let ub1: i64 = ubase + (36 as i64);
    let u2: i32 = ld_i32(vy, (ub1 as u64));
    let u3: i32 = ld_i32(vy, ( (ub1 + (16 as i64)) as u64));
    let dmx: f32 = ld_h2(vx, (xb as u64));
    // ld_h2b takes the HIGH half of the half2 at the given offset, so the base is the
    // block start -- passing xb+2 would read the element at xb+4.
    let dmy: f32 = ld_h2b(vx, (xb as u64));
    return impl_vmmq(v0, v1, u0, u1, u2, u3, sc0, sc1, m0, m1, dmx, dmy, d8_0, d8_1);
}
export fn fg185q_partial(vx: u64, vy: u64, ybase: i64, row_block: i64,
    blocks_per_row: i64, blocks_per_iter: i64, tid: i64) -> f32 {
    var sum: f32 = 0.0;
    var kbx = tid / 16;
    let kqs = 2 * (tid % 16);
    while kbx < blocks_per_row {
        sum += mmvq_q4_dot_row(vx, vy, ybase, row_block, kbx, 8 * kbx, kqs);
        kbx += blocks_per_iter;
    }
    return sum;
}
