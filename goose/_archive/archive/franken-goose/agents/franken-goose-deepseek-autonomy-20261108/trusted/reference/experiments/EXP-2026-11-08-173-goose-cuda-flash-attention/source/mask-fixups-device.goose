// fg168f: Apex FlashAttention dense mask scan + both stream-K fixup kernels as Goose,
// ported 1:1 from the pinned job input fattn-common.cuh:
//   flash_attn_mask_to_KV_max<ncols1>                    (input lines 669-724)
//   flash_attn_stream_k_fixup_uniform<256,ncols1,ncols2> (input lines 729-809)
//   flash_attn_stream_k_fixup_general<256,ncols1,ncols2> (input lines 813-920)
// Fixed target: D=DV=256 (FixedDV256), ncols1 in {4,8} runtime, ncols2=8 runtime value,
// CUDA physical warp 32, FATTN_KQ_STRIDE 256, no sparse compact path. Task label
// flash_attn_stream_k_fixup is the supplied flash_attn_stream_k_fixup_general.
// flash_attn_combine_results (non-stream-K) and the mma kernel body are out of scope.
// All block/tile/row/head mapping, meta offsets, loop accumulation, max/exp/rescale,
// normalization and mask reductions live here; only raw byte loads/stores, inline half
// bit decode, fmaxf/expf, shuffles, barriers and PDL are effects. No original numerical
// C++ is called. fast_div_modulo/fastdiv/fastmodulo are replaced by plain i64 division
// with the logical divisor passed explicitly (d0..d3); the native index layout is kept.
// Byte-vs-element: every address is a byte address; element indices are multiplied by
// explicit typesizes (f32/i32 *4, float2 *8, half2 *4). Index products use i64.
// Shared word map (i32 words, 32 words = 128 B): buf_iw[0..31] cross-warp scan flags.
fn ftz_thresh()->f32{return -20.0;}
// Raw effects; widths/units are listed in contract.json.
extern "fg168f_ld_u32" fn ld_u32(p:u64,a:u64)->u32; // raw 4-byte LE load (one aligned half2 word)
extern "fg168f_ld_f32" fn ld_f32(p:u64,a:u64)->f32; // raw 4-byte load, bits as f32
extern "fg168f_st_f32" fn st_f32(p:u64,a:u64,v:f32); // raw 4-byte store
extern "fg168f_st_i32" fn st_i32(p:u64,a:u64,v:i32); // raw 4-byte store (KV_max int)
extern "fg168f_sm_i32" fn sm_i32(p:u64,w:u64)->i32; // shared i32 load, 4-byte word index
extern "fg168f_sm_st_i32" fn sm_st_i32(p:u64,w:u64,v:i32); // shared i32 store, word index
extern "fg168f_shfl_xor_i32" fn shfl_xor_i32(v:i32,o:i32)->i32; // full-mask xor shuffle, width 32
extern "fg168f_barrier" fn barrier(); // __syncthreads
extern "fg168f_sync" fn pdl_sync(); // ggml_cuda_pdl_sync / cudaGridDependencySynchronize
extern "fg168f_lc" fn lc(); // ggml_cuda_pdl_lc / cudaTriggerProgrammaticLaunchCompletion
extern "fg168f_expf" fn expf(x:f32)->f32;
extern "fg168f_fmaxf" fn fmaxf(a:f32,b:f32)->f32;
// warp_reduce_all: exact AND over all 32 lanes via xor shuffles 16,8,4,2,1 (operands 0/1).
fn warp_all(v:i32)->i32{var x=v;x=x&shfl_xor_i32(x,16);x=x&shfl_xor_i32(x,8);x=x&shfl_xor_i32(x,4);x=x&shfl_xor_i32(x,2);x=x&shfl_xor_i32(x,1);return x;}
// int(isinf(__half2float(h))) == ((h & 0x7FFF) == 0x7C00): half->float conversion is exact
// and maps exactly the fp16 all-ones-exponent zero-mantissa encodings to +/-inf; a half
// NaN converts to NaN and isinf is false, matching the bit test for every encoding.
fn half_isinf(h:u32)->i32{if ((h&(32767 as u32))==(31744 as u32)){return 1;} return 0;}
// flash_attn_mask_to_KV_max<ncols1>: block (128,1,1), grid (ntiles_x, Q->ne[3], 1).
// ne30 = K->ne[1]/FATTN_KQ_STRIDE; s31/s33 = mask->nb[1]/nb[3] in half2 elements.
// Walks 256-wide KV tiles from the top: a tile is skippable iff for all ncols1 rows both
// halves of every lane word are inf; per-lane AND, warp AND, shared roundtrip (two
// barriers), warp AND; stops at the first non-skippable tile; tid 0 stores the
// walked-back tile edge at KV_max[sequence*ne31 + jt].
fn mask_to_kv_max(mask_p:u64,kv_max_p:u64,ne30:i32,s31:i64,s33:i64,nc1:i32,bx:i32,by:i32,gx:i32,tx:i32,smem:u64){
 let sequence=by;
 let jt=bx;
 if tx<32{sm_st_i32(smem,(tx as u64),1);}
 pdl_sync();
 barrier();
 var sj:i32=(ne30-1)*256;
 var run:i32=1;
 while run==1{
  if sj<0{run=0;}
  else{
   var all_inf:i32=1;
   let mbase=((sequence as i64)*s33)+((jt as i64)*((nc1 as i64)*s31))+((sj as i64)/2)+(tx as i64);
   var j:i32=0;
   while j<nc1{
    let w=ld_u32(mask_p,((mbase+((j as i64)*s31))*4) as u64);
    let lo=w&(65535 as u32);
    let hi=w>>(16 as u32);
    all_inf=all_inf*half_isinf(lo)*half_isinf(hi);
    j+=1;
   }
   all_inf=warp_all(all_inf);
   if tx%32==0{sm_st_i32(smem,((tx/32) as u64),all_inf);}
   barrier();
   all_inf=sm_i32(smem,((tx%32) as u64));
   barrier();
   all_inf=warp_all(all_inf);
   if all_inf==0{run=0;}
   if run==1{sj-=256;}
  }
 }
 let kvsj=sj+256;
 if tx==0{st_i32(kv_max_p,((((sequence as i64)*(gx as i64))+(jt as i64))*4) as u64,kvsj);}
}
// flash_attn_stream_k_fixup_uniform<256,ncols1,ncols2>: block (256,1,1), grid
// (ntiles_dst, ncols1, ncols2); runs only when blocks_num.x%ntiles_dst==0 and
// blocks_num.x>ntiles_dst; bpt = blocks_num.x/ntiles_dst blocks per output tile.
// dst is the native KQV->data buffer; fix is the dst_tmp_meta float2 pool:
//   [0, nb*ncols)            needs_fixup meta (fix[bidx*ncols+jc])
//   [nb*ncols, 2*nb*ncols)   is_fixup meta (fix[(nb+bidx)*ncols+jc])
//   base + nb*4*ncols floats partial data (fix_data[bidx*ncols*256 + jc*256 + tx])
// lc() first; pdl_sync() before the *dst read, exactly as native.
fn fixup_uniform(dst_p:u64,fix_p:u64,ne01:i32,ne02:i32,ne12:i32,nb:i32,gqa:i32,bpt:i32,d0:i64,d1:i64,d2:i64,nc1:i32,nc2:i32,bx:i32,by:i32,bz:i32,tx:i32){
 lc();
 let ncols=nc1*nc2;
 let j=by;
 let c=bz;
 let jc=j*nc2+c;
 let b_first=bx*bpt;
 let b_last=b_first+bpt-1;
 let fdata=fix_p+((((nb as i64)*4*(ncols as i64))*4) as u64);
 let q0=(bx as i64)/d0;
 let r0=(bx as i64)-q0*d0;
 let q1=r0/d1;
 let r1=r0-q1*d1;
 let q2=r1/d2;
 let r2=r1-q2*d2;
 let sequence=q0;
 let z_kv=q1;
 let zt_gqa=q2;
 let jt=r2;
 let zt_q=(z_kv*(gqa as i64))+(zt_gqa*(nc2 as i64));
 var ex:i32=0;
 if ((jt*(nc1 as i64))+(j as i64))>=(ne01 as i64){ex=1;}
 if ((zt_gqa*(nc2 as i64))+(c as i64))>=(gqa as i64){ex=1;}
 if ex==1{return;}
 let didx=(sequence*(ne02 as i64)*(ne01 as i64)*256)+(jt*(ne02 as i64)*((nc1 as i64)*256))+(zt_q*256)+(((j as i64)*(ne02 as i64)+(c as i64))*256)+(tx as i64);
 let db=(didx*4) as u64;
 pdl_sync();
 var dst_val=ld_f32(dst_p,db);
 let mend=(((b_last as i64)*(ncols as i64))+(jc as i64))*8;
 var max_val=ld_f32(fix_p,(mend as u64));
 var rowsum=ld_f32(fix_p,((mend+4) as u64));
 var bidx:i32=b_last-1;
 while bidx>=b_first{
  let doff=(((bidx as i64)*(ncols as i64)+(jc as i64))*256)+(tx as i64);
  let dst_add=ld_f32(fdata,(doff*4) as u64);
  let mi=(((nb as i64)+(bidx as i64))*(ncols as i64)+(jc as i64))*8;
  let tmpx=ld_f32(fix_p,(mi as u64));
  let tmpy=ld_f32(fix_p,((mi+4) as u64));
  let mv=fmaxf(max_val,tmpx);
  let dv=max_val-mv;
  let da=tmpx-mv;
  let sv=if dv>=ftz_thresh(){expf(dv)}else{0.0 as f32};
  let sa=if da>=ftz_thresh(){expf(da)}else{0.0 as f32};
  dst_val=sv*dst_val+sa*dst_add;
  rowsum=sv*rowsum+sa*tmpy;
  max_val=mv;
  bidx-=1;
 }
 st_f32(dst_p,db,dst_val/rowsum);
}
// flash_attn_stream_k_fixup_general<256,ncols1,ncols2>: block (256,1,1), grid
// (blocks_num.x, ncols1, ncols2); runs only when ntiles_dst%blocks_num.x!=0.
// gx = gridDim.x = blocks_num.x drives the kbc bounds, the fixup-data base and the meta
// region, exactly as native. No lc(); early exits come first; pdl_sync() before *dst.
fn fixup_general(dst_p:u64,fix_p:u64,ne01:i32,ne02:i32,gqa:i32,tw:i32,d0:i64,d1:i64,d2:i64,d3:i64,nc1:i32,nc2:i32,bx:i32,by:i32,bz:i32,gx:i32,tx:i32){
 let ncols=nc1*nc2;
 let j=by;
 let c=bz;
 let jc=j*nc2+c;
 let gx64=gx as i64;
 let fdata=fix_p+(((gx64*4*(ncols as i64))*4) as u64);
 let kbc0=((bx as i64)*(tw as i64))/gx64;
 let kbc0_stop=(((bx+1) as i64)*(tw as i64))/gx64;
 var ex:i32=0;
 if kbc0==kbc0_stop{ex=1;}
 let tk0=kbc0/d3;
 if (kbc0-tk0*d3)==(0 as i64){ex=1;}
 let q0s=kbc0_stop/d3;
 if tk0==q0s{
  if (kbc0_stop-q0s*d3)!=(0 as i64){ex=1;}
 }
 if ex==1{return;}
 let q0=kbc0/d0;
 let r0=kbc0-q0*d0;
 let q1=r0/d1;
 let r1=r0-q1*d1;
 let q2=r1/d2;
 let r2=r1-q2*d2;
 let q3=r2/d3;
 let sequence=q0;
 let z_kv=q1;
 let zt_gqa=q2;
 let jt=q3;
 let zt_q=(z_kv*(gqa as i64))+(zt_gqa*(nc2 as i64));
 var ex2:i32=0;
 if ((jt*(nc1 as i64))+(j as i64))>=(ne01 as i64){ex2=1;}
 if ((zt_gqa*(nc2 as i64))+(c as i64))>=(gqa as i64){ex2=1;}
 if ex2==1{return;}
 let didx=(sequence*(ne02 as i64)*(ne01 as i64)*256)+(jt*(ne02 as i64)*((nc1 as i64)*256))+(zt_q*256)+(((j as i64)*(ne02 as i64)+(c as i64))*256)+(tx as i64);
 let db=(didx*4) as u64;
 var dst_val:f32=0.0;
 var max_val:f32=0.0;
 var rowsum:f32=0.0;
 pdl_sync();
 dst_val=ld_f32(dst_p,db);
 let mend=(((bx as i64)*(ncols as i64))+(jc as i64))*8;
 max_val=ld_f32(fix_p,(mend as u64));
 rowsum=ld_f32(fix_p,((mend+4) as u64));
 var bidx:i32=bx-1;
 var kbc_stop:i64=kbc0;
 var run:i32=1;
 while run==1{
  let kbc=((bidx as i64)*(tw as i64))/gx64;
  if kbc!=kbc_stop{
   let doff=(((bidx as i64)*(ncols as i64)+(jc as i64))*256)+(tx as i64);
   let dst_add=ld_f32(fdata,(doff*4) as u64);
   let mi=((gx64+(bidx as i64))*(ncols as i64)+(jc as i64))*8;
   let tmpx=ld_f32(fix_p,(mi as u64));
   let tmpy=ld_f32(fix_p,((mi+4) as u64));
   let mv=fmaxf(max_val,tmpx);
   let dv=max_val-mv;
   let da=tmpx-mv;
   let sv=if dv>=ftz_thresh(){expf(dv)}else{0.0 as f32};
   let sa=if da>=ftz_thresh(){expf(da)}else{0.0 as f32};
   dst_val=sv*dst_val+sa*dst_add;
   rowsum=sv*rowsum+sa*tmpy;
   max_val=mv;
   let q=kbc/d3;
   if (kbc-q*d3)==(0 as i64){run=0;}
   if q<tk0{run=0;}
  }
  if run==1{bidx-=1;kbc_stop=kbc;}
 }
 st_f32(dst_p,db,dst_val/rowsum);
}
// Retained entries; parent never executes main.
fn main(){
 mask_to_kv_max(0 as u64,0 as u64,0,0 as i64,0 as i64,4,0,0,1,0,0 as u64);
 fixup_uniform(0 as u64,0 as u64,0,0,0,0,0,0,0 as i64,0 as i64,0 as i64,4,8,0,0,0,0);
 fixup_general(0 as u64,0 as u64,0,0,0,0,0 as i64,0 as i64,0 as i64,0 as i64,4,8,0,0,0,1,0);
}
