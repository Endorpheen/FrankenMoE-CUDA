// Multiples and quotients: `x * m` and `y * m` for a constant m relate as x
// and y do, scaled, and floor quotients `x >> k` and `y >> k` as x and y do,
// divided and rounded up. That is how `2 * i + 1` meets a `2 * n` length and
// `j >> 5` a `(n + 31) >> 5` one. One multiple of a base is one base, so
// `4 * i` and `4 * i + 4` bound a slice four long.
import std;

// A bit array as nsieve's second program keeps it: a completed resize also
// bounds n, which keeps `i << 1` and `j += i` from wrapping.
fn sieve(n: i64) -> i64 {
    var flags: u32[>..] = [];
    flags.resize((n + 31) >> 5, 0);
    var count = 0;
    for i in 2..n {
        if (flags[i >> 5] >> i) & 1 == 0 {             // bce:elide
            count++;
            var j = i << 1;
            while j < n {
                flags[j >> 5] |= (1 as u32) << j;       // bce:elide
                j += i;
            }
        }
    }
    return count;
}

// One word short: n = 33 needs two.
fn short_bits(n: i64) -> i64 {
    var flags: u32[>..] = [];
    flags.resize((n + 30) >> 5, 0);
    var s = 0;
    for i in n { s += flags[i >> 5]; }                 // bce:keep
    for i in n - 1 { s += flags[i >> 5]; }             // bce:elide
    return s;
}

fn interleave(r: i64[:], a: i64[:], m: i64) -> i64 {
    let n = a.len;
    assert(r.len == 2 * n);
    for i in n {
        r[2 * i] = a[i];                               // bce:elide
        r[2 * i + 1] = -a[i];                          // bce:elide
        if i < m { r[2 * i + 2] += 1; }                // bce:keep (i may be n - 1)
        if i < m / 2 { r[3 * i] += 1; }                // bce:keep (another factor)
    }
    if n > 0 { return r[2 * n - 1]; }                  // bce:elide
    return 0;
}

fn bytes_of(out: u8[:], xs: u32[:], m: i64) {
    assert(out.len == xs.len * 4);
    for i in xs.len {
        for b in 4 { out[i * 4 + b] = (xs[i] >> (b * 8)) as! u8; }   // bce:elide
        for b in 5 { if b < m { out[i * 4 + b] ^= 1; } }             // bce:keep (b may be 4)
        let w = out[4 * i..4 * i + 4];                               // bce:elide
        w[3] = w[0] + w[1] + w[2];                                   // bce:elide
        let v = out[(i + 1) * 4 - 4..(i + 1) * 4];                   // bce:elide
        v[0] = v[3];                                                 // bce:elide
    }
}

// A negative dividend floors below zero.
fn signed_shift(a: i64[:], x: i64) -> i64 {
    if x < a.len { return a[x >> 2]; }                 // bce:keep
    return 0;
}

fn main() {
    var a: i64[>..] = [];
    for i in 7 { a.push(i * 10 + 3); }
    // Values read from elements, so no call site states anything about them.
    let n = a[4] * 10 - 3;                             // bce:elide
    let k = a[0] - 3;                                  // bce:elide
    print(sieve(n), " ", sieve(1000), " ", short_bits(n + 11));
    var r: i64[>..] = [];
    r.resize(2 * a.len, 0);
    print(interleave(r, a, a.len - 1 + k), " ", r);
    var xs: u32[>..] = [0x04030201, 0x08070605, 0x0c0b0a09];
    var out: u8[>..] = [];
    out.resize(xs.len * 4, 0);
    bytes_of(out, xs, 4 + k);
    var sum = 0;
    for b, i in out { sum += b * (i + 1); }
    print(sum, " ", signed_shift(a, 9 + k), " ", signed_shift(a, 2 + k));
}
