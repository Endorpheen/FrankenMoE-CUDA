// error: const for binding C is not a type
fn main() { const for <C> in [1] { let x: C = 0; } }
