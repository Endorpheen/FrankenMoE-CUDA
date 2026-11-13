// error: continue cannot cross a const for expansion boundary
fn main() { for i in 0..2 { const for <C> in [1] { continue; } } }
