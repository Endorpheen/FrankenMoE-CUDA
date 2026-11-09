// Apex CUDA mul_mat_vec_q for Q6_K weights (Goose), from ggml-cuda/mmvq.cu and
// ggml-cuda/vecdotq.cuh: vec_dot_q6_K_q8_1 and vec_dot_q6_K_q8_1_impl_mmvq.
//
// This is the second half of the dominant per-token matvec work: the Apex census
// shows mul_mat_vec_q<(ggml_type)14> at roughly the same weight as Q4_K, and Q6_K is
// the output-projection weight type of the model.
//
// Everything algorithmic is Goose: the iqs decomposition into bq8_offset,
// scale_offset and vh_shift, the two get_int_b2 reads of ql and qh with the vh shift,
// the per-i q8 word loads, the (vil | vih) - 32 trick expressed as two dp4a passes
// with the 0xE0E0E0E0 bias, the int8 scale gather, and the super-block scale.
// Extern symbols are only raw integer loads. No original kernel or C++ routine is
// called.
//
// Constants for Q6_K: qk=256, qr=2, qi=16, bs=210, vdr=2, QK8_1=32, QI8_1=8,
// QI6_K=16 (so QI6_K/2=8, QI6_K/4=4, QI6_K/8=2).
// Layout of block_q6_K (210 bytes): uint8 ql[128] at 0, uint8 qh[64] at 128,
// int8 scales[16] at 192, ggml_half d at 208.
//
// get_int_b2(x, i) reads a 32-bit word at byte offset 4*i, and get_int_b4 is the same
// read, so both are expressed as a 32-bit load at 4*i.
extern "fg186q_ld_i32" fn ld_i32(src:u64,byte:u64)->i32;
// get_int_b2 is two 16-bit reads; block_q6_K is 210 bytes so words are only
// 2-byte aligned and a 4-byte load would fault.
extern "fg186q_ld_i16" fn ld_i16(src:u64,byte:u64)->u32;
extern "fg186q_ld_i8" fn ld_i8(src:u64,byte:u64)->i32;
extern "fg186q_ld_half" fn ld_half(src:u64,byte:u64)->f32;
extern "fg186q_dp4a" fn dp4a(a:i32,b:i32,c:i32)->i32;
extern "fg186q_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg186q_shfl_xor" fn shfl_xor(v:f32,off:i32,width:i32)->f32;
extern "fg186q_shm_put" fn shm_put(w:i32,lane:i32,v:f32);
extern "fg186q_shm_get" fn shm_get(w:i32,lane:i32)->f32;
extern "fg186q_barrier" fn barrier();
extern "fg186q_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// get_int_b2(x, i): low 16 bits from byte 4*i, high 16 bits from byte 4*i+2. Both are
// 2-byte aligned inside a 210-byte block.
fn ld_b2(src:u64,byte:i64)->i32{
 let lo:u32=ld_i16(src,(byte as u64));
 let hi:u32=ld_i16(src,((byte+(2 as i64)) as u64));
 return (((hi<<16) as i32)|((lo&65535 as u32) as i32));
}

fn warp_sum(v0:f32)->f32{
 var v:f32=v0;
 v=v+shfl_xor(v,16 as i32,32 as i32);
 v=v+shfl_xor(v,8 as i32,32 as i32);
 v=v+shfl_xor(v,4 as i32,32 as i32);
 v=v+shfl_xor(v,2 as i32,32 as i32);
 v=v+shfl_xor(v,1 as i32,32 as i32);
 return v;
}

// vec_dot_q6_K_q8_1_impl_mmvq: for each of the two qr groups accumulate
//   dp4a(vil|vih, u) and dp4a(0xE0E0E0E0, u), which together subtract 32 per byte,
// then scale by the int8 block scale and the q8 delta.
fn impl_mmvq(vl0:i32,vl1:i32,vh0:i32,vh1:i32,u0:i32,u1:i32,u2:i32,u3:i32,
 sc0:i32,sc1:i32,d:f32,d8_0:f32,d8_1:f32)->f32{
 var sum:f32=0.0;
 // i = 0: nibbles 0..3, q8 words u0 and u1
 let l0:i32=(vl0>>0)&(0x0F0F0F0F as i32);
 let h0:i32=((vh0>>0)<<4)&(0x30303030 as i32);
 var s0:i32=dp4a(l0|h0,u0,0 as i32);
 s0=dp4a(0xE0E0E0E0 as i32,u0,s0);
 let l1:i32=(vl1>>0)&(0x0F0F0F0F as i32);
 let h1:i32=((vh1>>0)<<4)&(0x30303030 as i32);
 s0=dp4a(l1|h1,u1,s0);
 s0=dp4a(0xE0E0E0E0 as i32,u1,s0);
 sum=sum+d8_0*((s0*sc0) as f32);
 // i = 1: nibbles 4..7, q8 words u2 and u3
 let l2:i32=(vl0>>4)&(0x0F0F0F0F as i32);
 let h2:i32=((vh0>>4)<<4)&(0x30303030 as i32);
 var s1:i32=dp4a(l2|h2,u2,0 as i32);
 s1=dp4a(0xE0E0E0E0 as i32,u2,s1);
 let l3:i32=(vl1>>4)&(0x0F0F0F0F as i32);
 let h3:i32=((vh1>>4)<<4)&(0x30303030 as i32);
 s1=dp4a(l3|h3,u3,s1);
 s1=dp4a(0xE0E0E0E0 as i32,u3,s1);
 sum=sum+d8_1*((s1*sc1) as f32);
 return d*sum;
}

// One thread's contribution for one absolute block index kbx with intra-block index iqs.
fn dot_row(vx:u64,vy:u64,ybase:i64,kbx_offset:i64,kbx:i64,kby:i64,iqs:i64)->f32{
 let xb:i64=(kbx_offset+kbx)*(210 as i64);
 // Index math is kept in 32-bit: native uses `int` here, and 64-bit divide/modulo by
 // constants does not fold to shifts the same way.
 let iq:i32=iqs as i32;
 let h:i32=iq/16 as i32;
 let r:i32=iq%16 as i32;
 let bq8:i64=(2*2*h+(r/8 as i32)) as i64;
 let scale_off:i64=(8*h+(r/4 as i32)) as i64;
 let vh_shift:i32=2*(r/8 as i32);
 // vl[j] = get_int_b2(ql, iqs+j) -> 32-bit word at byte 4*(iqs+j)
 let vl0:i32=ld_b2(vx,xb+4*(iqs+0 as i64));
 let vl1:i32=ld_b2(vx,xb+4*(iqs+1 as i64));
 // vh[j] = get_int_b2(qh, 4*h + iqs%4 + j) >> vh_shift, qh at xb+128
 let qhb:i64=xb+(128 as i64);
 let vh0:i32=ld_b2(vx,qhb+4*((8 as i64)*h+(iqs%8 as i64)+0))>>(vh_shift);
 let vh1:i32=ld_b2(vx,qhb+4*((8 as i64)*h+(iqs%8 as i64)+1))>>(vh_shift);
 // int8 scales[16] at xb+192; the two used entries are scale_off and scale_off+4
 let scb:i64=xb+(192 as i64)+scale_off;
 // scales is int8_t[16], so the byte must be sign-extended before the multiply.
 let sc0:i32=(ld_i8(vx,(scb as u64)) as i32);
 let sc1:i32=(ld_i8(vx,((scb+(4 as i64)) as u64)) as i32);
 // u words: q8 block bq8+2*i, word offset (iqs%QI8_1) + j with QI8_1 = 8
 let uoff:i64=iqs%8 as i64;
 let yb0:i64=(ybase+kby+bq8)*(36 as i64);
 let yb1:i64=(ybase+kby+bq8+(2 as i64))*(36 as i64);
 let d8_0:f32=ld_half(vy,(yb0 as u64));
 let d8_1:f32=ld_half(vy,(yb1 as u64));
 let u0:i32=ld_i32(vy,((yb0+(4 as i64)+4*uoff) as u64));
 let u1:i32=ld_i32(vy,((yb0+(4 as i64)+4*(uoff+1 as i64)) as u64));
 let u2:i32=ld_i32(vy,((yb1+(4 as i64)+4*uoff) as u64));
 let u3:i32=ld_i32(vy,((yb1+(4 as i64)+4*(uoff+1 as i64)) as u64));
 let d:f32=ld_half(vx,((xb+(208 as i64)) as u64));
 return impl_mmvq(vl0,vl1,vh0,vh1,u0,u1,u2,u3,sc0,sc1,d,d8_0,d8_1);
}

// Same block shape as the Q4_K module: blockDim = (32, nwarps), nwarps = 4,
// rows_per_block = 1, qi = 16 so kbx starts at tid/16 and iqs = 2*(tid%16).
fn fg186q_body(vx:u64,vy:u64,ids:u64,dst:u64,
 ncols_x:i64,blocks_per_row:i64,blocks_per_iter:i64,
 stride_row_x:i64,stride_col_y:i64,stride_col_dst:i64,
 stride_channel_x:i64,stride_channel_y:i64,stride_channel_dst:i64,
 stride_sample_x:i64,stride_sample_y:i64,stride_sample_dst:i64,
 nchannels_y:i64,channel_ratio:i64,use_ids:bool,ids_stride:i64,
 nwarps:i64,row:i64,tx:i64,ty:i64,by:i64,bz:i64){
 ensure(nwarps==4 as i64,10);
 ensure(tx>=0&&tx<32 as i64,11);
 ensure(blocks_per_iter>0&&blocks_per_row>0,12);
 let tid:i64=32 as i64*ty+tx;
 var channel_x:i64=by;
 var channel_y:i64=by;
 let sample_dst:i64=bz;
 if use_ids{
  let o:i64=(by+0 as i64*ids_stride)*(4 as i64);
  channel_x=(ld_i32(ids,(o as u64)) as i64);
  channel_y=by%nchannels_y;
 }else{
  ensure(channel_ratio>0,13);
  channel_x=by/channel_ratio;
  channel_y=by;
 }
 let sample_x:i64=sample_dst;
 let sample_y:i64=sample_dst;
 let kbx_offset:i64=sample_x*stride_sample_x+channel_x*stride_channel_x;
 let ybase:i64=sample_y*stride_sample_y+channel_y*stride_channel_y;
 var sum:f32=0.0;
 var kbx:i64=tid/16 as i64;
 while kbx<blocks_per_row{
  let kby:i64=kbx*(256 as i64/32 as i64);
  let iqs:i64=2*((tid%16 as i64));
  sum=sum+dot_row(vx,vy,ybase,kbx_offset+row*stride_row_x,kbx,kby,iqs);
  kbx=kbx+blocks_per_iter;
 }
 if ty>0 as i64{
  shm_put((ty-1) as i32,tx as i32,sum);
 }
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

fn main(){
 fg186q_body(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,false,0,4,0,0,0,0,0);
}
