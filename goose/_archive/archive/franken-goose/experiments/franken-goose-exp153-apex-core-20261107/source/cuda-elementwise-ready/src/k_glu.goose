// Apex CUDA SwiGLU. Mirrors unary.cu:unary_gated_op_kernel with op_silu.
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_bdim_x" fn bdim_x()->i64;
extern "apex_expf"   fn exp32(x:f32)->f32;

fn op_silu(x:f32)->f32 {return x/((1 as f32)+exp32((0 as f32)-x));}

fn glu_swiglu_f32(x:f32[:], g:f32[:], dst:f32[:], k:i64, n:i64, o0:i64, o1:i64) {
  let i=bdim_x()*bid_x()+tid_x();
  if i>=k {return;}
  let row=i/n;
  let j0=row*o0+(i%n);
  var j1=j0;
  if o0!=o1 {j1=row*o1+(i%n);}
  dst[i]=op_silu(x[j0])*g[j1];
}

fn main() {
  var x:f32[256]=default<f32[256]>();
  var g:f32[256]=default<f32[256]>();
  var d:f32[256]=default<f32[256]>();
  glu_swiglu_f32(x,g,d,128,128,128,128);
  print(d[0]);
}
