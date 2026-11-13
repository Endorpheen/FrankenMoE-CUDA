// error: const for field accessor is not callable
struct S { push: i64 }
fn main() {
    var a: i64[>..] = [];
    const for <N, A> in fields<S> { a.A(1); }
}
