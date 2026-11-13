// Division and remainder of operands that fit in 32 bits by a divisor a loop
// cannot change multiply by a 32-bit magic number (§6.2), a product a C
// compiler vectorizes. Each quotient and remainder is held to what defines
// them, q * d + r == x with r < d: every u8 dividend by every divisor, every
// u16 one, u32 ones at the ends of the range, next to the first and the
// last multiples of the divisor and spread across it, and nonnegative i16
// and i32 ones, by divisors of every shape (one, every power of two, small
// odd and even ones, ones just past a power of two, the largest, and values
// spread across the range). One divisor of a u32 division and of a u64 one
// takes a magic of each width. A zero divisor aborts at its division only
// when a division runs.
import std;

fn mix(x: u64) -> u64 { return (x ^ (x >> 29)) * 0xBF58476D1CE4E5B9; }

fn wrong(x: u64, d: u64, q: u64, r: u64) -> bool { return q * d + r != x || r >= d; }

fn check8(d: u8) -> i64 {
    var bad = 0;
    for x: u8 in 0..256 {
        if wrong(x, d, x / d, x % d) { bad += 1; }
    }
    return bad;
}

fn check16(d: u16) -> i64 {
    var bad = 0;
    for x: u16 in 0..65536 {
        if wrong(x, d, x / d, x % d) { bad += 1; }
    }
    return bad;
}

fn check32(d: u32, xs: u32[>..<]) -> i64 {
    var bad = 0;
    for x in xs {
        if wrong(x, d, x / d, x % d) { bad += 1; }
    }
    return bad;
}

// The u32 dividends for a divisor d.
fn dividends(d: u32) -> u32[>..<] {
    var xs: u32[>..<] = [0, 1, 2, 3, 0x7FFFFFFF, 0x80000000, 0x80000001, 0xFFFFFFFE, 0xFFFFFFFF];
    let dd = d as u64;
    let top = 0xFFFFFFFF / dd;
    for k: u64 in 0..40 {
        let ms: u64[] = [min(k, top) * dd, (top - min(k, top)) * dd];
        for m in ms {
            let near: u64[] = [m - min(m, 1), m, m + 1, m + dd - 1];
            for x in near { if x <= 0xFFFFFFFF { xs.push(x as u32); } }
        }
    }
    var s = dd;
    for i in 4000 {
        s = mix(s + i as! u64);
        xs.push((s >> 32) as! u32);
        xs.push((s >> 48) as! u32);
        xs.push((s >> 58) as! u32);
    }
    return xs;
}

fn check_i16(d: i16) -> i64 {
    var bad = 0;
    if d >= 1 {
        for x: i16 in 0..32767 {
            if wrong(x as u64, d as u64, (x / d) as u64, (x % d) as u64) { bad += 1; }
        }
    }
    return bad;
}

fn check_i32(d: i32, n: i32) -> i64 {
    var bad = 0;
    if d >= 1 {
        for x: i32 in 0..n {
            if wrong(x as u64, d as u64, (x / d) as u64, (x % d) as u64) { bad += 1; }
        }
    }
    return bad;
}

// The same divisor by a u32 dividend and by a u64 one.
fn mixed(d: u32, xs: u32[>..<]) -> i64 {
    var bad = 0;
    for x in xs {
        let w = (x as u64) * 0x100000 + 0xABCDE;
        if wrong(x, d, x / d, x % d) { bad += 1; }
        if wrong(w, d, w / d, w % d) { bad += 1; }
    }
    return bad;
}

// A divisor whose conversion a debug build would find failing: a release
// build divides by the truncated value (§6.3), for which no magic is made.
fn converted(d: i64, xs: u32[>..<]) -> i64 {
    var bad = 0;
    for x in xs {
        if wrong(x, d as! u32, x / (d as u32), x % (d as u32)) { bad += 1; }
    }
    return bad;
}

fn by_zero(d: u32, runs: i64) -> u32 {
    var s: u32 = 0;
    for i in runs { s += 100 / d; }
    return s;
}

fn main() {
    var bad = 0;
    for d: u8 in 1..256 { bad += check8(d); }
    print("u8 ", bad);

    bad = 0;
    let d16: u16[] = [3, 5, 6, 7, 10, 40, 100, 127, 255, 257, 641, 1000, 4097, 32767, 32769,
                      65534, 65535];
    for d in d16 { bad += check16(d); }
    var p16: u64 = 1;
    for k in 16 { bad += check16(p16 as u16); p16 *= 2; }
    print("u16 ", bad);

    bad = 0;
    let d32: u32[] = [3, 5, 6, 7, 10, 40, 641, 1000, 65535, 65537, 1000000007, 0x7FFFFFFF,
                      0x80000001, 0xFFFFFFFE, 0xFFFFFFFF];
    for d in d32 { bad += check32(d, dividends(d)); }
    var p32: u64 = 1;
    for k in 32 { bad += check32(p32 as u32, dividends(p32 as u32)); p32 *= 2; }
    var s: u64 = 99;
    for i in 30 {
        s = mix(s + i as! u64);
        let ds: u32[] = [(s >> 32) as! u32, (s >> 44) as! u32, (s >> 54) as! u32];
        for d in ds { if d > 0 { bad += check32(d, dividends(d)); } }
    }
    print("u32 ", bad);

    bad = 0;
    let ds16: i16[] = [1, 2, 3, 7, 40, 128, 255, 16384, 32767];
    for d in ds16 { bad += check_i16(d); }
    let ds32: i32[] = [1, 3, 7, 40, 65536, 1000000007, 2147483647];
    for d in ds32 { bad += check_i32(d, 200000); }
    print("signed ", bad, " ", check_i16(0), " ", check_i32(-5, 10));

    print("mixed ", mixed(7, dividends(7)) + mixed(0x80000001, dividends(0x80000001)));
    bad = 0;
    let truncated: i64[] = [-1, -2, -65536, 0x100000003];
    for d in truncated { bad += converted(d, dividends(d as! u32)); }
    print("converted ", bad);
    print(by_zero(0, 0), " ", by_zero(7, 3));
    print(by_zero(0, 1));
}
