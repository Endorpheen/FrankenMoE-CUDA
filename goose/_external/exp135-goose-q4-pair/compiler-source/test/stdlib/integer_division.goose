// runtime-debug: both division conventions, all integer widths and limits.
import std;

fn check<T>(a: T, b: T, q: T, r: T, eq: T, er: T) {
    let actual_r: T = rem(a, b);
    let actual_q: T = div_euclid(a, b);
    assert(a / b == q);
    assert(actual_r == r);
    assert(actual_q == eq);
    assert(a % b == er);
    assert(a.rem(b) == r && a.div_euclid(b) == eq);
}

fn signed<T>(lo: T, hi: T) {
    check<T>(7, 3, 2, 1, 2, 1);
    check<T>(-7, 3, -2, -1, -3, 2);
    check<T>(7, -3, -2, 1, -2, 1);
    check<T>(-7, -3, 2, -1, 3, 2);
    check<T>(-6, 3, -2, 0, -2, 0);
    check<T>(-6, -3, 2, 0, 2, 0);
    check<T>(0, -3, 0, 0, 0, 0);
    check<T>(-1, 10, 0, -1, -1, 9);
    check<T>(lo, 1, lo, 0, lo, 0);
    check<T>(lo, lo, 1, 0, 1, 0);
    check<T>(hi, lo, 0, hi, 0, hi);
    check<T>(-1, lo, 0, -1, 1, hi);
    check<T>(lo, hi, -1, -1, -2, hi - 1);
    let zero: T = rem(lo, -1);
    assert(zero == 0);
}

fn unsigned<T>(hi: T) {
    var values: T[>..] = [0, 1, 7, hi - 1, hi];
    var divisors: T[>..] = [1, 3, hi - 1, hi];
    for a in values {
        for b in divisors {
            let q: T = div_euclid(a, b);
            let r: T = rem(a, b);
            assert(q == a / b && r == a % b);
            assert(b * q + r == a);
        }
    }
}

fn exhaustive_i8() {
    for a in -128..128 {
        for b in -128..128 {
            if b == 0 { continue; }
            let x = a as i8;
            let y = b as i8;
            let r: i8 = rem(x, y);
            assert(r == a - (a / b) * b);
            assert(r == 0 || (r < 0) == (a < 0));
            if a == -128 && b == -1 { continue; }
            let q: i8 = div_euclid(x, y);
            let er = x % y;
            // Reconstruct at i64: the product can leave i8 even when q fits.
            assert(b * q + er == a);
            assert(er >= 0 && er < abs(b));
        }
    }
}

fn main() {
    signed<i8>(-128, 127);
    signed<i16>(-32768, 32767);
    signed<i32>(-2147483648, 2147483647);
    signed<i64>(-9223372036854775808, 9223372036854775807);
    unsigned<u8>(255);
    unsigned<u16>(65535);
    unsigned<u32>(4294967295);
    unsigned<u64>(18446744073709551615);
    check<i64>(-9223372036854775808, 3, -3074457345618258602, -2, -3074457345618258603, 1);
    check<i64>(-9223372036854775808, -3, 3074457345618258602, -2, 3074457345618258603, 1);
    check<i64>(-9223372036854775807, 3, -3074457345618258602, -1, -3074457345618258603, 2);
    // Storage keeps the limit cases runtime values at every optimization level.
    var cases: i64[6][>..] = [
        [-9223372036854775808, 3, -3074457345618258602, -2, -3074457345618258603, 1],
        [-9223372036854775808, -3, 3074457345618258602, -2, 3074457345618258603, 1],
        [-9223372036854775807, 3, -3074457345618258602, -1, -3074457345618258603, 2],
        [-1, -9223372036854775808, 0, -1, 1, 9223372036854775807],
        [-9223372036854775808, -9223372036854775808, 1, 0, 1, 0],
        [-9223372036854775808, 9223372036854775807, -1, -1, -2, 9223372036854775806]
    ];
    for c in cases { check(c[0], c[1], c[2], c[3], c[4], c[5]); }
    assert(rem(cases[0][0], -1) == 0);
    exhaustive_i8();
    print("integer division ok");
}
