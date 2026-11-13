// Apex CUDA IQ1_M and MXFP4 dequantization to half and bfloat16 (Goose).
// Extends EXP261/EXP254, which covered the F32 destinations. This completes the
// F32/F16/BF16 matrix for every templated dequantizer in dequantize.cuh.
//
// Arithmetic is the F32 modules': IQ1_M's ib16 sub-block, half scale pattern from
// four uint16 words, 3-bit scale, delta bit, 11-bit grid index and nibble split;
// MXFP4's E8M0 manual bit path, 8-blocks-per-CTA block index, nibble split and
// d*kv*0.5. iq1s_grid_gpu is an explicit device constant import; the two bitcast
// conversions are explicit primitives. Only the store differs.
//
// Layouts: block_iq1_m 56 bytes (qs[32] 0, qh[16] 32, scales[8] 48, no d field);
// block_mxfp4 17 bytes (e 0, qs[16] 1).
extern "fg272d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg272d_ld_u16" fn ld_u16(vx: u64, o: u64) -> u32;
extern "fg272d_f16" fn f16(u: u32) -> f32;
extern "fg272d_u32f" fn u32f(b: u32) -> f32;
extern "fg272d_grid_1" fn grid_1(i: u32) -> u32;
extern "fg272d_st_half" fn st_half(yy: u64, o: u64, v: f32);
extern "fg272d_st_bf16" fn st_bf16(yy: u64, o: u64, v: f32);
extern "fg272d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn kv(i: i64) -> i64 {
    if i == 0 as i64 {
        return 0 as i64;
    }

    if i == 1 as i64 {
        return 1 as i64;
    }

    if i == 2 as i64 {
        return 2 as i64;
    }

    if i == 3 as i64 {
        return 3 as i64;
    }

    if i == 4 as i64 {
        return 4 as i64;
    }

    if i == 5 as i64 {
        return 6 as i64;
    }

    if i == 6 as i64 {
        return 8 as i64;
    }

    if i == 7 as i64 {
        return 12 as i64;
    }

    if i == 8 as i64 {
        return 0 as i64;
    }

    if i == 9 as i64 {
        return -1 as i64;
    }

    if i == 10 as i64 {
        return -2 as i64;
    }

    if i == 11 as i64 {
        return -3 as i64;
    }

    if i == 12 as i64 {
        return -4 as i64;
    }

    if i == 13 as i64 {
        return -6 as i64;
    }

    if i == 14 as i64 {
        return -8 as i64;
    }
    - 12 as i64
}

fn e8m0(e: u32) -> f32 {
    var bits: u32 = 0;
    let sh: u32 = 23 as u32;

    if (e == (0 as u32)) {
        bits = 0x00400000 as u32;
    } else {
        bits = e << sh;
    }
    u32f(bits)
}

fn q1_m_f16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (56 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let s0: u32 = ld_u16(vx, ( (blk + 48 as i64) as u64));
    let s1: u32 = ld_u16(vx, ( (blk + 50 as i64) as u64));
    let s2: u32 = ld_u16(vx, ( (blk + 52 as i64) as u64));
    let s3: u32 = ld_u16(vx, ( (blk + 54 as i64) as u64));
    let pat: u32 = (s0 >> (12 as u32)) | ( (s1 >> (8 as u32)) & (0x00f0 as u32)) | ( (s2 >> (4 as u32)) & (0x0f00 as u32)) | (s3 & (0xf000 as u32));
    let dbase: f32 = f16(pat);
    let ib16: i64 = 2 * ib + il / 2;
    let sco: i64 = blk + 48 as i64 + 2 * (ib16 / 4);
    let scw: u32 = ld_u16(vx, (sco) as u64);
    let shs: u32 = (3 * (ib16 % 4)) as u32;
    let sc3: i64 = ( (scw >> shs) & (7 as u32)) as i64;
    let d: f32 = dbase * ( ( (2 * sc3) + (1 as i64)) as f32);
    let qhb: u32 = ld_u8(vx, ( (blk + 32 as i64 + 2 * ib + il / 2) as u64));
    let shg: u32 = (4 * (il % 2)) as u32;
    let deltam: u32 = (0x08 as u32) << shg;
    var delta: f32 = -0.875 as f32;

    if (qhb & deltam) > (0 as u32) {
        delta = -1.125 as f32;
    }
    let idx: u32 = ld_u8(vx, ( (blk + 4 * ib + il) as u64)) | ( ( (qhb >> shg) & (7 as u32)) << (8 as u32));
    let g: u32 = grid_1(idx);
    let g0: u32 = g & (0x0f0f0f0f as u32);
    let g1: u32 = (g >> (4 as u32)) & (0x0f0f0f0f as u32);
    var j: i64 = 0;

    while j < 8 as i64 {
        var w: u32 = 0;

        if j < 4 as i64 {
            let sj: u32 = (j * 8) as u32;
            w = g0 >> sj;
        } else {
            let sj: u32 = ( (j - 4 as i64) * 8) as u32;
            w = g1 >> sj;
        }
        let qb: i64 = (w & (255 as u32)) as i64;
        st_half(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( ( (qb) as f32) + delta));
        j = j + 1;
    }
}

fn q1_m_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let il: i64 = tid / 8;
    let ib: i64 = tid % 8;
    let blk: i64 = i * (56 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 8 * il;
    let s0: u32 = ld_u16(vx, ( (blk + 48 as i64) as u64));
    let s1: u32 = ld_u16(vx, ( (blk + 50 as i64) as u64));
    let s2: u32 = ld_u16(vx, ( (blk + 52 as i64) as u64));
    let s3: u32 = ld_u16(vx, ( (blk + 54 as i64) as u64));
    let pat: u32 = (s0 >> (12 as u32)) | ( (s1 >> (8 as u32)) & (0x00f0 as u32)) | ( (s2 >> (4 as u32)) & (0x0f00 as u32)) | (s3 & (0xf000 as u32));
    let dbase: f32 = f16(pat);
    let ib16: i64 = 2 * ib + il / 2;
    let sco: i64 = blk + 48 as i64 + 2 * (ib16 / 4);
    let scw: u32 = ld_u16(vx, (sco) as u64);
    let shs: u32 = (3 * (ib16 % 4)) as u32;
    let sc3: i64 = ( (scw >> shs) & (7 as u32)) as i64;
    let d: f32 = dbase * ( ( (2 * sc3) + (1 as i64)) as f32);
    let qhb: u32 = ld_u8(vx, ( (blk + 32 as i64 + 2 * ib + il / 2) as u64));
    let shg: u32 = (4 * (il % 2)) as u32;
    let deltam: u32 = (0x08 as u32) << shg;
    var delta: f32 = -0.875 as f32;

    if (qhb & deltam) > (0 as u32) {
        delta = -1.125 as f32;
    }
    let idx: u32 = ld_u8(vx, ( (blk + 4 * ib + il) as u64)) | ( ( (qhb >> shg) & (7 as u32)) << (8 as u32));
    let g: u32 = grid_1(idx);
    let g0: u32 = g & (0x0f0f0f0f as u32);
    let g1: u32 = (g >> (4 as u32)) & (0x0f0f0f0f as u32);
    var j: i64 = 0;

    while j < 8 as i64 {
        var w: u32 = 0;

        if j < 4 as i64 {
            let sj: u32 = (j * 8) as u32;
            w = g0 >> sj;
        } else {
            let sj: u32 = ( (j - 4 as i64) * 8) as u32;
            w = g1 >> sj;
        }
        let qb: i64 = (w & (255 as u32)) as i64;
        st_bf16(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( ( (qb) as f32) + delta));
        j = j + 1;
    }
}

fn mxfp4_f16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let bidx: i64 = i * 8 + ib;
    let blk: i64 = bidx * (17 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let e: u32 = ld_u8(vx, (blk) as u64);
    let d: f32 = e8m0(e);
    let qb: i64 = blk + 1 as i64 + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_half(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (v0) as f32) * (0.5 as f32));
        st_half(yy, ( ( (ybase + j + 16 as i64) * (2 as i64)) as u64), d * ( (v1) as f32) * (0.5 as f32));
        j = j + 1;
    }
}

fn mxfp4_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let bidx: i64 = i * 8 + ib;
    let blk: i64 = bidx * (17 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let e: u32 = ld_u8(vx, (blk) as u64);
    let d: f32 = e8m0(e);
    let qb: i64 = blk + 1 as i64 + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_bf16(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (v0) as f32) * (0.5 as f32));
        st_bf16(yy, ( ( (ybase + j + 16 as i64) * (2 as i64)) as u64), d * ( (v1) as f32) * (0.5 as f32));
        j = j + 1;
    }
}

fn main() {
    q1_m_f16(0, 0, 0 as i64, 0 as i64);
    q1_m_bf16(0, 0, 0 as i64, 0 as i64);
    mxfp4_f16(0, 0, 0 as i64, 0 as i64);
    mxfp4_bf16(0, 0, 0 as i64, 0 as i64);
}

