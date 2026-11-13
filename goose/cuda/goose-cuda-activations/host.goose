// Metadata,validation,pointer selection and launch policy for CUDA activations.
extern "fg166_arg" fn arg(p: i64, k: i64) -> i64;
extern "fg166_info" fn info(op: i64, key: i64) -> i64;
extern "fg166_ti" fn ti(t: i64, key: i64, i: i64) -> i64;
extern "fg166_tf" fn tf(t: i64, i: i64) -> f32;
extern "fg166_pred" fn pred(k: i64, a: i64, b: i64) -> i64;
extern "fg166_hi" fn hi(p: i64, k: i64, v: i64);
extern "fg166_hf" fn hf(p: i64, k: i64, v: f32);
extern "fg166_constant" fn constant(k: i64) -> i64;
extern "fg166_half_round" fn half_round(v: f32) -> f32;
extern "fg166_launch" fn launch(p: i64);
extern "fg166_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn put_shape(p: i64, t: i64, d: i64) {
    var i: i64 = 0;

    while i < 3 {
        hi(p, 7 + i, ti(t, 3, i));
        hi(p, 14 + i, ti(d, 4, i + 1));
        i += 1;
    }
    i = 0;

    while i < 4 {
        hi(p, 10 + i, ti(t, 4, i));
        i += 1;
    }
}

fn run(p: i64) {
    let code = arg(p, 0);
    let first = arg(p, 2);
    let second = arg(p, 3);
    let style = info(code, 2);
    ensure(first != 0 && arg(p, 1) != 0, 21);
    var dst = first;
    var src = ti(first, 0, 0);
    ensure(src != 0 && (style < 7 || second != 0), 22);
    var gate: i64 = 0;
    var mode = info(code, 0);
    var op = info(code, 1);

    if style == 8 {
        dst = second;
    }

    if style == 7 {
        dst = second;
        gate = if ti(second, 0, 0) == first {
            ti(second, 0, 1)
        } else {
            ti(second, 0, 0)
        }
        ;
        ensure(pred(3, src, gate) != 0, 1);
        let unary = ti(first, 7, 0);
        op = if unary == constant(8) {
            7
        } else {
            if unary == constant(9) {
                10
            } else {
                if unary == constant(10) {
                    25
                } else {
                    -1
                }
            }
        }
        ;
        ensure(op >= 0, 2);
    }
    let kind = ti(src, 2, 0);
    ensure(kind == constant(0) || kind == constant(1) || kind == constant(2), 3);
    ensure(ti(dst, 2, 0) == kind, 4);
    let ty = if kind == constant(0) {
        0
    } else {
        if kind == constant(1) {
            1
        } else {
            2
        }
    }
    ;
    let ts = if ty == 0 {
        4
    } else {
        2
    }
    ;
    let k = ti(dst, 5, 0);
    ensure(k >= 0 && k <= 9223372036854775552, 5);
    ensure(style == 1 || k == ti(src, 5, 0), 23);
    ensure(k == 0 || (ti(src, 1, 0) != 0 && ti(dst, 1, 0) != 0), 24);
    hi(p, 0, ti(src, 1, 0));
    hi(p, 2, ti(dst, 1, 0));
    hi(p, 3, k);
    hi(p, 17, ts);
    hi(p, 22, ty);
    put_shape(p, src, dst);
    var blocksize = constant(3);

    if style == 0 {
        ensure(k <= 2147483647, 6);

        if pred(0, src, 0) == 0 {
            mode = 1;
        }
    }

    if style == 1 || style == 7 {
        if style == 1 {
            gate = ti(first, 0, 1);
        }
        ensure(pred(1, src, 0) != 0 && ti(src, 4, 0) == ts, 7);

        if style == 1 {
            ensure(pred(0, dst, 0) != 0, 8);
        }

        if style == 7 {
            ensure(pred(1, gate, 0) != 0 && ti(gate, 4, 0) == ts, 9);
        }
        let nc = if style == 7 || gate != 0 {
            ti(src, 3, 0)
        } else {
            ti(src, 3, 0) / 2
        }
        ;
        ensure(ti(dst, 3, 0) == nc && ti(dst, 6, 0) == ti(src, 6, 0), 10);
        var g = if gate != 0 {
            ti(gate, 1, 0)
        } else {
            ti(src, 1, 0)
        }
        ;
        var x = ti(src, 1, 0);
        let o0 = ti(src, 4, 1);
        let o1 = if gate != 0 {
            ti(gate, 4, 1)
        } else {
            o0
        }
        ;

        if gate != 0 {
            ensure(
                pred(1, gate, 0) != 0 && ti(gate, 4, 0) == ts && ti(gate, 3, 0) == nc && ti(gate, 2, 0) == kind && ti(gate, 6, 0) == ti(dst, 6, 0),
                11);
        } else {
            let swap = ti(dst, 8, 1) != 0;

            if swap {
                x += nc * ts;
            } else {
                g += nc * ts;
            }
        }
        hi(p, 0, x);
        hi(p, 1, g);
        hi(p, 4, nc);
        hi(p, 5, o0 / ts);
        hi(p, 6, o1 / ts);
        blocksize = constant(4);

        if mode == 3 {
            ensure(ty == 0, 12);
            hf(p, 4, tf(dst, 2));
            hf(p, 5, tf(dst, 3));
        }

        if mode == 4 {
            ensure(ty != 2, 13);
            hf(p, 5, tf(dst, 3));
        }
    }

    if style == 2 {
        ensure(pred(0, src, 0) != 0 && k <= 2147483647, 14);
        var i: i64 = 0;

        while i < 4 {
            hf(p, i, tf(dst, i + 1));
            i += 1;
        }
        blocksize = constant(5);
    }

    if style == 3 {
        ensure(pred(0, src, 0) != 0 && ty != 2 && k <= 2147483647, 15);
        let grad = ti(first, 0, 1);
        ensure(grad != 0 && ti(grad, 2, 0) == kind && ti(grad, 5, 0) == k && pred(0, grad, 0) != 0, 25);
        hi(p, 0, ti(grad, 1, 0));
        hi(p, 1, ti(src, 1, 0));
        blocksize = constant(6);
    }

    if style == 4 {
        ensure(pred(0, src, 0) != 0 && ty != 2 && k <= 2147483647, 16);
        hf(p, 6, tf(dst, 0));
        blocksize = constant(7);
    }

    if style == 5 {
        ensure(ty != 1, 17);
        hf(p, 0, tf(dst, 0));
        hf(p, 1, tf(dst, 1));
        blocksize = constant(11);
    }

    if style == 6 {
        ensure(ty != 2 && k <= 4294967295 && pred(2, src, 0) != 0 && pred(2, dst, 0) != 0, 18);
        let lo = tf(dst, 0);
        let high = tf(dst, 1);
        hf(p, 4, if ty == 1 {
            half_round(lo)
        } else {
            lo
        }
        );
        hf(p, 5, if ty == 1 {
            half_round(high)
        } else {
            high
        }
        );
        blocksize = constant(12);
    }

    if style == 8 {
        ensure(pred(0, src, 0) != 0 && ty != 2 && k <= 2147483647, 19);
    }
    ensure(blocksize > 0, 20);
    var blocks = (k + blocksize - 1) / blocksize;

    if style == 2 {
        blocks = (k + blocksize) / blocksize;
    }

    if style == 5 && blocks > 2147483647 {
        blocks = 2147483647;
    }
    hi(p, 18, blocks);
    hi(p, 19, blocksize);
    hi(p, 20, mode);
    hi(p, 21, op);
    launch(p);
}

fn main() {
    run(0);
}

