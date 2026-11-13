// Complete shared broadcast host planning;no native scheduling or arithmetic imported.
extern "fg167_arg" fn arg(p: i64, k: i64) -> i64;
extern "fg167_plan_ptr" fn plan_ptr(p: i64) -> i64;
extern "fg167_hget" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg167_hput" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg167_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg167_pred" fn pred(op: i64, a: i64, b: i64) -> bool;
extern "fg167_constant" fn constant(k: i64) -> i64;
extern "fg167_launch" fn launch(packet: i64);
extern "fg167_fail" fn fail(code: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn U() -> u64 {
    return 18446744073709551615 as u64;
}

fn add(a: u64, b: u64) -> u64 {
    ensure(a <= U() - b, 1);
    return a + b;
}

fn mul(a: u64, b: u64) -> u64 {
    ensure(a == 0 || b <= U() / a, 2);
    return a * b;
}

fn minimum(a: u64, b: u64) -> u64 {
    return if a < b {
        a
    } else {
        b
    }
    ;
}

fn ceil(a: u64, b: u64) -> u64 {
    ensure(b != 0, 3);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn kind(t: i64) -> u64 {
    let ty = ti(t, 2, 0);
    ensure(ty == constant(0) || ty == constant(1) || ty == constant(2), 4);
    return if ty == constant(0) {
        0 as u64
    } else {
        if ty == constant(1) {
            1 as u64
        } else {
            2 as u64
        }
    }
    ;
}

fn width(kind: u64) -> u64 {
    return if kind == 0 {
        4 as u64
    } else {
        2 as u64
    }
    ;
}

fn magic(p: i64, key: i64, d: u64) {
    ensure(d > 0 && d <= 4294967295, 5);
    var l: u64 = 0;

    while l < 32 && ( (1 as u64) << l) < d {
        l += 1;
    }
    let mp = ( (1 as u64) << 32) * ( ( (1 as u64) << l) - d) / d + 1;
    ensure(mp <= 4294967295, 6);
    put(p, 7, key * 3, mp);
    put(p, 7, key * 3 + 1, l);
    put(p, 7, key * 3 + 2, d);
}

fn copy_shape(p: i64, t: i64, which: i64) {
    ensure(t != 0 && ti(t, 1, 0) != 0, 7);
    var i: i64 = 0;

    while i < 4 {
        let ne = ti(t, 3, i);
        let nb = ti(t, 4, i);
        ensure(ne > 0 && nb >= 0, 8);
        put(p, 1 + which, i, ne as u64);
        put(p, 4 + which, i, nb as u64);
        i += 1;
    }
    ti(t, 9, 0);
}

fn collapse(p: i64) {
    var row: i64 = 0;

    while row < 3 {
        var i: i64 = 1;

        while i < 4 {
            put(p, 4 + row, i, mul(get(p, 4 + row, i), get(p, 1 + row, i)));
            i += 1;
        }
        put(p, 1 + row, 0, mul(get(p, 1 + row, 0), get(p, 1 + row, 1)));
        put(p, 1 + row, 1, get(p, 1 + row, 2));
        put(p, 1 + row, 2, get(p, 1 + row, 3));
        put(p, 1 + row, 3, 1 as u64);
        row += 1;
    }
}

fn run(packet: i64) {
    let request = arg(packet, 0);
    let p = plan_ptr(packet);
    let dst = arg(packet, 2);
    ensure(dst != 0 && arg(packet, 1) != 0 && request >= 0 && request < 8, 9);
    let src = ti(dst, 0, 0);
    ensure(src != 0, 10);
    let a = if request == 0 {
        dst
    } else {
        src
    }
    ;
    let b = if request == 0 {
        src
    } else {
        if request == 5 {
            src
        } else {
            ti(dst, 0, 1)
        }
    }
    ;
    ensure(b != 0, 11);
    let ka = kind(a);
    let kb = kind(b);
    let kd = kind(dst);
    put(p, 10, 0, ka);
    put(p, 11, 0, kb);
    put(p, 12, 0, kd);
    copy_shape(p, dst, 0);
    copy_shape(p, a, 1);
    copy_shape(p, b, 2);
    put(p, 0, 9, ti(dst, 1, 0) as u64);
    put(p, 0, 0, if request == 0 {
        0 as u64
    } else {
        ti(a, 1, 0) as u64
    }
    );
    var count: i64 = 1;

    if request >= 6 {
        count = arg(packet, 3);
        ensure(count >= 2 && count <= 8, 12);
    }
    put(p, 14, 0, count as u64);
    let op = if request == 6 {
        1
    } else {
        if request == 7 {
            3
        } else {
            request
        }
    }
    ;
    put(p, 13, 0, op as u64);

    if request == 5 {
        ensure(ka == 0 && kd == 0 && pred(0, dst, 0) && pred(3, dst, a), 13);
        var i: i64 = 0;

        while i < 4 {
            ensure(get(p, 5, i) % 4 == 0, 14);
            i += 1;
        }
        let product = mul(get(p, 1, 2), get(p, 1, 3));
        ensure(product <= 32768 && get(p, 1, 1) <= 65535, 15);
        put(p, 15, 0, 2 as u64);
        put(p, 9, 0, 32 as u64);
        put(p, 9, 1, 1 as u64);
        put(p, 9, 2, 1 as u64);
        put(p, 8, 0, ceil(get(p, 1, 0), 32 as u64));
        put(p, 8, 1, get(p, 1, 1));
        put(p, 8, 2, product);
        launch(packet);
        return;
    }
    var mix: i64 = -1;

    if ka == 0 && kb == 0 && kd == 0 {
        mix = 0;
    }

    if ka == 2 && kb == 2 && kd == 2 {
        mix = 1;
    }

    if ka == 2 && kb == 0 && kd == 2 {
        mix = 2;
    }

    if ka == 1 && kb == 1 && kd == 1 {
        mix = 3;
    }

    if ka == 1 && kb == 0 && kd == 1 {
        mix = 4;
    }

    if ka == 1 && kb == 0 && kd == 0 {
        mix = 5;
    }

    if ka == 0 && kb == 1 && kd == 0 {
        mix = 6;
    }

    if ka == 1 && kb == 1 && kd == 0 {
        mix = 7;
    }
    ensure(mix >= 0, 16);
    put(p, 16, 0, mix as u64);
    var i: i64 = 0;

    while i < 4 {
        ensure(get(p, 1, i) == get(p, 2, i) && get(p, 1, i) % get(p, 3, i) == 0, 17);
        i += 1;
    }
    var j: i64 = 0;

    while j < count {
        let input = if request == 0 {
            b
        } else {
            ti(dst, 0, j + 1)
        }
        ;
        ensure(input != 0 && ti(input, 1, 0) != 0 && kind(input) == kb, 18);
        i = 0;

        while i < 4 {
            ensure( (ti(input, 3, i) as u64) == get(p, 3, i) && (ti(input, 4, i) as u64) == get(p, 6, i), 19);
            i += 1;
        }
        put(p, 0, j + 1, ti(input, 1, 0) as u64);
        j += 1;
    }

    if pred(0, a, 0) && pred(0, b, 0) && pred(0, dst, 0) && !pred(1, a, 0) && !pred(1, b, 0) {
        i = 0;

        while i < 4 {
            if ti(b, 3, i) / ti(dst, 3, i) != 1 {
                break;
            }

            if i > 0 {
                collapse(p);
            }
            i += 1;
        }
    }
    i = 0;

    while i < 4 {
        ensure(get(p, 1, i) <= 4294967295 && get(p, 3, i) <= 4294967295, 20);
        var row: i64 = 0;

        while row < 3 {
            let ts = width(if row == 0 {
                kd
            } else {
                if row == 1 {
                    ka
                } else {
                    kb
                }
            }
            );
            let nb = get(p, 4 + row, i);
            ensure(nb % ts == 0 && nb / ts <= 4294967295, 21);
            row += 1;
        }
        i += 1;
    }
    let n0 = get(p, 1, 0);
    let n1 = get(p, 1, 1);
    let n23 = mul(get(p, 1, 2), get(p, 1, 3));
    ensure(n23 <= 4294967295, 22);
    let half = if n0 / 2 > 0 {
        n0 / 2
    } else {
        1 as u64
    }
    ;
    let bx = minimum(half, 128 as u64);
    let by = minimum(n1, 128 / bx);
    let bz = minimum(minimum(n23, 128 / bx / by), 64 as u64);
    let gx = ceil(half, bx);
    let gy = ceil(n1, by);
    let gz = ceil(n23, bz);
    put(p, 9, 0, bx);
    put(p, 9, 1, by);
    put(p, 9, 2, bz);
    put(p, 8, 0, gx);
    put(p, 8, 1, gy);
    put(p, 8, 2, gz);
    magic(p, 0, get(p, 1, 3));
    i = 0;

    while i < 4 {
        magic(p, i + 1, get(p, 3, i));
        i += 1;
    }

    if gy > 65535 || gz > 65535 {
        let prod01 = mul(n0, n1);
        let prod012 = mul(prod01, get(p, 1, 2));
        let total = mul(prod012, get(p, 1, 3));
        let grid = ceil(total, 128 as u64);
        ensure(prod01 <= 4294967295 && prod012 <= 4294967295 && mul(grid, 128 as u64) <= 4294967295, 23);
        put(p, 15, 0, 1 as u64);
        put(p, 9, 0, 128 as u64);
        put(p, 9, 1, 1 as u64);
        put(p, 9, 2, 1 as u64);
        put(p, 8, 0, grid);
        put(p, 8, 1, 1 as u64);
        put(p, 8, 2, 1 as u64);
        magic(p, 5, n0);
        magic(p, 6, prod01);
        magic(p, 7, prod012);
    } else {
        ensure(mul(gx, bx) <= 4294967295 && mul(gy, by) <= 4294967295 && mul(gz, bz) <= 4294967295, 24);
    }
    launch(packet);
}

fn main() {
    run(0);
}

