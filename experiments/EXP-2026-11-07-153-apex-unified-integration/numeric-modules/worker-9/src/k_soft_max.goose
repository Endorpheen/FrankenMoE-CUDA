// Apex CUDA soft_max_ext path. Mirrors softmax.cu:soft_max_f32 for the
// non-sink, row-per-block case used by qwen35moe Apex (optional F32/F16 mask,
// scale and ALiBi slope). Values are cached in dst like the native low-shared
// fallback; the reduction tree is the native xor butterfly.
extern "apex_tid_x"  fn tid_x()->i64;
extern "apex_bid_x"  fn bid_x()->i64;
extern "apex_bid_y"  fn bid_y()->i64;
extern "apex_bid_z"  fn bid_z()->i64;
extern "apex_gdim_x" fn gdim_x()->i64;
extern "apex_gdim_y" fn gdim_y()->i64;
extern "apex_shfl_xor_f32" fn shfl_xor(v:f32,off:i64)->f32;
extern "apex_sync"   fn sync();
extern "apex_expf"   fn exp32(x:f32)->f32;
extern "apex_powf"   fn pow32(a:f32,b:f32)->f32;
extern "apex_fmaxf"  fn fmax32(a:f32,b:f32)->f32;
extern "apex_neg_inf" fn neg_inf()->f32;
extern "apex_from_bits" fn from_bits(u:u32)->f32;
extern "apex_f16_to_f32" fn f16_to_f32(b:u16)->f32;

fn read16(a:const u8[:],i:i64)->u16 {
  assert(i>=0&&i<=a.len-2);
  return (a[i] as u16)|((a[i+1] as u16)<<8);
}
fn read32(a:const u8[:],i:i64)->u32 {
  assert(i>=0&&i<=a.len-4);
  return (a[i] as u32)|((a[i+1] as u32)<<8)|((a[i+2] as u32)<<16)|((a[i+3] as u32)<<24);
}
fn warp_sum(v:f32)->f32 {
  var acc=v; var off:i64=16;
  while off>0 {acc=acc+shfl_xor(acc,off);off=off/2;}
  return acc;
}
fn warp_max(v:f32)->f32 {
  var acc=v; var off:i64=16;
  while off>0 {acc=fmax32(acc,shfl_xor(acc,off));off=off/2;}
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
fn block_max(v:f32,scratch:f32[:],block_size:i64)->f32 {
  var acc=warp_max(v);
  if block_size>32 {
    let lane=tid_x()%32; let warp=tid_x()/32;
    if lane==0 {scratch[warp]=acc;}
    sync();
    var seed=neg_inf();
    if lane<block_size/32 {seed=scratch[lane];}
    acc=warp_max(seed);
  }
  return acc;
}
fn alibi_slope(max_bias:f32,h:i64,n_head_log2:i64,m0:f32,m1:f32)->f32 {
  if max_bias<=(0 as f32) {return (1 as f32);}
  var base=m1; var exph=2*(h-n_head_log2)+1;
  if h<n_head_log2 {base=m0;exph=h+1;}
  return pow32(base,exph as f32);
}

fn soft_max_f32(x:f32[:], mask:const u8[:], mkind:i64, dst:f32[:],
                ncols:i64, ne01:i64, ne02:i64, ne03:i64, ne12:i64, ne13:i64,
                mb11:i64, mb12:i64, mb13:i64,
                scale:f32, max_bias:f32, m0:f32, m1:f32, n_head_log2:i64,
                block_size:i64, scratch:f32[:]) {
  let tid=tid_x();
  let i03=bid_z();
  let i02=bid_y();
  let i01=bid_x();
  let rowx=i01+i02*gdim_x()+i03*gdim_x()*gdim_y();
  let i12=i02%ne12;
  let i13=i03%ne13;
  let xb=rowx*ncols;
  let mbase=i01*mb11+i12*mb12+i13*mb13;
  let slope=alibi_slope(max_bias,i02,n_head_log2,m0,m1);
  var maxv=neg_inf();
  var col0:i64=0;
  while col0<ncols {
    let col=col0+tid;
    if col<ncols {
      var val=x[xb+col]*scale;
      if mask.len>0 {
        var m=(0 as f32);
        if mkind==0 {m=f16_to_f32(read16(mask,(mbase+col)*2));}
        else {m=from_bits(read32(mask,(mbase+col)*4));}
        val=val+slope*m;
      }
      dst[xb+col]=val;
      maxv=fmax32(maxv,val);
    }
    col0+=block_size;
  }
  maxv=block_max(maxv,scratch,block_size);
  var tmp:f32=(0 as f32);
  col0=0;
  while col0<ncols {
    let col=col0+tid;
    if col<ncols {
      let e=exp32(dst[xb+col]-maxv);
      tmp=tmp+e;
      dst[xb+col]=e;
    }
    col0+=block_size;
  }
  if block_size>32 {sync();}
  tmp=block_sum(tmp,scratch,block_size);
  let inv_sum=(1 as f32)/tmp;
  col0=0;
  while col0<ncols {
    let col=col0+tid;
    if col<ncols {dst[xb+col]=dst[xb+col]*inv_sum;}
    col0+=block_size;
  }
}

fn main() {
  var x:f32[4096]=default<f32[4096]>();
  var d:f32[4096]=default<f32[4096]>();
  var m:f32[4096]=default<f32[4096]>();
  var mu:u8[4096*4]=default<u8[4096*4]>();
  var s:f32[32]=default<f32[32]>();
  soft_max_f32(x,mu,0,d,4096,1,1,1,1,1,0,0,0,(1 as f32),(0 as f32),(1 as f32),(1 as f32),1,1024,s);
  soft_max_f32(x,mu,1,d,4096,1,1,1,1,1,0,0,0,(1 as f32),(0 as f32),(1 as f32),(1 as f32),1,1024,s);
  print(d[0]+m[0]);
}
