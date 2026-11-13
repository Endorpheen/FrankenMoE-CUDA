extern "apex_tid_x" fn tid_x()->i64;
extern "apex_bid_x" fn bid_x()->i64;
extern "apex_bdim_x" fn bdim_x()->i64;
extern "apex_expf" fn expf_dev(x:f32)->f32;
fn axpy(x:const f32[:], y:f32[:], a:f32) {
  let i=tid_x()+bdim_x()*bid_x();
  if i<x.len { y[i]=a*x[i]+y[i]; }
}
fn silu(x:f32)->f32 {return x/(1.0+expf_dev(0.0-x));}
fn entry(n:i64)->i64 {return n;}
fn main(){print(entry(3));}
