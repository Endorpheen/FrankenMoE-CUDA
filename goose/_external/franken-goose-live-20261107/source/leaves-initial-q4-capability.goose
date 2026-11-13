extern "expf" fn exp32(x:f32)->f32;
fn goose_silu(x:f32)->f32 { let one:f32=1.0; return x/(one+exp32(-x)); }
fn goose_scale(x:f32,scale:f32,bias:f32)->f32 { return scale*x+bias; }
fn goose_q4low(x:u8)->u8 { return x & 15; }
fn main() { let x:f32=0.5;print(goose_silu(x));print(goose_scale(x,x,x));let q:u8=37;print(goose_q4low(q)); }
