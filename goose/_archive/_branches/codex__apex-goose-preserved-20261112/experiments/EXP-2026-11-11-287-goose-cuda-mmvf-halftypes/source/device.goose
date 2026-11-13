// Apex CUDA mul_mat_vec_f with half and bfloat16 sources (Goose). Extends
// EXP184, which covered T=float. Same shape: type_acc=float, ncols_dst=1, no
// fusion, no multi-token id.
//
// Everything algorithmic is Goose: the channel/sample/row address arithmetic
// (including the ids path), the pair-strided dot product with the same FMA
// sequence, the warp reduction, the cross-warp pass through shared staging and
// the store. Externs are typed loads, the store, the FMA primitive, the shuffle,
// shared staging and the barrier.
//
// dtype: 0 = float source (4 bytes/element, 8 bytes/pair),
//        1 = half, 2 = bfloat16 (2 bytes/element, 4 bytes/pair).
extern "fg287m_ld_f32" fn ld_f32(src:u64,byte:u64)->f32;
extern "fg287m_ld_half" fn ld_half(src:u64,byte:u64)->f32;
extern "fg287m_ld_bf16" fn ld_bf16(src:u64,byte:u64)->f32;
extern "fg287m_ld_i32" fn ld_i32(src:u64,byte:u64)->i32;
extern "fg287m_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg287m_mad" fn mad(acc:f32,v:f32,u:f32)->f32;
extern "fg287m_shfl_xor" fn shfl_xor(v:f32,off:i32,width:i32)->f32;
extern "fg287m_shm_put" fn shm_put(slot:i32,v:f32);
extern "fg287m_shm_get" fn shm_get(slot:i32)->f32;
extern "fg287m_barrier" fn barrier();
extern "fg287m_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn warp_sum(v0:f32)->f32{
 var v:f32=v0;
 v=v+shfl_xor(v,16 as i32,32 as i32);
 v=v+shfl_xor(v,8 as i32,32 as i32);
 v=v+shfl_xor(v,4 as i32,32 as i32);
 v=v+shfl_xor(v,2 as i32,32 as i32);
 v=v+shfl_xor(v,1 as i32,32 as i32);
 v
}

fn mmvf(x:u64,y:u64,dst:u64,ids:u64,dtype:i64,
 ncols2:i64,stride_row:i64,stride_col_y2:i64,stride_col_dst:i64,
 stride_channel_x:i64,stride_channel_y:i64,stride_channel_dst:i64,
 stride_sample_x:i64,stride_sample_y:i64,stride_sample_dst:i64,
 nchannels_y:i64,channel_ratio:i64,sample_ratio:i64,use_ids:bool,ids_stride:i64,
 row:i64,channel_dst:i64,sample_dst:i64,block_size:i64,tid:i64){
 ensure(block_size>0 as i64,10);
 ensure(tid>=0 as i64&&tid<block_size as i64,11);
 ensure(ncols2>=0 as i64,12);
 ensure(dtype>=0 as i64&&dtype<=2 as i64,15);
 var channel_x:i64=channel_dst;
 var channel_y:i64=channel_dst;
 var sample_x:i64=sample_dst;
 if !use_ids{
  ensure(channel_ratio>0 as i64,13);
  channel_x=channel_dst/channel_ratio;
  channel_y=channel_dst%nchannels_y;
  ensure(sample_ratio>0 as i64,14);
  sample_x=sample_dst/sample_ratio;
 }
 if use_ids{
  let o:i64=(channel_dst+0 as i64*ids_stride)*(4 as i64);
  channel_x=(ld_i32(ids,((o as i64) as u64))) as i64;
  channel_y=channel_dst%nchannels_y;
  sample_x=0 as i64;
 }
 let xb:i64=sample_x*stride_sample_x+channel_x*stride_channel_x+row*stride_row;
 let yb:i64=sample_dst*stride_sample_y+channel_y*stride_channel_y;
 // yb counts floats; the pair walk starts at yb and steps by 8 bytes.
 let ybase:i64=yb*(4 as i64);
 var esz:i64=4 as i64;
 if dtype!=(0 as i64){ esz=2 as i64; }
 var psz:i64=8 as i64;
 if dtype!=(0 as i64){ psz=4 as i64; }
 let xbase:i64=xb*esz;
 var sum:f32=0.0 as f32;
 var col2:i64=tid;
 while col2<ncols2 as i64{
  let xo:i64=xbase+col2*psz;
  let yo:i64=ybase+col2*(8 as i64);
  var xa:f32=0.0 as f32;
  var xc:f32=0.0 as f32;
  if dtype==(0 as i64){
   xa=ld_f32(x,(xo as u64));
   xc=ld_f32(x,((xo+(4 as i64)) as u64));
  }else{
   if dtype==(1 as i64){
    xa=ld_half(x,(xo as u64));
    xc=ld_half(x,((xo+(2 as i64)) as u64));
   }else{
    xa=ld_bf16(x,(xo as u64));
    xc=ld_bf16(x,((xo+(2 as i64)) as u64));
   }
  }
  let ya:f32=ld_f32(y,(yo as u64));
  let yc:f32=ld_f32(y,((yo+(4 as i64)) as u64));
  sum=mad(sum,xa,ya);
  sum=mad(sum,xc,yc);
  col2=col2+block_size;
 }
 sum=warp_sum(sum);
 if block_size>32 as i64{
  if tid<32 as i64{ shm_put(tid as i32,0.0 as f32); }
  barrier();
  shm_put((tid/32) as i32,sum);
  barrier();
  if tid<32 as i64{
   sum=warp_sum(shm_get(tid as i32));
  }
 }
 if tid<1 as i64{
  let o:i64=(sample_dst*stride_sample_dst+channel_dst*stride_channel_dst+row)*(4 as i64);
  st_f32(dst,(o as u64),sum);
 }
}

// Retained entry; the parent never executes main.
fn main(){
 mmvf(0,0,0,0,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,false,0 as i64,0 as i64,0 as i64,0 as i64,32 as i64,0 as i64);
}
