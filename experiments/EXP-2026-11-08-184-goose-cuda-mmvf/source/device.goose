// Apex CUDA mul_mat_vec_f (Goose), from ggml-cuda/mmvf.cu: mul_mat_vec_f with
// T=float, type_acc=float, ncols_dst=1, has_fusion=false. That is the shape the
// Apex profile executes (the census shows mul_mat_vec_f<float, float, (int)1,
// (int)256, (bool)0, (bool)0>).
//
// Everything algorithmic is Goose: the channel/sample/row address arithmetic
// including the ids path, the float2-strided dot product over a row, the warp tree
// reduction, the cross-warp pass through shared staging, and the store.
// Extern symbols are only the raw float and int32 loads, the store, the shuffle and
// barrier primitives. No original kernel, GGML struct or C++ routine is called.
//
// Excluded here and left to the parent's fallback: half, bfloat16 and quantized
// sources, ncols_dst > 1, any fusion (gate/bias), and is_multi_token_id.
//
// Row layout: src0 row `row` starts at x + sample_x*stride_sample_x +
// channel_x*stride_channel_x + row*stride_row and is read as float2, so the column
// index counts pairs (ncols2 = ncols/2). y is indexed the same way, dst is scalar.
extern "fg184m_ld_f32" fn ld_f32(src:u64,byte:u64)->f32;
extern "fg184m_ld_i32" fn ld_i32(src:u64,byte:u64)->i32;
extern "fg184m_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg184m_shfl_xor" fn shfl_xor(v:f32,off:i32,width:i32)->f32;
// Native writes `acc += v*u`, which nvcc contracts into a single-rounded FMA.
// Doing the multiply and add separately here would differ by 1-2 ULP per term.
extern "fg184m_mad" fn mad(acc:f32,v:f32,u:f32)->f32;
// Fully rounded multiply then add, i.e. contraction explicitly forbidden.
extern "fg184m_nmad" fn nmad(acc:f32,v:f32,u:f32)->f32;
extern "fg184m_shm_put" fn shm_put(slot:i32,v:f32);
extern "fg184m_shm_get" fn shm_get(slot:i32)->f32;
extern "fg184m_barrier" fn barrier();
extern "fg184m_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// Warp tree reduction over 32 lanes. Native uses __shfl_xor_sync with offsets
// width/2 .. 1, which leaves the full sum in EVERY lane. Using down-shuffles here
// would leave it only in lane 0 and break the cross-warp pass.
fn warp_sum(v0:f32)->f32{
 var v:f32=v0;
 v=v+shfl_xor(v,16 as i32,32 as i32);
 v=v+shfl_xor(v,8 as i32,32 as i32);
 v=v+shfl_xor(v,4 as i32,32 as i32);
 v=v+shfl_xor(v,2 as i32,32 as i32);
 v=v+shfl_xor(v,1 as i32,32 as i32);
 return v;
}

// One row of one (channel, sample) pair. `bs` is the real block size, `block_size`
// the compile-time value the parent dispatched on; both are passed so the stride
// inside the column loop matches native (col2 += block_size).
fn fg184m_row(x:u64,y:u64,dst:u64,ids:u64,
 ncols2:i64,stride_row:i64,stride_col_y2:i64,stride_col_dst:i64,
 stride_channel_x:i64,stride_channel_y:i64,stride_channel_dst:i64,
 stride_sample_x:i64,stride_sample_y:i64,stride_sample_dst:i64,
 nchannels_y:i64,channel_ratio:i64,use_ids:bool,ids_stride:i64,
 row:i64,channel_dst:i64,sample_dst:i64,block_size:i64,tid:i64){
 ensure(block_size>0,10);
 ensure(tid>=0&&tid<block_size,11);
 ensure(ncols2>=0,12);
 // Without ids native derives the source channel by dividing by channel_ratio, and
 // wraps the y channel modulo nchannels_y. With ids both come from the id table.
 var channel_x:i64=channel_dst;
 var channel_y:i64=channel_dst;
 var sample_x:i64=sample_dst;
 if !use_ids{
  ensure(channel_ratio>0,13);
  channel_x=channel_dst/channel_ratio;
  channel_y=channel_dst%nchannels_y;
 }
 if use_ids{
  // ids[channel_dst + token_idx*ids_stride]; token_idx is zero on this profile.
  let o:i64=(channel_dst+0 as i64*ids_stride)*(4 as i64);
  channel_x=(ld_i32(ids,(o as u64)) as i64);
  channel_y=channel_dst%nchannels_y;
  sample_x=0;
 }
 // Native advances x and y in float ELEMENT units and only then casts to float2, so
 // the pair-indexed base is that element offset halved.
 let xb:i64=(sample_x*stride_sample_x+channel_x*stride_channel_x+row*stride_row)/2;
 let yb:i64=(sample_dst*stride_sample_y+channel_y*stride_channel_y)/2;
 // columns are float2 pairs, so the byte offset is pair*8
 var sum:f32=0.0;
 var col2:i64=tid;
 while col2<ncols2{
  let xo:i64=(xb+col2)*(8 as i64);
  let yo:i64=(yb+col2)*(8 as i64);
  let xa:f32=ld_f32(x,(xo as u64));
  let xc:f32=ld_f32(x,((xo+(4 as i64)) as u64));
  let ya:f32=ld_f32(y,(yo as u64));
  let yc:f32=ld_f32(y,((yo+(4 as i64)) as u64));
  sum=mad(sum,xa,ya);
  sum=mad(sum,xc,yc);
  col2=col2+block_size;
 }
 sum=warp_sum(sum);
 // Blocks wider than a warp combine per-warp partials in shared staging.
 // Native zeroes buf_iw[tid] for tid < warp_size FIRST, then every thread writes
 // buf_iw[tid/warp_size], then warp 0 re-reduces over warp_size slots. The zero fill
 // matters: with fewer warps than lanes, slots above nwarps stay zero instead of
 // holding whatever the previous launch left in the staging buffer.
 if block_size>32 as i64{
  if tid<32 as i64{shm_put(tid as i32,0.0);}
  barrier();
  shm_put((tid/32) as i32,sum);
  barrier();
  if tid<32 as i64{
   sum=warp_sum(shm_get(tid as i32));
  }
 }
 // ncols_dst == 1, so only thread 0 stores: dst[0*stride_col_dst + row].
 if tid<1 as i64{
  let o:i64=(sample_dst*stride_sample_dst+channel_dst*stride_channel_dst+row)*(4 as i64);
  st_f32(dst,(o as u64),sum);
 }
}

// Retained entry; the parent never executes main.
fn main(){
 let z:u64=0;
 fg184m_row(z,z,z,z,0,0,0,0,0,0,0,0,0,0,0,0,false,0,0,0,0,32 as i64,0);
}
