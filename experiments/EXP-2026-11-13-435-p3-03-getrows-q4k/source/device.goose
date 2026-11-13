// CUDA GET_ROWS for Q4_K source and F32 destination in Goose.
// The per-super-block arithmetic is the EXP182 Q4_K dequantization; the gather
// loop is k_get_rows_kq from getrows.cu. Apex token_embd is Q4_K, so this is the
// quantized gather on the inference path.
extern "fg435_ld_u8" fn ld_u8(src: u64, byte: u64) -> u32;
extern "fg435_ld_half" fn ld_half(src: u64, byte: u64) -> f32;
extern "fg435_ld_i32" fn ld_i32(src: u64, byte: u64) -> i64;
extern "fg435_st_f32" fn st_f32(dst: u64, byte: u64, v: f32);
extern "fg435_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn scale_min(q: u64, j: i64) -> i64 {
    var dv: i64 = 0;
    var mv: i64 = 0;

    if j < (4 as i64) {
        dv = (ld_u8(q, (j as u64)) & 63) as i64;
        mv = (ld_u8(q, ( (j + (4 as i64)) as u64)) & 63) as i64;
    } else {
        let a: u32 = ld_u8(q, ( (j + (4 as i64)) as u64));
        let b: u32 = ld_u8(q, ( (j - (4 as i64)) as u64));
        let c: u32 = ld_u8(q, (j as u64));
        dv = ( (a & 15) as i64) | ( ( ( (b >> 6) as i64) << (4 as i64)) as i64);
        mv = ( ( (a >> 4) as i64)) | ( ( ( (c >> 6) as i64) << (4 as i64)) as i64);
    }
    return (dv << (8 as i64)) | mv;
}

fn q4k(vx: u64, yy: u64, ib: i64, tid: i32) {
    ensure(tid >= (0 as i32) && tid < (32 as i32), 10);
    let il: i64 = (tid as i64) / (8 as i64);
    let ir: i64 = (tid as i64) % (8 as i64);
    let is: i64 = (2 as i64) * il;
    let n: i64 = 4 as i64;
    let blk: i64 = ib * (144 as i64);
    let ybase: i64 = ib * (256 as i64) + (64 as i64) * il + n * ir;
    let d: f32 = ld_half(vx, (blk) as u64);
    let dmin: f32 = ld_half(vx, ( (blk) + (2 as i64)) as u64);
    let q: u64 = vx + ( (blk + (16 as i64) + (32 as i64) * il + n * ir) as u64);
    let s1: i64 = scale_min(vx + ( (blk + (4 as i64)) as u64), is);
    let s2: i64 = scale_min(vx + ( (blk + (4 as i64)) as u64), is + (1 as i64));
    let d1: f32 = d * ( (s1 >> (8 as i64)) as f32);
    let m1: f32 = dmin * ( (s1 & (255 as i64)) as f32);
    let d2: f32 = d * ( (s2 >> (8 as i64)) as f32);
    let m2: f32 = dmin * ( (s2 & (255 as i64)) as f32);
    var l: i64 = 0 as i64;

    while l < n {
        let b: u32 = ld_u8(q, (l as u64));
        let lo: u32 = b & (15 as u32);
        let hi: u32 = b >> (4 as u32);
        let o0: i64 = (ybase + l) * (4 as i64);
        let o1: i64 = (ybase + l + (32 as i64)) * (4 as i64);
        st_f32(yy, (o0 as u64), d1 * (lo as f32) - m1);
        st_f32(yy, (o1 as u64), d2 * (hi as f32) - m2);
        l = l + (1 as i64);
    }
}

fn getrows(
    src0: u64,
    src1: u64,
    dst: u64,
    ne00: i64,
    ne11: i64,
    ne12: i64,
    s1: i64,
    s2: i64,
    s3: i64,
    nb01: i64,
    nb02: i64,
    nb03: i64,
    s10: i64,
    s11: i64,
    s12: i64,
    bx: i64,
    by: i64,
    bz: i64,
    tx: i64,
    gy: i64,
    gz: i64) -> i64 {
    if ne00 <= (0 as i64) || ne11 <= (0 as i64) || ne12 <= (0 as i64) {
        return 0 as i64;
    }
    let nsb: i64 = ne00 / (256 as i64);
    var z: i64 = bz;

    while z < ne11 * ne12 {
        let i11: i64 = z % ne12;
        let i12: i64 = z / ne12;
        let i10: i64 = bx;
        let i01: i64 = ld_i32(src1, ( (i10 * s10 + i11 * s11 + i12 * s12) * (4 as i64)) as u64);
        let dst_row: u64 = dst + ( (i10 * s1 + i11 * s2 + i12 * s3) * (4 as i64)) as u64;
        let src_row: u64 = src0 + ( (i01 * nb01 + i11 * nb02 + i12 * nb03) as u64);
        var ib: i64 = by;

        while ib < nsb {
            q4k(src_row, dst_row, ib, tx as i32);
            ib = ib + gy;
        }
        z = z + gz;
    }
    return 0 as i64;
}

fn main() {
    getrows(
        0 as u64,
        0 as u64,
        0 as u64,
        256 as i64,
        1 as i64,
        1 as i64,
        256 as i64,
        256 as i64,
        256 as i64,
        144 as i64,
        144 as i64,
        144 as i64,
        1 as i64,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        1 as i64,
        1 as i64);
}

