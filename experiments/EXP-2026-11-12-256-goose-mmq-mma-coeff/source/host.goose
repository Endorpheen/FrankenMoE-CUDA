// CUDA_MMQ: the float arithmetic of the MMA dot's accumulation, in Goose.
//
// Native (mmq-vec-dot.cuh, the shared mma dot):
//   float2 dmA = __half22float2(x_dm[i*sram_stride + k0/QI8_1]);
//   float2 dsB = __half22float2(y_dm[j*MMQ_TILE_Y_K + k01/QI8_1]);
//   sum[...] += dmA.x*dsB.x*C.x[l];
//   sum[...] += dmA.y*dsB.y;
// mma(C,A,B) and load_ldmatrix stay primitives; what Goose owns is the coefficient pair, which is the
// only float arithmetic in this loop besides the mma itself.

extern "fg256s_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg256s_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg256s_fail" fn fail(c:i64);
extern "fg256s_f2b" fn f2b(x:f32)->i64;
extern "fg256s_b2f" fn b2f(x:i64)->f32;
extern "fg256s_fma" fn fma(a:f32,b:f32,c:f32)->f32;
extern "fg256s_mul" fn mul(a:f32,b:f32)->f32;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn run(packet:i64)->bool {
    let ax:i64 = get(packet,0,0) as i64;
    let ay:i64 = get(packet,0,1) as i64;
    let bx:i64 = get(packet,0,2) as i64;
    let by:i64 = get(packet,0,3) as i64;
    // coefficient for C.x[l] and the additive term for the second half
    let cx:f32 = mul(b2f(ax), b2f(bx));
    let cy:f32 = mul(b2f(ay), b2f(by));
    put(packet,1,0, f2b(cx) as u64);
    put(packet,1,1, f2b(cy) as u64);
    true
}

fn main(){run(0);}
