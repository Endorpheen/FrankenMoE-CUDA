// MXFP4 encode in Goose, from ggml-quants.c: quantize_row_mxfp4_ref and best_index_mxfp4.
//
// block_mxfp4 is 17 bytes: e (u8) 0, qs[16] 1. Per block: absolute max, e = floor(log2(amax)) - 2 + 127
// (0 when amax is zero), the halved E8M0 scale, then each of the sixteen bytes packs two indices found by
// the strict-less absolute-error search over the doubled e2m1 table.
extern "fg331q_ld_f32" fn ld_f32(a: u64) -> f32;
extern "fg331q_st_u8" fn st_u8(a: u64, v: i64);
extern "fg331q_abs" fn absf(v: f32) -> f32;
extern "fg331q_floor" fn floorf(v: f32) -> f32;
extern "fg331q_log2" fn log2f(v: f32) -> f32;
extern "fg331q_e8m0half" fn e8m0half(e: i64) -> f32;
extern "fg331q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg331q_val(i: i64) -> i64 {
    if i == (0 as i64) {
        return 0 as i64;
    }

    if i == (1 as i64) {
        return 1 as i64;
    }

    if i == (2 as i64) {
        return 2 as i64;
    }

    if i == (3 as i64) {
        return 3 as i64;
    }

    if i == (4 as i64) {
        return 4 as i64;
    }

    if i == (5 as i64) {
        return 6 as i64;
    }

    if i == (6 as i64) {
        return 8 as i64;
    }

    if i == (7 as i64) {
        return 12 as i64;
    }

    if i == (8 as i64) {
        return 0 as i64;
    }

    if i == (9 as i64) {
        return 0 as i64 - (1 as i64);
    }

    if i == (10 as i64) {
        return 0 as i64 - (2 as i64);
    }

    if i == (11 as i64) {
        return 0 as i64 - (3 as i64);
    }

    if i == (12 as i64) {
        return 0 as i64 - (4 as i64);
    }

    if i == (13 as i64) {
        return 0 as i64 - (6 as i64);
    }

    if i == (14 as i64) {
        return 0 as i64 - (8 as i64);
    }
    return 0 as i64 - (12 as i64);
}
// strict less, so the first index wins ties

fn fg331q_best(x: f32, d: f32) -> i64 {
    var best: i64 = 0 as i64;
    var best_err: f32 = absf( (fg331q_val(0) as f32) * d - x);
    var i: i64 = 1 as i64;

    while i < (16 as i64) {
        let err: f32 = absf( (fg331q_val(i) as f32) * d - x);

        if err < best_err {
            best = i;
            best_err = err;
        }
        i = i + (1 as i64);
    }
    return best;
}

fn fg331q_block(x: u64, y: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let xb: u64 = x + ui * (128 as u64);
    let yb: u64 = y + ui * (17 as u64);
    var amax: f32 = 0 as f32;
    var j: i64 = 0 as i64;

    while j < (32 as i64) {
        let v: f32 = ld_f32(xb + (j as u64) * (4 as u64));

        if amax < absf(v) {
            amax = absf(v);
        }
        j = j + (1 as i64);
    }
    let e: i64 = if amax > (0 as f32) {
        (floorf(log2f(amax)) - (2 as f32) + (127 as f32)) as i64
    } else {
        0 as i64
    }
    ;
    st_u8(yb, e);
    let d: f32 = e8m0half(e);
    j = 0 as i64;

    while j < (16 as i64) {
        let x0: i64 = fg331q_best(ld_f32(xb + (j as u64) * (4 as u64)), d);
        let x1: i64 = fg331q_best(ld_f32(xb + ( (j + (16 as i64)) as u64) * (4 as u64)), d);
        st_u8(yb + (1 as u64) + (j as u64), x0 | (x1 << (4 as i64)));
        j = j + (1 as i64);
    }
    return 0 as i64;
}

fn fg331q_encode(x: u64, y: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 2100 as i64);
    ensure(k % (32 as i64) == (0 as i64), 2101 as i64);
    var nb: i64 = k / (32 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg331q_block(x, y, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg331q_encode(0 as u64, 0 as u64, 0 as i64);
}

