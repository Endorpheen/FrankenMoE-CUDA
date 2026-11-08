// Apex CUDA LayerNorm. Mirrors norm.cu:norm_f32 (sum and sum-of-squares are
// separate warp xors, matching float2 block_reduce order).
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_bid_y"  fn bid_y()->i64;
extern "apex_bid_z"  fn bid_z()->i64;
extern "apex_gdim_x" fn gdim_x()->i64;
extern "apex_gdim_y" fn gdim_y()->i64;
extern "apex_gdim_z" fn gdim_z()->i64;
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

fn norm_f32(x:f32[:], dst:f32[:], ncols:i64, nchannels:i64, nsamples:i64,
            srow:i64, sch:i64, ssamp:i64, eps:f32, block_size:i64, scratch:f32[:]) {
  let nrows=gdim_x();
  let row=bid_x();
  let tid=tid_x();
  var sample=bid_z();
  while sample<nsamples {
    var channel=bid_y();
    while channel<nchannels {
      let xc=sample*ssamp+channel*sch+row*srow;
      let dstc=((sample*nchannels+channel)*nrows+row)*ncols;
      var total:f32=(0 as f32);
      var squares:f32=(0 as f32);
      var col=tid;
      while col<ncols {
        let xi=x[xc+col];
        total=total+xi;
        squares=squares+xi*xi;
        col+=block_size;
      }
      total=block_sum(total,scratch,block_size);
      squares=block_sum(squares,scratch,block_size);
      let mean=total/(ncols as f32);
      let variance=squares/(ncols as f32)-mean*mean;
      let inv_std=rsqrt32(variance+eps);
      col=tid;
      while col<ncols {
        dst[dstc+col]=(x[xc+col]-mean)*inv_std;
        col+=block_size;
      }
      if block_size>32 {sync();}
      channel+=gdim_y();
    }
    sample+=gdim_z();
  }
}

fn main() {
  var x:f32[1024]=default<f32[1024]>();
  var d:f32[1024]=default<f32[1024]>();
  var s:f32[32]=default<f32[32]>();
  norm_f32(x,d,1024,1,1,1024,1024,1024,(0.00001 as f32),1024,s);
  print(x[0]+d[0]);
}
