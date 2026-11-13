// error: const for source requires exactly 2 binding(s)
struct S { a: i64 }
fn main() { const for <N, A, U> in fields<S> {} }
