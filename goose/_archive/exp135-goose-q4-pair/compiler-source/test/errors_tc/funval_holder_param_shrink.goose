// error: cannot clear cx.scratch while r
// error: is still used: it may refer into cx.scratch
// The holder a function value is passed by value points where the
// caller's did, so a shrink through it is one of the caller's array.
struct Cx { scratch: i64[>..<]& }

fn with_cx<H>() {
    var scratch: i64[>..<] = [1, 2, 3];
    let r: i64& = scratch[2];
    var cx = Cx { scratch: scratch };
    H(cx);
    print(r);
}

var first: i64[>..<] = [];

fn main() {
    with_cx() { cx => cx.scratch.clear(); };
}
