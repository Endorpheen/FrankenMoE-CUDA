extern "apex_tid_x" fn tid_x()->i64;
extern "apex_bid_x" fn bid_x()->i64;
extern "apex_bdim_x" fn bdim_x()->i64;
extern "apex_expf" fn expf_dev(x:f32)->f32;
fn silu(x:f32)->f32 {return x/(1.0+expf_dev(0.0-x));}
fn axpy(x:const f32[:], y:f32[:], a:f32) {
  let i=tid_x()+bdim_x()*bid_x();
  if i<x.len { y[i]=a*x[i]+y[i]; }
}
fn ksilu(x:const f32[:], y:f32[:]) {
  let i=tid_x()+bdim_x()*bid_x();
  if i<x.len { y[i]=silu(x[i]); }
}
fn entry(n:i64)->i64 {
  var buf:f32[8]=default<f32[8]>();
  var out:f32[8]=default<f32[8]>();
  for i in 8 {buf[i]=i as f32;}
  ksilu(buf,out);
  axpy(buf,out,2.0);
  return n;
}
fn main(){print(entry(3));}
