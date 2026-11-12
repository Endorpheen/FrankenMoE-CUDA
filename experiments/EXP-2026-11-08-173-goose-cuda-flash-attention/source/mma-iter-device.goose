// fg176i: Goose arithmetic/control of flash_attn_ext_f16_iter (fattn-mma-f16.cuh L583-1072), fixed
// Apex CUDA Ada target: DKQ=DV=256, ncols1=4 or 8, ncols2=8, nwarps=4, nbatch_fa=32,
// nbatch_K2=nbatch_V2=128, Q_in_reg, nstages=2 cp.async double stage, swizzled tiles,
// cols_per_warp=16, cols_per_thread=2, np=2 (ncols1=4) else 1, softcap/sparse/oob/V_is_K_view=false.
// Turing tile set (mma_tile_sizes<256,ncols>): T_A_KQ/T_B_KQ/T_A_VKQ/T_B_VKQ/T_C_VKQ=tile<16,8,half2>,
// T_C_KQ=tile<16,16,float> (wide column-major: physical I=Q column j, physical J=KV row i).
// Everything except raw single-ISA effects runs here: operand swaps (KQ mma A=Q_B B=K_A; VKQ mma
// A=probs B=transposed-V), f32/f16 pair B-register order 0,2 then 1,3, ldmatrix.x4.trans register
// remap 0,2,1,3, swizzled shared addressing, per-lane fragment index math, mask row/col mapping,
// online softmax (offset 3*0.6931, shfl_xor 2/1, FTZ bit-multiply at diff<-20, exact fused rowsum
// step), F32->half2 prob pack (no transpose on Turing) and the double-stage wait/barrier/load-next
// sequencing (next K+mask issued before the current VKQ mma). K/mask issue loops are the fg175l
// Goose loaders; no native algorithm callback. Shared map (bytes from smem): tile_K [0,16384) 32 rows
// x 128 half2 (row stride 512 B), tile_V [16384,32768), tile_mask [32768,32768+ncols1*80) ncols1 rows
// x 20 half2 (16 B pad). Block dim3(32,4): tx=threadIdx.x lane, ty=threadIdx.y warp.
extern "fg176i_bits" fn bits(v:f32)->u32;
extern "fg176i_frombits" fn frombits(v:u32)->f32;
struct U2 { a:u32, b:u32 }
struct U4 { a:u32, b:u32, c:u32, d:u32 }
struct F4 { x:f32, y:f32, z:f32, w:f32 }

// fg175l Goose loaders (cp.async.cg 16 B issue loops) imported under their raw adapter names.
extern "fg175l_load_kv" fn load_kv(src:u64,dst_shared:u64,stride_kv_h2:i64,kv_row0:i64,tx:i32,ty:i32);
extern "fg175l_load_mask" fn load_mask(src:u64,dst_shared:u64,stride_mask_half:i64,kv_row0:i64,query0:i64,n_queries:i64,ncols1:i32,tx:i32,ty:i32);
// Raw effects: absolute byte addresses (generic for POD state, shared for tiles/ldmatrix).
extern "fg176i_ldu" fn ldu(a:u64)->u32;
extern "fg176i_stu" fn stu(a:u64,v:u32);
extern "fg176i_ldm4" fn ldm4(a:u64)->U4;
extern "fg176i_ldm4t" fn ldm4t(a:u64)->U4;
extern "fg176i_mma_f32" fn mma_f32(d:F4,a0:u32,a1:u32,a2:u32,a3:u32,b0:u32,b1:u32)->F4;
extern "fg176i_mma_f16" fn mma_f16(d:U2,a0:u32,a1:u32,a2:u32,a3:u32,b0:u32,b1:u32)->U2;
extern "fg176i_hlo" fn hlo(w:u32)->f32;
extern "fg176i_hhi" fn hhi(w:u32)->f32;
extern "fg176i_h2" fn h2(a:f32,b:f32)->u32;
extern "fg176i_mulh2" fn mulh2(w:u32,s:u32)->u32;
extern "fg176i_expf" fn expf(x:f32)->f32;
extern "fg176i_fmaf" fn fmaf(a:f32,b:f32,c:f32)->f32;
extern "fg176i_shflx" fn shflx(x:f32,off:i32)->f32;
extern "fg176i_cpwait" fn cpwait();
extern "fg176i_barrier" fn barrier();

// ggml_cuda_mma::swizzle<128,half2>(off,i): stride_bytes=512 -> off ^ ((i%8)<<2) in half2 units.
fn swz_h2(off:i32,row:i32)->i32{ return off ^ ((row%8)<<2); }
// Per-lane ldmatrix source address (load_ldmatrix_swizzled/_trans_swizzled, I=16, J=8):
// i=tx%16, j=(tx/16)*(J/2)=4, offset_ij=off+i*stride+j, swizzled by i; stride_tile=128 half2.
fn ldm_addr(base:u64, off:i32, tx:i32)->u64{ let row=tx%16; return base+((swz_h2(off+row*128+(tx/16)*4,row) as u64)*(4 as u64)); }
// ldmatrix.x4.trans hardware registers h0..h3 bind to tile registers xi[0],xi[2],xi[1],xi[3]
// (native asm order 0,2,1,3); reorder here so downstream uses plain tile order x0..x3.
fn ldm4t_tile(a:u64)->U4{ let z=ldm4t(a); return U4{z.a,z.c,z.b,z.d}; }
// tile<16,16,float> fragment mapping (wide KQ_C): get_i(l)=((l/2)%2)*8+tx/4 -> Q column j,
// get_j(l)=(l/4)*8+(tx%4)*2+l%2 -> KV row i.
fn gi_c(l:i32, tx:i32)->i32{ return ((l/2)%2)*8 + tx/4; }
fn gj_c(l:i32, tx:i32)->i32{ return (l/4)*8 + (tx%4)*2 + l%2; }

fn iterate_nc4_np2(k_h2:u64, v_h2:u64, mask_h:u64, stride_k:i64, stride_v:i64, stride_mask:i64, slope:f32, jt:i32, kb0:i32, n_queries:i64, qregs:u64, vkqregs:u64, kmax:u64, rowsums:u64, smem:u64, tx:i32, ty:i32, last:bool) {let ncols1:i32=4;let np:i32=2;

  let kt=smem;
  let vt=smem+(16384 as u64);
  let mt=smem+(32768 as u64);
  let kv_row0=(kb0 as i64)*32;               // k_VKQ_0 = kb0*nbatch_fa
  let query0=(jt as i64)*(ncols1 as i64);    // j0 for the mask loader
  let cb=(ty/np)*16;                          // warp Q-column block base (cols_per_warp=16)
  let rh=(ty%np)*16;                          // warp KV-row half base (np*T_A_KQ::I split)
  let nt=32/(np*16);                          // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
  let kqoff=(3.0 as f32)*(0.6931 as f32);                       // FATTN_KQ_MAX_OFFSET
  var t:i32=0;
  // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
  // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
  cpwait();
  barrier();
  load_kv(v_h2,vt,stride_v,kv_row0,tx,ty);
  // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
  var kqc:f32[16]=default<f32[16]>();        // up to 2 fresh zero tile<16,16,float> C tiles
  var q:i32=0;                                // Q_B tile index = k_KQ_0/8
  if true {
let q:i32=0;

    let q0__u1=ldu(qregs+((q*16) as u64));
    let q1__u1=ldu(qregs+((q*16+4) as u64));
    let q2__u1=ldu(qregs+((q*16+8) as u64));
    let q3__u1=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u2=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u2=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u1,q1__u1,q2__u1,q3__u1,ka__u2.a,ka__u2.c);
      kqc[(0 as i32)]=lo__u2.x;kqc[(1 as i32)]=lo__u2.y;kqc[(2 as i32)]=lo__u2.z;kqc[(3 as i32)]=lo__u2.w;
      let hi__u2=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u1,q1__u1,q2__u1,q3__u1,ka__u2.b,ka__u2.d);
      kqc[(4 as i32)]=hi__u2.x;kqc[(5 as i32)]=hi__u2.y;kqc[(6 as i32)]=hi__u2.z;kqc[(7 as i32)]=hi__u2.w;

}
t=(1 as i32);


}
if true {
let q:i32=1;

    let q0__u3=ldu(qregs+((q*16) as u64));
    let q1__u3=ldu(qregs+((q*16+4) as u64));
    let q2__u3=ldu(qregs+((q*16+8) as u64));
    let q3__u3=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u4=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u4=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u3,q1__u3,q2__u3,q3__u3,ka__u4.a,ka__u4.c);
      kqc[(0 as i32)]=lo__u4.x;kqc[(1 as i32)]=lo__u4.y;kqc[(2 as i32)]=lo__u4.z;kqc[(3 as i32)]=lo__u4.w;
      let hi__u4=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u3,q1__u3,q2__u3,q3__u3,ka__u4.b,ka__u4.d);
      kqc[(4 as i32)]=hi__u4.x;kqc[(5 as i32)]=hi__u4.y;kqc[(6 as i32)]=hi__u4.z;kqc[(7 as i32)]=hi__u4.w;

}
t=(1 as i32);


}
if true {
let q:i32=2;

    let q0__u5=ldu(qregs+((q*16) as u64));
    let q1__u5=ldu(qregs+((q*16+4) as u64));
    let q2__u5=ldu(qregs+((q*16+8) as u64));
    let q3__u5=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u6=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u6=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u5,q1__u5,q2__u5,q3__u5,ka__u6.a,ka__u6.c);
      kqc[(0 as i32)]=lo__u6.x;kqc[(1 as i32)]=lo__u6.y;kqc[(2 as i32)]=lo__u6.z;kqc[(3 as i32)]=lo__u6.w;
      let hi__u6=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u5,q1__u5,q2__u5,q3__u5,ka__u6.b,ka__u6.d);
      kqc[(4 as i32)]=hi__u6.x;kqc[(5 as i32)]=hi__u6.y;kqc[(6 as i32)]=hi__u6.z;kqc[(7 as i32)]=hi__u6.w;

}
t=(1 as i32);


}
if true {
let q:i32=3;

    let q0__u7=ldu(qregs+((q*16) as u64));
    let q1__u7=ldu(qregs+((q*16+4) as u64));
    let q2__u7=ldu(qregs+((q*16+8) as u64));
    let q3__u7=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u8=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u8=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u7,q1__u7,q2__u7,q3__u7,ka__u8.a,ka__u8.c);
      kqc[(0 as i32)]=lo__u8.x;kqc[(1 as i32)]=lo__u8.y;kqc[(2 as i32)]=lo__u8.z;kqc[(3 as i32)]=lo__u8.w;
      let hi__u8=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u7,q1__u7,q2__u7,q3__u7,ka__u8.b,ka__u8.d);
      kqc[(4 as i32)]=hi__u8.x;kqc[(5 as i32)]=hi__u8.y;kqc[(6 as i32)]=hi__u8.z;kqc[(7 as i32)]=hi__u8.w;

}
t=(1 as i32);


}
if true {
let q:i32=4;

    let q0__u9=ldu(qregs+((q*16) as u64));
    let q1__u9=ldu(qregs+((q*16+4) as u64));
    let q2__u9=ldu(qregs+((q*16+8) as u64));
    let q3__u9=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u10=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u10=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u9,q1__u9,q2__u9,q3__u9,ka__u10.a,ka__u10.c);
      kqc[(0 as i32)]=lo__u10.x;kqc[(1 as i32)]=lo__u10.y;kqc[(2 as i32)]=lo__u10.z;kqc[(3 as i32)]=lo__u10.w;
      let hi__u10=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u9,q1__u9,q2__u9,q3__u9,ka__u10.b,ka__u10.d);
      kqc[(4 as i32)]=hi__u10.x;kqc[(5 as i32)]=hi__u10.y;kqc[(6 as i32)]=hi__u10.z;kqc[(7 as i32)]=hi__u10.w;

}
t=(1 as i32);


}
if true {
let q:i32=5;

    let q0__u11=ldu(qregs+((q*16) as u64));
    let q1__u11=ldu(qregs+((q*16+4) as u64));
    let q2__u11=ldu(qregs+((q*16+8) as u64));
    let q3__u11=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u12=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u12=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u11,q1__u11,q2__u11,q3__u11,ka__u12.a,ka__u12.c);
      kqc[(0 as i32)]=lo__u12.x;kqc[(1 as i32)]=lo__u12.y;kqc[(2 as i32)]=lo__u12.z;kqc[(3 as i32)]=lo__u12.w;
      let hi__u12=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u11,q1__u11,q2__u11,q3__u11,ka__u12.b,ka__u12.d);
      kqc[(4 as i32)]=hi__u12.x;kqc[(5 as i32)]=hi__u12.y;kqc[(6 as i32)]=hi__u12.z;kqc[(7 as i32)]=hi__u12.w;

}
t=(1 as i32);


}
if true {
let q:i32=6;

    let q0__u13=ldu(qregs+((q*16) as u64));
    let q1__u13=ldu(qregs+((q*16+4) as u64));
    let q2__u13=ldu(qregs+((q*16+8) as u64));
    let q3__u13=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u14=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u14=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u13,q1__u13,q2__u13,q3__u13,ka__u14.a,ka__u14.c);
      kqc[(0 as i32)]=lo__u14.x;kqc[(1 as i32)]=lo__u14.y;kqc[(2 as i32)]=lo__u14.z;kqc[(3 as i32)]=lo__u14.w;
      let hi__u14=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u13,q1__u13,q2__u13,q3__u13,ka__u14.b,ka__u14.d);
      kqc[(4 as i32)]=hi__u14.x;kqc[(5 as i32)]=hi__u14.y;kqc[(6 as i32)]=hi__u14.z;kqc[(7 as i32)]=hi__u14.w;

}
t=(1 as i32);


}
if true {
let q:i32=7;

    let q0__u15=ldu(qregs+((q*16) as u64));
    let q1__u15=ldu(qregs+((q*16+4) as u64));
    let q2__u15=ldu(qregs+((q*16+8) as u64));
    let q3__u15=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u16=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u16=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u15,q1__u15,q2__u15,q3__u15,ka__u16.a,ka__u16.c);
      kqc[(0 as i32)]=lo__u16.x;kqc[(1 as i32)]=lo__u16.y;kqc[(2 as i32)]=lo__u16.z;kqc[(3 as i32)]=lo__u16.w;
      let hi__u16=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u15,q1__u15,q2__u15,q3__u15,ka__u16.b,ka__u16.d);
      kqc[(4 as i32)]=hi__u16.x;kqc[(5 as i32)]=hi__u16.y;kqc[(6 as i32)]=hi__u16.z;kqc[(7 as i32)]=hi__u16.w;

}
t=(1 as i32);


}
if true {
let q:i32=8;

    let q0__u17=ldu(qregs+((q*16) as u64));
    let q1__u17=ldu(qregs+((q*16+4) as u64));
    let q2__u17=ldu(qregs+((q*16+8) as u64));
    let q3__u17=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u18=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u18=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u17,q1__u17,q2__u17,q3__u17,ka__u18.a,ka__u18.c);
      kqc[(0 as i32)]=lo__u18.x;kqc[(1 as i32)]=lo__u18.y;kqc[(2 as i32)]=lo__u18.z;kqc[(3 as i32)]=lo__u18.w;
      let hi__u18=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u17,q1__u17,q2__u17,q3__u17,ka__u18.b,ka__u18.d);
      kqc[(4 as i32)]=hi__u18.x;kqc[(5 as i32)]=hi__u18.y;kqc[(6 as i32)]=hi__u18.z;kqc[(7 as i32)]=hi__u18.w;

}
t=(1 as i32);


}
if true {
let q:i32=9;

    let q0__u19=ldu(qregs+((q*16) as u64));
    let q1__u19=ldu(qregs+((q*16+4) as u64));
    let q2__u19=ldu(qregs+((q*16+8) as u64));
    let q3__u19=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u20=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u20=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u19,q1__u19,q2__u19,q3__u19,ka__u20.a,ka__u20.c);
      kqc[(0 as i32)]=lo__u20.x;kqc[(1 as i32)]=lo__u20.y;kqc[(2 as i32)]=lo__u20.z;kqc[(3 as i32)]=lo__u20.w;
      let hi__u20=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u19,q1__u19,q2__u19,q3__u19,ka__u20.b,ka__u20.d);
      kqc[(4 as i32)]=hi__u20.x;kqc[(5 as i32)]=hi__u20.y;kqc[(6 as i32)]=hi__u20.z;kqc[(7 as i32)]=hi__u20.w;

}
t=(1 as i32);


}
if true {
let q:i32=10;

    let q0__u21=ldu(qregs+((q*16) as u64));
    let q1__u21=ldu(qregs+((q*16+4) as u64));
    let q2__u21=ldu(qregs+((q*16+8) as u64));
    let q3__u21=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u22=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u22=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u21,q1__u21,q2__u21,q3__u21,ka__u22.a,ka__u22.c);
      kqc[(0 as i32)]=lo__u22.x;kqc[(1 as i32)]=lo__u22.y;kqc[(2 as i32)]=lo__u22.z;kqc[(3 as i32)]=lo__u22.w;
      let hi__u22=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u21,q1__u21,q2__u21,q3__u21,ka__u22.b,ka__u22.d);
      kqc[(4 as i32)]=hi__u22.x;kqc[(5 as i32)]=hi__u22.y;kqc[(6 as i32)]=hi__u22.z;kqc[(7 as i32)]=hi__u22.w;

}
t=(1 as i32);


}
if true {
let q:i32=11;

    let q0__u23=ldu(qregs+((q*16) as u64));
    let q1__u23=ldu(qregs+((q*16+4) as u64));
    let q2__u23=ldu(qregs+((q*16+8) as u64));
    let q3__u23=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u24=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u24=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u23,q1__u23,q2__u23,q3__u23,ka__u24.a,ka__u24.c);
      kqc[(0 as i32)]=lo__u24.x;kqc[(1 as i32)]=lo__u24.y;kqc[(2 as i32)]=lo__u24.z;kqc[(3 as i32)]=lo__u24.w;
      let hi__u24=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u23,q1__u23,q2__u23,q3__u23,ka__u24.b,ka__u24.d);
      kqc[(4 as i32)]=hi__u24.x;kqc[(5 as i32)]=hi__u24.y;kqc[(6 as i32)]=hi__u24.z;kqc[(7 as i32)]=hi__u24.w;

}
t=(1 as i32);


}
if true {
let q:i32=12;

    let q0__u25=ldu(qregs+((q*16) as u64));
    let q1__u25=ldu(qregs+((q*16+4) as u64));
    let q2__u25=ldu(qregs+((q*16+8) as u64));
    let q3__u25=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u26=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u26=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u25,q1__u25,q2__u25,q3__u25,ka__u26.a,ka__u26.c);
      kqc[(0 as i32)]=lo__u26.x;kqc[(1 as i32)]=lo__u26.y;kqc[(2 as i32)]=lo__u26.z;kqc[(3 as i32)]=lo__u26.w;
      let hi__u26=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u25,q1__u25,q2__u25,q3__u25,ka__u26.b,ka__u26.d);
      kqc[(4 as i32)]=hi__u26.x;kqc[(5 as i32)]=hi__u26.y;kqc[(6 as i32)]=hi__u26.z;kqc[(7 as i32)]=hi__u26.w;

}
t=(1 as i32);


}
if true {
let q:i32=13;

    let q0__u27=ldu(qregs+((q*16) as u64));
    let q1__u27=ldu(qregs+((q*16+4) as u64));
    let q2__u27=ldu(qregs+((q*16+8) as u64));
    let q3__u27=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u28=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u28=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u27,q1__u27,q2__u27,q3__u27,ka__u28.a,ka__u28.c);
      kqc[(0 as i32)]=lo__u28.x;kqc[(1 as i32)]=lo__u28.y;kqc[(2 as i32)]=lo__u28.z;kqc[(3 as i32)]=lo__u28.w;
      let hi__u28=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u27,q1__u27,q2__u27,q3__u27,ka__u28.b,ka__u28.d);
      kqc[(4 as i32)]=hi__u28.x;kqc[(5 as i32)]=hi__u28.y;kqc[(6 as i32)]=hi__u28.z;kqc[(7 as i32)]=hi__u28.w;

}
t=(1 as i32);


}
if true {
let q:i32=14;

    let q0__u29=ldu(qregs+((q*16) as u64));
    let q1__u29=ldu(qregs+((q*16+4) as u64));
    let q2__u29=ldu(qregs+((q*16+8) as u64));
    let q3__u29=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u30=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u30=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u29,q1__u29,q2__u29,q3__u29,ka__u30.a,ka__u30.c);
      kqc[(0 as i32)]=lo__u30.x;kqc[(1 as i32)]=lo__u30.y;kqc[(2 as i32)]=lo__u30.z;kqc[(3 as i32)]=lo__u30.w;
      let hi__u30=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u29,q1__u29,q2__u29,q3__u29,ka__u30.b,ka__u30.d);
      kqc[(4 as i32)]=hi__u30.x;kqc[(5 as i32)]=hi__u30.y;kqc[(6 as i32)]=hi__u30.z;kqc[(7 as i32)]=hi__u30.w;

}
t=(1 as i32);


}
if true {
let q:i32=15;

    let q0__u31=ldu(qregs+((q*16) as u64));
    let q1__u31=ldu(qregs+((q*16+4) as u64));
    let q2__u31=ldu(qregs+((q*16+8) as u64));
    let q3__u31=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u32=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u32=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u31,q1__u31,q2__u31,q3__u31,ka__u32.a,ka__u32.c);
      kqc[(0 as i32)]=lo__u32.x;kqc[(1 as i32)]=lo__u32.y;kqc[(2 as i32)]=lo__u32.z;kqc[(3 as i32)]=lo__u32.w;
      let hi__u32=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u31,q1__u31,q2__u31,q3__u31,ka__u32.b,ka__u32.d);
      kqc[(4 as i32)]=hi__u32.x;kqc[(5 as i32)]=hi__u32.y;kqc[(6 as i32)]=hi__u32.z;kqc[(7 as i32)]=hi__u32.w;

}
t=(1 as i32);


}
q=(16 as i32);

  // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
  var kqold:f32[2]=default<f32[2]>();        // KQ_max[cols_per_thread=2]
  var kqmn:f32[2]=default<f32[2]>();         // KQ_max_new
  var rsa:f32[2]=default<f32[2]>();          // KQ_rowsum_add
  kqold[(0 as i32)]=frombits(ldu(kmax));
  kqold[(1 as i32)]=frombits(ldu(kmax+(4 as u64)));
  kqmn[(0 as i32)]=kqold[(0 as i32)];
  kqmn[(1 as i32)]=kqold[(1 as i32)];
  t=0;
  if true {
let t:i32=0;

    let rb__u33=t*np*16+rh;
    var l0__u33:i32=0;
    if true {
let l0__u33:i32=0;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u34=((cb+gi_c(l0__u33,tx))/8)*20+((rb__u33+gj_c(l0__u33,tx))/2);
      let w__u34=ldu(mt+((mh2__u34 as u64)*(4 as u64)));
      kqc[(0 as i32)]=fmaf(slope,hlo(w__u34),kqc[(0 as i32)]);
      kqc[(1 as i32)]=fmaf(slope,hhi(w__u34),kqc[(1 as i32)]);

}
if true {
let l0__u33:i32=2;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u35=((cb+gi_c(l0__u33,tx))/8)*20+((rb__u33+gj_c(l0__u33,tx))/2);
      let w__u35=ldu(mt+((mh2__u35 as u64)*(4 as u64)));
      kqc[(2 as i32)]=fmaf(slope,hlo(w__u35),kqc[(2 as i32)]);
      kqc[(3 as i32)]=fmaf(slope,hhi(w__u35),kqc[(3 as i32)]);

}
if true {
let l0__u33:i32=4;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u36=((cb+gi_c(l0__u33,tx))/8)*20+((rb__u33+gj_c(l0__u33,tx))/2);
      let w__u36=ldu(mt+((mh2__u36 as u64)*(4 as u64)));
      kqc[(4 as i32)]=fmaf(slope,hlo(w__u36),kqc[(4 as i32)]);
      kqc[(5 as i32)]=fmaf(slope,hhi(w__u36),kqc[(5 as i32)]);

}
if true {
let l0__u33:i32=6;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u37=((cb+gi_c(l0__u33,tx))/8)*20+((rb__u33+gj_c(l0__u33,tx))/2);
      let w__u37=ldu(mt+((mh2__u37 as u64)*(4 as u64)));
      kqc[(6 as i32)]=fmaf(slope,hlo(w__u37),kqc[(6 as i32)]);
      kqc[(7 as i32)]=fmaf(slope,hhi(w__u37),kqc[(7 as i32)]);

}
l0__u33=(8 as i32);

    var l__u33:i32=0;
    if true {
let l__u33:i32=0;

      let idx__u38=(l__u33/2)%2;
      let mv__u38=kqc[(0 as i32)]+kqoff;
      if mv__u38>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u38;}

}
if true {
let l__u33:i32=1;

      let idx__u39=(l__u33/2)%2;
      let mv__u39=kqc[(1 as i32)]+kqoff;
      if mv__u39>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u39;}

}
if true {
let l__u33:i32=2;

      let idx__u40=(l__u33/2)%2;
      let mv__u40=kqc[(2 as i32)]+kqoff;
      if mv__u40>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u40;}

}
if true {
let l__u33:i32=3;

      let idx__u41=(l__u33/2)%2;
      let mv__u41=kqc[(3 as i32)]+kqoff;
      if mv__u41>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u41;}

}
if true {
let l__u33:i32=4;

      let idx__u42=(l__u33/2)%2;
      let mv__u42=kqc[(4 as i32)]+kqoff;
      if mv__u42>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u42;}

}
if true {
let l__u33:i32=5;

      let idx__u43=(l__u33/2)%2;
      let mv__u43=kqc[(5 as i32)]+kqoff;
      if mv__u43>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u43;}

}
if true {
let l__u33:i32=6;

      let idx__u44=(l__u33/2)%2;
      let mv__u44=kqc[(6 as i32)]+kqoff;
      if mv__u44>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u44;}

}
if true {
let l__u33:i32=7;

      let idx__u45=(l__u33/2)%2;
      let mv__u45=kqc[(7 as i32)]+kqoff;
      if mv__u45>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u45;}

}
l__u33=(8 as i32);


}
t=(1 as i32);

  // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
  var col:i32=0;
  if true {
let col:i32=0;

    var s__u46=shflx(kqmn[(0 as i32)],2);
    if s__u46>kqmn[(0 as i32)] {kqmn[(0 as i32)]=s__u46;}
    s__u46=shflx(kqmn[(0 as i32)],1);
    if s__u46>kqmn[(0 as i32)] {kqmn[(0 as i32)]=s__u46;}

}
if true {
let col:i32=1;

    var s__u47=shflx(kqmn[(1 as i32)],2);
    if s__u47>kqmn[(1 as i32)] {kqmn[(1 as i32)]=s__u47;}
    s__u47=shflx(kqmn[(1 as i32)],1);
    if s__u47>kqmn[(1 as i32)] {kqmn[(1 as i32)]=s__u47;}

}
col=(2 as i32);

  // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
  t=0;
  if true {
let t:i32=0;

    var l__u48:i32=0;
    if true {
let l__u48:i32=0;

      let idx__u49=(l__u48/2)%2;
      let e__u49=expf(kqc[(0 as i32)]-kqmn[(0 as i32)]);
      kqc[(0 as i32)]=e__u49;
      rsa[(0 as i32)]+=e__u49;

}
if true {
let l__u48:i32=1;

      let idx__u50=(l__u48/2)%2;
      let e__u50=expf(kqc[(1 as i32)]-kqmn[(0 as i32)]);
      kqc[(1 as i32)]=e__u50;
      rsa[(0 as i32)]+=e__u50;

}
if true {
let l__u48:i32=2;

      let idx__u51=(l__u48/2)%2;
      let e__u51=expf(kqc[(2 as i32)]-kqmn[(1 as i32)]);
      kqc[(2 as i32)]=e__u51;
      rsa[(1 as i32)]+=e__u51;

}
if true {
let l__u48:i32=3;

      let idx__u52=(l__u48/2)%2;
      let e__u52=expf(kqc[(3 as i32)]-kqmn[(1 as i32)]);
      kqc[(3 as i32)]=e__u52;
      rsa[(1 as i32)]+=e__u52;

}
if true {
let l__u48:i32=4;

      let idx__u53=(l__u48/2)%2;
      let e__u53=expf(kqc[(4 as i32)]-kqmn[(0 as i32)]);
      kqc[(4 as i32)]=e__u53;
      rsa[(0 as i32)]+=e__u53;

}
if true {
let l__u48:i32=5;

      let idx__u54=(l__u48/2)%2;
      let e__u54=expf(kqc[(5 as i32)]-kqmn[(0 as i32)]);
      kqc[(5 as i32)]=e__u54;
      rsa[(0 as i32)]+=e__u54;

}
if true {
let l__u48:i32=6;

      let idx__u55=(l__u48/2)%2;
      let e__u55=expf(kqc[(6 as i32)]-kqmn[(1 as i32)]);
      kqc[(6 as i32)]=e__u55;
      rsa[(1 as i32)]+=e__u55;

}
if true {
let l__u48:i32=7;

      let idx__u56=(l__u48/2)%2;
      let e__u56=expf(kqc[(7 as i32)]-kqmn[(1 as i32)]);
      kqc[(7 as i32)]=e__u56;
      rsa[(1 as i32)]+=e__u56;

}
l__u48=(8 as i32);


}
t=(1 as i32);

  // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
  //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
  var scl:f32[2]=default<f32[2]>();
  col=0;
  if true {
let col:i32=0;

    let diff__u57=kqold[(0 as i32)]-kqmn[(0 as i32)];
    let s0__u57=expf(diff__u57);
    let keep__u57:u32=if diff__u57>=-20.0 {1 as u32} else {0 as u32};
    scl[(0 as i32)]=frombits(bits(s0__u57)*keep__u57);    // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
    stu(kmax+((col*4) as u64),bits(kqmn[(0 as i32)]));
    let rs__u57=frombits(ldu(rowsums+((col*4) as u64)));
    stu(rowsums+((col*4) as u64),bits(fmaf(scl[(0 as i32)],rs__u57,rsa[(0 as i32)])));

}
if true {
let col:i32=1;

    let diff__u58=kqold[(1 as i32)]-kqmn[(1 as i32)];
    let s0__u58=expf(diff__u58);
    let keep__u58:u32=if diff__u58>=-20.0 {1 as u32} else {0 as u32};
    scl[(1 as i32)]=frombits(bits(s0__u58)*keep__u58);    // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
    stu(kmax+((col*4) as u64),bits(kqmn[(1 as i32)]));
    let rs__u58=frombits(ldu(rowsums+((col*4) as u64)));
    stu(rowsums+((col*4) as u64),bits(fmaf(scl[(1 as i32)],rs__u58,rsa[(1 as i32)])));

}
col=(2 as i32);

  // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
  var sh2:u32[2]=default<u32[2]>();
  col=0;
  if true {
let col:i32=0;
 sh2[(0 as i32)]=h2(scl[(0 as i32)],scl[(0 as i32)]);
}
if true {
let col:i32=1;
 sh2[(1 as i32)]=h2(scl[(1 as i32)],scl[(1 as i32)]);
}
col=(2 as i32);

  var v:i32=0;
  if true {
let v:i32=0;

    col=0;
    if true {
let col:i32=0;

      var l0__u62:i32=0;
      if true {
let l0__u62:i32=0;

        let e__u63=v*4+l0__u62*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u63*4) as u64),mulh2(ldu(vkqregs+((e__u63*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u62:i32=1;

        let e__u64=v*4+l0__u62*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u64*4) as u64),mulh2(ldu(vkqregs+((e__u64*4) as u64)),sh2[(0 as i32)]));

}
l0__u62=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u65:i32=0;
      if true {
let l0__u65:i32=0;

        let e__u66=v*4+l0__u65*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u66*4) as u64),mulh2(ldu(vkqregs+((e__u66*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u65:i32=1;

        let e__u67=v*4+l0__u65*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u67*4) as u64),mulh2(ldu(vkqregs+((e__u67*4) as u64)),sh2[(1 as i32)]));

}
l0__u65=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=1;

    col=0;
    if true {
let col:i32=0;

      var l0__u69:i32=0;
      if true {
let l0__u69:i32=0;

        let e__u70=v*4+l0__u69*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u70*4) as u64),mulh2(ldu(vkqregs+((e__u70*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u69:i32=1;

        let e__u71=v*4+l0__u69*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u71*4) as u64),mulh2(ldu(vkqregs+((e__u71*4) as u64)),sh2[(0 as i32)]));

}
l0__u69=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u72:i32=0;
      if true {
let l0__u72:i32=0;

        let e__u73=v*4+l0__u72*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u73*4) as u64),mulh2(ldu(vkqregs+((e__u73*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u72:i32=1;

        let e__u74=v*4+l0__u72*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u74*4) as u64),mulh2(ldu(vkqregs+((e__u74*4) as u64)),sh2[(1 as i32)]));

}
l0__u72=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=2;

    col=0;
    if true {
let col:i32=0;

      var l0__u76:i32=0;
      if true {
let l0__u76:i32=0;

        let e__u77=v*4+l0__u76*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u77*4) as u64),mulh2(ldu(vkqregs+((e__u77*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u76:i32=1;

        let e__u78=v*4+l0__u76*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u78*4) as u64),mulh2(ldu(vkqregs+((e__u78*4) as u64)),sh2[(0 as i32)]));

}
l0__u76=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u79:i32=0;
      if true {
let l0__u79:i32=0;

        let e__u80=v*4+l0__u79*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u80*4) as u64),mulh2(ldu(vkqregs+((e__u80*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u79:i32=1;

        let e__u81=v*4+l0__u79*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u81*4) as u64),mulh2(ldu(vkqregs+((e__u81*4) as u64)),sh2[(1 as i32)]));

}
l0__u79=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=3;

    col=0;
    if true {
let col:i32=0;

      var l0__u83:i32=0;
      if true {
let l0__u83:i32=0;

        let e__u84=v*4+l0__u83*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u84*4) as u64),mulh2(ldu(vkqregs+((e__u84*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u83:i32=1;

        let e__u85=v*4+l0__u83*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u85*4) as u64),mulh2(ldu(vkqregs+((e__u85*4) as u64)),sh2[(0 as i32)]));

}
l0__u83=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u86:i32=0;
      if true {
let l0__u86:i32=0;

        let e__u87=v*4+l0__u86*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u87*4) as u64),mulh2(ldu(vkqregs+((e__u87*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u86:i32=1;

        let e__u88=v*4+l0__u86*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u88*4) as u64),mulh2(ldu(vkqregs+((e__u88*4) as u64)),sh2[(1 as i32)]));

}
l0__u86=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=4;

    col=0;
    if true {
let col:i32=0;

      var l0__u90:i32=0;
      if true {
let l0__u90:i32=0;

        let e__u91=v*4+l0__u90*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u91*4) as u64),mulh2(ldu(vkqregs+((e__u91*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u90:i32=1;

        let e__u92=v*4+l0__u90*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u92*4) as u64),mulh2(ldu(vkqregs+((e__u92*4) as u64)),sh2[(0 as i32)]));

}
l0__u90=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u93:i32=0;
      if true {
let l0__u93:i32=0;

        let e__u94=v*4+l0__u93*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u94*4) as u64),mulh2(ldu(vkqregs+((e__u94*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u93:i32=1;

        let e__u95=v*4+l0__u93*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u95*4) as u64),mulh2(ldu(vkqregs+((e__u95*4) as u64)),sh2[(1 as i32)]));

}
l0__u93=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=5;

    col=0;
    if true {
let col:i32=0;

      var l0__u97:i32=0;
      if true {
let l0__u97:i32=0;

        let e__u98=v*4+l0__u97*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u98*4) as u64),mulh2(ldu(vkqregs+((e__u98*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u97:i32=1;

        let e__u99=v*4+l0__u97*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u99*4) as u64),mulh2(ldu(vkqregs+((e__u99*4) as u64)),sh2[(0 as i32)]));

}
l0__u97=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u100:i32=0;
      if true {
let l0__u100:i32=0;

        let e__u101=v*4+l0__u100*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u101*4) as u64),mulh2(ldu(vkqregs+((e__u101*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u100:i32=1;

        let e__u102=v*4+l0__u100*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u102*4) as u64),mulh2(ldu(vkqregs+((e__u102*4) as u64)),sh2[(1 as i32)]));

}
l0__u100=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=6;

    col=0;
    if true {
let col:i32=0;

      var l0__u104:i32=0;
      if true {
let l0__u104:i32=0;

        let e__u105=v*4+l0__u104*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u105*4) as u64),mulh2(ldu(vkqregs+((e__u105*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u104:i32=1;

        let e__u106=v*4+l0__u104*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u106*4) as u64),mulh2(ldu(vkqregs+((e__u106*4) as u64)),sh2[(0 as i32)]));

}
l0__u104=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u107:i32=0;
      if true {
let l0__u107:i32=0;

        let e__u108=v*4+l0__u107*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u108*4) as u64),mulh2(ldu(vkqregs+((e__u108*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u107:i32=1;

        let e__u109=v*4+l0__u107*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u109*4) as u64),mulh2(ldu(vkqregs+((e__u109*4) as u64)),sh2[(1 as i32)]));

}
l0__u107=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=7;

    col=0;
    if true {
let col:i32=0;

      var l0__u111:i32=0;
      if true {
let l0__u111:i32=0;

        let e__u112=v*4+l0__u111*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u112*4) as u64),mulh2(ldu(vkqregs+((e__u112*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u111:i32=1;

        let e__u113=v*4+l0__u111*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u113*4) as u64),mulh2(ldu(vkqregs+((e__u113*4) as u64)),sh2[(0 as i32)]));

}
l0__u111=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u114:i32=0;
      if true {
let l0__u114:i32=0;

        let e__u115=v*4+l0__u114*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u115*4) as u64),mulh2(ldu(vkqregs+((e__u115*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u114:i32=1;

        let e__u116=v*4+l0__u114*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u116*4) as u64),mulh2(ldu(vkqregs+((e__u116*4) as u64)),sh2[(1 as i32)]));

}
l0__u114=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=8;

    col=0;
    if true {
let col:i32=0;

      var l0__u118:i32=0;
      if true {
let l0__u118:i32=0;

        let e__u119=v*4+l0__u118*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u119*4) as u64),mulh2(ldu(vkqregs+((e__u119*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u118:i32=1;

        let e__u120=v*4+l0__u118*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u120*4) as u64),mulh2(ldu(vkqregs+((e__u120*4) as u64)),sh2[(0 as i32)]));

}
l0__u118=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u121:i32=0;
      if true {
let l0__u121:i32=0;

        let e__u122=v*4+l0__u121*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u122*4) as u64),mulh2(ldu(vkqregs+((e__u122*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u121:i32=1;

        let e__u123=v*4+l0__u121*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u123*4) as u64),mulh2(ldu(vkqregs+((e__u123*4) as u64)),sh2[(1 as i32)]));

}
l0__u121=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=9;

    col=0;
    if true {
let col:i32=0;

      var l0__u125:i32=0;
      if true {
let l0__u125:i32=0;

        let e__u126=v*4+l0__u125*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u126*4) as u64),mulh2(ldu(vkqregs+((e__u126*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u125:i32=1;

        let e__u127=v*4+l0__u125*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u127*4) as u64),mulh2(ldu(vkqregs+((e__u127*4) as u64)),sh2[(0 as i32)]));

}
l0__u125=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u128:i32=0;
      if true {
let l0__u128:i32=0;

        let e__u129=v*4+l0__u128*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u129*4) as u64),mulh2(ldu(vkqregs+((e__u129*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u128:i32=1;

        let e__u130=v*4+l0__u128*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u130*4) as u64),mulh2(ldu(vkqregs+((e__u130*4) as u64)),sh2[(1 as i32)]));

}
l0__u128=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=10;

    col=0;
    if true {
let col:i32=0;

      var l0__u132:i32=0;
      if true {
let l0__u132:i32=0;

        let e__u133=v*4+l0__u132*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u133*4) as u64),mulh2(ldu(vkqregs+((e__u133*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u132:i32=1;

        let e__u134=v*4+l0__u132*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u134*4) as u64),mulh2(ldu(vkqregs+((e__u134*4) as u64)),sh2[(0 as i32)]));

}
l0__u132=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u135:i32=0;
      if true {
let l0__u135:i32=0;

        let e__u136=v*4+l0__u135*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u136*4) as u64),mulh2(ldu(vkqregs+((e__u136*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u135:i32=1;

        let e__u137=v*4+l0__u135*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u137*4) as u64),mulh2(ldu(vkqregs+((e__u137*4) as u64)),sh2[(1 as i32)]));

}
l0__u135=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=11;

    col=0;
    if true {
let col:i32=0;

      var l0__u139:i32=0;
      if true {
let l0__u139:i32=0;

        let e__u140=v*4+l0__u139*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u140*4) as u64),mulh2(ldu(vkqregs+((e__u140*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u139:i32=1;

        let e__u141=v*4+l0__u139*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u141*4) as u64),mulh2(ldu(vkqregs+((e__u141*4) as u64)),sh2[(0 as i32)]));

}
l0__u139=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u142:i32=0;
      if true {
let l0__u142:i32=0;

        let e__u143=v*4+l0__u142*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u143*4) as u64),mulh2(ldu(vkqregs+((e__u143*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u142:i32=1;

        let e__u144=v*4+l0__u142*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u144*4) as u64),mulh2(ldu(vkqregs+((e__u144*4) as u64)),sh2[(1 as i32)]));

}
l0__u142=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=12;

    col=0;
    if true {
let col:i32=0;

      var l0__u146:i32=0;
      if true {
let l0__u146:i32=0;

        let e__u147=v*4+l0__u146*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u147*4) as u64),mulh2(ldu(vkqregs+((e__u147*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u146:i32=1;

        let e__u148=v*4+l0__u146*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u148*4) as u64),mulh2(ldu(vkqregs+((e__u148*4) as u64)),sh2[(0 as i32)]));

}
l0__u146=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u149:i32=0;
      if true {
let l0__u149:i32=0;

        let e__u150=v*4+l0__u149*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u150*4) as u64),mulh2(ldu(vkqregs+((e__u150*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u149:i32=1;

        let e__u151=v*4+l0__u149*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u151*4) as u64),mulh2(ldu(vkqregs+((e__u151*4) as u64)),sh2[(1 as i32)]));

}
l0__u149=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=13;

    col=0;
    if true {
let col:i32=0;

      var l0__u153:i32=0;
      if true {
let l0__u153:i32=0;

        let e__u154=v*4+l0__u153*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u154*4) as u64),mulh2(ldu(vkqregs+((e__u154*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u153:i32=1;

        let e__u155=v*4+l0__u153*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u155*4) as u64),mulh2(ldu(vkqregs+((e__u155*4) as u64)),sh2[(0 as i32)]));

}
l0__u153=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u156:i32=0;
      if true {
let l0__u156:i32=0;

        let e__u157=v*4+l0__u156*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u157*4) as u64),mulh2(ldu(vkqregs+((e__u157*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u156:i32=1;

        let e__u158=v*4+l0__u156*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u158*4) as u64),mulh2(ldu(vkqregs+((e__u158*4) as u64)),sh2[(1 as i32)]));

}
l0__u156=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=14;

    col=0;
    if true {
let col:i32=0;

      var l0__u160:i32=0;
      if true {
let l0__u160:i32=0;

        let e__u161=v*4+l0__u160*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u161*4) as u64),mulh2(ldu(vkqregs+((e__u161*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u160:i32=1;

        let e__u162=v*4+l0__u160*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u162*4) as u64),mulh2(ldu(vkqregs+((e__u162*4) as u64)),sh2[(0 as i32)]));

}
l0__u160=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u163:i32=0;
      if true {
let l0__u163:i32=0;

        let e__u164=v*4+l0__u163*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u164*4) as u64),mulh2(ldu(vkqregs+((e__u164*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u163:i32=1;

        let e__u165=v*4+l0__u163*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u165*4) as u64),mulh2(ldu(vkqregs+((e__u165*4) as u64)),sh2[(1 as i32)]));

}
l0__u163=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=15;

    col=0;
    if true {
let col:i32=0;

      var l0__u167:i32=0;
      if true {
let l0__u167:i32=0;

        let e__u168=v*4+l0__u167*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u168*4) as u64),mulh2(ldu(vkqregs+((e__u168*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u167:i32=1;

        let e__u169=v*4+l0__u167*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u169*4) as u64),mulh2(ldu(vkqregs+((e__u169*4) as u64)),sh2[(0 as i32)]));

}
l0__u167=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u170:i32=0;
      if true {
let l0__u170:i32=0;

        let e__u171=v*4+l0__u170*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u171*4) as u64),mulh2(ldu(vkqregs+((e__u171*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u170:i32=1;

        let e__u172=v*4+l0__u170*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u172*4) as u64),mulh2(ldu(vkqregs+((e__u172*4) as u64)),sh2[(1 as i32)]));

}
l0__u170=(2 as i32);


}
col=(2 as i32);


}
v=(16 as i32);

  // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
  var b:u32[8]=default<u32[8]>();
  t=0;
  if true {
let t:i32=0;

    var l0__u173:i32=0;
    if true {
let l0__u173:i32=0;

      b[(0 as i32)]=h2(kqc[(0 as i32)],kqc[(1 as i32)]);

}
if true {
let l0__u173:i32=2;

      b[(1 as i32)]=h2(kqc[(2 as i32)],kqc[(3 as i32)]);

}
if true {
let l0__u173:i32=4;

      b[(2 as i32)]=h2(kqc[(4 as i32)],kqc[(5 as i32)]);

}
if true {
let l0__u173:i32=6;

      b[(3 as i32)]=h2(kqc[(6 as i32)],kqc[(7 as i32)]);

}
l0__u173=(8 as i32);


}
t=(1 as i32);

  // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
  //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
  cpwait();
  barrier();
  if !last {
    load_mask(mask_h,mt,stride_mask,kv_row0+32,query0,n_queries,ncols1,tx,ty);
    load_kv(k_h2,kt,stride_k,kv_row0+32,tx,ty);
  }
  // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
  //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
  v=0;
  if true {
let v:i32=0;

    var d0__u178:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u178:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u178:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u178:i32=0;

      let k0__u179=kk__u178*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u179=ldm4t_tile(ldm_addr(vt,2*k0__u179*128+v*8,tx));
      d0__u178=mma_f16(d0__u178,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u179.a,av__u179.c);
      d1__u178=mma_f16(d1__u178,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u179.b,av__u179.d);

}
kk__u178=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u178.a);
    stu(vkqregs+((v*16+4) as u64),d0__u178.b);
    stu(vkqregs+((v*16+8) as u64),d1__u178.a);
    stu(vkqregs+((v*16+12) as u64),d1__u178.b);

}
if true {
let v:i32=1;

    var d0__u180:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u180:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u180:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u180:i32=0;

      let k0__u181=kk__u180*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u181=ldm4t_tile(ldm_addr(vt,2*k0__u181*128+v*8,tx));
      d0__u180=mma_f16(d0__u180,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u181.a,av__u181.c);
      d1__u180=mma_f16(d1__u180,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u181.b,av__u181.d);

}
kk__u180=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u180.a);
    stu(vkqregs+((v*16+4) as u64),d0__u180.b);
    stu(vkqregs+((v*16+8) as u64),d1__u180.a);
    stu(vkqregs+((v*16+12) as u64),d1__u180.b);

}
if true {
let v:i32=2;

    var d0__u182:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u182:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u182:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u182:i32=0;

      let k0__u183=kk__u182*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u183=ldm4t_tile(ldm_addr(vt,2*k0__u183*128+v*8,tx));
      d0__u182=mma_f16(d0__u182,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u183.a,av__u183.c);
      d1__u182=mma_f16(d1__u182,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u183.b,av__u183.d);

}
kk__u182=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u182.a);
    stu(vkqregs+((v*16+4) as u64),d0__u182.b);
    stu(vkqregs+((v*16+8) as u64),d1__u182.a);
    stu(vkqregs+((v*16+12) as u64),d1__u182.b);

}
if true {
let v:i32=3;

    var d0__u184:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u184:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u184:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u184:i32=0;

      let k0__u185=kk__u184*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u185=ldm4t_tile(ldm_addr(vt,2*k0__u185*128+v*8,tx));
      d0__u184=mma_f16(d0__u184,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u185.a,av__u185.c);
      d1__u184=mma_f16(d1__u184,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u185.b,av__u185.d);

}
kk__u184=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u184.a);
    stu(vkqregs+((v*16+4) as u64),d0__u184.b);
    stu(vkqregs+((v*16+8) as u64),d1__u184.a);
    stu(vkqregs+((v*16+12) as u64),d1__u184.b);

}
if true {
let v:i32=4;

    var d0__u186:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u186:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u186:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u186:i32=0;

      let k0__u187=kk__u186*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u187=ldm4t_tile(ldm_addr(vt,2*k0__u187*128+v*8,tx));
      d0__u186=mma_f16(d0__u186,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u187.a,av__u187.c);
      d1__u186=mma_f16(d1__u186,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u187.b,av__u187.d);

}
kk__u186=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u186.a);
    stu(vkqregs+((v*16+4) as u64),d0__u186.b);
    stu(vkqregs+((v*16+8) as u64),d1__u186.a);
    stu(vkqregs+((v*16+12) as u64),d1__u186.b);

}
if true {
let v:i32=5;

    var d0__u188:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u188:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u188:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u188:i32=0;

      let k0__u189=kk__u188*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u189=ldm4t_tile(ldm_addr(vt,2*k0__u189*128+v*8,tx));
      d0__u188=mma_f16(d0__u188,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u189.a,av__u189.c);
      d1__u188=mma_f16(d1__u188,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u189.b,av__u189.d);

}
kk__u188=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u188.a);
    stu(vkqregs+((v*16+4) as u64),d0__u188.b);
    stu(vkqregs+((v*16+8) as u64),d1__u188.a);
    stu(vkqregs+((v*16+12) as u64),d1__u188.b);

}
if true {
let v:i32=6;

    var d0__u190:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u190:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u190:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u190:i32=0;

      let k0__u191=kk__u190*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u191=ldm4t_tile(ldm_addr(vt,2*k0__u191*128+v*8,tx));
      d0__u190=mma_f16(d0__u190,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u191.a,av__u191.c);
      d1__u190=mma_f16(d1__u190,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u191.b,av__u191.d);

}
kk__u190=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u190.a);
    stu(vkqregs+((v*16+4) as u64),d0__u190.b);
    stu(vkqregs+((v*16+8) as u64),d1__u190.a);
    stu(vkqregs+((v*16+12) as u64),d1__u190.b);

}
if true {
let v:i32=7;

    var d0__u192:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u192:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u192:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u192:i32=0;

      let k0__u193=kk__u192*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u193=ldm4t_tile(ldm_addr(vt,2*k0__u193*128+v*8,tx));
      d0__u192=mma_f16(d0__u192,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u193.a,av__u193.c);
      d1__u192=mma_f16(d1__u192,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u193.b,av__u193.d);

}
kk__u192=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u192.a);
    stu(vkqregs+((v*16+4) as u64),d0__u192.b);
    stu(vkqregs+((v*16+8) as u64),d1__u192.a);
    stu(vkqregs+((v*16+12) as u64),d1__u192.b);

}
if true {
let v:i32=8;

    var d0__u194:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u194:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u194:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u194:i32=0;

      let k0__u195=kk__u194*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u195=ldm4t_tile(ldm_addr(vt,2*k0__u195*128+v*8,tx));
      d0__u194=mma_f16(d0__u194,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u195.a,av__u195.c);
      d1__u194=mma_f16(d1__u194,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u195.b,av__u195.d);

}
kk__u194=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u194.a);
    stu(vkqregs+((v*16+4) as u64),d0__u194.b);
    stu(vkqregs+((v*16+8) as u64),d1__u194.a);
    stu(vkqregs+((v*16+12) as u64),d1__u194.b);

}
if true {
let v:i32=9;

    var d0__u196:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u196:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u196:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u196:i32=0;

      let k0__u197=kk__u196*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u197=ldm4t_tile(ldm_addr(vt,2*k0__u197*128+v*8,tx));
      d0__u196=mma_f16(d0__u196,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u197.a,av__u197.c);
      d1__u196=mma_f16(d1__u196,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u197.b,av__u197.d);

}
kk__u196=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u196.a);
    stu(vkqregs+((v*16+4) as u64),d0__u196.b);
    stu(vkqregs+((v*16+8) as u64),d1__u196.a);
    stu(vkqregs+((v*16+12) as u64),d1__u196.b);

}
if true {
let v:i32=10;

    var d0__u198:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u198:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u198:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u198:i32=0;

      let k0__u199=kk__u198*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u199=ldm4t_tile(ldm_addr(vt,2*k0__u199*128+v*8,tx));
      d0__u198=mma_f16(d0__u198,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u199.a,av__u199.c);
      d1__u198=mma_f16(d1__u198,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u199.b,av__u199.d);

}
kk__u198=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u198.a);
    stu(vkqregs+((v*16+4) as u64),d0__u198.b);
    stu(vkqregs+((v*16+8) as u64),d1__u198.a);
    stu(vkqregs+((v*16+12) as u64),d1__u198.b);

}
if true {
let v:i32=11;

    var d0__u200:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u200:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u200:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u200:i32=0;

      let k0__u201=kk__u200*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u201=ldm4t_tile(ldm_addr(vt,2*k0__u201*128+v*8,tx));
      d0__u200=mma_f16(d0__u200,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u201.a,av__u201.c);
      d1__u200=mma_f16(d1__u200,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u201.b,av__u201.d);

}
kk__u200=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u200.a);
    stu(vkqregs+((v*16+4) as u64),d0__u200.b);
    stu(vkqregs+((v*16+8) as u64),d1__u200.a);
    stu(vkqregs+((v*16+12) as u64),d1__u200.b);

}
if true {
let v:i32=12;

    var d0__u202:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u202:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u202:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u202:i32=0;

      let k0__u203=kk__u202*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u203=ldm4t_tile(ldm_addr(vt,2*k0__u203*128+v*8,tx));
      d0__u202=mma_f16(d0__u202,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u203.a,av__u203.c);
      d1__u202=mma_f16(d1__u202,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u203.b,av__u203.d);

}
kk__u202=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u202.a);
    stu(vkqregs+((v*16+4) as u64),d0__u202.b);
    stu(vkqregs+((v*16+8) as u64),d1__u202.a);
    stu(vkqregs+((v*16+12) as u64),d1__u202.b);

}
if true {
let v:i32=13;

    var d0__u204:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u204:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u204:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u204:i32=0;

      let k0__u205=kk__u204*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u205=ldm4t_tile(ldm_addr(vt,2*k0__u205*128+v*8,tx));
      d0__u204=mma_f16(d0__u204,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u205.a,av__u205.c);
      d1__u204=mma_f16(d1__u204,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u205.b,av__u205.d);

}
kk__u204=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u204.a);
    stu(vkqregs+((v*16+4) as u64),d0__u204.b);
    stu(vkqregs+((v*16+8) as u64),d1__u204.a);
    stu(vkqregs+((v*16+12) as u64),d1__u204.b);

}
if true {
let v:i32=14;

    var d0__u206:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u206:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u206:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u206:i32=0;

      let k0__u207=kk__u206*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u207=ldm4t_tile(ldm_addr(vt,2*k0__u207*128+v*8,tx));
      d0__u206=mma_f16(d0__u206,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u207.a,av__u207.c);
      d1__u206=mma_f16(d1__u206,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u207.b,av__u207.d);

}
kk__u206=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u206.a);
    stu(vkqregs+((v*16+4) as u64),d0__u206.b);
    stu(vkqregs+((v*16+8) as u64),d1__u206.a);
    stu(vkqregs+((v*16+12) as u64),d1__u206.b);

}
if true {
let v:i32=15;

    var d0__u208:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u208:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u208:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u208:i32=0;

      let k0__u209=kk__u208*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u209=ldm4t_tile(ldm_addr(vt,2*k0__u209*128+v*8,tx));
      d0__u208=mma_f16(d0__u208,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u209.a,av__u209.c);
      d1__u208=mma_f16(d1__u208,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u209.b,av__u209.d);

}
kk__u208=(1 as i32);

    stu(vkqregs+((v*16) as u64),d0__u208.a);
    stu(vkqregs+((v*16+4) as u64),d0__u208.b);
    stu(vkqregs+((v*16+8) as u64),d1__u208.a);
    stu(vkqregs+((v*16+12) as u64),d1__u208.b);

}
v=(16 as i32);

}
fn iterate_nc8_np1(k_h2:u64, v_h2:u64, mask_h:u64, stride_k:i64, stride_v:i64, stride_mask:i64, slope:f32, jt:i32, kb0:i32, n_queries:i64, qregs:u64, vkqregs:u64, kmax:u64, rowsums:u64, smem:u64, tx:i32, ty:i32, last:bool) {let ncols1:i32=8;let np:i32=1;

  let kt=smem;
  let vt=smem+(16384 as u64);
  let mt=smem+(32768 as u64);
  let kv_row0=(kb0 as i64)*32;               // k_VKQ_0 = kb0*nbatch_fa
  let query0=(jt as i64)*(ncols1 as i64);    // j0 for the mask loader
  let cb=(ty/np)*16;                          // warp Q-column block base (cols_per_warp=16)
  let rh=(ty%np)*16;                          // warp KV-row half base (np*T_A_KQ::I split)
  let nt=32/(np*16);                          // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
  let kqoff=(3.0 as f32)*(0.6931 as f32);                       // FATTN_KQ_MAX_OFFSET
  var t:i32=0;
  // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
  // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
  cpwait();
  barrier();
  load_kv(v_h2,vt,stride_v,kv_row0,tx,ty);
  // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
  var kqc:f32[16]=default<f32[16]>();        // up to 2 fresh zero tile<16,16,float> C tiles
  var q:i32=0;                                // Q_B tile index = k_KQ_0/8
  if true {
let q:i32=0;

    let q0__u1=ldu(qregs+((q*16) as u64));
    let q1__u1=ldu(qregs+((q*16+4) as u64));
    let q2__u1=ldu(qregs+((q*16+8) as u64));
    let q3__u1=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u2=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u2=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u1,q1__u1,q2__u1,q3__u1,ka__u2.a,ka__u2.c);
      kqc[(0 as i32)]=lo__u2.x;kqc[(1 as i32)]=lo__u2.y;kqc[(2 as i32)]=lo__u2.z;kqc[(3 as i32)]=lo__u2.w;
      let hi__u2=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u1,q1__u1,q2__u1,q3__u1,ka__u2.b,ka__u2.d);
      kqc[(4 as i32)]=hi__u2.x;kqc[(5 as i32)]=hi__u2.y;kqc[(6 as i32)]=hi__u2.z;kqc[(7 as i32)]=hi__u2.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u3=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u3=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u1,q1__u1,q2__u1,q3__u1,ka__u3.a,ka__u3.c);
      kqc[(8 as i32)]=lo__u3.x;kqc[(9 as i32)]=lo__u3.y;kqc[(10 as i32)]=lo__u3.z;kqc[(11 as i32)]=lo__u3.w;
      let hi__u3=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u1,q1__u1,q2__u1,q3__u1,ka__u3.b,ka__u3.d);
      kqc[(12 as i32)]=hi__u3.x;kqc[(13 as i32)]=hi__u3.y;kqc[(14 as i32)]=hi__u3.z;kqc[(15 as i32)]=hi__u3.w;

}
t=(2 as i32);


}
if true {
let q:i32=1;

    let q0__u4=ldu(qregs+((q*16) as u64));
    let q1__u4=ldu(qregs+((q*16+4) as u64));
    let q2__u4=ldu(qregs+((q*16+8) as u64));
    let q3__u4=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u5=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u5=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u4,q1__u4,q2__u4,q3__u4,ka__u5.a,ka__u5.c);
      kqc[(0 as i32)]=lo__u5.x;kqc[(1 as i32)]=lo__u5.y;kqc[(2 as i32)]=lo__u5.z;kqc[(3 as i32)]=lo__u5.w;
      let hi__u5=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u4,q1__u4,q2__u4,q3__u4,ka__u5.b,ka__u5.d);
      kqc[(4 as i32)]=hi__u5.x;kqc[(5 as i32)]=hi__u5.y;kqc[(6 as i32)]=hi__u5.z;kqc[(7 as i32)]=hi__u5.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u6=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u6=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u4,q1__u4,q2__u4,q3__u4,ka__u6.a,ka__u6.c);
      kqc[(8 as i32)]=lo__u6.x;kqc[(9 as i32)]=lo__u6.y;kqc[(10 as i32)]=lo__u6.z;kqc[(11 as i32)]=lo__u6.w;
      let hi__u6=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u4,q1__u4,q2__u4,q3__u4,ka__u6.b,ka__u6.d);
      kqc[(12 as i32)]=hi__u6.x;kqc[(13 as i32)]=hi__u6.y;kqc[(14 as i32)]=hi__u6.z;kqc[(15 as i32)]=hi__u6.w;

}
t=(2 as i32);


}
if true {
let q:i32=2;

    let q0__u7=ldu(qregs+((q*16) as u64));
    let q1__u7=ldu(qregs+((q*16+4) as u64));
    let q2__u7=ldu(qregs+((q*16+8) as u64));
    let q3__u7=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u8=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u8=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u7,q1__u7,q2__u7,q3__u7,ka__u8.a,ka__u8.c);
      kqc[(0 as i32)]=lo__u8.x;kqc[(1 as i32)]=lo__u8.y;kqc[(2 as i32)]=lo__u8.z;kqc[(3 as i32)]=lo__u8.w;
      let hi__u8=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u7,q1__u7,q2__u7,q3__u7,ka__u8.b,ka__u8.d);
      kqc[(4 as i32)]=hi__u8.x;kqc[(5 as i32)]=hi__u8.y;kqc[(6 as i32)]=hi__u8.z;kqc[(7 as i32)]=hi__u8.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u9=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u9=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u7,q1__u7,q2__u7,q3__u7,ka__u9.a,ka__u9.c);
      kqc[(8 as i32)]=lo__u9.x;kqc[(9 as i32)]=lo__u9.y;kqc[(10 as i32)]=lo__u9.z;kqc[(11 as i32)]=lo__u9.w;
      let hi__u9=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u7,q1__u7,q2__u7,q3__u7,ka__u9.b,ka__u9.d);
      kqc[(12 as i32)]=hi__u9.x;kqc[(13 as i32)]=hi__u9.y;kqc[(14 as i32)]=hi__u9.z;kqc[(15 as i32)]=hi__u9.w;

}
t=(2 as i32);


}
if true {
let q:i32=3;

    let q0__u10=ldu(qregs+((q*16) as u64));
    let q1__u10=ldu(qregs+((q*16+4) as u64));
    let q2__u10=ldu(qregs+((q*16+8) as u64));
    let q3__u10=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u11=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u11=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u10,q1__u10,q2__u10,q3__u10,ka__u11.a,ka__u11.c);
      kqc[(0 as i32)]=lo__u11.x;kqc[(1 as i32)]=lo__u11.y;kqc[(2 as i32)]=lo__u11.z;kqc[(3 as i32)]=lo__u11.w;
      let hi__u11=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u10,q1__u10,q2__u10,q3__u10,ka__u11.b,ka__u11.d);
      kqc[(4 as i32)]=hi__u11.x;kqc[(5 as i32)]=hi__u11.y;kqc[(6 as i32)]=hi__u11.z;kqc[(7 as i32)]=hi__u11.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u12=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u12=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u10,q1__u10,q2__u10,q3__u10,ka__u12.a,ka__u12.c);
      kqc[(8 as i32)]=lo__u12.x;kqc[(9 as i32)]=lo__u12.y;kqc[(10 as i32)]=lo__u12.z;kqc[(11 as i32)]=lo__u12.w;
      let hi__u12=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u10,q1__u10,q2__u10,q3__u10,ka__u12.b,ka__u12.d);
      kqc[(12 as i32)]=hi__u12.x;kqc[(13 as i32)]=hi__u12.y;kqc[(14 as i32)]=hi__u12.z;kqc[(15 as i32)]=hi__u12.w;

}
t=(2 as i32);


}
if true {
let q:i32=4;

    let q0__u13=ldu(qregs+((q*16) as u64));
    let q1__u13=ldu(qregs+((q*16+4) as u64));
    let q2__u13=ldu(qregs+((q*16+8) as u64));
    let q3__u13=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u14=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u14=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u13,q1__u13,q2__u13,q3__u13,ka__u14.a,ka__u14.c);
      kqc[(0 as i32)]=lo__u14.x;kqc[(1 as i32)]=lo__u14.y;kqc[(2 as i32)]=lo__u14.z;kqc[(3 as i32)]=lo__u14.w;
      let hi__u14=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u13,q1__u13,q2__u13,q3__u13,ka__u14.b,ka__u14.d);
      kqc[(4 as i32)]=hi__u14.x;kqc[(5 as i32)]=hi__u14.y;kqc[(6 as i32)]=hi__u14.z;kqc[(7 as i32)]=hi__u14.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u15=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u15=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u13,q1__u13,q2__u13,q3__u13,ka__u15.a,ka__u15.c);
      kqc[(8 as i32)]=lo__u15.x;kqc[(9 as i32)]=lo__u15.y;kqc[(10 as i32)]=lo__u15.z;kqc[(11 as i32)]=lo__u15.w;
      let hi__u15=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u13,q1__u13,q2__u13,q3__u13,ka__u15.b,ka__u15.d);
      kqc[(12 as i32)]=hi__u15.x;kqc[(13 as i32)]=hi__u15.y;kqc[(14 as i32)]=hi__u15.z;kqc[(15 as i32)]=hi__u15.w;

}
t=(2 as i32);


}
if true {
let q:i32=5;

    let q0__u16=ldu(qregs+((q*16) as u64));
    let q1__u16=ldu(qregs+((q*16+4) as u64));
    let q2__u16=ldu(qregs+((q*16+8) as u64));
    let q3__u16=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u17=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u17=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u16,q1__u16,q2__u16,q3__u16,ka__u17.a,ka__u17.c);
      kqc[(0 as i32)]=lo__u17.x;kqc[(1 as i32)]=lo__u17.y;kqc[(2 as i32)]=lo__u17.z;kqc[(3 as i32)]=lo__u17.w;
      let hi__u17=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u16,q1__u16,q2__u16,q3__u16,ka__u17.b,ka__u17.d);
      kqc[(4 as i32)]=hi__u17.x;kqc[(5 as i32)]=hi__u17.y;kqc[(6 as i32)]=hi__u17.z;kqc[(7 as i32)]=hi__u17.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u18=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u18=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u16,q1__u16,q2__u16,q3__u16,ka__u18.a,ka__u18.c);
      kqc[(8 as i32)]=lo__u18.x;kqc[(9 as i32)]=lo__u18.y;kqc[(10 as i32)]=lo__u18.z;kqc[(11 as i32)]=lo__u18.w;
      let hi__u18=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u16,q1__u16,q2__u16,q3__u16,ka__u18.b,ka__u18.d);
      kqc[(12 as i32)]=hi__u18.x;kqc[(13 as i32)]=hi__u18.y;kqc[(14 as i32)]=hi__u18.z;kqc[(15 as i32)]=hi__u18.w;

}
t=(2 as i32);


}
if true {
let q:i32=6;

    let q0__u19=ldu(qregs+((q*16) as u64));
    let q1__u19=ldu(qregs+((q*16+4) as u64));
    let q2__u19=ldu(qregs+((q*16+8) as u64));
    let q3__u19=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u20=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u20=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u19,q1__u19,q2__u19,q3__u19,ka__u20.a,ka__u20.c);
      kqc[(0 as i32)]=lo__u20.x;kqc[(1 as i32)]=lo__u20.y;kqc[(2 as i32)]=lo__u20.z;kqc[(3 as i32)]=lo__u20.w;
      let hi__u20=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u19,q1__u19,q2__u19,q3__u19,ka__u20.b,ka__u20.d);
      kqc[(4 as i32)]=hi__u20.x;kqc[(5 as i32)]=hi__u20.y;kqc[(6 as i32)]=hi__u20.z;kqc[(7 as i32)]=hi__u20.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u21=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u21=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u19,q1__u19,q2__u19,q3__u19,ka__u21.a,ka__u21.c);
      kqc[(8 as i32)]=lo__u21.x;kqc[(9 as i32)]=lo__u21.y;kqc[(10 as i32)]=lo__u21.z;kqc[(11 as i32)]=lo__u21.w;
      let hi__u21=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u19,q1__u19,q2__u19,q3__u19,ka__u21.b,ka__u21.d);
      kqc[(12 as i32)]=hi__u21.x;kqc[(13 as i32)]=hi__u21.y;kqc[(14 as i32)]=hi__u21.z;kqc[(15 as i32)]=hi__u21.w;

}
t=(2 as i32);


}
if true {
let q:i32=7;

    let q0__u22=ldu(qregs+((q*16) as u64));
    let q1__u22=ldu(qregs+((q*16+4) as u64));
    let q2__u22=ldu(qregs+((q*16+8) as u64));
    let q3__u22=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u23=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u23=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u22,q1__u22,q2__u22,q3__u22,ka__u23.a,ka__u23.c);
      kqc[(0 as i32)]=lo__u23.x;kqc[(1 as i32)]=lo__u23.y;kqc[(2 as i32)]=lo__u23.z;kqc[(3 as i32)]=lo__u23.w;
      let hi__u23=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u22,q1__u22,q2__u22,q3__u22,ka__u23.b,ka__u23.d);
      kqc[(4 as i32)]=hi__u23.x;kqc[(5 as i32)]=hi__u23.y;kqc[(6 as i32)]=hi__u23.z;kqc[(7 as i32)]=hi__u23.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u24=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u24=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u22,q1__u22,q2__u22,q3__u22,ka__u24.a,ka__u24.c);
      kqc[(8 as i32)]=lo__u24.x;kqc[(9 as i32)]=lo__u24.y;kqc[(10 as i32)]=lo__u24.z;kqc[(11 as i32)]=lo__u24.w;
      let hi__u24=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u22,q1__u22,q2__u22,q3__u22,ka__u24.b,ka__u24.d);
      kqc[(12 as i32)]=hi__u24.x;kqc[(13 as i32)]=hi__u24.y;kqc[(14 as i32)]=hi__u24.z;kqc[(15 as i32)]=hi__u24.w;

}
t=(2 as i32);


}
if true {
let q:i32=8;

    let q0__u25=ldu(qregs+((q*16) as u64));
    let q1__u25=ldu(qregs+((q*16+4) as u64));
    let q2__u25=ldu(qregs+((q*16+8) as u64));
    let q3__u25=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u26=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u26=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u25,q1__u25,q2__u25,q3__u25,ka__u26.a,ka__u26.c);
      kqc[(0 as i32)]=lo__u26.x;kqc[(1 as i32)]=lo__u26.y;kqc[(2 as i32)]=lo__u26.z;kqc[(3 as i32)]=lo__u26.w;
      let hi__u26=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u25,q1__u25,q2__u25,q3__u25,ka__u26.b,ka__u26.d);
      kqc[(4 as i32)]=hi__u26.x;kqc[(5 as i32)]=hi__u26.y;kqc[(6 as i32)]=hi__u26.z;kqc[(7 as i32)]=hi__u26.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u27=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u27=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u25,q1__u25,q2__u25,q3__u25,ka__u27.a,ka__u27.c);
      kqc[(8 as i32)]=lo__u27.x;kqc[(9 as i32)]=lo__u27.y;kqc[(10 as i32)]=lo__u27.z;kqc[(11 as i32)]=lo__u27.w;
      let hi__u27=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u25,q1__u25,q2__u25,q3__u25,ka__u27.b,ka__u27.d);
      kqc[(12 as i32)]=hi__u27.x;kqc[(13 as i32)]=hi__u27.y;kqc[(14 as i32)]=hi__u27.z;kqc[(15 as i32)]=hi__u27.w;

}
t=(2 as i32);


}
if true {
let q:i32=9;

    let q0__u28=ldu(qregs+((q*16) as u64));
    let q1__u28=ldu(qregs+((q*16+4) as u64));
    let q2__u28=ldu(qregs+((q*16+8) as u64));
    let q3__u28=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u29=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u29=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u28,q1__u28,q2__u28,q3__u28,ka__u29.a,ka__u29.c);
      kqc[(0 as i32)]=lo__u29.x;kqc[(1 as i32)]=lo__u29.y;kqc[(2 as i32)]=lo__u29.z;kqc[(3 as i32)]=lo__u29.w;
      let hi__u29=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u28,q1__u28,q2__u28,q3__u28,ka__u29.b,ka__u29.d);
      kqc[(4 as i32)]=hi__u29.x;kqc[(5 as i32)]=hi__u29.y;kqc[(6 as i32)]=hi__u29.z;kqc[(7 as i32)]=hi__u29.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u30=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u30=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u28,q1__u28,q2__u28,q3__u28,ka__u30.a,ka__u30.c);
      kqc[(8 as i32)]=lo__u30.x;kqc[(9 as i32)]=lo__u30.y;kqc[(10 as i32)]=lo__u30.z;kqc[(11 as i32)]=lo__u30.w;
      let hi__u30=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u28,q1__u28,q2__u28,q3__u28,ka__u30.b,ka__u30.d);
      kqc[(12 as i32)]=hi__u30.x;kqc[(13 as i32)]=hi__u30.y;kqc[(14 as i32)]=hi__u30.z;kqc[(15 as i32)]=hi__u30.w;

}
t=(2 as i32);


}
if true {
let q:i32=10;

    let q0__u31=ldu(qregs+((q*16) as u64));
    let q1__u31=ldu(qregs+((q*16+4) as u64));
    let q2__u31=ldu(qregs+((q*16+8) as u64));
    let q3__u31=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u32=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u32=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u31,q1__u31,q2__u31,q3__u31,ka__u32.a,ka__u32.c);
      kqc[(0 as i32)]=lo__u32.x;kqc[(1 as i32)]=lo__u32.y;kqc[(2 as i32)]=lo__u32.z;kqc[(3 as i32)]=lo__u32.w;
      let hi__u32=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u31,q1__u31,q2__u31,q3__u31,ka__u32.b,ka__u32.d);
      kqc[(4 as i32)]=hi__u32.x;kqc[(5 as i32)]=hi__u32.y;kqc[(6 as i32)]=hi__u32.z;kqc[(7 as i32)]=hi__u32.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u33=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u33=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u31,q1__u31,q2__u31,q3__u31,ka__u33.a,ka__u33.c);
      kqc[(8 as i32)]=lo__u33.x;kqc[(9 as i32)]=lo__u33.y;kqc[(10 as i32)]=lo__u33.z;kqc[(11 as i32)]=lo__u33.w;
      let hi__u33=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u31,q1__u31,q2__u31,q3__u31,ka__u33.b,ka__u33.d);
      kqc[(12 as i32)]=hi__u33.x;kqc[(13 as i32)]=hi__u33.y;kqc[(14 as i32)]=hi__u33.z;kqc[(15 as i32)]=hi__u33.w;

}
t=(2 as i32);


}
if true {
let q:i32=11;

    let q0__u34=ldu(qregs+((q*16) as u64));
    let q1__u34=ldu(qregs+((q*16+4) as u64));
    let q2__u34=ldu(qregs+((q*16+8) as u64));
    let q3__u34=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u35=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u35=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u34,q1__u34,q2__u34,q3__u34,ka__u35.a,ka__u35.c);
      kqc[(0 as i32)]=lo__u35.x;kqc[(1 as i32)]=lo__u35.y;kqc[(2 as i32)]=lo__u35.z;kqc[(3 as i32)]=lo__u35.w;
      let hi__u35=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u34,q1__u34,q2__u34,q3__u34,ka__u35.b,ka__u35.d);
      kqc[(4 as i32)]=hi__u35.x;kqc[(5 as i32)]=hi__u35.y;kqc[(6 as i32)]=hi__u35.z;kqc[(7 as i32)]=hi__u35.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u36=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u36=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u34,q1__u34,q2__u34,q3__u34,ka__u36.a,ka__u36.c);
      kqc[(8 as i32)]=lo__u36.x;kqc[(9 as i32)]=lo__u36.y;kqc[(10 as i32)]=lo__u36.z;kqc[(11 as i32)]=lo__u36.w;
      let hi__u36=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u34,q1__u34,q2__u34,q3__u34,ka__u36.b,ka__u36.d);
      kqc[(12 as i32)]=hi__u36.x;kqc[(13 as i32)]=hi__u36.y;kqc[(14 as i32)]=hi__u36.z;kqc[(15 as i32)]=hi__u36.w;

}
t=(2 as i32);


}
if true {
let q:i32=12;

    let q0__u37=ldu(qregs+((q*16) as u64));
    let q1__u37=ldu(qregs+((q*16+4) as u64));
    let q2__u37=ldu(qregs+((q*16+8) as u64));
    let q3__u37=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u38=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u38=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u37,q1__u37,q2__u37,q3__u37,ka__u38.a,ka__u38.c);
      kqc[(0 as i32)]=lo__u38.x;kqc[(1 as i32)]=lo__u38.y;kqc[(2 as i32)]=lo__u38.z;kqc[(3 as i32)]=lo__u38.w;
      let hi__u38=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u37,q1__u37,q2__u37,q3__u37,ka__u38.b,ka__u38.d);
      kqc[(4 as i32)]=hi__u38.x;kqc[(5 as i32)]=hi__u38.y;kqc[(6 as i32)]=hi__u38.z;kqc[(7 as i32)]=hi__u38.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u39=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u39=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u37,q1__u37,q2__u37,q3__u37,ka__u39.a,ka__u39.c);
      kqc[(8 as i32)]=lo__u39.x;kqc[(9 as i32)]=lo__u39.y;kqc[(10 as i32)]=lo__u39.z;kqc[(11 as i32)]=lo__u39.w;
      let hi__u39=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u37,q1__u37,q2__u37,q3__u37,ka__u39.b,ka__u39.d);
      kqc[(12 as i32)]=hi__u39.x;kqc[(13 as i32)]=hi__u39.y;kqc[(14 as i32)]=hi__u39.z;kqc[(15 as i32)]=hi__u39.w;

}
t=(2 as i32);


}
if true {
let q:i32=13;

    let q0__u40=ldu(qregs+((q*16) as u64));
    let q1__u40=ldu(qregs+((q*16+4) as u64));
    let q2__u40=ldu(qregs+((q*16+8) as u64));
    let q3__u40=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u41=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u41=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u40,q1__u40,q2__u40,q3__u40,ka__u41.a,ka__u41.c);
      kqc[(0 as i32)]=lo__u41.x;kqc[(1 as i32)]=lo__u41.y;kqc[(2 as i32)]=lo__u41.z;kqc[(3 as i32)]=lo__u41.w;
      let hi__u41=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u40,q1__u40,q2__u40,q3__u40,ka__u41.b,ka__u41.d);
      kqc[(4 as i32)]=hi__u41.x;kqc[(5 as i32)]=hi__u41.y;kqc[(6 as i32)]=hi__u41.z;kqc[(7 as i32)]=hi__u41.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u42=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u42=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u40,q1__u40,q2__u40,q3__u40,ka__u42.a,ka__u42.c);
      kqc[(8 as i32)]=lo__u42.x;kqc[(9 as i32)]=lo__u42.y;kqc[(10 as i32)]=lo__u42.z;kqc[(11 as i32)]=lo__u42.w;
      let hi__u42=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u40,q1__u40,q2__u40,q3__u40,ka__u42.b,ka__u42.d);
      kqc[(12 as i32)]=hi__u42.x;kqc[(13 as i32)]=hi__u42.y;kqc[(14 as i32)]=hi__u42.z;kqc[(15 as i32)]=hi__u42.w;

}
t=(2 as i32);


}
if true {
let q:i32=14;

    let q0__u43=ldu(qregs+((q*16) as u64));
    let q1__u43=ldu(qregs+((q*16+4) as u64));
    let q2__u43=ldu(qregs+((q*16+8) as u64));
    let q3__u43=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u44=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u44=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u43,q1__u43,q2__u43,q3__u43,ka__u44.a,ka__u44.c);
      kqc[(0 as i32)]=lo__u44.x;kqc[(1 as i32)]=lo__u44.y;kqc[(2 as i32)]=lo__u44.z;kqc[(3 as i32)]=lo__u44.w;
      let hi__u44=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u43,q1__u43,q2__u43,q3__u43,ka__u44.b,ka__u44.d);
      kqc[(4 as i32)]=hi__u44.x;kqc[(5 as i32)]=hi__u44.y;kqc[(6 as i32)]=hi__u44.z;kqc[(7 as i32)]=hi__u44.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u45=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u45=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u43,q1__u43,q2__u43,q3__u43,ka__u45.a,ka__u45.c);
      kqc[(8 as i32)]=lo__u45.x;kqc[(9 as i32)]=lo__u45.y;kqc[(10 as i32)]=lo__u45.z;kqc[(11 as i32)]=lo__u45.w;
      let hi__u45=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u43,q1__u43,q2__u43,q3__u43,ka__u45.b,ka__u45.d);
      kqc[(12 as i32)]=hi__u45.x;kqc[(13 as i32)]=hi__u45.y;kqc[(14 as i32)]=hi__u45.z;kqc[(15 as i32)]=hi__u45.w;

}
t=(2 as i32);


}
if true {
let q:i32=15;

    let q0__u46=ldu(qregs+((q*16) as u64));
    let q1__u46=ldu(qregs+((q*16+4) as u64));
    let q2__u46=ldu(qregs+((q*16+8) as u64));
    let q3__u46=ldu(qregs+((q*16+12) as u64));
    t=0;
    if true {
let t:i32=0;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u47=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u47=mma_f32(F4{kqc[(0 as i32)],kqc[(1 as i32)],kqc[(2 as i32)],kqc[(3 as i32)]},q0__u46,q1__u46,q2__u46,q3__u46,ka__u47.a,ka__u47.c);
      kqc[(0 as i32)]=lo__u47.x;kqc[(1 as i32)]=lo__u47.y;kqc[(2 as i32)]=lo__u47.z;kqc[(3 as i32)]=lo__u47.w;
      let hi__u47=mma_f32(F4{kqc[(4 as i32)],kqc[(5 as i32)],kqc[(6 as i32)],kqc[(7 as i32)]},q0__u46,q1__u46,q2__u46,q3__u46,ka__u47.b,ka__u47.d);
      kqc[(4 as i32)]=hi__u47.x;kqc[(5 as i32)]=hi__u47.y;kqc[(6 as i32)]=hi__u47.z;kqc[(7 as i32)]=hi__u47.w;

}
if true {
let t:i32=1;

      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka__u48=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo__u48=mma_f32(F4{kqc[(8 as i32)],kqc[(9 as i32)],kqc[(10 as i32)],kqc[(11 as i32)]},q0__u46,q1__u46,q2__u46,q3__u46,ka__u48.a,ka__u48.c);
      kqc[(8 as i32)]=lo__u48.x;kqc[(9 as i32)]=lo__u48.y;kqc[(10 as i32)]=lo__u48.z;kqc[(11 as i32)]=lo__u48.w;
      let hi__u48=mma_f32(F4{kqc[(12 as i32)],kqc[(13 as i32)],kqc[(14 as i32)],kqc[(15 as i32)]},q0__u46,q1__u46,q2__u46,q3__u46,ka__u48.b,ka__u48.d);
      kqc[(12 as i32)]=hi__u48.x;kqc[(13 as i32)]=hi__u48.y;kqc[(14 as i32)]=hi__u48.z;kqc[(15 as i32)]=hi__u48.w;

}
t=(2 as i32);


}
q=(16 as i32);

  // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
  var kqold:f32[2]=default<f32[2]>();        // KQ_max[cols_per_thread=2]
  var kqmn:f32[2]=default<f32[2]>();         // KQ_max_new
  var rsa:f32[2]=default<f32[2]>();          // KQ_rowsum_add
  kqold[(0 as i32)]=frombits(ldu(kmax));
  kqold[(1 as i32)]=frombits(ldu(kmax+(4 as u64)));
  kqmn[(0 as i32)]=kqold[(0 as i32)];
  kqmn[(1 as i32)]=kqold[(1 as i32)];
  t=0;
  if true {
let t:i32=0;

    let rb__u49=t*np*16+rh;
    var l0__u49:i32=0;
    if true {
let l0__u49:i32=0;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u50=((cb+gi_c(l0__u49,tx))/8)*20+((rb__u49+gj_c(l0__u49,tx))/2);
      let w__u50=ldu(mt+((mh2__u50 as u64)*(4 as u64)));
      kqc[(0 as i32)]=fmaf(slope,hlo(w__u50),kqc[(0 as i32)]);
      kqc[(1 as i32)]=fmaf(slope,hhi(w__u50),kqc[(1 as i32)]);

}
if true {
let l0__u49:i32=2;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u51=((cb+gi_c(l0__u49,tx))/8)*20+((rb__u49+gj_c(l0__u49,tx))/2);
      let w__u51=ldu(mt+((mh2__u51 as u64)*(4 as u64)));
      kqc[(2 as i32)]=fmaf(slope,hlo(w__u51),kqc[(2 as i32)]);
      kqc[(3 as i32)]=fmaf(slope,hhi(w__u51),kqc[(3 as i32)]);

}
if true {
let l0__u49:i32=4;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u52=((cb+gi_c(l0__u49,tx))/8)*20+((rb__u49+gj_c(l0__u49,tx))/2);
      let w__u52=ldu(mt+((mh2__u52 as u64)*(4 as u64)));
      kqc[(4 as i32)]=fmaf(slope,hlo(w__u52),kqc[(4 as i32)]);
      kqc[(5 as i32)]=fmaf(slope,hhi(w__u52),kqc[(5 as i32)]);

}
if true {
let l0__u49:i32=6;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u53=((cb+gi_c(l0__u49,tx))/8)*20+((rb__u49+gj_c(l0__u49,tx))/2);
      let w__u53=ldu(mt+((mh2__u53 as u64)*(4 as u64)));
      kqc[(6 as i32)]=fmaf(slope,hlo(w__u53),kqc[(6 as i32)]);
      kqc[(7 as i32)]=fmaf(slope,hhi(w__u53),kqc[(7 as i32)]);

}
l0__u49=(8 as i32);

    var l__u49:i32=0;
    if true {
let l__u49:i32=0;

      let idx__u54=(l__u49/2)%2;
      let mv__u54=kqc[(0 as i32)]+kqoff;
      if mv__u54>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u54;}

}
if true {
let l__u49:i32=1;

      let idx__u55=(l__u49/2)%2;
      let mv__u55=kqc[(1 as i32)]+kqoff;
      if mv__u55>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u55;}

}
if true {
let l__u49:i32=2;

      let idx__u56=(l__u49/2)%2;
      let mv__u56=kqc[(2 as i32)]+kqoff;
      if mv__u56>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u56;}

}
if true {
let l__u49:i32=3;

      let idx__u57=(l__u49/2)%2;
      let mv__u57=kqc[(3 as i32)]+kqoff;
      if mv__u57>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u57;}

}
if true {
let l__u49:i32=4;

      let idx__u58=(l__u49/2)%2;
      let mv__u58=kqc[(4 as i32)]+kqoff;
      if mv__u58>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u58;}

}
if true {
let l__u49:i32=5;

      let idx__u59=(l__u49/2)%2;
      let mv__u59=kqc[(5 as i32)]+kqoff;
      if mv__u59>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u59;}

}
if true {
let l__u49:i32=6;

      let idx__u60=(l__u49/2)%2;
      let mv__u60=kqc[(6 as i32)]+kqoff;
      if mv__u60>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u60;}

}
if true {
let l__u49:i32=7;

      let idx__u61=(l__u49/2)%2;
      let mv__u61=kqc[(7 as i32)]+kqoff;
      if mv__u61>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u61;}

}
l__u49=(8 as i32);


}
if true {
let t:i32=1;

    let rb__u62=t*np*16+rh;
    var l0__u62:i32=0;
    if true {
let l0__u62:i32=0;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u63=((cb+gi_c(l0__u62,tx))/8)*20+((rb__u62+gj_c(l0__u62,tx))/2);
      let w__u63=ldu(mt+((mh2__u63 as u64)*(4 as u64)));
      kqc[(8 as i32)]=fmaf(slope,hlo(w__u63),kqc[(8 as i32)]);
      kqc[(9 as i32)]=fmaf(slope,hhi(w__u63),kqc[(9 as i32)]);

}
if true {
let l0__u62:i32=2;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u64=((cb+gi_c(l0__u62,tx))/8)*20+((rb__u62+gj_c(l0__u62,tx))/2);
      let w__u64=ldu(mt+((mh2__u64 as u64)*(4 as u64)));
      kqc[(10 as i32)]=fmaf(slope,hlo(w__u64),kqc[(10 as i32)]);
      kqc[(11 as i32)]=fmaf(slope,hhi(w__u64),kqc[(11 as i32)]);

}
if true {
let l0__u62:i32=4;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u65=((cb+gi_c(l0__u62,tx))/8)*20+((rb__u62+gj_c(l0__u62,tx))/2);
      let w__u65=ldu(mt+((mh2__u65 as u64)*(4 as u64)));
      kqc[(12 as i32)]=fmaf(slope,hlo(w__u65),kqc[(12 as i32)]);
      kqc[(13 as i32)]=fmaf(slope,hhi(w__u65),kqc[(13 as i32)]);

}
if true {
let l0__u62:i32=6;
                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2__u66=((cb+gi_c(l0__u62,tx))/8)*20+((rb__u62+gj_c(l0__u62,tx))/2);
      let w__u66=ldu(mt+((mh2__u66 as u64)*(4 as u64)));
      kqc[(14 as i32)]=fmaf(slope,hlo(w__u66),kqc[(14 as i32)]);
      kqc[(15 as i32)]=fmaf(slope,hhi(w__u66),kqc[(15 as i32)]);

}
l0__u62=(8 as i32);

    var l__u62:i32=0;
    if true {
let l__u62:i32=0;

      let idx__u67=(l__u62/2)%2;
      let mv__u67=kqc[(8 as i32)]+kqoff;
      if mv__u67>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u67;}

}
if true {
let l__u62:i32=1;

      let idx__u68=(l__u62/2)%2;
      let mv__u68=kqc[(9 as i32)]+kqoff;
      if mv__u68>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u68;}

}
if true {
let l__u62:i32=2;

      let idx__u69=(l__u62/2)%2;
      let mv__u69=kqc[(10 as i32)]+kqoff;
      if mv__u69>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u69;}

}
if true {
let l__u62:i32=3;

      let idx__u70=(l__u62/2)%2;
      let mv__u70=kqc[(11 as i32)]+kqoff;
      if mv__u70>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u70;}

}
if true {
let l__u62:i32=4;

      let idx__u71=(l__u62/2)%2;
      let mv__u71=kqc[(12 as i32)]+kqoff;
      if mv__u71>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u71;}

}
if true {
let l__u62:i32=5;

      let idx__u72=(l__u62/2)%2;
      let mv__u72=kqc[(13 as i32)]+kqoff;
      if mv__u72>kqmn[(0 as i32)] {kqmn[(0 as i32)]=mv__u72;}

}
if true {
let l__u62:i32=6;

      let idx__u73=(l__u62/2)%2;
      let mv__u73=kqc[(14 as i32)]+kqoff;
      if mv__u73>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u73;}

}
if true {
let l__u62:i32=7;

      let idx__u74=(l__u62/2)%2;
      let mv__u74=kqc[(15 as i32)]+kqoff;
      if mv__u74>kqmn[(1 as i32)] {kqmn[(1 as i32)]=mv__u74;}

}
l__u62=(8 as i32);


}
t=(2 as i32);

  // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
  var col:i32=0;
  if true {
let col:i32=0;

    var s__u75=shflx(kqmn[(0 as i32)],2);
    if s__u75>kqmn[(0 as i32)] {kqmn[(0 as i32)]=s__u75;}
    s__u75=shflx(kqmn[(0 as i32)],1);
    if s__u75>kqmn[(0 as i32)] {kqmn[(0 as i32)]=s__u75;}

}
if true {
let col:i32=1;

    var s__u76=shflx(kqmn[(1 as i32)],2);
    if s__u76>kqmn[(1 as i32)] {kqmn[(1 as i32)]=s__u76;}
    s__u76=shflx(kqmn[(1 as i32)],1);
    if s__u76>kqmn[(1 as i32)] {kqmn[(1 as i32)]=s__u76;}

}
col=(2 as i32);

  // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
  t=0;
  if true {
let t:i32=0;

    var l__u77:i32=0;
    if true {
let l__u77:i32=0;

      let idx__u78=(l__u77/2)%2;
      let e__u78=expf(kqc[(0 as i32)]-kqmn[(0 as i32)]);
      kqc[(0 as i32)]=e__u78;
      rsa[(0 as i32)]+=e__u78;

}
if true {
let l__u77:i32=1;

      let idx__u79=(l__u77/2)%2;
      let e__u79=expf(kqc[(1 as i32)]-kqmn[(0 as i32)]);
      kqc[(1 as i32)]=e__u79;
      rsa[(0 as i32)]+=e__u79;

}
if true {
let l__u77:i32=2;

      let idx__u80=(l__u77/2)%2;
      let e__u80=expf(kqc[(2 as i32)]-kqmn[(1 as i32)]);
      kqc[(2 as i32)]=e__u80;
      rsa[(1 as i32)]+=e__u80;

}
if true {
let l__u77:i32=3;

      let idx__u81=(l__u77/2)%2;
      let e__u81=expf(kqc[(3 as i32)]-kqmn[(1 as i32)]);
      kqc[(3 as i32)]=e__u81;
      rsa[(1 as i32)]+=e__u81;

}
if true {
let l__u77:i32=4;

      let idx__u82=(l__u77/2)%2;
      let e__u82=expf(kqc[(4 as i32)]-kqmn[(0 as i32)]);
      kqc[(4 as i32)]=e__u82;
      rsa[(0 as i32)]+=e__u82;

}
if true {
let l__u77:i32=5;

      let idx__u83=(l__u77/2)%2;
      let e__u83=expf(kqc[(5 as i32)]-kqmn[(0 as i32)]);
      kqc[(5 as i32)]=e__u83;
      rsa[(0 as i32)]+=e__u83;

}
if true {
let l__u77:i32=6;

      let idx__u84=(l__u77/2)%2;
      let e__u84=expf(kqc[(6 as i32)]-kqmn[(1 as i32)]);
      kqc[(6 as i32)]=e__u84;
      rsa[(1 as i32)]+=e__u84;

}
if true {
let l__u77:i32=7;

      let idx__u85=(l__u77/2)%2;
      let e__u85=expf(kqc[(7 as i32)]-kqmn[(1 as i32)]);
      kqc[(7 as i32)]=e__u85;
      rsa[(1 as i32)]+=e__u85;

}
l__u77=(8 as i32);


}
if true {
let t:i32=1;

    var l__u86:i32=0;
    if true {
let l__u86:i32=0;

      let idx__u87=(l__u86/2)%2;
      let e__u87=expf(kqc[(8 as i32)]-kqmn[(0 as i32)]);
      kqc[(8 as i32)]=e__u87;
      rsa[(0 as i32)]+=e__u87;

}
if true {
let l__u86:i32=1;

      let idx__u88=(l__u86/2)%2;
      let e__u88=expf(kqc[(9 as i32)]-kqmn[(0 as i32)]);
      kqc[(9 as i32)]=e__u88;
      rsa[(0 as i32)]+=e__u88;

}
if true {
let l__u86:i32=2;

      let idx__u89=(l__u86/2)%2;
      let e__u89=expf(kqc[(10 as i32)]-kqmn[(1 as i32)]);
      kqc[(10 as i32)]=e__u89;
      rsa[(1 as i32)]+=e__u89;

}
if true {
let l__u86:i32=3;

      let idx__u90=(l__u86/2)%2;
      let e__u90=expf(kqc[(11 as i32)]-kqmn[(1 as i32)]);
      kqc[(11 as i32)]=e__u90;
      rsa[(1 as i32)]+=e__u90;

}
if true {
let l__u86:i32=4;

      let idx__u91=(l__u86/2)%2;
      let e__u91=expf(kqc[(12 as i32)]-kqmn[(0 as i32)]);
      kqc[(12 as i32)]=e__u91;
      rsa[(0 as i32)]+=e__u91;

}
if true {
let l__u86:i32=5;

      let idx__u92=(l__u86/2)%2;
      let e__u92=expf(kqc[(13 as i32)]-kqmn[(0 as i32)]);
      kqc[(13 as i32)]=e__u92;
      rsa[(0 as i32)]+=e__u92;

}
if true {
let l__u86:i32=6;

      let idx__u93=(l__u86/2)%2;
      let e__u93=expf(kqc[(14 as i32)]-kqmn[(1 as i32)]);
      kqc[(14 as i32)]=e__u93;
      rsa[(1 as i32)]+=e__u93;

}
if true {
let l__u86:i32=7;

      let idx__u94=(l__u86/2)%2;
      let e__u94=expf(kqc[(15 as i32)]-kqmn[(1 as i32)]);
      kqc[(15 as i32)]=e__u94;
      rsa[(1 as i32)]+=e__u94;

}
l__u86=(8 as i32);


}
t=(2 as i32);

  // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
  //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
  var scl:f32[2]=default<f32[2]>();
  col=0;
  if true {
let col:i32=0;

    let diff__u95=kqold[(0 as i32)]-kqmn[(0 as i32)];
    let s0__u95=expf(diff__u95);
    let keep__u95:u32=if diff__u95>=-20.0 {1 as u32} else {0 as u32};
    scl[(0 as i32)]=frombits(bits(s0__u95)*keep__u95);    // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
    stu(kmax+((col*4) as u64),bits(kqmn[(0 as i32)]));
    let rs__u95=frombits(ldu(rowsums+((col*4) as u64)));
    stu(rowsums+((col*4) as u64),bits(fmaf(scl[(0 as i32)],rs__u95,rsa[(0 as i32)])));

}
if true {
let col:i32=1;

    let diff__u96=kqold[(1 as i32)]-kqmn[(1 as i32)];
    let s0__u96=expf(diff__u96);
    let keep__u96:u32=if diff__u96>=-20.0 {1 as u32} else {0 as u32};
    scl[(1 as i32)]=frombits(bits(s0__u96)*keep__u96);    // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
    stu(kmax+((col*4) as u64),bits(kqmn[(1 as i32)]));
    let rs__u96=frombits(ldu(rowsums+((col*4) as u64)));
    stu(rowsums+((col*4) as u64),bits(fmaf(scl[(1 as i32)],rs__u96,rsa[(1 as i32)])));

}
col=(2 as i32);

  // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
  var sh2:u32[2]=default<u32[2]>();
  col=0;
  if true {
let col:i32=0;
 sh2[(0 as i32)]=h2(scl[(0 as i32)],scl[(0 as i32)]);
}
if true {
let col:i32=1;
 sh2[(1 as i32)]=h2(scl[(1 as i32)],scl[(1 as i32)]);
}
col=(2 as i32);

  var v:i32=0;
  if true {
let v:i32=0;

    col=0;
    if true {
let col:i32=0;

      var l0__u100:i32=0;
      if true {
let l0__u100:i32=0;

        let e__u101=v*4+l0__u100*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u101*4) as u64),mulh2(ldu(vkqregs+((e__u101*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u100:i32=1;

        let e__u102=v*4+l0__u100*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u102*4) as u64),mulh2(ldu(vkqregs+((e__u102*4) as u64)),sh2[(0 as i32)]));

}
l0__u100=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u103:i32=0;
      if true {
let l0__u103:i32=0;

        let e__u104=v*4+l0__u103*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u104*4) as u64),mulh2(ldu(vkqregs+((e__u104*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u103:i32=1;

        let e__u105=v*4+l0__u103*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u105*4) as u64),mulh2(ldu(vkqregs+((e__u105*4) as u64)),sh2[(1 as i32)]));

}
l0__u103=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=1;

    col=0;
    if true {
let col:i32=0;

      var l0__u107:i32=0;
      if true {
let l0__u107:i32=0;

        let e__u108=v*4+l0__u107*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u108*4) as u64),mulh2(ldu(vkqregs+((e__u108*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u107:i32=1;

        let e__u109=v*4+l0__u107*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u109*4) as u64),mulh2(ldu(vkqregs+((e__u109*4) as u64)),sh2[(0 as i32)]));

}
l0__u107=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u110:i32=0;
      if true {
let l0__u110:i32=0;

        let e__u111=v*4+l0__u110*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u111*4) as u64),mulh2(ldu(vkqregs+((e__u111*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u110:i32=1;

        let e__u112=v*4+l0__u110*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u112*4) as u64),mulh2(ldu(vkqregs+((e__u112*4) as u64)),sh2[(1 as i32)]));

}
l0__u110=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=2;

    col=0;
    if true {
let col:i32=0;

      var l0__u114:i32=0;
      if true {
let l0__u114:i32=0;

        let e__u115=v*4+l0__u114*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u115*4) as u64),mulh2(ldu(vkqregs+((e__u115*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u114:i32=1;

        let e__u116=v*4+l0__u114*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u116*4) as u64),mulh2(ldu(vkqregs+((e__u116*4) as u64)),sh2[(0 as i32)]));

}
l0__u114=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u117:i32=0;
      if true {
let l0__u117:i32=0;

        let e__u118=v*4+l0__u117*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u118*4) as u64),mulh2(ldu(vkqregs+((e__u118*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u117:i32=1;

        let e__u119=v*4+l0__u117*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u119*4) as u64),mulh2(ldu(vkqregs+((e__u119*4) as u64)),sh2[(1 as i32)]));

}
l0__u117=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=3;

    col=0;
    if true {
let col:i32=0;

      var l0__u121:i32=0;
      if true {
let l0__u121:i32=0;

        let e__u122=v*4+l0__u121*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u122*4) as u64),mulh2(ldu(vkqregs+((e__u122*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u121:i32=1;

        let e__u123=v*4+l0__u121*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u123*4) as u64),mulh2(ldu(vkqregs+((e__u123*4) as u64)),sh2[(0 as i32)]));

}
l0__u121=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u124:i32=0;
      if true {
let l0__u124:i32=0;

        let e__u125=v*4+l0__u124*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u125*4) as u64),mulh2(ldu(vkqregs+((e__u125*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u124:i32=1;

        let e__u126=v*4+l0__u124*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u126*4) as u64),mulh2(ldu(vkqregs+((e__u126*4) as u64)),sh2[(1 as i32)]));

}
l0__u124=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=4;

    col=0;
    if true {
let col:i32=0;

      var l0__u128:i32=0;
      if true {
let l0__u128:i32=0;

        let e__u129=v*4+l0__u128*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u129*4) as u64),mulh2(ldu(vkqregs+((e__u129*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u128:i32=1;

        let e__u130=v*4+l0__u128*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u130*4) as u64),mulh2(ldu(vkqregs+((e__u130*4) as u64)),sh2[(0 as i32)]));

}
l0__u128=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u131:i32=0;
      if true {
let l0__u131:i32=0;

        let e__u132=v*4+l0__u131*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u132*4) as u64),mulh2(ldu(vkqregs+((e__u132*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u131:i32=1;

        let e__u133=v*4+l0__u131*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u133*4) as u64),mulh2(ldu(vkqregs+((e__u133*4) as u64)),sh2[(1 as i32)]));

}
l0__u131=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=5;

    col=0;
    if true {
let col:i32=0;

      var l0__u135:i32=0;
      if true {
let l0__u135:i32=0;

        let e__u136=v*4+l0__u135*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u136*4) as u64),mulh2(ldu(vkqregs+((e__u136*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u135:i32=1;

        let e__u137=v*4+l0__u135*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u137*4) as u64),mulh2(ldu(vkqregs+((e__u137*4) as u64)),sh2[(0 as i32)]));

}
l0__u135=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u138:i32=0;
      if true {
let l0__u138:i32=0;

        let e__u139=v*4+l0__u138*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u139*4) as u64),mulh2(ldu(vkqregs+((e__u139*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u138:i32=1;

        let e__u140=v*4+l0__u138*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u140*4) as u64),mulh2(ldu(vkqregs+((e__u140*4) as u64)),sh2[(1 as i32)]));

}
l0__u138=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=6;

    col=0;
    if true {
let col:i32=0;

      var l0__u142:i32=0;
      if true {
let l0__u142:i32=0;

        let e__u143=v*4+l0__u142*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u143*4) as u64),mulh2(ldu(vkqregs+((e__u143*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u142:i32=1;

        let e__u144=v*4+l0__u142*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u144*4) as u64),mulh2(ldu(vkqregs+((e__u144*4) as u64)),sh2[(0 as i32)]));

}
l0__u142=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u145:i32=0;
      if true {
let l0__u145:i32=0;

        let e__u146=v*4+l0__u145*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u146*4) as u64),mulh2(ldu(vkqregs+((e__u146*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u145:i32=1;

        let e__u147=v*4+l0__u145*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u147*4) as u64),mulh2(ldu(vkqregs+((e__u147*4) as u64)),sh2[(1 as i32)]));

}
l0__u145=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=7;

    col=0;
    if true {
let col:i32=0;

      var l0__u149:i32=0;
      if true {
let l0__u149:i32=0;

        let e__u150=v*4+l0__u149*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u150*4) as u64),mulh2(ldu(vkqregs+((e__u150*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u149:i32=1;

        let e__u151=v*4+l0__u149*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u151*4) as u64),mulh2(ldu(vkqregs+((e__u151*4) as u64)),sh2[(0 as i32)]));

}
l0__u149=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u152:i32=0;
      if true {
let l0__u152:i32=0;

        let e__u153=v*4+l0__u152*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u153*4) as u64),mulh2(ldu(vkqregs+((e__u153*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u152:i32=1;

        let e__u154=v*4+l0__u152*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u154*4) as u64),mulh2(ldu(vkqregs+((e__u154*4) as u64)),sh2[(1 as i32)]));

}
l0__u152=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=8;

    col=0;
    if true {
let col:i32=0;

      var l0__u156:i32=0;
      if true {
let l0__u156:i32=0;

        let e__u157=v*4+l0__u156*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u157*4) as u64),mulh2(ldu(vkqregs+((e__u157*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u156:i32=1;

        let e__u158=v*4+l0__u156*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u158*4) as u64),mulh2(ldu(vkqregs+((e__u158*4) as u64)),sh2[(0 as i32)]));

}
l0__u156=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u159:i32=0;
      if true {
let l0__u159:i32=0;

        let e__u160=v*4+l0__u159*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u160*4) as u64),mulh2(ldu(vkqregs+((e__u160*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u159:i32=1;

        let e__u161=v*4+l0__u159*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u161*4) as u64),mulh2(ldu(vkqregs+((e__u161*4) as u64)),sh2[(1 as i32)]));

}
l0__u159=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=9;

    col=0;
    if true {
let col:i32=0;

      var l0__u163:i32=0;
      if true {
let l0__u163:i32=0;

        let e__u164=v*4+l0__u163*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u164*4) as u64),mulh2(ldu(vkqregs+((e__u164*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u163:i32=1;

        let e__u165=v*4+l0__u163*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u165*4) as u64),mulh2(ldu(vkqregs+((e__u165*4) as u64)),sh2[(0 as i32)]));

}
l0__u163=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u166:i32=0;
      if true {
let l0__u166:i32=0;

        let e__u167=v*4+l0__u166*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u167*4) as u64),mulh2(ldu(vkqregs+((e__u167*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u166:i32=1;

        let e__u168=v*4+l0__u166*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u168*4) as u64),mulh2(ldu(vkqregs+((e__u168*4) as u64)),sh2[(1 as i32)]));

}
l0__u166=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=10;

    col=0;
    if true {
let col:i32=0;

      var l0__u170:i32=0;
      if true {
let l0__u170:i32=0;

        let e__u171=v*4+l0__u170*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u171*4) as u64),mulh2(ldu(vkqregs+((e__u171*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u170:i32=1;

        let e__u172=v*4+l0__u170*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u172*4) as u64),mulh2(ldu(vkqregs+((e__u172*4) as u64)),sh2[(0 as i32)]));

}
l0__u170=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u173:i32=0;
      if true {
let l0__u173:i32=0;

        let e__u174=v*4+l0__u173*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u174*4) as u64),mulh2(ldu(vkqregs+((e__u174*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u173:i32=1;

        let e__u175=v*4+l0__u173*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u175*4) as u64),mulh2(ldu(vkqregs+((e__u175*4) as u64)),sh2[(1 as i32)]));

}
l0__u173=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=11;

    col=0;
    if true {
let col:i32=0;

      var l0__u177:i32=0;
      if true {
let l0__u177:i32=0;

        let e__u178=v*4+l0__u177*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u178*4) as u64),mulh2(ldu(vkqregs+((e__u178*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u177:i32=1;

        let e__u179=v*4+l0__u177*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u179*4) as u64),mulh2(ldu(vkqregs+((e__u179*4) as u64)),sh2[(0 as i32)]));

}
l0__u177=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u180:i32=0;
      if true {
let l0__u180:i32=0;

        let e__u181=v*4+l0__u180*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u181*4) as u64),mulh2(ldu(vkqregs+((e__u181*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u180:i32=1;

        let e__u182=v*4+l0__u180*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u182*4) as u64),mulh2(ldu(vkqregs+((e__u182*4) as u64)),sh2[(1 as i32)]));

}
l0__u180=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=12;

    col=0;
    if true {
let col:i32=0;

      var l0__u184:i32=0;
      if true {
let l0__u184:i32=0;

        let e__u185=v*4+l0__u184*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u185*4) as u64),mulh2(ldu(vkqregs+((e__u185*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u184:i32=1;

        let e__u186=v*4+l0__u184*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u186*4) as u64),mulh2(ldu(vkqregs+((e__u186*4) as u64)),sh2[(0 as i32)]));

}
l0__u184=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u187:i32=0;
      if true {
let l0__u187:i32=0;

        let e__u188=v*4+l0__u187*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u188*4) as u64),mulh2(ldu(vkqregs+((e__u188*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u187:i32=1;

        let e__u189=v*4+l0__u187*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u189*4) as u64),mulh2(ldu(vkqregs+((e__u189*4) as u64)),sh2[(1 as i32)]));

}
l0__u187=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=13;

    col=0;
    if true {
let col:i32=0;

      var l0__u191:i32=0;
      if true {
let l0__u191:i32=0;

        let e__u192=v*4+l0__u191*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u192*4) as u64),mulh2(ldu(vkqregs+((e__u192*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u191:i32=1;

        let e__u193=v*4+l0__u191*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u193*4) as u64),mulh2(ldu(vkqregs+((e__u193*4) as u64)),sh2[(0 as i32)]));

}
l0__u191=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u194:i32=0;
      if true {
let l0__u194:i32=0;

        let e__u195=v*4+l0__u194*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u195*4) as u64),mulh2(ldu(vkqregs+((e__u195*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u194:i32=1;

        let e__u196=v*4+l0__u194*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u196*4) as u64),mulh2(ldu(vkqregs+((e__u196*4) as u64)),sh2[(1 as i32)]));

}
l0__u194=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=14;

    col=0;
    if true {
let col:i32=0;

      var l0__u198:i32=0;
      if true {
let l0__u198:i32=0;

        let e__u199=v*4+l0__u198*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u199*4) as u64),mulh2(ldu(vkqregs+((e__u199*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u198:i32=1;

        let e__u200=v*4+l0__u198*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u200*4) as u64),mulh2(ldu(vkqregs+((e__u200*4) as u64)),sh2[(0 as i32)]));

}
l0__u198=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u201:i32=0;
      if true {
let l0__u201:i32=0;

        let e__u202=v*4+l0__u201*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u202*4) as u64),mulh2(ldu(vkqregs+((e__u202*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u201:i32=1;

        let e__u203=v*4+l0__u201*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u203*4) as u64),mulh2(ldu(vkqregs+((e__u203*4) as u64)),sh2[(1 as i32)]));

}
l0__u201=(2 as i32);


}
col=(2 as i32);


}
if true {
let v:i32=15;

    col=0;
    if true {
let col:i32=0;

      var l0__u205:i32=0;
      if true {
let l0__u205:i32=0;

        let e__u206=v*4+l0__u205*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u206*4) as u64),mulh2(ldu(vkqregs+((e__u206*4) as u64)),sh2[(0 as i32)]));

}
if true {
let l0__u205:i32=1;

        let e__u207=v*4+l0__u205*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u207*4) as u64),mulh2(ldu(vkqregs+((e__u207*4) as u64)),sh2[(0 as i32)]));

}
l0__u205=(2 as i32);


}
if true {
let col:i32=1;

      var l0__u208:i32=0;
      if true {
let l0__u208:i32=0;

        let e__u209=v*4+l0__u208*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u209*4) as u64),mulh2(ldu(vkqregs+((e__u209*4) as u64)),sh2[(1 as i32)]));

}
if true {
let l0__u208:i32=1;

        let e__u210=v*4+l0__u208*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e__u210*4) as u64),mulh2(ldu(vkqregs+((e__u210*4) as u64)),sh2[(1 as i32)]));

}
l0__u208=(2 as i32);


}
col=(2 as i32);


}
v=(16 as i32);

  // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
  var b:u32[8]=default<u32[8]>();
  t=0;
  if true {
let t:i32=0;

    var l0__u211:i32=0;
    if true {
let l0__u211:i32=0;

      b[(0 as i32)]=h2(kqc[(0 as i32)],kqc[(1 as i32)]);

}
if true {
let l0__u211:i32=2;

      b[(1 as i32)]=h2(kqc[(2 as i32)],kqc[(3 as i32)]);

}
if true {
let l0__u211:i32=4;

      b[(2 as i32)]=h2(kqc[(4 as i32)],kqc[(5 as i32)]);

}
if true {
let l0__u211:i32=6;

      b[(3 as i32)]=h2(kqc[(6 as i32)],kqc[(7 as i32)]);

}
l0__u211=(8 as i32);


}
if true {
let t:i32=1;

    var l0__u216:i32=0;
    if true {
let l0__u216:i32=0;

      b[(4 as i32)]=h2(kqc[(8 as i32)],kqc[(9 as i32)]);

}
if true {
let l0__u216:i32=2;

      b[(5 as i32)]=h2(kqc[(10 as i32)],kqc[(11 as i32)]);

}
if true {
let l0__u216:i32=4;

      b[(6 as i32)]=h2(kqc[(12 as i32)],kqc[(13 as i32)]);

}
if true {
let l0__u216:i32=6;

      b[(7 as i32)]=h2(kqc[(14 as i32)],kqc[(15 as i32)]);

}
l0__u216=(8 as i32);


}
t=(2 as i32);

  // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
  //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
  cpwait();
  barrier();
  if !last {
    load_mask(mask_h,mt,stride_mask,kv_row0+32,query0,n_queries,ncols1,tx,ty);
    load_kv(k_h2,kt,stride_k,kv_row0+32,tx,ty);
  }
  // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
  //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
  v=0;
  if true {
let v:i32=0;

    var d0__u221:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u221:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u221:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u221:i32=0;

      let k0__u222=kk__u221*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u222=ldm4t_tile(ldm_addr(vt,2*k0__u222*128+v*8,tx));
      d0__u221=mma_f16(d0__u221,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u222.a,av__u222.c);
      d1__u221=mma_f16(d1__u221,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u222.b,av__u222.d);

}
if true {
let kk__u221:i32=1;

      let k0__u223=kk__u221*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u223=ldm4t_tile(ldm_addr(vt,2*k0__u223*128+v*8,tx));
      d0__u221=mma_f16(d0__u221,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u223.a,av__u223.c);
      d1__u221=mma_f16(d1__u221,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u223.b,av__u223.d);

}
kk__u221=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u221.a);
    stu(vkqregs+((v*16+4) as u64),d0__u221.b);
    stu(vkqregs+((v*16+8) as u64),d1__u221.a);
    stu(vkqregs+((v*16+12) as u64),d1__u221.b);

}
if true {
let v:i32=1;

    var d0__u224:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u224:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u224:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u224:i32=0;

      let k0__u225=kk__u224*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u225=ldm4t_tile(ldm_addr(vt,2*k0__u225*128+v*8,tx));
      d0__u224=mma_f16(d0__u224,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u225.a,av__u225.c);
      d1__u224=mma_f16(d1__u224,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u225.b,av__u225.d);

}
if true {
let kk__u224:i32=1;

      let k0__u226=kk__u224*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u226=ldm4t_tile(ldm_addr(vt,2*k0__u226*128+v*8,tx));
      d0__u224=mma_f16(d0__u224,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u226.a,av__u226.c);
      d1__u224=mma_f16(d1__u224,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u226.b,av__u226.d);

}
kk__u224=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u224.a);
    stu(vkqregs+((v*16+4) as u64),d0__u224.b);
    stu(vkqregs+((v*16+8) as u64),d1__u224.a);
    stu(vkqregs+((v*16+12) as u64),d1__u224.b);

}
if true {
let v:i32=2;

    var d0__u227:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u227:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u227:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u227:i32=0;

      let k0__u228=kk__u227*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u228=ldm4t_tile(ldm_addr(vt,2*k0__u228*128+v*8,tx));
      d0__u227=mma_f16(d0__u227,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u228.a,av__u228.c);
      d1__u227=mma_f16(d1__u227,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u228.b,av__u228.d);

}
if true {
let kk__u227:i32=1;

      let k0__u229=kk__u227*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u229=ldm4t_tile(ldm_addr(vt,2*k0__u229*128+v*8,tx));
      d0__u227=mma_f16(d0__u227,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u229.a,av__u229.c);
      d1__u227=mma_f16(d1__u227,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u229.b,av__u229.d);

}
kk__u227=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u227.a);
    stu(vkqregs+((v*16+4) as u64),d0__u227.b);
    stu(vkqregs+((v*16+8) as u64),d1__u227.a);
    stu(vkqregs+((v*16+12) as u64),d1__u227.b);

}
if true {
let v:i32=3;

    var d0__u230:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u230:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u230:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u230:i32=0;

      let k0__u231=kk__u230*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u231=ldm4t_tile(ldm_addr(vt,2*k0__u231*128+v*8,tx));
      d0__u230=mma_f16(d0__u230,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u231.a,av__u231.c);
      d1__u230=mma_f16(d1__u230,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u231.b,av__u231.d);

}
if true {
let kk__u230:i32=1;

      let k0__u232=kk__u230*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u232=ldm4t_tile(ldm_addr(vt,2*k0__u232*128+v*8,tx));
      d0__u230=mma_f16(d0__u230,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u232.a,av__u232.c);
      d1__u230=mma_f16(d1__u230,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u232.b,av__u232.d);

}
kk__u230=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u230.a);
    stu(vkqregs+((v*16+4) as u64),d0__u230.b);
    stu(vkqregs+((v*16+8) as u64),d1__u230.a);
    stu(vkqregs+((v*16+12) as u64),d1__u230.b);

}
if true {
let v:i32=4;

    var d0__u233:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u233:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u233:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u233:i32=0;

      let k0__u234=kk__u233*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u234=ldm4t_tile(ldm_addr(vt,2*k0__u234*128+v*8,tx));
      d0__u233=mma_f16(d0__u233,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u234.a,av__u234.c);
      d1__u233=mma_f16(d1__u233,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u234.b,av__u234.d);

}
if true {
let kk__u233:i32=1;

      let k0__u235=kk__u233*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u235=ldm4t_tile(ldm_addr(vt,2*k0__u235*128+v*8,tx));
      d0__u233=mma_f16(d0__u233,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u235.a,av__u235.c);
      d1__u233=mma_f16(d1__u233,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u235.b,av__u235.d);

}
kk__u233=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u233.a);
    stu(vkqregs+((v*16+4) as u64),d0__u233.b);
    stu(vkqregs+((v*16+8) as u64),d1__u233.a);
    stu(vkqregs+((v*16+12) as u64),d1__u233.b);

}
if true {
let v:i32=5;

    var d0__u236:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u236:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u236:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u236:i32=0;

      let k0__u237=kk__u236*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u237=ldm4t_tile(ldm_addr(vt,2*k0__u237*128+v*8,tx));
      d0__u236=mma_f16(d0__u236,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u237.a,av__u237.c);
      d1__u236=mma_f16(d1__u236,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u237.b,av__u237.d);

}
if true {
let kk__u236:i32=1;

      let k0__u238=kk__u236*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u238=ldm4t_tile(ldm_addr(vt,2*k0__u238*128+v*8,tx));
      d0__u236=mma_f16(d0__u236,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u238.a,av__u238.c);
      d1__u236=mma_f16(d1__u236,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u238.b,av__u238.d);

}
kk__u236=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u236.a);
    stu(vkqregs+((v*16+4) as u64),d0__u236.b);
    stu(vkqregs+((v*16+8) as u64),d1__u236.a);
    stu(vkqregs+((v*16+12) as u64),d1__u236.b);

}
if true {
let v:i32=6;

    var d0__u239:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u239:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u239:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u239:i32=0;

      let k0__u240=kk__u239*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u240=ldm4t_tile(ldm_addr(vt,2*k0__u240*128+v*8,tx));
      d0__u239=mma_f16(d0__u239,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u240.a,av__u240.c);
      d1__u239=mma_f16(d1__u239,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u240.b,av__u240.d);

}
if true {
let kk__u239:i32=1;

      let k0__u241=kk__u239*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u241=ldm4t_tile(ldm_addr(vt,2*k0__u241*128+v*8,tx));
      d0__u239=mma_f16(d0__u239,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u241.a,av__u241.c);
      d1__u239=mma_f16(d1__u239,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u241.b,av__u241.d);

}
kk__u239=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u239.a);
    stu(vkqregs+((v*16+4) as u64),d0__u239.b);
    stu(vkqregs+((v*16+8) as u64),d1__u239.a);
    stu(vkqregs+((v*16+12) as u64),d1__u239.b);

}
if true {
let v:i32=7;

    var d0__u242:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u242:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u242:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u242:i32=0;

      let k0__u243=kk__u242*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u243=ldm4t_tile(ldm_addr(vt,2*k0__u243*128+v*8,tx));
      d0__u242=mma_f16(d0__u242,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u243.a,av__u243.c);
      d1__u242=mma_f16(d1__u242,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u243.b,av__u243.d);

}
if true {
let kk__u242:i32=1;

      let k0__u244=kk__u242*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u244=ldm4t_tile(ldm_addr(vt,2*k0__u244*128+v*8,tx));
      d0__u242=mma_f16(d0__u242,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u244.a,av__u244.c);
      d1__u242=mma_f16(d1__u242,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u244.b,av__u244.d);

}
kk__u242=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u242.a);
    stu(vkqregs+((v*16+4) as u64),d0__u242.b);
    stu(vkqregs+((v*16+8) as u64),d1__u242.a);
    stu(vkqregs+((v*16+12) as u64),d1__u242.b);

}
if true {
let v:i32=8;

    var d0__u245:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u245:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u245:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u245:i32=0;

      let k0__u246=kk__u245*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u246=ldm4t_tile(ldm_addr(vt,2*k0__u246*128+v*8,tx));
      d0__u245=mma_f16(d0__u245,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u246.a,av__u246.c);
      d1__u245=mma_f16(d1__u245,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u246.b,av__u246.d);

}
if true {
let kk__u245:i32=1;

      let k0__u247=kk__u245*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u247=ldm4t_tile(ldm_addr(vt,2*k0__u247*128+v*8,tx));
      d0__u245=mma_f16(d0__u245,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u247.a,av__u247.c);
      d1__u245=mma_f16(d1__u245,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u247.b,av__u247.d);

}
kk__u245=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u245.a);
    stu(vkqregs+((v*16+4) as u64),d0__u245.b);
    stu(vkqregs+((v*16+8) as u64),d1__u245.a);
    stu(vkqregs+((v*16+12) as u64),d1__u245.b);

}
if true {
let v:i32=9;

    var d0__u248:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u248:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u248:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u248:i32=0;

      let k0__u249=kk__u248*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u249=ldm4t_tile(ldm_addr(vt,2*k0__u249*128+v*8,tx));
      d0__u248=mma_f16(d0__u248,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u249.a,av__u249.c);
      d1__u248=mma_f16(d1__u248,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u249.b,av__u249.d);

}
if true {
let kk__u248:i32=1;

      let k0__u250=kk__u248*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u250=ldm4t_tile(ldm_addr(vt,2*k0__u250*128+v*8,tx));
      d0__u248=mma_f16(d0__u248,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u250.a,av__u250.c);
      d1__u248=mma_f16(d1__u248,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u250.b,av__u250.d);

}
kk__u248=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u248.a);
    stu(vkqregs+((v*16+4) as u64),d0__u248.b);
    stu(vkqregs+((v*16+8) as u64),d1__u248.a);
    stu(vkqregs+((v*16+12) as u64),d1__u248.b);

}
if true {
let v:i32=10;

    var d0__u251:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u251:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u251:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u251:i32=0;

      let k0__u252=kk__u251*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u252=ldm4t_tile(ldm_addr(vt,2*k0__u252*128+v*8,tx));
      d0__u251=mma_f16(d0__u251,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u252.a,av__u252.c);
      d1__u251=mma_f16(d1__u251,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u252.b,av__u252.d);

}
if true {
let kk__u251:i32=1;

      let k0__u253=kk__u251*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u253=ldm4t_tile(ldm_addr(vt,2*k0__u253*128+v*8,tx));
      d0__u251=mma_f16(d0__u251,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u253.a,av__u253.c);
      d1__u251=mma_f16(d1__u251,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u253.b,av__u253.d);

}
kk__u251=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u251.a);
    stu(vkqregs+((v*16+4) as u64),d0__u251.b);
    stu(vkqregs+((v*16+8) as u64),d1__u251.a);
    stu(vkqregs+((v*16+12) as u64),d1__u251.b);

}
if true {
let v:i32=11;

    var d0__u254:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u254:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u254:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u254:i32=0;

      let k0__u255=kk__u254*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u255=ldm4t_tile(ldm_addr(vt,2*k0__u255*128+v*8,tx));
      d0__u254=mma_f16(d0__u254,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u255.a,av__u255.c);
      d1__u254=mma_f16(d1__u254,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u255.b,av__u255.d);

}
if true {
let kk__u254:i32=1;

      let k0__u256=kk__u254*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u256=ldm4t_tile(ldm_addr(vt,2*k0__u256*128+v*8,tx));
      d0__u254=mma_f16(d0__u254,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u256.a,av__u256.c);
      d1__u254=mma_f16(d1__u254,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u256.b,av__u256.d);

}
kk__u254=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u254.a);
    stu(vkqregs+((v*16+4) as u64),d0__u254.b);
    stu(vkqregs+((v*16+8) as u64),d1__u254.a);
    stu(vkqregs+((v*16+12) as u64),d1__u254.b);

}
if true {
let v:i32=12;

    var d0__u257:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u257:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u257:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u257:i32=0;

      let k0__u258=kk__u257*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u258=ldm4t_tile(ldm_addr(vt,2*k0__u258*128+v*8,tx));
      d0__u257=mma_f16(d0__u257,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u258.a,av__u258.c);
      d1__u257=mma_f16(d1__u257,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u258.b,av__u258.d);

}
if true {
let kk__u257:i32=1;

      let k0__u259=kk__u257*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u259=ldm4t_tile(ldm_addr(vt,2*k0__u259*128+v*8,tx));
      d0__u257=mma_f16(d0__u257,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u259.a,av__u259.c);
      d1__u257=mma_f16(d1__u257,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u259.b,av__u259.d);

}
kk__u257=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u257.a);
    stu(vkqregs+((v*16+4) as u64),d0__u257.b);
    stu(vkqregs+((v*16+8) as u64),d1__u257.a);
    stu(vkqregs+((v*16+12) as u64),d1__u257.b);

}
if true {
let v:i32=13;

    var d0__u260:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u260:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u260:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u260:i32=0;

      let k0__u261=kk__u260*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u261=ldm4t_tile(ldm_addr(vt,2*k0__u261*128+v*8,tx));
      d0__u260=mma_f16(d0__u260,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u261.a,av__u261.c);
      d1__u260=mma_f16(d1__u260,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u261.b,av__u261.d);

}
if true {
let kk__u260:i32=1;

      let k0__u262=kk__u260*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u262=ldm4t_tile(ldm_addr(vt,2*k0__u262*128+v*8,tx));
      d0__u260=mma_f16(d0__u260,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u262.a,av__u262.c);
      d1__u260=mma_f16(d1__u260,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u262.b,av__u262.d);

}
kk__u260=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u260.a);
    stu(vkqregs+((v*16+4) as u64),d0__u260.b);
    stu(vkqregs+((v*16+8) as u64),d1__u260.a);
    stu(vkqregs+((v*16+12) as u64),d1__u260.b);

}
if true {
let v:i32=14;

    var d0__u263:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u263:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u263:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u263:i32=0;

      let k0__u264=kk__u263*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u264=ldm4t_tile(ldm_addr(vt,2*k0__u264*128+v*8,tx));
      d0__u263=mma_f16(d0__u263,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u264.a,av__u264.c);
      d1__u263=mma_f16(d1__u263,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u264.b,av__u264.d);

}
if true {
let kk__u263:i32=1;

      let k0__u265=kk__u263*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u265=ldm4t_tile(ldm_addr(vt,2*k0__u265*128+v*8,tx));
      d0__u263=mma_f16(d0__u263,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u265.a,av__u265.c);
      d1__u263=mma_f16(d1__u263,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u265.b,av__u265.d);

}
kk__u263=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u263.a);
    stu(vkqregs+((v*16+4) as u64),d0__u263.b);
    stu(vkqregs+((v*16+8) as u64),d1__u263.a);
    stu(vkqregs+((v*16+12) as u64),d1__u263.b);

}
if true {
let v:i32=15;

    var d0__u266:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1__u266:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk__u266:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    if true {
let kk__u266:i32=0;

      let k0__u267=kk__u266*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u267=ldm4t_tile(ldm_addr(vt,2*k0__u267*128+v*8,tx));
      d0__u266=mma_f16(d0__u266,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u267.a,av__u267.c);
      d1__u266=mma_f16(d1__u266,b[(0 as i32)],b[(1 as i32)],b[(2 as i32)],b[(3 as i32)],av__u267.b,av__u267.d);

}
if true {
let kk__u266:i32=1;

      let k0__u268=kk__u266*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av__u268=ldm4t_tile(ldm_addr(vt,2*k0__u268*128+v*8,tx));
      d0__u266=mma_f16(d0__u266,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u268.a,av__u268.c);
      d1__u266=mma_f16(d1__u266,b[(4 as i32)],b[(5 as i32)],b[(6 as i32)],b[(7 as i32)],av__u268.b,av__u268.d);

}
kk__u266=(2 as i32);

    stu(vkqregs+((v*16) as u64),d0__u266.a);
    stu(vkqregs+((v*16+4) as u64),d0__u266.b);
    stu(vkqregs+((v*16+8) as u64),d1__u266.a);
    stu(vkqregs+((v*16+12) as u64),d1__u266.b);

}
v=(16 as i32);

}
fn iterate_generic(k_h2:u64, v_h2:u64, mask_h:u64, stride_k:i64, stride_v:i64, stride_mask:i64, slope:f32,
           jt:i32, kb0:i32, n_queries:i64, ncols1:i32,
           qregs:u64, vkqregs:u64, kmax:u64, rowsums:u64, smem:u64, tx:i32, ty:i32, np:i32, last:bool) {
  let kt=smem;
  let vt=smem+(16384 as u64);
  let mt=smem+(32768 as u64);
  let kv_row0=(kb0 as i64)*32;               // k_VKQ_0 = kb0*nbatch_fa
  let query0=(jt as i64)*(ncols1 as i64);    // j0 for the mask loader
  let cb=(ty/np)*16;                          // warp Q-column block base (cols_per_warp=16)
  let rh=(ty%np)*16;                          // warp KV-row half base (np*T_A_KQ::I split)
  let nt=32/(np*16);                          // KQ_C/B tiles per warp: 1 (np=2) or 2 (np=1)
  let kqoff=(3.0 as f32)*(0.6931 as f32);                       // FATTN_KQ_MAX_OFFSET
  var t:i32=0;
  // nstages>1 entry: this iteration's K tile + mask were preloaded by the prologue or the previous
  // iter tail; wait for them, then issue the V tile of this iteration into the pipeline.
  cpwait();
  barrier();
  load_kv(v_h2,vt,stride_v,kv_row0,tx,ty);
  // ---- KQ = Q_B @ K_A over the single nbatch_K2 block k in [0,128) half2 (16 x T_A_KQ::J=8) ----
  var kqc:f32[16]=default<f32[16]>();        // up to 2 fresh zero tile<16,16,float> C tiles
  var q:i32=0;                                // Q_B tile index = k_KQ_0/8
  while q<16 {
    let q0=ldu(qregs+((q*16) as u64));
    let q1=ldu(qregs+((q*16+4) as u64));
    let q2=ldu(qregs+((q*16+8) as u64));
    let q3=ldu(qregs+((q*16+12) as u64));
    t=0;
    while t<nt {
      // K_A = load_ldmatrix_swizzled(tile_K, i_KQ_0*stride_tile_K + k_KQ_0-k0_start)
      let ka=ldm4(ldm_addr(kt,(t*np*16+rh)*128+q*8,tx));
      // Wide KQ_C is column-major on CUDA: swap operands -> mma(KQ_C, Q_B, K_A). The f32 pair
      // m16n8k16.f32.f16.f16.f32 consumes B registers 0,2 (C elems 0..3) then 1,3 (elems 4..7).
      let lo=mma_f32(F4{kqc[t*8],kqc[t*8+1],kqc[t*8+2],kqc[t*8+3]},q0,q1,q2,q3,ka.a,ka.c);
      kqc[t*8]=lo.x;kqc[t*8+1]=lo.y;kqc[t*8+2]=lo.z;kqc[t*8+3]=lo.w;
      let hi=mma_f32(F4{kqc[t*8+4],kqc[t*8+5],kqc[t*8+6],kqc[t*8+7]},q0,q1,q2,q3,ka.b,ka.d);
      kqc[t*8+4]=hi.x;kqc[t*8+5]=hi.y;kqc[t*8+6]=hi.z;kqc[t*8+7]=hi.w;
      t+=1;
    }
    q+=1;
  }
  // ---- mask add + running max: half2 mask word at row (cb+get_i)/8, half2 col (rb+get_j)/2 ----
  var kqold:f32[2]=default<f32[2]>();        // KQ_max[cols_per_thread=2]
  var kqmn:f32[2]=default<f32[2]>();         // KQ_max_new
  var rsa:f32[2]=default<f32[2]>();          // KQ_rowsum_add
  kqold[0]=frombits(ldu(kmax));
  kqold[1]=frombits(ldu(kmax+(4 as u64)));
  kqmn[0]=kqold[0];
  kqmn[1]=kqold[1];
  t=0;
  while t<nt {
    let rb=t*np*16+rh;
    var l0:i32=0;
    while l0<8 {                              // half2 pairs, l0 even: (j,i),(j,i+1)
      let mh2=((cb+gi_c(l0,tx))/8)*20+((rb+gj_c(l0,tx))/2);
      let w=ldu(mt+((mh2 as u64)*(4 as u64)));
      kqc[t*8+l0]=fmaf(slope,hlo(w),kqc[t*8+l0]);
      kqc[t*8+l0+1]=fmaf(slope,hhi(w),kqc[t*8+l0+1]);
      l0+=2;
    }
    var l:i32=0;
    while l<8 {
      let idx=(l/2)%2;
      let mv=kqc[t*8+l]+kqoff;
      if mv>kqmn[idx] {kqmn[idx]=mv;}
      l+=1;
    }
    t+=1;
  }
  // Values per KQ column spread across 4 threads: __shfl_xor offsets 2 then 1 (TURING wide).
  var col:i32=0;
  while col<2 {
    var s=shflx(kqmn[col],2);
    if s>kqmn[col] {kqmn[col]=s;}
    s=shflx(kqmn[col],1);
    if s>kqmn[col] {kqmn[col]=s;}
    col+=1;
  }
  // ---- softmax with current max: expf(KQ - KQ_max_new); divisor stays in KQ_rowsum ----
  t=0;
  while t<nt {
    var l:i32=0;
    while l<8 {
      let idx=(l/2)%2;
      let e=expf(kqc[t*8+l]-kqmn[idx]);
      kqc[t*8+l]=e;
      rsa[idx]+=e;
      l+=1;
    }
    t+=1;
  }
  // ---- rescale: KQ_max_scale=expf(KQ_max-KQ_max_new); FTZ bit-multiply for diff < -20;
  //      exact fused step KQ_rowsum = scale*KQ_rowsum + KQ_rowsum_add ----
  var scl:f32[2]=default<f32[2]>();
  col=0;
  while col<2 {
    let diff=kqold[col]-kqmn[col];
    let s0=expf(diff);
    let keep:u32=if diff>=-20.0 {1 as u32} else {0 as u32};
    scl[col]=frombits(bits(s0)*keep);    // *((uint32_t*)&scale) *= diff>=SOFTMAX_FTZ_THRESHOLD
    stu(kmax+((col*4) as u64),bits(kqmn[col]));
    let rs=frombits(ldu(rowsums+((col*4) as u64)));
    stu(rowsums+((col*4) as u64),bits(fmaf(scl[col],rs,rsa[col])));
    col+=1;
  }
  // ---- VKQ f16 accumulator rescale: VKQ_C[i].x[l0+col] *= make_half2(scale,scale) ----
  var sh2:u32[2]=default<u32[2]>();
  col=0;
  while col<2 { sh2[col]=h2(scl[col],scl[col]); col+=1; }
  var v:i32=0;
  while v<16 {
    col=0;
    while col<2 {
      var l0:i32=0;
      while l0<2 {
        let e=v*4+l0*2+col;                   // col 0 scales x[0],x[2]; col 1 scales x[1],x[3]
        stu(vkqregs+((e*4) as u64),mulh2(ldu(vkqregs+((e*4) as u64)),sh2[col]));
        l0+=1;
      }
      col+=1;
    }
    v+=1;
  }
  // ---- B[k] = get_half2(KQ_C[k]): pure F32->half2 pack on Turing, no transpose/shuffle ----
  var b:u32[8]=default<u32[8]>();
  t=0;
  while t<nt {
    var l0:i32=0;
    while l0<8 {
      b[t*4+l0/2]=h2(kqc[t*8+l0],kqc[t*8+l0+1]);
      l0+=2;
    }
    t+=1;
  }
  // ---- double-stage tail: wait the V tile, sync, then preload next K + mask before the current
  //      VKQ mma (skipped only on the last iteration; waits/barriers stay unconditional) ----
  cpwait();
  barrier();
  if !last {
    load_mask(mask_h,mt,stride_mask,kv_row0+32,query0,n_queries,ncols1,tx,ty);
    load_kv(k_h2,kt,stride_k,kv_row0+32,tx,ty);
  }
  // ---- VKQ += probs @ V^T, swapped operands (A=B_probs, B=A_V trans-V fragment); f16
  //      accumulation with B registers 0,2 -> D.x[0..1] then 1,3 -> D.x[2..3] ----
  v=0;
  while v<16 {
    var d0:U2=U2{ldu(vkqregs+((v*16) as u64)),ldu(vkqregs+((v*16+4) as u64))};
    var d1:U2=U2{ldu(vkqregs+((v*16+8) as u64)),ldu(vkqregs+((v*16+12) as u64))};
    var kk:i32=0;                             // k00: nbatch_fa/2=16 step np*T_A_VKQ::J=np*8
    while kk<16/(np*8) {
      let k0=kk*np*8+rh/2;                    // (ty%np)*T_A_VKQ::J KV-row-pair base
      // A_V = load_ldmatrix_trans_swizzled(tile_V, 2*k0*stride_tile_V + (i_VKQ_0-i0_start)/2)
      let av=ldm4t_tile(ldm_addr(vt,2*k0*128+v*8,tx));
      d0=mma_f16(d0,b[kk*4],b[kk*4+1],b[kk*4+2],b[kk*4+3],av.a,av.c);
      d1=mma_f16(d1,b[kk*4],b[kk*4+1],b[kk*4+2],b[kk*4+3],av.b,av.d);
      kk+=1;
    }
    stu(vkqregs+((v*16) as u64),d0.a);
    stu(vkqregs+((v*16+4) as u64),d0.b);
    stu(vkqregs+((v*16+8) as u64),d1.a);
    stu(vkqregs+((v*16+12) as u64),d1.b);
    v+=1;
  }
}
fn iterate(k_h2:u64, v_h2:u64, mask_h:u64, stride_k:i64, stride_v:i64, stride_mask:i64, slope:f32, jt:i32, kb0:i32, n_queries:i64, ncols1:i32, qregs:u64, vkqregs:u64, kmax:u64, rowsums:u64, smem:u64, tx:i32, ty:i32, np:i32, last:bool){
if ncols1==(4 as i32)&&np==(2 as i32){iterate_nc4_np2(k_h2, v_h2, mask_h, stride_k, stride_v, stride_mask, slope, jt, kb0, n_queries, qregs, vkqregs, kmax, rowsums, smem, tx, ty, last);return;}
if ncols1==(8 as i32)&&np==(1 as i32){iterate_nc8_np1(k_h2, v_h2, mask_h, stride_k, stride_v, stride_mask, slope, jt, kb0, n_queries, qregs, vkqregs, kmax, rowsums, smem, tx, ty, last);return;}
iterate_generic(k_h2, v_h2, mask_h, stride_k, stride_v, stride_mask, slope, jt, kb0, n_queries, ncols1, qregs, vkqregs, kmax, rowsums, smem, tx, ty, np, last);
}

// Retained entry; the parent never executes main.
fn main(){iterate(0 as u64,0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0.0,0,0,0 as i64,4,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0,0,2,true);}
