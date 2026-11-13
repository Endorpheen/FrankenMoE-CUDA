// CUDA_MMQ: the MMA-dot accumulation step, as device code.
//   sum += dmA.x*dsB.x*C.x[l];
// Native evaluates (dmA.x*dsB.x)*C.x[l] left to right, then adds; the same order is kept here.

extern "fg256d_fail" fn fail(c:i64);
extern "fg256d_b2f" fn b2f(x:i64)->f32;
extern "fg256d_f2b" fn f2b(x:f32)->i64;
extern "fg256d_mul" fn mul(a:f32,b:f32)->f32;
extern "fg256d_add" fn add(a:f32,b:f32)->f32;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn acc(ax:i64, bx:i64, cx:i64, sum:i64) -> i64 {
    let coef:f32 = mul(b2f(ax), b2f(bx));
    let term:f32 = mul(coef, b2f(cx));
    let s:f32 = add(b2f(sum), term);
    f2b(s)
}

// second half of the native pair: sum += dmA.y*dsB.y
fn acc2(ay:i64, by:i64, sum:i64) -> i64 {
    let term:f32 = mul(b2f(ay), b2f(by));
    f2b(add(b2f(sum), term))
}

fn main(){ acc(0 as i64,0 as i64,0 as i64,0 as i64); acc2(0 as i64,0 as i64,0 as i64); }
