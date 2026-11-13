// error: cannot reference a resizable value nested in a variable-size prefix
// error: in const for field S.tail
struct S { n: varint, tail: i64[>..] }
fn main() {
    var s = S { 2, [3] };
    const for <N, A> in fields<S> { print(&s.A); }
}
