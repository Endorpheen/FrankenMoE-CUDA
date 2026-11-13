// A variable's `v <= len(P) + c` invariant takes its offset from the
// declaration: a stride loop that starts at 1 stays within len + 1, which
// keeps `j + 4` from wrapping so its condition bounds the reads, and a masked
// probe index starts and stays below len, which bounds it as an index.
import std;

fn stride(xs: f64[:]) -> f64 {
    var s = 0.0;
    var j = 1;
    while j + 4 <= xs.len {
        s += xs[j] + xs[j + 1] + xs[j + 2] + xs[j + 3];   // bce:elide
        j += 4;
    }
    return s;
}

// A start the facts say nothing about may be negative.
fn stride_from(xs: f64[:], k: i64) -> f64 {
    var s = 0.0;
    var j = k;
    while j + 4 <= xs.len {
        s += xs[j];                                       // bce:keep
        j += 4;
    }
    return s;
}

fn probe(slots: i64[:], h: i64) -> i64 {
    if slots.len == 0 { return -1; }
    let mask = slots.len - 1;
    var i = h & mask;
    while slots[i] != 0 { i = (i + 1) & mask; }          // bce:elide
    return i;
}

fn probe_u64(slots: u64[:], h: u64) -> i64 {
    if slots.len == 0 { return -1; }
    let mask = slots.len - 1;
    var i = (h & mask as u64) as! i64;
    while slots[i] != 0 { i = (i + 1) & mask; }          // bce:elide
    return i;
}

// A mask wider than the length can reach past it.
fn probe_wide(slots: i64[:], h: i64) -> i64 {
    let mask = slots.len * 2 - 1;
    var i = h & mask;
    while slots[i] != 0 { i = (i + 1) & mask; }          // bce:keep
    return i;
}

fn main() {
    var xs: f64[>..] = [];
    for i in 11 { xs.push(i * 0.5); }
    var slots: i64[>..] = [3, 0, 5, 7, 0, 2, 9, 4];
    var wide: u64[>..] = [3, 0, 5, 7];
    let k = slots[0] - 2;                                 // bce:elide
    print(stride(xs), " ", stride_from(xs, k), " ", stride_from(xs, k + 1));
    print(probe(slots, 5), " ", probe(slots, 13 + k), " ", probe_u64(wide, 2));
    print(probe_wide(slots, 4), " ", probe_wide(slots, k));
}
