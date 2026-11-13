// Apex CUDA group norm. Mirrors norm.cu:group_norm_f32 including the
// denominator group_size and the two shared reduction phases.
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_shfl_xor_f32" fn shfl_xor(v:f32,off:i64)->f32;
extern "apex_sync"   fn sync();
extern "apex_rsqrtf" fn rsqrt32(x:f32)->f32;

fn warp_sum(v:f32)->f32 {
  var acc=v; var off:i64=16;
  while off>0 {acc=acc+shfl_xor(acc,off);off=off/2;}
  return acc;
}
fn block_sum(v:f32,scratch:f32[:],block_size:i64)->f32 {
  var acc=warp_sum(v);
  if block_size>32 {
    let lane=tid_x()%32; let warp=tid_x()/32;
    if lane==0 {scratch[warp]=acc;}
    sync();
    var seed:f32=(0 as f32);
    if lane<block_size/32 {seed=scratch[lane];}
    acc=warp_sum(seed);
  }
  return acc;
}

fn group_norm_f32(x:f32[:], dst:f32[:], group_size:i64, ne_elements:i64,
                  eps:f32, block_size:i64, scratch0:f32[:], scratch1:f32[:]) {
  let tid=tid_x();
  let start=bid_x()*group_size+tid;
  var end=bid_x()*group_size+group_size;
  if end>ne_elements {end=ne_elements;}
  var tmp:f32=(0 as f32);
  var j=start;
  while j<end {tmp=tmp+x[j];j+=block_size;}
  tmp=block_sum(tmp,scratch0,block_size);
  let mean=tmp/(group_size as f32);
  tmp=(0 as f32);
  j=start;
  while j<end {
    let xi=x[j]-mean;
    dst[j]=xi;
    tmp=tmp+xi*xi;
    j+=block_size;
  }
  tmp=block_sum(tmp,scratch1,block_size);
  let variance=tmp/(group_size as f32);
  let scale=rsqrt32(variance+eps);
  j=start;
  while j<end {dst[j]=dst[j]*scale;j+=block_size;}
}

fn main() {
  var x:f32[1024]=default<f32[1024]>();
  var d:f32[1024]=default<f32[1024]>();
  var a:f32[32]=default<f32[32]>();
  var b:f32[32]=default<f32[32]>();
  group_norm_f32(x,d,1024,1024,(0.00001 as f32),1024,a,b);
  print(x[0]+d[0]);
}
