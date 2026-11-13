// error: const for accessor belongs to Left, not Right
struct Left { x: i64 }
struct Right { x: i64 }
fn main() { print(Right { const for <N, A> in fields<Left> { A: 1, } }); }
