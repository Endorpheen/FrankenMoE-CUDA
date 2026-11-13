// fg175v: Apex FlashAttention decode vector device path as Goose, from fattn-vec.cuh
// flash_attn_ext_vec plus its fattn-common.cuh helpers quantize_q8_1_to_shared<float2,32>,
// vec_dot_fattn_vec_KQ_q4_0<256,32>, dequantize_V_q4_0<float,4> and
// flash_attn_combine_results<256>. Fixed target: D=256, Q=F32, K=V=Q4_0,
// ncols in {1,2} (runtime parameter), WARP_SIZE=32, nthreads=128 (nwarps=4),
// use_logit_softcap=false template instance (softcap variant deferred).
// All math stays here: dp4a Q4_0-vs-Q8_1 dot on raw nibble words, half block scales,
// online softmax, warp/block reductions, KQ/VKQ staging. Only global/shared data
// movement, shuffles, expf/powf/roundf, h2f, dp4a and barriers are effects.
// ggml_cuda_pdl_lc/pdl_sync deferred (parent owns PDL launch attributes).
// No V_DOT2_F32_F16_AVAILABLE (CUDA path): KQ probs and VKQ accumulate in f32.
// Shared word map (f32/u32 words, 1152 words = 4608 B): KQ buffer 0..1024
// (Q q8_1 staging per column j at j*256: 64 i32 words + 16 f32 words of (d,sum) pairs
// at j*256+64; probs KQ[j*128+tid]; combine staging warp slice w at w*256..w*256+255),
// KQ_max_shared[j] at 1024+j*32, KQ_sum_shared[j] at 1088+j*32.
// Byte-vs-element: every address is a byte address; element indices are multiplied
// by explicit typesizes (f32 *4, half *2, i32 *4, block_q4_0 stride 18 = 2 + 16).
// Parent verified the supplied native ggml-common.h block_q4_0 layout and constants.
extern "fg175v_mul_rn" fn mul_rn(a:f32,b:f32)->f32;
extern "fg175v_fma_rn" fn fma_rn(a:f32,b:f32,c:f32)->f32;
extern "fg175v_trace" fn trace(slot:i32,v:f32);
struct V4 { v0:f32, v1:f32, v2:f32, v3:f32 }
extern "fg175v_ld_f32" fn ld_f32(p:u64, a:u64)->f32;
extern "fg175v_st_f32" fn st_f32(p:u64, a:u64, v:f32);
extern "fg175v_ld_u16" fn ld_u16(p:u64, a:u64)->u32;
extern "fg175v_ld_i32" fn ld_i32(p:u64, a:u64)->i32;
extern "fg175v_h2f" fn h2f(w:u32)->f32;
extern "fg175v_dp4a" fn dp4a(a:i32, b:i32, c:i32)->i32;
extern "fg175v_expf" fn expf(x:f32)->f32;
extern "fg175v_powf" fn powf(b:f32, e:i32)->f32;
extern "fg175v_fmaxf" fn fmaxf(a:f32, b:f32)->f32;
extern "fg175v_round_i8" fn round_i8(x:f32)->i32;
extern "fg175v_shfl_xor" fn shfl_xor(v:f32, o:i32)->f32;
extern "fg175v_syncwarp" fn syncwarp();
extern "fg175v_barrier" fn barrier();
extern "fg175v_lc" fn lc();
extern "fg175v_sync" fn pdl_sync();
extern "fg175v_sm_f32" fn sm_f32(p:u64, i:u64)->f32;
extern "fg175v_sm_st_f32" fn sm_st_f32(p:u64, i:u64, v:f32);
extern "fg175v_sm_i32" fn sm_i32(p:u64, i:u64)->i32;
extern "fg175v_sm_st_i32" fn sm_st_i32(p:u64, i:u64, v:i32);
// -FLT_MAX/2 = -(2^127 - 2^103), exactly representable in f32.
fn negmax()->f32{ return -170141173319264429905852091742258332672.0; }
// fabsf exact, including -0.0 -> +0.0.
fn absf(v:f32)->f32{ if v<=0.0{return 0.0-v;} return v; }
// warp_reduce_sum<32>: xor shuffle offsets 16,8,4,2,1.
fn wr_sum(v:f32)->f32{ var x=v; x+=shfl_xor(x,16); x+=shfl_xor(x,8); x+=shfl_xor(x,4); x+=shfl_xor(x,2); x+=shfl_xor(x,1); return x; }
// warp_reduce_max<32>.
fn wr_max(v:f32)->f32{ var x=v; x=fmaxf(x,shfl_xor(x,16)); x=fmaxf(x,shfl_xor(x,8)); x=fmaxf(x,shfl_xor(x,4)); x=fmaxf(x,shfl_xor(x,2)); x=fmaxf(x,shfl_xor(x,1)); return x; }
// get_alibi_slope(common.cuh): powf kept as the source libm call via primitive.
fn alibi_slope(max_bias:f32, h:i32, nh2:i32, m0:f32, m1:f32)->f32{ if max_bias<=0.0{return 1.0;} let base=if h<nh2{m0}else{m1}; let exph=if h<nh2{h+1}else{2*(h-nh2)+1}; return powf(base,exph); }
// ggml_cuda_memcpy_1<4,2> + (v>>4*shift)&0x0F0F0F0F of block_q4_0 qs (16 B at byte 2 of
// the 18-byte block, half d at byte 0). Two aligned-2 halfword loads form the u32 word
// (lo + 65536*hi); per-byte unsigned div/mod extracts nibbles (arithmetic shift + mask
// equals logical shift + mask on these lanes); nibbles repack disjointly by Horner.
fn q4_word(base:u64,byte:i32,sh:i32)->i32{let lo=ld_u16(base,byte as u64);let hi=ld_u16(base,(byte+2) as u64);let word=lo|(hi<<(16 as u32));let nib=(word>>((sh*4) as u32))&(252645135 as u32);return nib as! i32;}

// vec_dot_fattn_vec_KQ_q4_0<D=256,nthreads=32>: k_KQ_0 in {0,32}; ib=k_KQ/QI8_1 (QI8_1=8),
// iqs4=k_KQ%QI4_0 (QI4_0=4 ints -> byte 4*iqs4), shift=k_KQ&(QI8_1/2)=k_KQ&4. dp4a of the raw
// K nibble word against the Q q8_1 word; scale = half d * (sumi*Q_ds.x - (8/QI8_1)*Q_ds.y)
// with 8/QI8_1 == 1 exactly.
fn vec_dot_kq4(kb:u64, tx:i32, u0:i32, u1:i32, d0x:f32, d0y:f32, d1x:f32, d1y:f32)->f32{
 var sum:f32=0.0;
 var h:i32=0;
 while h<2{
  let kk=h*32+tx;
  let b=kb+((((kk/8) as i64)*18) as u64);
  let iq4=kk%4;
  let sh=(kk%8)/4;
  let v=q4_word(b,2+4*iq4,sh);
  let sumi=dp4a(v,if h==0{u0}else{u1},0);
  let d4=h2f(ld_u16(b,0));
  let qx=if h==0{d0x}else{d1x};
  let qy=if h==0{d0y}else{d1y};
  sum+=d4*((sumi as f32)*qx-qy);
  h+=1;
 }
 return sum;
}
// dequantize_V_q4_0<float,4>: ib=i0/QK4_0 (QK4_0=32), iqs=i0%(QK4_0/2) bytes, shift=(i0%QK4_0)/(QK4_0/2).
// Nibbles -8 per lane (__vsub4 + int8 view) times half d, all four elements.
fn dequant_v4(base:u64,i0:i64)->V4{let p=base+(((i0/32)*18) as u64);let qs=(i0%16) as u64;let word=ld_u16(p,2+qs)|(ld_u16(p,4+qs)<<(16 as u32));let nib=(word>>((((i0%32)/16)*4) as u32))&(252645135 as u32);let d=h2f(ld_u16(p,0));return V4{d*((nib&(15 as u32)) as f32-8.0),d*(((nib>>(8 as u32))&(15 as u32)) as f32-8.0),d*(((nib>>(16 as u32))&(15 as u32)) as f32-8.0),d*(((nib>>(24 as u32))&(15 as u32)) as f32-8.0)};}

// quantize_q8_1_to_shared<float2,32>: ni==WARP_SIZE; per-lane 4 scaled values, amax/sum
// xor-reduced over QI8_1/2=4,2,1 (8 lanes = one 32-element q8_1 block); d=amax/127;
// q8=roundf(v/d) packed little-endian by Horner (exact two's complement bytes);
// yq32[threadIdx.x] and yds[threadIdx.x/QI8_1]=(d,sum) at words yds+2*(tx/8),+1.
fn quant_q8(xb:u64, scale:f32, yq:i64, yds:i64, tx:i32, smem:u64){
 let v0=scale*ld_f32(xb,((4*tx)*4) as u64);
 let v1=scale*ld_f32(xb,((4*tx+1)*4) as u64);
 let v2=scale*ld_f32(xb,((4*tx+2)*4) as u64);
 let v3=scale*ld_f32(xb,((4*tx+3)*4) as u64);
 var amax=absf(v0);
 var sum=v0;
 amax=fmaxf(amax,absf(v1)); sum+=v1;
 amax=fmaxf(amax,absf(v2)); sum+=v2;
 amax=fmaxf(amax,absf(v3)); sum+=v3;
 amax=fmaxf(amax,shfl_xor(amax,4)); sum+=shfl_xor(sum,4);
 amax=fmaxf(amax,shfl_xor(amax,2)); sum+=shfl_xor(sum,2);
 amax=fmaxf(amax,shfl_xor(amax,1)); sum+=shfl_xor(sum,1);
 let d=amax/127.0;
 var q32:i32=0;
 if d!=0.0{
  let c0=round_i8(v0/d); let c1=round_i8(v1/d); let c2=round_i8(v2/d); let c3=round_i8(v3/d);
  let b0=(c0 as! u32)&(255 as u32);let b1=(c1 as! u32)&(255 as u32);let b2=(c2 as! u32)&(255 as u32);let b3=(c3 as! u32)&(255 as u32);q32=(b0|(b1<<(8 as u32))|(b2<<(16 as u32))|(b3<<(24 as u32))) as! i32;
 }
 sm_st_i32(smem,((yq+(tx as i64))) as u64,q32);
 if tx%8==0{
  sm_st_f32(smem,((yds+((2*(tx/8)) as i64))) as u64,d);
  sm_st_f32(smem,((yds+((2*(tx/8)) as i64)+1)) as u64,sum);
 }
}
// flash_attn_ext_vec<256,Q4_0,Q4_0,false>: block dim (32,4,1), lane=tx, warp=ty.
// ncols is the runtime cols_per_block (1 or 2); D/4=64 i32 per row; D/QK8_1=8 ds pairs.
fn fattn_vec(
 q_p:u64, k_p:u64, v_p:u64, mask_p:u64, sinks_p:u64, kv_max_p:u64, dst_p:u64, dst_meta_p:u64,
 scale:f32, max_bias:f32, m0:f32, m1:f32, n_head_log2:i32, logit_softcap:f32,
 ne00:i32, ne01_z:i32, ne02:i32, ne03:i32,
 nb01:i64, nb02:i64, nb03:i64,
 ne10:i32, ne11:i32, ne12:i32, ne13:i32,
 nb11:i64, nb12:i64, nb13:i64,
 nb21:i64, nb22:i64, nb23:i64,
 ne31:i32, ne32:i32, ne33:i32,
 nb31:i64, nb32:i64, nb33:i64,
 ncols:i32,
 bx:i32, by:i32, bz:i32, gx:i32, gy:i32, tx:i32, ty:i32, smem:u64){
 lc();
 let ic0=bx*ncols;
 let sequence=bz/ne02;
 let head=bz-sequence*ne02;
 let gqa_ratio=ne02/ne12;
 let qb=q_p+((nb03*(sequence as i64))+(nb02*(head as i64))+(nb01*(ic0 as i64))) as u64;
 let kb0=k_p+((nb13*(sequence as i64))+(nb12*((head/gqa_ratio) as i64))) as u64;
 let vb0=v_p+((nb23*(sequence as i64))+(nb22*((head/gqa_ratio) as i64))) as u64;
 let maskb0=if mask_p!=0{mask_p+((nb33*((sequence%ne33) as i64))+(nb31*(ic0 as i64))) as u64}else{0 as u64};
 let slope=alibi_slope(max_bias,head,n_head_log2,m0,m1);
 let tid=32*ty+tx;
 var kmax:f32[2]=default<f32[2]>();
 var ksum:f32[2]=default<f32[2]>();
 var vkqx:f32[8]=default<f32[8]>();
 var vkqy:f32[8]=default<f32[8]>();
 var kqk:f32[2]=default<f32[2]>();
 var j:i32=0;
 while j<ncols{kmax[j]=negmax();ksum[j]=0.0;j+=1;}
 // Q -> q8_1 in the shared KQ buffer: source j0-loop with break; nwarps=4>=ncols so warp ty
 // owns column ty exactly when ty<ncols.
 pdl_sync();
 let jq=ty;
 if !(4>ncols && jq>=ncols){
  let colw=jq*256;
  if ncols>1 && ic0+jq>=ne01_z{
   var i0:i32=0;
   while i0<64{sm_st_i32(smem,((colw+i0+tx)) as u64,0);i0+=32;}
   if tx<8{sm_st_f32(smem,((colw+64+2*tx)) as u64,0.0);sm_st_f32(smem,((colw+64+2*tx+1)) as u64,0.0);}
  }else{
   let xb=qb+((nb01*(jq as i64))) as u64;
   var i0:i32=0;
   while i0<64{quant_q8(xb+((i0*16) as u64),scale,(colw+i0) as i64,(colw+64+2*(i0/8)) as i64,tx,smem);i0+=32;}
  }
 }
 barrier();
 var qi32:i32[4]=default<i32[4]>();
 var qdx:f32[4]=default<f32[4]>();
 var qdy:f32[4]=default<f32[4]>();
 j=0;
 while j<ncols{
  var i0:i32=0;
  while i0<64{
   let i=i0+tx;
   qi32[j*2+i0/32]=sm_i32(smem,((j*256+i)) as u64);
   let dw=((j*256+64+2*(i/8))) as u64;
   qdx[j*2+i0/32]=sm_f32(smem,dw);
   qdy[j*2+i0/32]=sm_f32(smem,dw+1);
   i0+=32;
  }
  j+=1;
 }
 barrier();
 var kvlim:i32=ne11;
 if kv_max_p!=0{kvlim=ld_i32(kv_max_p,(((sequence*gx+bx)*4)) as u64);}
 var kbase=kb0+((nb11*((by*128) as i64))) as u64;
 var vbase=vb0+((nb21*((by*128) as i64))) as u64;
 var maskb=maskb0+(((by*128)*2) as u64);
 var k0v:i64=(by*128) as i64;
 while k0v<(kvlim as i64){
  var kreg:f32[2]=default<f32[2]>();
  var kmaxn:f32[2]=default<f32[2]>();
  j=0;while j<ncols{kmaxn[j]=kmax[j];j+=1;}
  var iq:i32=0;
  while iq<32{
   let i_kq=ty*32+iq;
   j=0;
   while j<ncols{
    var sum=vec_dot_kq4(kbase+((nb11*(i_kq as i64))) as u64,tx,qi32[j*2],qi32[j*2+1],qdx[j*2],qdy[j*2],qdx[j*2+1],qdy[j*2+1]);
    sum=wr_sum(sum);
    // use_logit_softcap=false instance: softcap/tanhf branch absent (deferred).
    if mask_p!=0 && (ncols==1 || ic0+j<ne01_z){
     sum+=slope*h2f(ld_u16(maskb,(((j*ne11+i_kq)*2)) as u64));
    }
    kmaxn[j]=fmaxf(kmaxn[j],sum+(3.0 as f32)*(0.6931 as f32));
    if tx==iq{kreg[j]=sum;}
    j+=1;
   }
   iq+=1;
  }
  j=0;
  while j<ncols{
   // nthreads_KQ==WARP_SIZE: source KQ_max_new shfl loop is a no-op here.
   let ksc=expf(kmax[j]-kmaxn[j]);
   kmax[j]=kmaxn[j];
   kreg[j]=expf(kreg[j]-kmax[j]);
   ksum[j]=ksum[j]*ksc+kreg[j];
   sm_st_f32(smem,((j*128+tid)) as u64,kreg[j]);
   var kk:i32=0;
   while kk<4{vkqx[j*4+kk]=mul_rn(vkqx[j*4+kk],ksc);vkqy[j*4+kk]=mul_rn(vkqy[j*4+kk],ksc);kk+=1;}
   j+=1;
  }
  syncwarp();
  var k0:i32=0;
  while k0<32{
   let k=ty*32+k0;
   j=0;while j<ncols{kqk[j]=sm_f32(smem,((j*128+k)) as u64);j+=1;}
   var iv:i32=0;
   while iv<128{
    let v4=dequant_v4(vbase+((nb21*(k as i64))) as u64,((2*iv+tx*4)) as i64);
    j=0;
    while j<ncols{
     vkqx[j*4+iv/32]=fma_rn(v4.v0,kqk[j],vkqx[j*4+iv/32]);
     vkqy[j*4+iv/32]=fma_rn(v4.v1,kqk[j],vkqy[j*4+iv/32]);
     vkqx[j*4+iv/32+1]=fma_rn(v4.v2,kqk[j],vkqx[j*4+iv/32+1]);
     vkqy[j*4+iv/32+1]=fma_rn(v4.v3,kqk[j],vkqy[j*4+iv/32+1]);
     j+=1;
    }
    iv+=64;
   }
   k0+=1;
  }
  syncwarp();
  trace(((k0v/128)*10) as i32,kmax[0]);trace((((k0v/128)*10)+1) as i32,ksum[0]);var tr:i32=0;while tr<4{trace((((k0v/128)*10)+2+tr*2) as i32,if tr==0{kreg[0]}else{vkqx[tr]});trace((((k0v/128)*10)+3+tr*2) as i32,vkqy[tr]);tr+=1;}
  k0v+=((gy*128) as i64);
  kbase+=(((gy*128) as i64)*nb11) as u64;
  vbase+=(((gy*128) as i64)*nb21) as u64;
  maskb+=(((gy*128)*2) as u64);
 }
 if sinks_p!=0 && by==0{
  let sink=ld_f32(sinks_p,((head*4)) as u64);
  let js=ty;
  if !(4>ncols && js>=ncols){
   let kmaxn_j=fmaxf(sink,kmax[js]);
   let ksc=expf(kmax[js]-kmaxn_j);
   kmax[js]=kmaxn_j;
   ksum[js]=ksum[js]*ksc+if tx==0{expf(sink-kmax[js])}else{0.0};
   var kk:i32=0;
   while kk<4{vkqx[js*4+kk]=mul_rn(vkqx[js*4+kk],ksc);vkqy[js*4+kk]=mul_rn(vkqy[js*4+kk],ksc);kk+=1;}
  }
 }
 j=0;
 while j<ncols{
  if ty==0{sm_st_f32(smem,((1024+j*32+tx)) as u64,negmax());sm_st_f32(smem,((1088+j*32+tx)) as u64,0.0);}
  j+=1;
 }
 barrier();
 j=0;
 while j<ncols{
  if tx==0{sm_st_f32(smem,((1024+j*32+ty)) as u64,kmax[j]);}
  j+=1;
 }
 barrier();
 // Effective columns after the source break (ncols>1 && ic0+j>=ne01.z); ic0<ne01_z holds.
 var jc=ncols;
 if ncols>1 && ic0+ncols>ne01_z{jc=ne01_z-ic0;}
 j=0;
 while j<jc{
  var kmaxn2=sm_f32(smem,((1024+j*32+tx)) as u64);
  kmaxn2=wr_max(kmaxn2);
  let ksc2=expf(kmax[j]-kmaxn2);
  kmax[j]=kmaxn2;
  var kk:i32=0;
  while kk<4{vkqx[j*4+kk]=mul_rn(vkqx[j*4+kk],ksc2);vkqy[j*4+kk]=mul_rn(vkqy[j*4+kk],ksc2);kk+=1;}
  var iv:i32=0;
  while iv<128{
   let ivk=iv+tx*2;
   sm_st_f32(smem,((ty*256+2*ivk)) as u64,vkqx[j*4+iv/32]);
   sm_st_f32(smem,((ty*256+2*ivk+1)) as u64,vkqy[j*4+iv/32]);
   sm_st_f32(smem,((ty*256+2*ivk+2)) as u64,vkqx[j*4+iv/32+1]);
   sm_st_f32(smem,((ty*256+2*ivk+3)) as u64,vkqy[j*4+iv/32+1]);
   iv+=64;
  }
  ksum[j]*=ksc2;
  ksum[j]=wr_sum(ksum[j]);
  if tx==0{sm_st_f32(smem,((1088+j*32+ty)) as u64,ksum[j]);}
  barrier();
  ksum[j]=sm_f32(smem,((1088+j*32+tx)) as u64);
  ksum[j]=wr_sum(ksum[j]);
  var i0:i32=0;
  while i0<256{
   var dv:f32=0.0;
   var w:i32=0;
   while w<4{dv+=sm_f32(smem,((w*256+i0+tid)) as u64);w+=1;}
   if gy==1{dv/=ksum[j];}
   let didx=((((((sequence as i64)*(ne01_z as i64))+((ic0+j) as i64))*(ne02 as i64))+((head as i64)))*(gy as i64))+((by as i64));
   st_f32(dst_p,(((didx*256)+((i0+tid) as i64))*4) as u64,dv);
   i0+=128;
  }
  if j<jc-1{barrier();}
  j+=1;
 }
 if gy!=1 && tid<ncols && (ncols==1 || ic0+tid<ne01_z){
  let midx=(((((sequence as i64)*(ne01_z as i64))+((ic0+tid) as i64))*(ne02 as i64))+((head as i64)))*(gy as i64)+((by as i64));
  st_f32(dst_meta_p,(midx*8) as u64,kmax[tid]);
  st_f32(dst_meta_p,((midx*8)+4) as u64,ksum[tid]);
 }
}
// flash_attn_combine_results<256>: block (256,1,1), grid (Q->ne[1],Q->ne[2],Q->ne[3]),
// dynamic shared = parallel_blocks float2 (pb*8 bytes at smem). Parent launches it when
// the vec kernel ran with gridDim.y>1 (dst/dst_meta partition outputs).
fn combine(parts_p:u64, meta_p:u64, dst_p:u64, pb:i32, bx:i32, by:i32, bz:i32, gx:i32, gy:i32, tx:i32, smem:u64){
 let j_dst=(bz*gx+bx)*gy+by;
 let pb64=pb as i64;
 let parts=parts_p+((((j_dst as i64)*pb64)*1024)) as u64;
 let meta=meta_p+((((j_dst as i64)*pb64)*8)) as u64;
 let dstb=dst_p+((((j_dst as i64))*1024)) as u64;
 var i:i32=tx;
 while i<2*pb{
  sm_st_f32(smem,(i as u64),ld_f32(meta,((i*4)) as u64));
  i+=256;
 }
 barrier();
 var kqmax=sm_f32(smem,0 as u64);
 var l:i32=1;
 while l<pb{kqmax=fmaxf(kqmax,sm_f32(smem,((2*l)) as u64));l+=1;}
 var num:f32=0.0;
 var den:f32=0.0;
 l=0;
 while l<pb{
  let sc=expf(sm_f32(smem,((2*l)) as u64)-kqmax);
  num+=sc*ld_f32(parts,(((l*256+tx)*4)) as u64);
  den+=sc*sm_f32(smem,((2*l+1)) as u64);
  l+=1;
 }
 st_f32(dstb,((tx*4)) as u64,num/den);
}
// Retained entries; parent never executes main.
fn main(){
 fattn_vec(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0.0,0.0,0.0,0.0,0,0.0,0,0,0,0,0 as i64,0 as i64,0 as i64,0,0,0,0,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0,0,0,0 as i64,0 as i64,0 as i64,1,0,0,0,1,1,0,0,0 as u64);
 combine(0 as u64,0 as u64,0 as u64,1,0,0,0,1,1,0,0 as u64);
}
