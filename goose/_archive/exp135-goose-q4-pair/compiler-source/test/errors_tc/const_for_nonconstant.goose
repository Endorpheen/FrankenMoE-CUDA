// error: const for source C names a local variable, not a constant
const C = 1;
fn main() { const C = 2; const for <V> in [C] { print(V); } }
