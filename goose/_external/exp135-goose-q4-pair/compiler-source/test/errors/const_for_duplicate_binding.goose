// error: duplicate const for binding: A
struct S { a: i64 }
fn main() { const for <A, A> in fields<S> {} }
