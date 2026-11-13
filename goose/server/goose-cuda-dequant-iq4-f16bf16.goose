// Apex CUDA IQ4_NL and IQ4_XS dequantization to half and bfloat16 (Goose).
// Extends EXP252/EXP253, which covered the F32 destinations.
//
// Arithmetic is the F32 modules': for IQ4_NL the nb32 guard index, block
// i*8+ib, base i*QK_K + 32*ib + 4*il and the nibble split; for IQ4_XS the
// per-32 scale assembled from scales_l/scales_h, base with 16*ib + 4*il in qs,
// and d = x.d*(scale-32). The 16-entry kvalues_iq4nl table is reproduced here
// as an explicit constant function. Only the store differs.
//
// Layouts: block_iq4_nl 18 bytes (d at 0, qs[16] at 2); block_iq4_xs 136 bytes
// (d at 0, scales_h u16 at 2, scales_l[4] at 4, qs[128] at 8).
extern "fg268d_ld_u8" fn ld_u8(vx: u64, o: u64) -> u32;
extern "fg268d_ld_u16" fn ld_u16(vx: u64, o: u64) -> u32;
extern "fg268d_ld_half" fn ld_half(vx: u64, o: u64) -> f32;
extern "fg268d_st_half" fn st_half(yy: u64, o: u64, v: f32);
extern "fg268d_st_bf16" fn st_bf16(yy: u64, o: u64, v: f32);
extern "fg268d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn kv(i: i64) -> i64 {
    if i == 0 as i64 {
        return -127 as i64;
    }

    if i == 1 as i64 {
        return -104 as i64;
    }

    if i == 2 as i64 {
        return -83 as i64;
    }

    if i == 3 as i64 {
        return -65 as i64;
    }

    if i == 4 as i64 {
        return -49 as i64;
    }

    if i == 5 as i64 {
        return -35 as i64;
    }

    if i == 6 as i64 {
        return -22 as i64;
    }

    if i == 7 as i64 {
        return -10 as i64;
    }

    if i == 8 as i64 {
        return 1 as i64;
    }

    if i == 9 as i64 {
        return 13 as i64;
    }

    if i == 10 as i64 {
        return 25 as i64;
    }

    if i == 11 as i64 {
        return 38 as i64;
    }

    if i == 12 as i64 {
        return 53 as i64;
    }

    if i == 13 as i64 {
        return 69 as i64;
    }

    if i == 14 as i64 {
        return 89 as i64;
    }
    113 as i64
}

fn q4_nl_f16(vx: u64, yy: u64, i: i64, nb32: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ibg: i64 = 8 * i + tid % 8;

    if ibg >= nb32 {
        return;
    }
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let bidx: i64 = i * 8 + ib;
    let blk: i64 = bidx * (18 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let d: f32 = ld_half(vx, (blk) as u64);
    let qb: i64 = blk + 2 as i64 + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_half(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (v0) as f32));
        st_half(yy, ( ( (ybase + j + 16 as i64) * (2 as i64)) as u64), d * ( (v1) as f32));
        j = j + 1;
    }
}

fn q4_nl_bf16(vx: u64, yy: u64, i: i64, nb32: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ibg: i64 = 8 * i + tid % 8;

    if ibg >= nb32 {
        return;
    }
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let bidx: i64 = i * 8 + ib;
    let blk: i64 = bidx * (18 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let d: f32 = ld_half(vx, (blk) as u64);
    let qb: i64 = blk + 2 as i64 + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_bf16(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (v0) as f32));
        st_bf16(yy, ( ( (ybase + j + 16 as i64) * (2 as i64)) as u64), d * ( (v1) as f32));
        j = j + 1;
    }
}

fn q4_xs_f16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let blk: i64 = i * (136 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let dall: f32 = ld_half(vx, (blk) as u64);
    let scl: i64 = blk + 4 as i64 + ib / 2;
    let sl: u32 = ld_u8(vx, (scl) as u64);
    let sh: u32 = ld_u16(vx, (blk + 2 as i64) as u64);
    let slb: u32 = (4 * (ib % 2)) as u32;
    let shb: u32 = (2 * ib) as u32;
    let lo: i64 = ( ( (sl >> slb) & 15) as i64);
    let hi: i64 = ( ( ( (sh >> shb) & 3) as i64) << 4);
    let sc: i64 = lo | hi;
    let d: f32 = dall * ( (sc - 32 as i64) as f32);
    let qb: i64 = blk + 8 as i64 + 16 * ib + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_half(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (v0) as f32));
        st_half(yy, ( ( (ybase + j + 16 as i64) * (2 as i64)) as u64), d * ( (v1) as f32));
        j = j + 1;
    }
}

fn q4_xs_bf16(vx: u64, yy: u64, i: i64, tid: i64) {
    ensure(tid >= 0 && tid < 32 as i64, 10);
    let ib: i64 = tid % 8;
    let il: i64 = tid / 8;
    let blk: i64 = i * (136 as i64);
    let ybase: i64 = i * (256 as i64) + 32 * ib + 4 * il;
    let dall: f32 = ld_half(vx, (blk) as u64);
    let scl: i64 = blk + 4 as i64 + ib / 2;
    let sl: u32 = ld_u8(vx, (scl) as u64);
    let sh: u32 = ld_u16(vx, (blk + 2 as i64) as u64);
    let slb: u32 = (4 * (ib % 2)) as u32;
    let shb: u32 = (2 * ib) as u32;
    let lo: i64 = ( ( (sl >> slb) & 15) as i64);
    let hi: i64 = ( ( ( (sh >> shb) & 3) as i64) << 4);
    let sc: i64 = lo | hi;
    let d: f32 = dall * ( (sc - 32 as i64) as f32);
    let qb: i64 = blk + 8 as i64 + 16 * ib + 4 * il;
    var j: i64 = 0;

    while j < 4 as i64 {
        let b: u32 = ld_u8(vx, ( (qb + j) as u64));
        let v0: i64 = kv( (b & 15) as i64);
        let v1: i64 = kv( (b >> 4) as i64);
        st_bf16(yy, ( ( (ybase + j) * (2 as i64)) as u64), d * ( (v0) as f32));
        st_bf16(yy, ( ( (ybase + j + 16 as i64) * (2 as i64)) as u64), d * ( (v1) as f32));
        j = j + 1;
    }
}

fn main() {
    q4_nl_f16(0, 0, 0 as i64, 0 as i64, 0 as i64);
    q4_nl_bf16(0, 0, 0 as i64, 0 as i64, 0 as i64);
    q4_xs_f16(0, 0, 0 as i64, 0 as i64);
    q4_xs_bf16(0, 0, 0 as i64, 0 as i64);
}

