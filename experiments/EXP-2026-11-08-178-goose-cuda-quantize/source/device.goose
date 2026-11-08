// Apex CUDA Q8_1 activation quantization device path (Goose), from
// ggml-cuda/quantize.cu: quantize_q8_1.
//
// Scope: quantize_q8_1, the non-MMQ Q8_1 row quantizer used by quantize_row_q8_1_cuda
// with ids == nullptr. The MMQ layouts (D4/DS4/D2S6) and the scatter variant are a
// different kernel and are not part of this module.
//
// Everything algorithmic is Goose: the flat index, the fastdivide of blockIdx.z
// into (i2,i3), the guarded source load, the absolute max, the warp max and sum
// reductions, the delta, the int8 quant, the block index and the two half stores.
// Extern symbols are only the raw float load, the int8 store, the half stores, the
// shuffle primitive used by the reductions, the fastdiv multiplier and the PDL
// primitives. No original kernel, GGML struct or C++ routine is called.
//
// Layout: block_q8_1 is 36 bytes, two ggml_half (d, s) followed by 32 int8 qs, so
// y[ib].qs[iqs] is a byte at ib*36 + 4 + iqs and the ds pair occupies ib*36..+3.
//
// Raw ABI: u64 values are global byte addresses. s01..s03 are source strides in
// FLOAT elements. i_cont is the flattened element index in dst order.
extern "fg178q_ld_f32" fn ld_f32(src:u64,byte:u64)->f32;
extern "fg178q_st_i8" fn st_i8(dst:u64,byte:u64,v:i32);
extern "fg178q_st_f16" fn st_f16(dst:u64,byte:u64,v:f32);
extern "fg178q_shfl_xor" fn shfl_xor(v:f32,mask:u32,off:i32)->f32;
extern "fg178q_round" fn rnd(v:f32)->f32;
extern "fg178q_trunc_i32" fn trunc_i32(v:f32)->i32;
extern "fg178q_fabs" fn fabsv(v:f32)->f32;
extern "fg178q_mulhi" fn mulhi(a:u32,b:u32)->u32;
extern "fg178q_pdl_sync" fn pdl_sync();
extern "fg178q_pdl_lc" fn pdl_lc();
extern "fg178q_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
struct FastDiv { mp:u32, shift:u32, divisor:u32 }

// warp_reduce_max<32> / warp_reduce_sum<32>: xor shuffles over offsets 16,8,4,2,1.
fn red_max(v:f32)->f32{
 var a:f32=v;
 var off:i32=16;
 while off>0{
  let o:f32=shfl_xor(a,4294967295 as u32,off);
  if a<o{a=o;}
  off=off/2;
 }
 return a;
}
fn red_sum(v:f32)->f32{
 var s:f32=v;
 var off:i32=16;
 while off>0{
  s=s+shfl_xor(s,4294967295 as u32,off);
  off=off/2;
 }
 return s;
}
// quantize_q8_1 body, one element per GPU thread.
//   i3 = fastdiv(blockIdx.z); i2 = blockIdx.z - i3*ne2; i1 = blockIdx.y
//   i_cont = ((i3*ne2 + i2)*ne1 + i1)*ne0 + i0
//   ib = i_cont/32 ; iqs = i_cont%32
//   xi = i0 < ne00 ? x[i03*s03 + i02*s02 + i01*s01 + i00] : 0
//   amax = warp_max(|xi|) ; sum = warp_sum(xi) ; d = amax/127
//   q = amax == 0 ? 0 : round(xi/d) ; y[ib].qs[iqs] = q
//   if iqs == 0: y[ib].ds = (d, sum)
fn fg178q_quant(src:u64,y:u64,ne00:i64,s01:i64,s02:i64,s03:i64,ne0:i64,ne1:i64,ne2:i64,
 fd:FastDiv,bx:i32,by:i32,bz:i32,tx:i32,nt:i32){
 pdl_lc();
 ensure(nt>0&&bx>=0&&by>=0&&bz>=0&&tx>=0,10);
 ensure(ne0>0&&ne1>0&&ne2>0,11);
 let i0:i64=(nt as i64)*(bx as i64)+(tx as i64);
 if i0>=ne0{return;}
 // fastdiv(blockIdx.z, ne2): (umulhi(n,mp)+n) >> L
 let zu:u32=bz as u32;
 let hi:u32=mulhi(zu,fd.mp);
 let q:u32=(hi+zu)>>(fd.shift as u32);
 let i3:i64=q as i64;
 let i2:i64=(bz as u32-q*fd.divisor) as i64;
 let i1:i64=by as i64;
 let i_cont:i64=((i3*ne2+i2)*ne1+i1)*ne0+i0;
 let ib:i64=i_cont/(32 as i64);
 let iqs:i64=i_cont-(ib*(32 as i64));
 var xi:f32=0.0;
 if i0<ne00{
  // s01..s03 are element strides, as in native; ld_f32 takes a byte offset.
  let o:i64=i3*s03+i2*s02+i1*s01+i0;
  xi=ld_f32(src,(o*(4 as i64)) as u64);
 }
 let amax:f32=red_max(fabsv(xi));
 let sum:f32=red_sum(xi);
 pdl_sync();
 let d:f32=amax/127.0;
 var qv:i32=0;
 if amax!=0.0{
  qv=trunc_i32(rnd(xi/d));
 }
 st_i8(y,((ib*(36 as i64))+(4 as i64)+iqs) as u64,(qv as i32));
 if iqs==0{
  st_f16(y,(ib*(36 as i64)) as u64,d);
  st_f16(y,((ib*(36 as i64))+(2 as i64)) as u64,sum);
 }
}

// Retained entry; the parent never executes main. ne0=0 makes the body return
// before any memory effect.
fn main(){
 let z:u64=0;
 let f=FastDiv{1 as u32,0 as u32,1 as u32};
 fg178q_quant(z,z,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,1 as i64,1 as i64,f,0,0,0,0,32 as i32);
}
