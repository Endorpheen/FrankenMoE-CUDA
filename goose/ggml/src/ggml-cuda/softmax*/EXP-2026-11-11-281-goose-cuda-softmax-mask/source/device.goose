// Apex CUDA softmax with a mask in Goose, from ggml-cuda/softmax.cu:
// soft_max_f32 for mask T = float and T = half (sinks still not ported).
// Extends EXP280, which covered the mask-free path.
//
// Mask pointer arithmetic follows the native line:
//   mask += (i11*nb11 + i12*nb12 + i13*nb13) / sizeof(T) * (mask != nullptr)
// i.e. a byte offset divided by the element size, then mask[col]. The value pass
// is x[col]*scale + slope*t2f32(mask[col]); with a mask the +0.0f term is replaced
// by that product.
extern "fg281d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg281d_ld_half" fn ld_half(x: u64, o: u64) -> f32;
extern "fg281d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg281d_smem" fn smem_base() -> u64;
extern "fg281d_sload" fn sload(a: u64) -> f32;
extern "fg281d_sstore" fn sstore(a: u64, v: f32);
extern "fg281d_sync" fn sync();
extern "fg281d_shfl_xor" fn shfl_xor(v: f32, m: i32) -> f32;
extern "fg281d_fmaxf" fn fmaxf(a: f32, b: f32) -> f32;
extern "fg281d_expf" fn expf(x: f32) -> f32;
extern "fg281d_powf" fn powf(a: f32, b: f32) -> f32;
extern "fg281d_ninf" fn ninf() -> f32;
extern "fg281d_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn warp_max(x: f32) -> f32 {
    var v: f32 = x;
    var off: i64 = 16 as i64;

    while off >= 1 as i64 {
        let t: f32 = shfl_xor(v, off as i32);
        v = fmaxf(v, t);
        off = off / 2 as i64;
    }
    v
}

fn warp_sum(x: f32) -> f32 {
    var v: f32 = x;
    var off: i64 = 16 as i64;

    while off >= 1 as i64 {
        let t: f32 = shfl_xor(v, off as i32);
        v = v + t;
        off = off / 2 as i64;
    }
    v
}

fn breduce_max(v0: f32, blk: i64, tid: i64, buf: u64) -> f32 {
    var v: f32 = warp_max(v0);

    if blk > 32 as i64 {
        if tid % 32 as i64 == 0 as i64 {
            sstore(buf + ( (tid / 32 * 4 as i64) as u64), v);
        }
        sync();
        var s: f32 = ninf();

        if tid % 32 as i64 < (blk / 32 as i64) {
            s = sload(buf + ( (tid % 32 * 4 as i64) as u64));
        }
        v = warp_max(s);
    }
    v
}

fn breduce_sum(v0: f32, blk: i64, tid: i64, buf: u64) -> f32 {
    var v: f32 = warp_sum(v0);

    if blk > 32 as i64 {
        if tid % 32 as i64 == 0 as i64 {
            sstore(buf + ( (tid / 32 * 4 as i64) as u64), v);
        }
        sync();
        var s: f32 = 0.0 as f32;

        if tid % 32 as i64 < (blk / 32 as i64) {
            s = sload(buf + ( (tid % 32 * 4 as i64) as u64));
        }
        v = warp_sum(s);
    }
    v
}

fn slope_of(max_bias: f32, h: i64, nhl: i64, m0: f32, m1: f32) -> f32 {
    if max_bias <= 0.0 as f32 {
        return 1.0 as f32;
    }
    var base: f32 = m1;

    if h < nhl as i64 {
        base = m0;
    }
    var exph: i64 = 2 * (h - nhl) + 1 as i64;

    if h < nhl as i64 {
        exph = h + 1 as i64;
    }
    powf(base, exph as f32)
}
// Shared body with a mask element supplied by the caller for the current column.

fn softmax_mask_body(
    x: u64,
    mask: u64,
    dst: u64,
    ncols: i64,
    scale: f32,
    slope: f32,
    moff: i64,
    is_f16: i64,
    blk: i64,
    tid: i64,
    bx: i64,
    by: i64,
    bz: i64,
    gw: i64,
    gh: i64,
    use_shared: i64) {
    let rowx: i64 = bx + by * gw + bz * gw * gh;
    let xo: i64 = rowx * ncols;
    let db: i64 = rowx * ncols;
    let buf: u64 = smem_base();
    let vals: i64 = 32 as i64;
    var maxv: f32 = ninf();
    var c0: i64 = 0 as i64;

    while c0 < ncols {
        let col: i64 = c0 + tid;

        if col < ncols as i64 {
            let xv: f32 = ld_f32(x, ( ( (xo + col) * 4 as i64) as u64));
            var mval: f32 = 0.0 as f32;

            if is_f16 != 0 as i64 {
                mval = ld_half(mask, ( ( (moff + col) * 2 as i64) as u64));
            } else {
                mval = ld_f32(mask, ( ( (moff + col) * 4 as i64) as u64));
            }
            let val: f32 = xv * scale + (slope * mval);

            if use_shared != 0 as i64 {
                sstore(buf + ( ( (vals + col) * 4 as i64) as u64), val);
            } else {
                st_f32(dst, ( ( (db + col) * 4 as i64) as u64), val);
            }
            maxv = fmaxf(maxv, val);
        }
        c0 = c0 + blk;
    }
    maxv = breduce_max(maxv, blk, tid, buf);
    var tmp: f32 = 0.0 as f32;
    c0 = 0 as i64;

    while c0 < ncols {
        let col: i64 = c0 + tid;

        if col < ncols as i64 {
            var vv: f32 = 0.0 as f32;

            if use_shared != 0 as i64 {
                vv = sload(buf + ( ( (vals + col) * 4 as i64) as u64));
            } else {
                vv = ld_f32(dst, ( ( (db + col) * 4 as i64) as u64));
            }
            let e: f32 = expf(vv - maxv);
            tmp = tmp + e;

            if use_shared != 0 as i64 {
                sstore(buf + ( ( (vals + col) * 4 as i64) as u64), e);
            } else {
                st_f32(dst, ( ( (db + col) * 4 as i64) as u64), e);
            }
        }
        c0 = c0 + blk;
    }

    if blk > 32 as i64 {
        sync();
    }
    tmp = breduce_sum(tmp, blk, tid, buf);
    let inv: f32 = 1.0 as f32 / tmp;
    c0 = 0 as i64;

    while c0 < ncols {
        let col: i64 = c0 + tid;

        if col < ncols as i64 {
            var e: f32 = 0.0 as f32;

            if use_shared != 0 as i64 {
                e = sload(buf + ( ( (vals + col) * 4 as i64) as u64));
            } else {
                e = ld_f32(dst, ( ( (db + col) * 4 as i64) as u64));
            }
            st_f32(dst, ( ( (db + col) * 4 as i64) as u64), e * inv);
        }
        c0 = c0 + blk;
    }
}

fn mask_f32(
    x: u64,
    msk: u64,
    dst: u64,
    ncols: i64,
    scale: f32,
    max_bias: f32,
    nhl: i64,
    m0: f32,
    m1: f32,
    ne12: i64,
    ne13: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    blk: i64,
    tid: i64,
    bx: i64,
    by: i64,
    bz: i64,
    gw: i64,
    gh: i64,
    use_shared: i64) {
    let i12: i64 = by % ne12;
    let i13: i64 = bz % ne13;
    let moff: i64 = (bx * nb11 + i12 * nb12 + i13 * nb13) / (4 as i64);
    let slope: f32 = slope_of(max_bias, by, nhl, m0, m1);
    softmax_mask_body(x, msk, dst, ncols, scale, slope, moff, 0 as i64, blk, tid, bx, by, bz, gw, gh, use_shared);
}

fn mask_f16(
    x: u64,
    msk: u64,
    dst: u64,
    ncols: i64,
    scale: f32,
    max_bias: f32,
    nhl: i64,
    m0: f32,
    m1: f32,
    ne12: i64,
    ne13: i64,
    nb11: i64,
    nb12: i64,
    nb13: i64,
    blk: i64,
    tid: i64,
    bx: i64,
    by: i64,
    bz: i64,
    gw: i64,
    gh: i64,
    use_shared: i64) {
    let i12: i64 = by % ne12;
    let i13: i64 = bz % ne13;
    let moff: i64 = (bx * nb11 + i12 * nb12 + i13 * nb13) / (2 as i64);
    let slope: f32 = slope_of(max_bias, by, nhl, m0, m1);
    softmax_mask_body(x, msk, dst, ncols, scale, slope, moff, 1 as i64, blk, tid, bx, by, bz, gw, gh, use_shared);
}

fn main() {
    mask_f32(
        0,
        0,
        0,
        0 as i64,
        0.0 as f32,
        0.0 as f32,
        0 as i64,
        0.0 as f32,
        0.0 as f32,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64);
    mask_f16(
        0,
        0,
        0,
        0 as i64,
        0.0 as f32,
        0.0 as f32,
        0 as i64,
        0.0 as f32,
        0.0 as f32,
        1 as i64,
        1 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64);
}

