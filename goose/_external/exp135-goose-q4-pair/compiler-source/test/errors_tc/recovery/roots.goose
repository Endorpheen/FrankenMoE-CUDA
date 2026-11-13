// error: expected a value of type i64, got const u8[:]
// error: expected a value of type bool, got i64
// error: operator + cannot be applied to i64 and bool
// error: no field access on a value of type i64
// error: expected a value of type i64, got const u8[:]
// error: operator + cannot be applied to i64 and const u8[:]
// The checker goes on past an error to the next root
// (docs/design/language_server.md §5 D2): the globals' initializers, main,
// the thread entry points, the functions nothing reached, and the exports,
// in that order. An error ends its root: main's second error, after the
// call of helper, goes unreported, as does unreached_a's, whose call of
// helper would reuse the specialization main's error cut short.
let g: i64 = "not a number";

fn helper(x: i64) -> i64 {
    let bad: bool = x;
    x
}

fn fine(x: i64) -> i64 { x * 2 }

fn unreached_a() -> i64 {
    let y = helper(1);
    let z: i64 = true;
    y + z
}

fn unreached_b() -> i64 {
    let y = fine(1);
    y.nope
}

fn unreached_c() -> i64 {
    let s: i64 = "str";
    s
}

thread_fn worker(n: i64) {
    print(n + true);
}

export fn exported(a: i64) -> i64 {
    a + "x"
}

fn main() {
    let a = fine(2);
    let b = helper(a);
    let c: bool = a;
    print(a + b + g);
}
