// Apex CUDA SET_ROWS host validation, routing and launch planning from inputs/set-rows.cu.
// Scope: src0 F32 -> dst F32 or Q4_0, index I32 or I64. Other native type pairs return
// false and stay on the parent's preserved native path. Shapes, counts, strides,
// broadcast, checked spans, grid and control live here. C++ effects are limited to raw
// tensor-field reads, enum constant queries, plan access and the parent-owned raw SDK
// launch adapter. No CUDA kernel, ggml struct, heap, VM or original algorithm is wrapped.
//
// Caller owns storage bounds. This module proves only that every computed byte span is
// representable (no u64/i64 overflow), that the index descriptor is I32/I64 with its exact
// element width, and that dst does not alias src0/src1. It cannot see allocation sizes.
// The device counterpart additionally guards dst_row with has_cap+dst_rows. The launch
// adapter must pass has_cap=true and the dst_rows value written below; if that is not
// possible the caller must not launch this path.
extern "fg175s_arg" fn arg(packet: i64, k: i64) -> i64;
extern "fg175s_plan" fn plan_ptr(packet: i64) -> i64;
extern "fg175s_put" fn put(p: i64, k: i64, i: i64, v: u64);
extern "fg175s_get" fn get(p: i64, k: i64, i: i64) -> u64;
extern "fg175s_ti" fn ti(t: i64, k: i64, i: i64) -> i64;
extern "fg175s_tc" fn tc(k: i64) -> i64;
extern "fg175s_oc" fn oc(k: i64) -> i64;
extern "fg175s_launch" fn launch(packet: i64);
extern "fg175s_fail" fn fail(code: i64);

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

fn sub(a: u64, b: u64) -> u64 {
    ensure(a >= b, 3);
    return a - b;
}

fn itg(a: i64) -> u64 {
    ensure(a >= 0, 4);
    return a as u64;
}

fn cld(a: u64, b: u64) -> u64 {
    ensure(b != 0, 5);
    return a / b + if a % b != 0 {
        1 as u64
    } else {
        0 as u64
    }
    ;
}

fn nev(t: i64, i: i64) -> u64 {
    return itg(ti(t, 3, i));
}

fn nbv(t: i64, i: i64) -> u64 {
    return itg(ti(t, 4, i));
}
// init_fastdiv_values: [mp,L,d] exactly as native; d is the preserved logical divisor.

fn magic(p: i64, key: i64, d: u64) {
    ensure(d > 0 && d <= 4294967295, 6);
    var l: u64 = 0;

    while l < 32 && ( (1 as u64) << l) < d {
        l += 1;
    }
    let mp = ( (1 as u64) << 32) * ( ( (1 as u64) << l) - d) / d + 1;
    ensure(mp <= 4294967295, 7);
    put(p, 8, key * 3, mp);
    put(p, 8, key * 3 + 1, l);
    put(p, 8, key * 3 + 2, d);
}

fn ovl(a: u64, na: u64, b: u64, nb: u64) -> bool {
    return a < add(b, nb) && b < add(a, na);
}

fn run(packet: i64) -> bool {
    let dst = arg(packet, 0);

    if dst == 0 {
        return false;
    }

    if ti(dst, 5, 0) != oc(0) {
        return false;
    }
    let s0 = ti(dst, 0, 0);
    let s1 = ti(dst, 0, 1);

    if s0 == 0 || s1 == 0 {
        return false;
    }
    let t0 = ti(s0, 2, 0);
    let t1 = ti(s1, 2, 0);
    let td = ti(dst, 2, 0);

    if t0 != tc(1) {
        return false;
    }

    if t1 != tc(8) && t1 != tc(9) {
        return false;
    }
    var q4: bool = false;

    if td == tc(3) {
        q4 = true;
    } else {
        if td != tc(1) {
            return false;
        }
    }
    let n00 = nev(s0, 0);
    let n01 = nev(s0, 1);
    let n02 = nev(s0, 2);
    let n03 = nev(s0, 3);
    let n10 = nev(s1, 0);
    let n11 = nev(s1, 1);
    let n12 = nev(s1, 2);
    let n13 = nev(s1, 3);
    let d0 = nev(dst, 0);
    let d1 = nev(dst, 1);
    let d2 = nev(dst, 2);
    let d3 = nev(dst, 3);

    if n00 == 0 || n01 == 0 || n02 == 0 || n03 == 0 || n10 == 0 || n11 == 0 || n12 == 0 || n13 == 0 {
        return false;
    }

    if d0 == 0 || d1 == 0 || d2 == 0 || d3 == 0 {
        return false;
    }
    ensure(n10 >= n01, 20);
    ensure(d0 >= n00, 21);
    ensure(d2 >= n02 && d3 >= n03, 22);

    if q4 {
        ensure(n00 % 32 == 0, 23);
        ensure(d0 % 32 == 0, 24);
    }
    let nb00 = nbv(s0, 0);
    let nb01 = nbv(s0, 1);
    let nb02 = nbv(s0, 2);
    let nb03 = nbv(s0, 3);
    let nb10 = nbv(s1, 0);
    let nb11 = nbv(s1, 1);
    let nb12 = nbv(s1, 2);
    let nb13 = nbv(s1, 3);
    let db0 = nbv(dst, 0);
    let db1 = nbv(dst, 1);
    let db2 = nbv(dst, 2);
    let db3 = nbv(dst, 3);
    let isz: u64 = if t1 == tc(8) {
        4 as u64
    } else {
        8 as u64
    }
    ;
    let dbs: u64 = if q4 {
        18 as u64
    } else {
        4 as u64
    }
    ;
    let qk: u64 = if q4 {
        32 as u64
    } else {
        1 as u64
    }
    ;
    ensure(nb00 == 4, 30);
    ensure(nb01 % 4 == 0 && nb02 % 4 == 0 && nb03 % 4 == 0, 31);
    ensure(nb10 == isz, 32);
    ensure(nb11 % isz == 0 && nb12 % isz == 0 && nb13 % isz == 0, 33);
    ensure(db0 == dbs, 34);
    ensure(db1 % dbs == 0 && db2 % dbs == 0 && db3 % dbs == 0, 35);
    let a0 = ti(s0, 1, 0) as u64;
    let a1 = ti(s1, 1, 0) as u64;
    let ad = ti(dst, 1, 0) as u64;

    if a0 == 0 || a1 == 0 || ad == 0 {
        fail(36);
    }
    let s01 = nb01 / 4;
    let s02 = nb02 / 4;
    let s03 = nb03 / 4;
    let s10 = nb10 / isz;
    let s11 = nb11 / isz;
    let s12 = nb12 / isz;
    let s1n: u64 = if q4 {
        db1
    } else {
        db1 / 4
    }
    ;
    let s2n: u64 = if q4 {
        db2
    } else {
        db2 / 4
    }
    ;
    let s3n: u64 = if q4 {
        db3
    } else {
        db3 / 4
    }
    ;
    let total = mul(mul(mul(n00, n01), n02), n03);
    let nt: u64 = if q4 {
        total / 32
    } else {
        total
    }
    ;
    ensure(nt > 0, 37);

    if q4 {
        ensure(nt <= 134217728, 38);
    } else {
        ensure(nt <= 4294967296, 39);
    }
    ensure(n00 <= 4294967295 && n01 <= 4294967295 && n02 <= 4294967295 && n11 <= 4294967295 && n12 <= 4294967295, 40);
    let gx = cld(nt, 256);
    ensure(gx <= 2147483647, 41);
    let sp0 = add(add(mul(4, n00), mul(sub(n01, 1), nb01)), add(mul(sub(n02, 1), nb02), mul(sub(n03, 1), nb03)));
    let sp1 = add(add(mul(isz, n10), mul(sub(n11, 1), nb11)), add(mul(sub(n12, 1), nb12), mul(sub(n13, 1), nb13)));
    let db0sz: u64 = if q4 {
        mul(d0 / 32, 18)
    } else {
        mul(4, d0)
    }
    ;
    let spd = add(add(db0sz, mul(sub(d1, 1), db1)), add(mul(sub(d2, 1), db2), mul(sub(d3, 1), db3)));
    ensure(sp0 <= 9223372036854775807 && sp1 <= 9223372036854775807 && spd <= 9223372036854775807, 42);
    add(a0, sp0);
    add(a1, sp1);
    add(ad, spd);

    if ovl(ad, spd, a1, sp1) {
        fail(43);
    }

    if ovl(ad, spd, a0, sp0) {
        fail(44);
    }
    let vs = ti(dst, 6, 0) as u64;
    let vo = itg(ti(dst, 7, 0));

    if vo > 0 && vs == 0 {
        fail(45);
    }
    let p = plan_ptr(packet);
    put(p, 0, 0, a0);
    put(p, 0, 1, a1);
    put(p, 0, 2, ad);
    put(p, 0, 3, vs);
    put(p, 0, 4, vo);
    put(p, 1, 0, n00);
    put(p, 1, 1, n01);
    put(p, 1, 2, n02);
    put(p, 1, 3, n03);
    put(p, 2, 0, n10);
    put(p, 2, 1, n11);
    put(p, 2, 2, n12);
    put(p, 2, 3, n13);
    put(p, 3, 0, d0);
    put(p, 3, 1, d1);
    put(p, 3, 2, d2);
    put(p, 3, 3, d3);
    put(p, 4, 0, s01);
    put(p, 4, 1, s02);
    put(p, 4, 2, s03);
    put(p, 5, 0, s10);
    put(p, 5, 1, s11);
    put(p, 5, 2, s12);
    put(p, 6, 0, s1n);
    put(p, 6, 1, s2n);
    put(p, 6, 2, s3n);
    put(p, 7, 0, nt);
    put(p, 7, 1, n10);
    put(p, 7, 2, n11);
    put(p, 7, 3, n12);
    put(p, 7, 4, n13);
    put(p, 7, 5, gx);
    put(p, 7, 6, 1 as u64);
    put(p, 7, 7, 1 as u64);
    put(p, 7, 8, 256 as u64);
    put(p, 7, 9, 0 as u64);
    let kind: u64 = if q4 {
        1 as u64
    } else {
        0 as u64
    }
    ;
    let ik: u64 = if t1 == tc(8) {
        0 as u64
    } else {
        1 as u64
    }
    ;
    put(p, 7, 10, kind);
    put(p, 7, 11, ik);
    put(p, 7, 12, kind * 2 + ik);
    put(p, 7, 13, 1 as u64);
    put(p, 7, 14, d1);
    put(p, 7, 15, spd);
    put(p, 7, 16, 0 as u64);
    put(p, 7, 17, qk);
    put(p, 7, 18, dbs);
    put(p, 7, 19, sp0);
    put(p, 7, 20, sp1);
    magic(p, 0, n00);
    magic(p, 1, n01);
    magic(p, 2, n02);
    magic(p, 3, n11);
    magic(p, 4, n12);
    put(p, 9, 0, n00);
    put(p, 9, 1, n01);
    put(p, 9, 2, n02);
    put(p, 9, 3, n11);
    put(p, 9, 4, n12);
    launch(packet);
    return true;
}

fn main() {
    run(0);
}

