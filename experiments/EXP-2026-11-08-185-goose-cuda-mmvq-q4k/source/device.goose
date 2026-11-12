// Apex CUDA mul_mat_vec_q for Q4_K weights (Goose), from ggml-cuda/mmvq.cu and
// ggml-cuda/vecdotq.cuh: mul_mat_vec_q<GGML_TYPE_Q4_K, ncols_dst=1, no fusion>
// together with vec_dot_q4_K_q8_1 and vec_dot_q4_K_q8_1_impl_vmmq.
//
// This is the largest kernel family in the Apex census and Q4_K is the dominant
// weight type of the model, so it sits directly on the per-token path.
//
// Everything algorithmic is Goose: the warp/tid decomposition, the block walk with
// vdr/qi striding, the bq8_offset and q4 pointer derivation, the 6-bit scale/min
// unpack of the interleaved `scales` array (including the branchless hi-mask form),
// the nibble-shifted dp4a dot and the q8 sum, the per-i accumulation, the cross-warp
// staging and the super-block scale/min combination. Extern symbols are only the raw
// loads, the half2 unpack, dp4a and the shared staging primitives. No original
// kernel or C++ routine is called.
//
// Constants for Q4_K: qk=256, qr=2, qi=32, bs=144, vdr=2, QK8_1=32, QI8_1=8.
// On the SM86+ table for ncols_dst=1: nwarps=4, rows_per_block=1, so
// blocks_per_iter = vdr*nwarps*warp_size/qi = 2*4*32/32 = 8.
//
// Layout: block_q4_K is 144 bytes (half d at 0, half dmin at 2, uint8 scales[12] at
// 4, uint8 qs[128] at 16); block_q8_1 is 36 bytes (half d at 0, half s at 2,
// int8 qs[32] at 4).
extern "fg185q_ld_i32" fn ld_i32(src:u64,byte:u64)->i32;
extern "fg185q_ld_u16" fn ld_u16(src:u64,byte:u64)->u32;
extern "fg185q_ld_h2" fn ld_h2(src:u64,byte:u64)->f32;
extern "fg185q_ld_h2b" fn ld_h2b(src:u64,byte:u64)->f32;
extern "fg185q_dp4a" fn dp4a(a:i32,b:i32,c:i32)->i32;
extern "fg185q_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg185q_shfl_xor" fn shfl_xor(v:f32,off:i32,width:i32)->f32;
extern "fg185q_shm_put" fn shm_put(w:i32,lane:i32,v:f32);
extern "fg185q_shm_get" fn shm_get(w:i32,lane:i32)->f32;
extern "fg185q_barrier" fn barrier();
extern "fg185q_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// Warp tree reduction; native uses __shfl_xor_sync with offsets width/2..1, which
// leaves the full sum in every lane (needed by the cross-warp staging below).
fn warp_sum(v0:f32)->f32{
 var v:f32=v0;
 v=v+shfl_xor(v,16 as i32,32 as i32);
 v=v+shfl_xor(v,8 as i32,32 as i32);
 v=v+shfl_xor(v,4 as i32,32 as i32);
 v=v+shfl_xor(v,2 as i32,32 as i32);
 v=v+shfl_xor(v,1 as i32,32 as i32);
 return v;
}

// vec_dot_q4_K_q8_1_impl_vmmq: four 4-bit nibble lanes dotted against q8 in one
// dp4a, plus the q8 sum for the constant min term, then the super-block scale/min.
fn impl_vmmq(v0:i32,v1:i32,u0:i32,u1:i32,u2:i32,u3:i32,
 sc0:i32,sc1:i32,m0:i32,m1:i32,dmx:f32,dmy:f32,d8_0:f32,d8_1:f32)->f32{
 var sd:f32=0.0;
 var sm:f32=0.0;
 // i = 0: nibbles 0..3, q8 words (u0, u1)
 let a0:i32=(v0>>0)&(0x0F0F0F0F as i32);
 let b0:i32=(v1>>0)&(0x0F0F0F0F as i32);
 let d1:i32=dp4a(b0,u1,dp4a(a0,u0,0 as i32));
 let s1:i32=dp4a(0x01010101 as i32,u1,dp4a(0x01010101 as i32,u0,0 as i32));
 sd=sd+d8_0*((d1*sc0) as f32);
 sm=sm+d8_0*((s1*m0) as f32);
 // i = 1: nibbles 4..7, q8 words (u2, u3)
 let a1:i32=(v0>>4)&(0x0F0F0F0F as i32);
 let b1:i32=(v1>>4)&(0x0F0F0F0F as i32);
 let d2:i32=dp4a(b1,u3,dp4a(a1,u2,0 as i32));
 let s2:i32=dp4a(0x01010101 as i32,u3,dp4a(0x01010101 as i32,u2,0 as i32));
 sd=sd+d8_1*((d2*sc1) as f32);
 sm=sm+d8_1*((s2*m1) as f32);
 return dmx*sd-dmy*sm;
}

// One thread's contribution for one row block: kbx is the absolute block index in the
// row and kqs the intra-block quad index.
fn dot_row(vx:u64,vy:u64,ybase:i64,kbx_offset:i64,kbx:i64,kby:i64,kqs:i64)->f32{
 let xb:i64=(kbx_offset+kbx)*(144 as i64);
 // bq8_offset = QR4_K*((iqs/2)/(QI8_1/2)) = 2*((iqs/2)/4)
 let bq8:i64=2*((kqs/2)/4);
 // q4 = qs + 16*bq8_offset + 4*((iqs/2)%4), read as q4[0] and q4[4]
 let qoff:i64=xb+(16 as i64)+16*bq8+4*((kqs/2)%4);
 let v0:i32=ld_i32(vx,(qoff as u64));
 let v1:i32=ld_i32(vx,((qoff+(16 as i64)) as u64));
 // scales is uint8[12] at xb+4; native reads u16 words at jm, jm+2, jm+4
 let j:i64=bq8/2;
 let jm:i64=j&1;
 let sbase:i64=xb+(4 as i64);
 let s0:u32=ld_u16(vx,((sbase+2*jm) as u64));
 let s2:u32=ld_u16(vx,((sbase+2*(jm+2)) as u64));
 let s4:u32=ld_u16(vx,((sbase+2*(jm+4)) as u64));
 // branchless hi mask: hi = -(j >= 2)
 var hi:u32=0 as u32;
 if j>=2 as i64{hi=4294967295 as u32;}
 let a0:u32=((s0&(0x3f3f as u32))&(~hi))|((((s4&(0x0f0f as u32))|((s0&(0xc0c0 as u32))>>2))&hi));
 let a1:u32=((s2&(0x3f3f as u32))&(~hi))|((((s4>>4)&(0x0f0f as u32))|((s2&(0xc0c0 as u32))>>2))&hi);
 // In native `sc` and `m` alias the two u16 words of aux at different byte offsets:
 //   sc = (uint8_t*)aux      -> bytes 0,1 = the two low bytes of aux[0]
 //   m  = sc + 2             -> bytes 2,3 = the two low bytes of aux[1]
 // So the two scales come from aux[0] and the two mins from aux[1]; taking the low
 // byte of each word as a scale and the high byte as a min is wrong.
 let sc0:i32=(a0&(255 as u32)) as i32;
 let sc1:i32=((a0>>8)&(255 as u32)) as i32;
 let m0:i32=(a1&(255 as u32)) as i32;
 let m1:i32=((a1>>8)&(255 as u32)) as i32;
 // q8 blocks bq8i = bq8_1 + bq8_offset + i, i in 0..1
 // ybase is a BLOCK index (native advances the block_q8_1 pointer), so it must be
 // scaled to bytes along with kby and bq8. Adding it raw read block 64's data as
 // byte offset 64 of block 0.
 let yb:i64=(ybase+kby+bq8)*(36 as i64);
 let d8_0:f32=ld_h2(vy,(yb as u64));
 let d8_1:f32=ld_h2(vy,((yb+(36 as i64)) as u64));
 // u words: q8.qs + 4*((iqs/2)%4), read at +0 and +16 for both q8 blocks
 let ubase:i64=yb+(4 as i64)+4*((kqs/2)%4);
 let u0:i32=ld_i32(vy,(ubase as u64));
 let u1:i32=ld_i32(vy,((ubase+(16 as i64)) as u64));
 let ub1:i64=ubase+(36 as i64);
 let u2:i32=ld_i32(vy,(ub1 as u64));
 let u3:i32=ld_i32(vy,((ub1+(16 as i64)) as u64));
 let dmx:f32=ld_h2(vx,(xb as u64));
 // ld_h2b takes the HIGH half of the half2 at the given offset, so the base is the
 // block start -- passing xb+2 would read the element at xb+4.
 let dmy:f32=ld_h2b(vx,(xb as u64));
 return impl_vmmq(v0,v1,u0,u1,u2,u3,sc0,sc1,m0,m1,dmx,dmy,d8_0,d8_1);
}

// The block as native launches it: blockDim = (32, nwarps), tid = 32*threadIdx.y +
// threadIdx.x. For Q4_K with ncols_dst=1 on this table nwarps = 4 and
// rows_per_block = 1, so row = blockIdx.x and only lane 0 of warp 0 stores.
fn fg185q_body(vx:u64,vy:u64,ids:u64,dst:u64,
 ncols_x:i64,blocks_per_row:i64,blocks_per_iter:i64,
 stride_row_x:i64,stride_col_y:i64,stride_col_dst:i64,
 stride_channel_x:i64,stride_channel_y:i64,stride_channel_dst:i64,
 stride_sample_x:i64,stride_sample_y:i64,stride_sample_dst:i64,
 nchannels_y:i64,channel_ratio:i64,sample_ratio:i64,use_ids:bool,ids_stride:i64,
 nwarps:i64,row:i64,tx:i64,ty:i64,by:i64,bz:i64){
 ensure(nwarps==4 as i64,10);
 ensure(tx>=0&&tx<32 as i64,11);
 ensure(blocks_per_iter>0&&blocks_per_row>0,12);
 let tid:i64=32 as i64*ty+tx;
 var channel_x:i64=by;
 var channel_y:i64=by;
 let sample_dst:i64=bz;
 if use_ids{
  // ids by + token*ids_stride; token index is zero on this profile
  let o:i64=(by+0 as i64*ids_stride)*(4 as i64);
  channel_x=(ld_i32(ids,(o as u64)) as i64);
  channel_y=by%nchannels_y;
 }else{
  // Native takes the y channel straight from blockIdx.y when there are no ids, and
  // only applies the modulo in the ids branch.
  ensure(channel_ratio>0,13);
  channel_x=by/channel_ratio;
  channel_y=by;
 }
 // Native divides the destination sample by nsamples_dst/nsamples_x (mmvf.cu:43), which
 // broadcasts a source that has fewer samples; using sample_dst directly reads past it.
 var sample_x:i64=sample_dst;
 if !use_ids{
  ensure(sample_ratio>0,14);
  sample_x=sample_dst/sample_ratio;
 }
 let sample_y:i64=sample_dst;
 let kbx_offset:i64=sample_x*stride_sample_x+channel_x*stride_channel_x;
 let ybase:i64=sample_y*stride_sample_y+channel_y*stride_channel_y;
 var sum:f32=0.0;
 // kbx starts at tid/(qi/vdr) = tid/16 and advances by blocks_per_iter
 var kbx:i64=tid/16 as i64;
 while kbx<blocks_per_row{
  let kby:i64=kbx*(256 as i64/32 as i64);
  let kqs:i64=2*((tid%16 as i64));
  sum=sum+dot_row_row(vx,vy,ybase,kbx_offset,row,kbx,kby,kqs,stride_row_x);
  kbx=kbx+blocks_per_iter;
 }
 // Native order matters: warps with ty>0 publish their RAW partial sum into slot
 // ty-1 and return, and only warp 0 adds those partials and then reduces. Reducing
 // before publishing would publish the warp total four times over.
 if ty>0 as i64{
  shm_put((ty-1) as i32,tx as i32,sum);
 }
 // Native places __syncthreads() after the publish and outside the ty>0 branch, so
 // every thread of the block reaches it.
 barrier();
 if ty>0 as i64{
  sum=sum;
 }else{
  var l:i64=0;
  while l<nwarps-1{
   sum=sum+shm_get(l as i32,tx as i32);
   l=l+1;
  }
  sum=warp_sum(sum);
  if tx==0 as i64{
   let o:i64=(sample_dst*stride_sample_dst+by*stride_channel_dst+row)*(4 as i64);
   st_f32(dst,(o as u64),sum);
  }
 }
}

// rows_per_block is 1 here, so the row offset folds into kbx_offset.
fn dot_row_row(vx:u64,vy:u64,ybase:i64,kbx_offset:i64,row:i64,kbx:i64,kby:i64,kqs:i64,stride_row_x:i64)->f32{
 return dot_row(vx,vy,ybase,kbx_offset+row*stride_row_x,kbx,kby,kqs);
}

fn main(){
 fg185q_body(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,false,0,4,0,0,0,0,0);
}
