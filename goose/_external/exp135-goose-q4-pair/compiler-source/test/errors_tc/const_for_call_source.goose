// error: const for source requires a scalar or string constant
fn f() -> i64 { 1 }
fn main() { const for <C> in [f()] {} }
