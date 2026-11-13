// error: const for accessor belongs to Left, not Right
// error: in const for field Left.x
struct Left { x: i64 }
struct Right { x: i64 }
fn main() {
    let r = Right { 1 };
    const for <N, A> in fields<Left> { print(r.A); }
}
