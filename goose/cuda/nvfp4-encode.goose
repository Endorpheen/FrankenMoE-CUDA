// NVFP4 encode in Goose, from ggml-quants.c: quantize_row_nvfp4_ref.
//
// block_nvfp4 is 36 bytes: d[4] (u8) 0, qs[32] 4. Four sub-blocks of 16: absolute max, UE4M3 scale of
// amax/6 (0 when amax is not positive, clipped at 448), then eight bytes packing two strict-less
// absolute-error indices from the doubled e2m1 table. The two UE4M3 conversions stay host primitives.
extern "fg332q_ld_f32" fn ld_f32(a: u64) -> f32;
extern "fg332q_st_u8" fn st_u8(a: u64, v: i64);
extern "fg332q_abs" fn absf(v: f32) -> f32;
extern "fg332q_ue4m3" fn ue4m3(v: i64) -> f32;
extern "fg332q_to_ue4m3" fn to_ue4m3(v: f32) -> i64;
extern "fg332q_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fg332q_val(i: i64) -> i64 {
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

fn fg332q_best(x: f32, d: f32) -> i64 {
    var best: i64 = 0 as i64;
    var best_err: f32 = absf( (fg332q_val(0) as f32) * d - x);
    var i: i64 = 1 as i64;

    while i < (16 as i64) {
        let err: f32 = absf( (fg332q_val(i) as f32) * d - x);

        if err < best_err {
            best = i;
            best_err = err;
        }
        i = i + (1 as i64);
    }
    return best;
}

fn fg332q_block(x: u64, y: u64, i: i64) -> i64 {
    let ui: u64 = i as u64;
    let xb: u64 = x + ui * (256 as u64);
    let yb: u64 = y + ui * (36 as u64);
    var s: i64 = 0 as i64;

    while s < (4 as i64) {
        let xs: u64 = xb + (s as u64) * (64 as u64);
        var amax: f32 = 0 as f32;
        var j: i64 = 0 as i64;

        while j < (16 as i64) {
            let v: f32 = ld_f32(xs + (j as u64) * (4 as u64));

            if amax < absf(v) {
                amax = absf(v);
            }
            j = j + (1 as i64);
        }
        let ue: i64 = to_ue4m3(amax / (6 as f32));
        st_u8(yb + (s as u64), ue);
        let d: f32 = ue4m3(ue);
        let qs: u64 = yb + (4 as u64) + (s as u64) * (8 as u64);
        j = 0 as i64;

        while j < (8 as i64) {
            let x0: i64 = fg332q_best(ld_f32(xs + (j as u64) * (4 as u64)), d);
            let x1: i64 = fg332q_best(ld_f32(xs + ( (j + (8 as i64)) as u64) * (4 as u64)), d);
            st_u8(qs + (j as u64), x0 | (x1 << (4 as i64)));
            j = j + (1 as i64);
        }
        s = s + (1 as i64);
    }
    return 0 as i64;
}

fn fg332q_encode(x: u64, y: u64, k: i64) -> i64 {
    ensure(k >= (0 as i64), 2200 as i64);
    ensure(k % (64 as i64) == (0 as i64), 2201 as i64);
    var nb: i64 = k / (64 as i64);
    var i: i64 = 0 as i64;

    while i < nb {
        fg332q_block(x, y, i);
        i = i + (1 as i64);
    }
    return 0 as i64;
}

fn main() {
    fg332q_encode(0 as u64, 0 as u64, 0 as i64);
}

