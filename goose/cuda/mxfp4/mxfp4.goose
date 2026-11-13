// MXFP4 MMQ activation quantizer in Goose, from ggml-cuda/quantize.cu
// quantize_mmq_mxfp4<scatter>.
//
// One warp handles two 32-value sub-blocks. The kernel computes the warp amax,
// an E8M0 shared scale, quantizes each value with the strict-less FP4 LUT, then
// shuffles four lane values into each packed char2 and writes block_fp4_mmq.
// Only loads/stores, shuffles, libm log2/rint, reciprocal and the E8M0 bitcast
// are extern primitives; every index, comparison and packing step is Goose.
extern "fg431_ld_f32" fn ld_f32(p: u64, o: u64) -> f32;
extern "fg431_ld_i32" fn ld_i32(p: u64, o: u64) -> i64;
extern "fg431_st_u16" fn st_u16(p: u64, o: u64, v: i32);
extern "fg431_st_i32" fn st_i32(p: u64, o: u64, v: i32);
extern "fg431_shfl_f32" fn shfl_f32(v: f32, m: u32, o: i32) -> f32;
extern "fg431_shfl_i32" fn shfl_i32(v: i32, m: u32, s: i32) -> i32;
extern "fg431_fabs" fn fabsv(v: f32) -> f32;
extern "fg431_log2" fn log2v(v: f32) -> f32;
extern "fg431_rint" fn rintv(v: f32) -> i32;
extern "fg431_rcp" fn rcp(v: f32) -> f32;
extern "fg431_bits_f32" fn f32_from_bits(b: u32) -> f32;
extern "fg431_fail" fn fail(c: i64);

fn fmax2(a: f32, b: f32) -> f32 {
    return if a < b {
        b
    } else {
        a
    }
    ;
}

fn warp_max(v: f32) -> f32 {
    var a: f32 = v;
    a = fmax2(a, shfl_f32(a, 4294967295 as u32, 16 as i32));
    a = fmax2(a, shfl_f32(a, 4294967295 as u32, 8 as i32));
    a = fmax2(a, shfl_f32(a, 4294967295 as u32, 4 as i32));
    a = fmax2(a, shfl_f32(a, 4294967295 as u32, 2 as i32));
    a = fmax2(a, shfl_f32(a, 4294967295 as u32, 1 as i32));
    return a;
}

fn e8m0(amax: f32) -> i32 {
    if ! (amax > (0.0 as f32)) {
        return 0 as i32;
    }
    let l: f32 = log2v(amax);
    let ei: i32 = rintv(l);
    var b: i32 = ei - (2 as i32) + (127 as i32);

    if b < (0 as i32) {
        b = 0 as i32;
    }

    if b > (254 as i32) {
        b = 254 as i32;
    }
    return b;
}

fn e8m0_f32(e: i32) -> f32 {
    var bits: u32 = 0 as u32;

    if e == (0 as i32) {
        bits = 0x00400000 as u32;
    } else {
        bits = ( (e as u32) << (23 as u32));
    }
    return f32_from_bits(bits);
}
// strict less: the first index wins ties.

fn fp4(x: f32, e: f32) -> i32 {
    let sign: i32 = if x < (0.0 as f32) {
        8 as i32
    } else {
        0 as i32
    }
    ;
    let ax: f32 = fabsv(x) * e;
    var best: i32 = 0 as i32;
    var err: f32 = fabsv(ax - (0.0 as f32));

    if fabsv(ax - (0.5 as f32)) < err {
        best = 1 as i32;
        err = fabsv(ax - (0.5 as f32));
    }

    if fabsv(ax - (1.0 as f32)) < err {
        best = 2 as i32;
        err = fabsv(ax - (1.0 as f32));
    }

    if fabsv(ax - (1.5 as f32)) < err {
        best = 3 as i32;
        err = fabsv(ax - (1.5 as f32));
    }

    if fabsv(ax - (2.0 as f32)) < err {
        best = 4 as i32;
        err = fabsv(ax - (2.0 as f32));
    }

    if fabsv(ax - (3.0 as f32)) < err {
        best = 5 as i32;
        err = fabsv(ax - (3.0 as f32));
    }

    if fabsv(ax - (4.0 as f32)) < err {
        best = 6 as i32;
        err = fabsv(ax - (4.0 as f32));
    }

    if fabsv(ax - (6.0 as f32)) < err {
        best = 7 as i32;
    }
    return best | sign;
}

fn kernel(
    x: u64,
    ids: u64,
    vy: u64,
    ne00: i64,
    s01: i64,
    s02: i64,
    s03: i64,
    ne0: i64,
    ne1: i64,
    ne2: i64,
    n_used: i32,
    bx: i32,
    by: i32,
    bz: i32,
    tx: i32,
    ty: i32,
    bdx: i32,
    bdy: i32,
    scatter: i32) -> i64 {
    let wsp: i64 = ( ( (by as i64) * (bdy as i64)) + (ty as i64)) * (64 as i64);

    if wsp >= ne0 {
        return 0 as i64;
    }
    let kb: i64 = wsp / (256 as i64);
    let quad: i64 = (wsp % (256 as i64)) / (64 as i64);
    let group: i32 = tx / (4 as i32);
    let lin: i32 = tx % (4 as i32);
    let base: i32 = group * (2 as i32);
    var bp: i64 = 0 as i64;

    if scatter != (0 as i32) {
        bp = (bx as i64) * s02;
    } else {
        let i2: i64 = (bz as i64) % ne2;
        let i3: i64 = (bz as i64) / ne2;
        var i01: i64 = bx as i64;

        if ids != (0 as u64) {
            i01 = ld_i32(ids, ( (bx as i64) * (4 as i64)) as u64);
        }
        bp = i3 * s03 + i2 * s02 + i01 * s01;
    }
    var s0: i32 = 0 as i32;
    var s1: i32 = 0 as i32;
    var p0: i32 = 0 as i32;
    var p1: i32 = 0 as i32;
    var b: i32 = 0 as i32;

    while b < (2 as i32) {
        let i0: i64 = wsp + (b as i64) * (32 as i64) + (tx as i64);
        var xi: f32 = 0.0;

        if i0 < ne00 {
            xi = ld_f32(x, ( (bp + i0) * (4 as i64)) as u64);
        }
        let amax: f32 = warp_max(fabsv(xi));
        let e: i32 = e8m0(amax);
        var inv: f32 = 0.0;

        if amax > (0.0 as f32) {
            inv = rcp(e8m0_f32(e));
        }
        let q: i32 = fp4(xi, inv);
        let q00: i32 = shfl_i32(q, 4294967295 as u32, base);
        let q01: i32 = shfl_i32(q, 4294967295 as u32, base + (1 as i32));
        let q10: i32 = shfl_i32(q, 4294967295 as u32, base + (16 as i32));
        let q11: i32 = shfl_i32(q, 4294967295 as u32, base + (17 as i32));
        let pk: i32 = ( (q10 << (4 as i32)) | q00) | ( ( (q11 << (4 as i32)) | q01) << (8 as i32));

        if b == (0 as i32) {
            s0 = e;
            p0 = pk;
        } else {
            s1 = e;
            p1 = pk;
        }
        b = b + (1 as i32);
    }
    let nwrite: i32 = if scatter != (0 as i32) {
        n_used
    } else {
        1 as i32
    }
    ;
    var slot: i32 = 0 as i32;

    while slot < nwrite {
        var ib: i64 = 0 as i64;

        if scatter != (0 as i32) {
            let idx: i64 = (bx as i64) * (n_used as i64) + (slot as i64);
            let ii: i64 = ld_i32(ids, (idx * (4 as i64)) as u64);
            ib = kb * ne1 + ii;
        } else {
            let bpc: i64 = ne0 / (256 as i64);
            let ib0: i64 = (bz as i64) * (ne1 * bpc);
            ib = ib0 + kb * ne1 + (bx as i64);
        }
        let yb: i64 = ib * (144 as i64);

        if lin == (0 as i32) {
            st_u16(vy, (yb + (16 as i64) + ( (quad * (16 as i64) + (group as i64)) * (2 as i64))) as u64, p0);
            st_u16(
                vy,
                (yb + (16 as i64) + ( (quad * (16 as i64) + (8 as i64) + (group as i64)) * (2 as i64))) as u64,
                p1);
        }

        if tx == (0 as i32) {
            st_i32(vy, (yb + (quad * (4 as i64))) as u64, (s1 << (8 as i32)) | s0);
        }
        slot = slot + (1 as i32);
    }
    return 0 as i64;
}

fn main() {
    kernel(
        0 as u64,
        0 as u64,
        0 as u64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        0 as i64,
        1 as i64,
        1 as i64,
        0 as i32,
        0,
        0,
        0,
        0,
        0,
        32 as i32,
        8 as i32,
        0 as i32);
}

