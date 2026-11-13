// error: const for accessor belongs to Box<i32>, not Box<i64>
struct Box<T> { x: T }
fn main() {
    let r = Box<i64> { 1 };
    const for <N, A> in fields<Box<i32>> { print(r.A); }
}
