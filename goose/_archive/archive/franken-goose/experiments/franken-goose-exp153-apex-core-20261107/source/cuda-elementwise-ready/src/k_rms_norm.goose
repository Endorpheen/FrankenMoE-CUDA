// Apex CUDA RMS norm. Mirrors ggml-cuda norm.cu:rms_norm_f32 including the
// vectorised float4 path, block_reduce SUM order (xor butterfly within a warp,
// then cross-warp shared-memory phase) and the scalar fallback.
extern "apex_tid_x"   fn tid_x()->i64;
extern "apex_bid_x"   fn bid_x()->i64;
extern "apex_bid_y"   fn bid_y()->i64;
extern "apex_bid_z"   fn bid_z()->i64;
extern "apex_gdim_x"  fn gdim_x()->i64;
extern "apex_gdim_y"  fn gdim_y()->i64;
extern "apex_gdim_z"  fn gdim_z()->i64;
extern "apex_shfl_xor_f32" fn shfl_xor(v:f32,off:i64)->f32;
extern "apex_sync"    fn sync();
extern "apex_rsqrtf"  fn rsqrt32(x:f32)->f32;

fn warp_sum(v:f32)->f32 {
  var acc=v;
  var off:i64=16;
  while off>0 { acc=acc+shfl_xor(acc,off); off=off/2; }
  return acc;
}
fn block_sum(v:f32,scratch:f32[:],block_size:i64)->f32 {
  var acc=warp_sum(v);
  if block_size>32 {
    let lane=tid_x()%32;
    let warp=tid_x()/32;
    if lane==0 {scratch[warp]=acc;}
    sync();
    var seed:f32=(0 as f32);
    if lane<block_size/32 {seed=scratch[lane];}
    acc=warp_sum(seed);
  }
  return acc;
}

fn rms_norm_f32(x:f32[:], dst:f32[:], ncols:i64, nchannels:i64, nsamples:i64,
                srow:i64, sch:i64, ssamp:i64, eps:f32,
                vec:i64, block_size:i64, vpt:i64, scratch:f32[:]) {
  let nrows=gdim_x();
  let row=bid_x();
  let tid=tid_x();
  var sample=bid_z();
  while sample<nsamples {
    var channel=bid_y();
    while channel<nchannels {
      let xc=sample*ssamp+channel*sch+row*srow;
      let dstc=((sample*nchannels+channel)*nrows+row)*ncols;
      var tmp:f32=(0 as f32);
      if vec!=0 {
        let ncols4=ncols/4;
        var xs:f32[4][4]=default<f32[4][4]>();
        var k:i64=0;
        while k<vpt {
          let i=tid+k*block_size;
          if i<ncols4 {
            let base=xc+i*4;
            for q in 4 {xs[k][q]=x[base+q];}
            tmp=tmp+(((xs[k][0]*xs[k][0]+xs[k][1]*xs[k][1])+xs[k][2]*xs[k][2])+xs[k][3]*xs[k][3]);
          }
          k+=1;
        }
        tmp=block_sum(tmp,scratch,block_size);
        let scale=rsqrt32(tmp/(ncols as f32)+eps);
        k=0;
        while k<vpt {
          let i=tid+k*block_size;
          if i<ncols4 {
            for q in 4 {dst[dstc+i*4+q]=scale*xs[k][q];}
          }
          k+=1;
        }
      } else {
        var col=tid;
        while col<ncols {
          let xi=x[xc+col];
          tmp=tmp+xi*xi;
          col+=block_size;
        }
        tmp=block_sum(tmp,scratch,block_size);
        let scale=rsqrt32(tmp/(ncols as f32)+eps);
        col=tid;
        while col<ncols {
          dst[dstc+col]=scale*x[xc+col];
          col+=block_size;
        }
      }
      if block_size>32 {sync();}
      channel+=gdim_y();
    }
    sample+=gdim_z();
  }
}

fn main() {
  var x:f32[4096]=default<f32[4096]>();
  var d:f32[4096]=default<f32[4096]>();
  var s:f32[32]=default<f32[32]>();
  rms_norm_f32(x,d,4096,1,1,4096,4096,4096,(0.00001 as f32),1,1024,4,s);
  print(x[0]+d[0]);
}
