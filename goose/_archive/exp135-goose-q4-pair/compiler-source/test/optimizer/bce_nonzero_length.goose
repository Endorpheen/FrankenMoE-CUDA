// `!=` bounds nothing alone, but against a side the facts already order it
// is strict: past `if xs.len == 0 { return; }` the length is at least one.
import std;

fn ends(xs: i64[:]) -> i64 {
    if xs.len == 0 { return 0; }
    return xs[xs.len - 1] - xs[0];                 // bce:elide
}

fn before(xs: i64[:], k: i64) -> i64 {
    if k >= 0 && k < xs.len {
        if k != 0 { return xs[k - 1]; }            // bce:elide
    }
    return -1;
}

// k may be negative, so k != 0 says nothing about k - 1.
fn before_any(xs: i64[:], k: i64) -> i64 {
    if k < xs.len && k != 0 { return xs[k - 1]; }  // bce:keep
    return -1;
}

fn main() {
    var xs: i64[>..] = [4, 8, 15, 16, 23, 42];
    var none: i64[>..] = [];
    print(ends(xs), " ", ends(none));
    let k = xs[0] - 4;
    print(before(xs, k), " ", before(xs, k + 3), " ", before_any(xs, k + 5), " ", before_any(xs, k));
}
