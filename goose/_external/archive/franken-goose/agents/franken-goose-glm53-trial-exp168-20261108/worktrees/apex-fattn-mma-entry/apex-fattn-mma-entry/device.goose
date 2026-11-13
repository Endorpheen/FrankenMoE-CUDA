// fg175m_fattn: Goose port of the outer __global__ flash_attn_ext_f16 kernel control
// (fattn-mma-f16.cuh L1796-2003), fixed Apex Ada instantiation ONLY: DKQ=DV=256,
// ncols2=8, ncols1 in {4,8} (ncols=32|64), nthreads=128 (nwarps=4), warp_size=32,
// nbatch_fa=32, Q_in_reg=true, nstages=2, swizzled, use_logit_softcap=false,
// V_is_K_view=false, use_sparse=false, stream_k=true (fixed at call site L2105).
// Every other configuration/architecture is DEFERRED: no fallback, no config table.
// The entry owns exactly the native outer control: PDL sync, stream-K kbc work
// partition, sequence/z_KV/zt_gqa/jt decode, per-tile pointer bases and strides,
// the dense KV_max kb0_stop clamp, and the exact needs_fixup/is_fixup boundary flags.
// All numerical process_tile work is delegated through ONE extern adapter to the
// actual Goose process module (inputs/process-draft.goose, symbol fg175m_process_tile,
// signature matched verbatim); no original C++ math callback exists or is called.
// Native fastdiv/fastmodulo are replaced by explicit / and % on the same exact
// divisors (allowed); i32/i64 operand widths, wrap and truncation of the native
// source are preserved. Guards, limits and __builtin_assume conditions are caller
// requirements (documented in contract.json), not re-validated here, mirroring the
// native template instantiation. No global architecture guarantee is assumed.
extern "fg175p_pdl_sync" fn pdl_sync();            // ggml_cuda_pdl_sync() L1820: griddepcontrol.wait; this kernel has no lc()
extern "fg175p_ldu32" fn ldu32(p:u64,i:u64)->u32;  // shared primitive family: *(u32*)(p+4i); used only for dense KV_max int32 reads
// Extern adapter to the actual Goose process_tile port (process-draft.goose L79, verbatim signature).
extern "fg175m_process_tile" fn process_tile(q_f2:u64,k_h2:u64,v_h2:u64,mask_h:u64,sinks_f:u64,dstk:u64,dstk_fixup:u64,scale:f32,slope:f32,ne01z:i64,ne02:i32,gqa_ratio:i32,stride_q1:i64,stride_q2:i64,stride_k:i64,stride_v:i64,stride_mask:i64,jt:i32,zt_gqa:i32,kb0_start:i32,kb0_stop:i32,ncols1:i32,np:i32,needs_fixup:bool,is_fixup:bool,bx:i32,gx:i32,tx:i32,ty:i32,qregs:u64,vkqregs:u64,kmax:u64,rowsums:u64,smem:u64);
fn imin(a:i32,b:i32)->i32{return if a<b{a}else{b};}
struct TileIdx { sequence:i32, z_kv:i32, zt_gqa:i32, jt:i32 }
// Native decode L1912-1915/L1961-1964; ijzn=iter_k*iter_j*iter_z_gqa*ne12, ijz=iter_k*iter_j*iter_z_gqa, ij=iter_k*iter_j.
fn decode(kbc:i32,ijzn:i32,ijz:i32,ij:i32,iter_k:i32)->TileIdx{
 let sequence:i32=kbc/ijzn;
 let z_kv:i32=(kbc-ijzn*sequence)/ijz;
 let zt_gqa:i32=(kbc-ijzn*sequence-ijz*z_kv)/ij;
 let jt:i32=(kbc-ijzn*sequence-ijz*z_kv-ij*zt_gqa)/iter_k;
 return TileIdx{sequence,z_kv,zt_gqa,jt};
}
// One process_tile dispatch = native L1917-1947 and L1966-1990: pointer bases, dense
// KV_max clamp, then the extern call with the exact fixup flags handed in.
fn tile_call(sequence:i32,z_kv:i32,zt_gqa:i32,jt:i32,kb0_start:i32,kb0_stop_raw:i32,needs_fixup:bool,is_fixup:bool,q_p:u64,k_p:u64,v_p:u64,mask_p:u64,sinks_p:u64,kv_max_p:u64,dst_p:u64,dst_meta_p:u64,scale:f32,ne01_z:i32,ne02:i32,gqa_ratio:i32,ne33:i32,nb02:i32,nb03:i32,nb12:i32,nb13:i64,nb22:i32,nb23:i64,nb33:i64,stride_q1:i64,stride_q2:i64,stride_k:i64,stride_v:i64,stride_mask:i64,iter_j:i32,ncols1:i32,np:i32,bx:i32,gx:i32,tx:i32,ty:i32,qregs:u64,vkqregs:u64,kmax:u64,rowsums:u64,smem:u64){
 let zt_q:i32=z_kv*gqa_ratio+zt_gqa*8; // ncols2=8 (L1917)
 let q_f2:u64=q_p+(((nb03*sequence)+(nb02*zt_q)) as i64) as u64; // L1919, native i32 offsets sign-extended on pointer add
 let k_h2:u64=k_p+((nb13*(sequence as i64))+((nb12 as i64)*(z_kv as i64))) as u64; // L1920, nb13 is int64
 let mask_h:u64=mask_p+((nb33*((sequence%ne33) as i64))) as u64; // L1921-1922 else branch (ncols2==8 fixed)
 let dstk:u64=dst_p+((((sequence*ne01_z*ne02+zt_q)*128) as i64)*8) as u64; // L1923, DV/2=128 float2 elements
 let v_h2:u64=v_p+((nb23*(sequence as i64))+((nb22 as i64)*(z_kv as i64))) as u64; // L1925, V_is_K_view=false
 let sinks_f:u64=if sinks_p!=0{sinks_p+(((zt_q as i64)*4) as u64)}else{0 as u64}; // L1926
 var kb0_stop:i32=kb0_stop_raw;
 if kv_max_p!=0{ // use_sparse=false -> dense else-if branch (L1933-1935)
  let kvlim:i32=(ldu32(kv_max_p,(((sequence*iter_j)+jt) as i64) as u64)) as! i32;
  kb0_stop=imin(kb0_stop,kvlim/32); // nbatch_fa=32, native signed i32 division
 }
 process_tile(q_f2,k_h2,v_h2,mask_h,sinks_f,dstk,dst_meta_p,scale,1.0,(ne01_z as i64),ne02,gqa_ratio,stride_q1,stride_q2,stride_k,stride_v,stride_mask,jt,zt_gqa,kb0_start,kb0_stop,ncols1,np,needs_fixup,is_fixup,bx,gx,tx,ty,qregs,vkqregs,kmax,rowsums,smem);
}
fn fattn_mma(q_p:u64,k_p:u64,v_p:u64,mask_p:u64,sinks_p:u64,kv_max_p:u64,dst_p:u64,dst_meta_p:u64,scale:f32,max_bias:f32,m0:f32,m1:f32,n_head_log2:i32,logit_softcap:f32,ne00:i32,ne01_z:i32,ne02:i32,ne03:i32,nb01:i32,nb02:i32,nb03:i32,ne10:i32,ne11:i32,ne12:i32,ne13:i32,nb11:i32,nb12:i32,nb13:i64,nb21:i32,nb22:i32,nb23:i64,ne31:i32,ne32:i32,ne33:i32,nb31:i32,nb32:i32,nb33:i64,ncols1:i32,bx:i32,gx:i32,tx:i32,ty:i32,qregs:u64,vkqregs:u64,kmax:u64,rowsums:u64,smem:u64){
 pdl_sync(); // native L1820, first statement
 let gqa_ratio:i32=ne02/ne12; // L1881
 let stride_q1:i64=(nb01/8) as i64; // nb01/sizeof(float2), L1883
 let stride_q2:i64=(nb02/8) as i64; // L1884
 let stride_k:i64=(nb11/4) as i64;  // nb11/sizeof(half2), L1885
 let stride_mask:i64=(nb31/2) as i64; // nb31/sizeof(half), L1886
 let stride_v:i64=(nb21/4) as i64;  // V_is_K_view=false, L1888
 let iter_k:i32=(ne11+31)/32;       // (ne11+(nbatch_fa-1))/nbatch_fa, nbatch_fa=32, L1890
 let iter_j:i32=(ne01_z+(ncols1-1))/ncols1; // L1891
 let iter_z_gqa:i32=(gqa_ratio+7)/8; // ncols2=8, L1892
 // use_sparse=false: native L1894-1896 KV_max rebase and sparse_indices are absent.
 let total:i32=(((iter_k*iter_j)*iter_z_gqa)*ne12)*ne03; // native i32 product, operand order of L1899
 var kbc:i32=(((bx as i64)*(total as i64))/(gx as i64)) as! i32; // L1899 stream-K partition, i64 mul/div then native int truncation
 let kbc_stop:i32=((((bx+1) as i64)*(total as i64))/(gx as i64)) as! i32; // L1900
 var kb0_start:i32=kbc%iter_k; // L1907
 var kb0_stop:i32=imin(iter_k,kb0_start+kbc_stop-kbc); // L1908, native left-associative sum
 let np:i32=(4*16)/(ncols1*8); // nwarps*cols_per_warp/ncols, cols_per_warp=16 (L609/L1229): 2 for ncols1=4, 1 for ncols1=8
 let ijz:i32=(iter_k*iter_j)*iter_z_gqa;
 let ijzn:i32=ijz*ne12;
 let ij:i32=iter_k*iter_j;
 while (kbc<kbc_stop)&&(kb0_stop==iter_k){ // L1910, uniform tiles only
  let t=decode(kbc,ijzn,ijz,ij,iter_k);
  tile_call(t.sequence,t.z_kv,t.zt_gqa,t.jt,kb0_start,kb0_stop,kb0_start!=0,false,q_p,k_p,v_p,mask_p,sinks_p,kv_max_p,dst_p,dst_meta_p,scale,ne01_z,ne02,gqa_ratio,ne33,nb02,nb03,nb12,nb13,nb22,nb23,nb33,stride_q1,stride_q2,stride_k,stride_v,stride_mask,iter_j,ncols1,np,bx,gx,tx,ty,qregs,vkqregs,kmax,rowsums,smem); // is_fixup=false, needs_fixup=kb0_start!=0 (L1936-1947)
  kbc=kbc+iter_k; // L1949
  kbc=kbc-(kbc%iter_k); // L1950
  kb0_start=0; // L1952
  kb0_stop=imin(iter_k,kbc_stop-kbc); // L1953
 }
 if kbc<kbc_stop{ // native early return L1956-1958 inverted around the final partial tile
  let t=decode(kbc,ijzn,ijz,ij,iter_k);
  tile_call(t.sequence,t.z_kv,t.zt_gqa,t.jt,kb0_start,kb0_stop,false,true,q_p,k_p,v_p,mask_p,sinks_p,kv_max_p,dst_p,dst_meta_p,scale,ne01_z,ne02,gqa_ratio,ne33,nb02,nb03,nb12,nb13,nb22,nb23,nb33,stride_q1,stride_q2,stride_k,stride_v,stride_mask,iter_j,ncols1,np,bx,gx,tx,ty,qregs,vkqregs,kmax,rowsums,smem); // is_fixup=true, needs_fixup=false (L1986-1990)
 }
}
// Retention entry; the parent never executes main.
fn main(){fattn_mma(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,1.0,0.0,0.0,0.0,0,0.0,256,4,8,1,1024,8192,8192,256,32,1,1,512,16384,0 as i64,512,16384,0 as i64,0,0,1,64,0,0 as i64,4,0,1,0,0,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64);}