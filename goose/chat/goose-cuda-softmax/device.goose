// Apex CUDA softmax in Goose, from ggml-cuda/softmax.cu: soft_max_f32 for the
// mask-free, sink-free path with alibi support.
//
// Scope: no mask and no sinks. The mask (f16/f32) and sink variants, the
// parallelize_cols cooperative path and soft_max_back are not ported here.
//
// Everything algorithmic is Goose: the row/grid mapping, the alibi slope, the
// value pass (x*scale + 0.0f, written to shared or dst), the block max reduce,
// the exp pass, the block sum reduce, the 1/sum and the final pass. Shared memory
// is explicit primitives; the reduction ladders are shuffles. Statement order is
// preserved, including the `+ 0.0f` mask term that affects signed zero.
extern "fg280d_ld_f32" fn ld_f32(x: u64, o: u64) -> f32;
extern "fg280d_st_f32" fn st_f32(dst: u64, o: u64, v: f32);
extern "fg280d_smem" fn smem_base() -> u64;
extern "fg280d_sload" fn sload(a: u64) -> f32;
extern "fg280d_sstore" fn sstore(a: u64, v: f32);
extern "fg280d_sync" fn sync();
extern "fg280d_shfl_xor" fn shfl_xor(v: f32, m: i32) -> f32;
extern "fg280d_fmaxf" fn fmaxf(a: f32, b: f32) -> f32;
extern "fg280d_expf" fn expf(x: f32) -> f32;
extern "fg280d_powf" fn powf(a: f32, b: f32) -> f32;
extern "fg280d_ninf" fn ninf() -> f32;
extern "fg280d_fail" fn fail(code: i64);

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

fn softmax(
    x: u64,
    dst: u64,
    ncols: i64,
    scale: f32,
    max_bias: f32,
    nhl: i64,
    m0: f32,
    m1: f32,
    blk: i64,
    tid: i64,
    bx: i64,
    by: i64,
    bz: i64,
    gw: i64,
    gh: i64,
    use_shared: i64) {
    ensure(ncols > 0 as i64 && blk > 0 as i64, 20);
    let rowx: i64 = bx + by * gw + bz * gw * gh;
    let xo: i64 = rowx * ncols;
    let db: i64 = rowx * ncols;
    let buf: u64 = smem_base();
    let vals: i64 = 32 as i64;
    let slope: f32 = slope_of(max_bias, by, nhl, m0, m1);
    var maxv: f32 = ninf();
    var c0: i64 = 0 as i64;

    while c0 < ncols {
        let col: i64 = c0 + tid;

        if col < ncols as i64 {
            let xv: f32 = ld_f32(x, ( ( (xo + col) * 4 as i64) as u64));
            let val: f32 = xv * scale + (0.0 as f32);

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
// Retained entry; the parent never executes main.

fn main() {
    softmax(
        0,
        0,
        0 as i64,
        0.0 as f32,
        0.0 as f32,
        0 as i64,
        0.0 as f32,
        0.0 as f32,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64);
}

