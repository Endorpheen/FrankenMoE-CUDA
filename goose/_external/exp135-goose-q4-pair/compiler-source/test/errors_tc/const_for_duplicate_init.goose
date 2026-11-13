// error: duplicate initializer for field x
struct S { x: i64 }
fn main() { print(S { x: 1, const for <N, A> in fields<S> { A: 2, } }); }
