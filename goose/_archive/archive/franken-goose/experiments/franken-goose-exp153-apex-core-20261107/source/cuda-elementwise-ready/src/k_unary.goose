// Apex CUDA unary activations required by qwen35moe Apex:
// SILU (0), SIGMOID (1), SOFTPLUS (2). Mirrors unary.cu:unary_op_kernel.
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_bdim_x" fn bdim_x()->i64;
extern "apex_expf"   fn exp32(x:f32)->f32;
extern "apex_logf"   fn log32(x:f32)->f32;

fn op_silu(x:f32)->f32 {return x/((1 as f32)+exp32((0 as f32)-x));}
fn op_sigmoid(x:f32)->f32 {return (1 as f32)/((1 as f32)+exp32((0 as f32)-x));}
fn op_softplus(x:f32)->f32 {if x>(20 as f32) {return x;}return log32((1 as f32)+exp32(x));}

fn unary_f32(op:i64, x:f32[:], dst:f32[:], k:i64) {
  let i=bdim_x()*bid_x()+tid_x();
  if i>=k {return;}
  var v:f32=(0 as f32);
  if op==0 {v=op_silu(x[i]);}
  else if op==1 {v=op_sigmoid(x[i]);}
  else {v=op_softplus(x[i]);}
  dst[i]=v;
}

fn main() {
  var x:f32[256]=default<f32[256]>();
  var d:f32[256]=default<f32[256]>();
  unary_f32(0,x,d,256);
  unary_f32(1,x,d,256);
  unary_f32(2,x,d,256);
  print(d[0]);
}
