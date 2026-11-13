// error: struct literal mixes named and positional initializers
struct S { x: i64, y: i64 }
fn main() { print(S { x: 1, const for <C> in [2] { C, } }); }
