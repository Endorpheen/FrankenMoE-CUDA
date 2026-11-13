// fg172r Apex RoPE host policy: ownership-free projection of native
// ggml_cuda_op_rope / ggml_cuda_op_rope_back (rope.cu) onto the fg172r plan.
// Validation and launch selection only; no arithmetic on tensor contents here.
extern "fg172r_arg" fn arg(packet: i64, k: i64) -> i64;
extern "fg172r_plan_ptr" fn plan_ptr(packet: i64) -> i64;
extern "fg172r_hget" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg172r_hput" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg172r_hfloat" fn put_float(p: i64, i: i64, v: f32);
extern "fg172r_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg172r_tf" fn tf(t: i64, i: i64) -> f32;
extern "fg172r_contiguous" fn contiguous(t: i64) -> bool;
extern "fg172r_f32_type" fn f32_type() -> i64;
extern "fg172r_f16_type" fn f16_type() -> i64;
extern "fg172r_i32_type" fn i32_type() -> i64;
extern "fg172r_hlog" fn logf(v: f32) -> f32;
extern "fg172r_hpow" fn powf(a: f32, b: f32) -> f32;
extern "fg172r_launch" fn launch(packet: i64);
extern "fg172r_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn fin(v: f32) -> bool {
    return v == v && v - v == 0.0;
}

fn mul_u(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= 18446744073709551615 as u64 / a, 1);
    return a * b;
}

fn ceil_u(a: u64, b: u64) -> u64 {
    ensure(b != 0, 1);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn floorh(x: f32) -> f32 {
    let t = (x as i64) as f32;
    return if t > x {
        t - 1.0
    } else {
        t
    }
    ;
}

fn ceilh(x: f32) -> f32 {
    let t = (x as i64) as f32;
    return if t < x {
        t + 1.0
    } else {
        t
    }
    ;
}

fn fmaxh(a: f32, b: f32) -> f32 {
    return if a > b {
        a
    } else {
        b
    }
    ;
}

fn fminh(a: f32, b: f32) -> f32 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}
// native ggml_rope_yarn_corr_dim, f32 projection (native inner divide is double)

fn corr_dim(n_dims: f32, n_ctx: f32, n_rot: f32, base: f32) -> f32 {
    return n_dims * logf(n_ctx / ( (n_rot * 2.0) * 3.14159265358979)) / (2.0 * logf(base));
}
// copy dims (elements) + byte strides (as element strides), verify checked span

fn tensor(p: i64, t: i64, dslot: i64, sslot: i64, es: i64, code: i64) -> u64 {
    var i: i64 = 0;
    var total: u64 = 1;
    var maxrow: u64 = 0;

    while i < 4 {
        let n = ti(t, 3, i);
        let nb = ti(t, 4, i);
        ensure(n > 0 && n <= 2147483647, code);
        ensure(nb >= es && (nb as u64) % (es as u64) == 0, code + 1);
        let st = (nb as u64) / (es as u64);
        put(p, 1, dslot * 4 + i, n as u64);
        put(p, 2, sslot * 4 + i, st);

        if i > 0 {
            maxrow += mul_u(st, (n - 1) as u64);
        }
        total = mul_u(total, n as u64);
        i += 1;
    }
    let span = ti(t, 9, 0);
    ensure(span >= 0 && (span as u64) / (es as u64) >= maxrow + (ti(t, 3, 0) as u64), code + 2);
    return total;
}

fn run(packet: i64) {
    let op = arg(packet, 0);
    let node = arg(packet, 2);
    ensure(op >= 0 && op <= 3 && arg(packet, 1) != 0 && node != 0, 1);
    // op 2 ggml_cuda_op_rope_fused / op 3 ggml_cuda_op_rms_norm_mul_rope_fused are not
    // part of this module: set_rows fusion needs I64 row_indices outside the fg172r
    // effect whitelist; rms+mul+rope is a separate block-reduction kernel.
    ensure(op <= 1, 40);
    let p = plan_ptr(packet);
    let dt = ti(node, 2, 0);
    ensure(dt == f32_type() || dt == f16_type(), 2);
    let dh = dt == f16_type();
    ensure(ti(node, 4, 0) == if dh {
        2
    } else {
        4
    }
    , 3);
    let x = ti(node, 0, 0);
    ensure(x != 0, 4);
    let xt = ti(x, 2, 0);
    ensure(xt == f32_type() || xt == f16_type(), 5);
    let xh = xt == f16_type();
    ensure(ti(x, 4, 0) == if xh {
        2
    } else {
        4
    }
    , 6);
    // native rule: same type, or F32 source with F16 destination
    ensure(xt == dt || (xt == f32_type() && dh), 7);
    tensor(p, x, 0, 0, if xh {
        2
    } else {
        4
    }
    , 8);
    tensor(p, node, 1, 1, if dh {
        2
    } else {
        4
    }
    , 11);
    var i: i64 = 0;

    while i < 4 {
        ensure(get(p, 1, i) == get(p, 1, 4 + i), 14);
        i += 1;
    }
    let ne00 = get(p, 1, 0);
    ensure(ne00 % 2 == 0, 15);
    let n_dims = ti(node, 8, 1);
    let mode = ti(node, 8, 2);
    let n_ctx = ti(node, 8, 4);
    let n_offs = ti(node, 8, 15);
    ensure(n_dims > 0 && n_dims % 2 == 0 && (n_dims as u64) <= ne00, 16);
    ensure(n_offs >= 0 && n_offs % 2 == 0 && ( (n_offs + n_dims) as u64) <= ne00, 17);
    ensure(mode >= 0, 18);
    let is_mrope = (mode & 8) != 0;

    if mode == 24 {
        fail(41);
        return;
    }
    let s0 = ti(node, 8, 11);
    let s1 = ti(node, 8, 12);
    let s2 = ti(node, 8, 13);
    let s3 = ti(node, 8, 14);
    ensure(s0 >= 0 && s1 >= 0 && s2 >= 0 && s3 >= 0, 20);

    if is_mrope {
        ensure(s0 > 0 || s1 > 0 || s2 > 0, 19);
    }
    let fb = tf(node, 5);
    let fs = tf(node, 6);
    let ef = tf(node, 7);
    let af = tf(node, 8);
    let bf = tf(node, 9);
    let bs = tf(node, 10);
    ensure(fin(fb) && fb > 0.0, 21);
    ensure(fin(fs) && fs > 0.0, 22);
    ensure(fin(ef), 23);
    ensure(fin(af), 24);
    ensure(fin(bf) && bf > 0.0, 25);
    ensure(fin(bs) && bs > 0.0, 26);
    ensure(n_ctx > 0, 27);
    let pos = ti(node, 0, 1);
    ensure(pos != 0, 28);
    ensure(ti(pos, 2, 0) == i32_type(), 29);
    ensure(ti(pos, 4, 0) == 4, 30);
    ensure(contiguous(pos), 31);
    tensor(p, pos, 2, 2, 4, 33);
    let need = if is_mrope {
        mul_u(get(p, 1, 2), 4)
    } else {
        get(p, 1, 2)
    }
    ;
    ensure(mul_u(mul_u(get(p, 1, 8), get(p, 1, 9)), mul_u(get(p, 1, 10), get(p, 1, 11))) >= need, 32);
    let xp = ti(x, 1, 0);
    let dp = ti(node, 1, 0);
    let pp = ti(pos, 1, 0);
    ensure(xp != 0, 46);
    ensure(dp != 0, 47);
    ensure(pp != 0, 48);
    put(p, 0, 0, xp as u64);
    put(p, 0, 1, dp as u64);
    put(p, 0, 2, pp as u64);
    var ffp: u64 = 0;
    var has_ff: u64 = 0;
    let ff = ti(node, 0, 2);

    if ff != 0 {
        ensure(ti(ff, 2, 0) == f32_type(), 36);
        ensure(ti(ff, 4, 0) == 4, 37);
        ensure(contiguous(ff), 38);
        tensor(p, ff, 3, 3, 4, 42);
        ensure(
            mul_u(mul_u(get(p, 1, 12), get(p, 1, 13)), mul_u(get(p, 1, 14), get(p, 1, 15))) >= ( (n_dims / 2) as u64),
            39);
        ffp = ti(ff, 1, 0) as u64;
        has_ff = 1;
    }
    put(p, 0, 3, ffp);
    put(p, 0, 4, 0);
    put(p, 3, 0, mode as u64);
    put(p, 3, 1, n_dims as u64);
    put(p, 3, 2, n_offs as u64);
    put(p, 3, 3, s0 as u64);
    put(p, 3, 4, s1 as u64);
    put(p, 3, 5, s2 as u64);
    put(p, 3, 6, s3 as u64);
    put(p, 3, 7, if op == 0 {
        1
    } else {
        0
    }
    );
    put(p, 3, 8, if xp == dp {
        1
    } else {
        0
    }
    );
    put(p, 3, 9, has_ff);
    put(p, 3, 10, if mode == 40 {
        1
    } else {
        0
    }
    );
    put(p, 3, 11, if xh {
        1
    } else {
        0
    }
    );
    put(p, 3, 12, if dh {
        1
    } else {
        0
    }
    );
    // native launch shape: CUDA_ROPE_BLOCK_SIZE=256, block(1,256,1), grid(nr,ceil(ne00/512),1)
    let nr = mul_u(mul_u(get(p, 1, 1), get(p, 1, 2)), get(p, 1, 3));
    ensure(nr <= 2147483647, 45);
    let gby = ceil_u(ne00, 512);
    ensure(gby <= 65535, 45);
    put(p, 3, 13, nr);
    put(p, 3, 14, gby);
    put(p, 3, 15, 1);
    put(p, 3, 16, 256);
    put(p, 3, 17, 1);
    put_float(p, 0, powf(fb, -2.0 / (n_dims as f32)));
    put_float(p, 1, fs);
    put_float(p, 2, ef);
    put_float(p, 3, af);
    // native ggml_rope_yarn_corr_dims: [0]=MAX(0,floor(..beta_fast)), [1]=MIN(n_dims/2,ceil(..beta_slow))
    put_float(p, 4, fmaxh(0.0, floorh(corr_dim(n_dims as f32, n_ctx as f32, bf, fb))));
    put_float(p, 5, fminh( ( (n_dims / 2) as f32), ceilh(corr_dim(n_dims as f32, n_ctx as f32, bs, fb))));
    launch(packet);
}
// retained entry via dummy call; the parent never executes dummy main

fn main() {
    run(0);
}

