// error: const for binding A is not a value
struct S { x: i64 }
fn main() { const for <N, A> in fields<S> { print(A); } }
