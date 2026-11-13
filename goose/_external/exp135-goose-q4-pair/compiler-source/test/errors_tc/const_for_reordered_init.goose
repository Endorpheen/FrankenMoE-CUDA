// error: reordering may cross a side effect
struct S { x: i64, y: i64 }
fn effect() -> i64 { print("effect"); 1 }
fn main() { print(S { y: effect(), const for <C> in [1] { x: effect(), } }); }
