// Apex CUDA SET_ROWS device path (Goose), finalized from the supplied draft.
// Scope, exactly ggml-cuda/set-rows.cu with src0 F32:
//   fg176r_set_rows_q4_0  <- k_set_rows_quant<idx_t, block_q4_0, 32, quantize_f32_q4_0_block>
//   fg176r_set_rows_f32   <- k_set_rows<src_t, idx_t, dst_t> with src_t = dst_t = f32
// idx_t is the src1 row-index type: int32_t when explicittype=false, int64_t when true.
//
// Everything algorithmic is Goose: grid/thread mapping, index decode, the src1 row
// load, Q4_0 absmax scan with first-tie sign, scale/id step order, signed-8
// truncation, 15-clip, nibble packing and the strided address sums. Extern symbols
// are only raw scalar loads/stores, hardware float<->half and float->int8
// conversions, the hardware abs.f32 instruction, and the PDL primitives. No
// quantizer, cast helper, fastdiv table or C++ routine is called.
//
// Q4_0 block layout (ggml-common.h: block_q4_0, QK4_0=32): 18 bytes, ggml_half d at
// bytes 0..1 stored round-to-nearest, uint8_t qs[0..15] at bytes 2..17. qs[j] low
// nibble is element j and high nibble is element 16+j.
//
// Raw ABI: each u64 is a global byte address and every offset below is a byte
// offset. Stride units are those of the counterpart host plan: s01..s03 in FLOAT
// ELEMENTS, s10..s12 in IDX ELEMENTS, s1..s3 in BYTES for Q4_0 and in FLOAT
// ELEMENTS for F32. ne_total is the native launch bound (product/32 for Q4_0,
// product for F32). bx/tx/nt reproduce blockIdx.x/threadIdx.x/blockDim.x.
//
// Caller-owned bounds: has_cap plus dst_rows is the only index guard. While
// decoding, the src1 read lies inside the span the host already validated. With
// has_cap=true a thread whose dst_row is negative or >= dst_rows returns before the
// destination store; kernel topology and other threads are unchanged. When bounds
// cannot be checked the caller must pass has_cap=false, which is plain native
// behaviour (out-of-range row indices are undefined in ggml_set_rows). Nothing here
// synchronizes host code against GPU indices.
//
// Width: decode runs in 64-bit arithmetic. The host refuses i >= ne_total,
// ne_total*32 > 2^32 (Q4_0) or ne_total > 2^32 (F32), and any of ne00/ne01/ne02/
// ne11/ne12 above UINT32_MAX, so the native uint32_t narrowing cannot truncate and
// 64-bit decode is bit-identical to native fast_div_modulo/fastmodulo there.
extern "fg176r_ld_f32" fn ld_f32(src:u64,byte:u64)->f32;
extern "fg176r_st_f32" fn st_f32(dst:u64,byte:u64,v:f32);
extern "fg176r_st_f16" fn st_f16(dst:u64,byte:u64,v:f32);
extern "fg176r_st_u8" fn st_u8(dst:u64,byte:u64,v:u32);
extern "fg176r_ld_i32" fn ld_i32(src:u64,byte:u64)->i64;
extern "fg176r_ld_i64" fn ld_i64(src:u64,byte:u64)->i64;
extern "fg176r_fabs" fn fabsv(v:f32)->f32;
extern "fg176r_i8trunc" fn i8trunc(v:f32)->i32;
extern "fg176r_pdl_sync" fn pdl_sync();
extern "fg176r_pdl_lc" fn pdl_lc();
struct Fg176rIdx{i0:i64,i1:i64,i2:i64,i3:i64}
// Native decode: i00=tmp%ne00; tmp/=ne00; i01=tmp%ne01; tmp/=ne01; i02=tmp%ne02;
// i03=tmp/ne02. fast_div_modulo/fastmodulo become exact / and -, equal for any
// positive divisor <= UINT32_MAX.
fn fg176r_decode(base:u64,ne00:i64,ne01:i64,ne02:i64)->Fg176rIdx{
 var tmp:u64=base;
 let n0:u64=ne00 as u64;
 let q0:u64=tmp/n0;
 let r0:u64=tmp-q0*n0;
 let n1:u64=ne01 as u64;
 let q1:u64=q0/n1;
 let r1:u64=q0-q1*n1;
 let n2:u64=ne02 as u64;
 let q2:u64=q1/n2;
 let r2:u64=q1-q2*n2;
 return Fg176rIdx{r0 as i64,r1 as i64,r2 as i64,q2 as i64};
}
// quantize_f32_q4_0_block: sb is the byte offset of 32 source floats from src; db
// is the byte offset of the 18-byte destination block from dst.
fn fg176r_quant_q4_0(src:u64,sb:u64,dst:u64,db:u64){
 let zero:f32=0.0;
 let neg8:f32=-8.0;
 let bias:f32=8.5;
 let one:f32=1.0;
 let four:u64=4;
 let nqk:i32=32;
 let halfn:i32=16;
 let nbyte:i32=16;
 let maxq:i32=15;
 let mask:i32=255;
 var amax:f32=zero;
 var vmax:f32=zero;
 var j:i32=0;
 while j<nqk{
  let o:u64=sb+((j as i64) as u64)*four;
  let v:f32=ld_f32(src,o);
  let a:f32=fabsv(v);
  if amax<a{amax=a;vmax=v;}
  j+=1;
 }
 let d:f32=vmax/neg8;
 var id:f32=zero;
 if d!=zero{id=one/d;}
 st_f16(dst,db,d);
 var k:i32=0;
 while k<nbyte{
  let o0:u64=sb+((k as i64) as u64)*four;
  let o1:u64=sb+(((k+halfn) as i64) as u64)*four;
  let x0:f32=ld_f32(src,o0)*id;
  let x1:f32=ld_f32(src,o1)*id;
  let t0:i32=i8trunc(x0+bias);
  let t1:i32=i8trunc(x1+bias);
  let q0:i32=if t0>maxq{maxq}else{t0};
  let q1:i32=if t1>maxq{maxq}else{t1};
  let lo:u32=(q0&mask) as u32;
  let hi:u32=(q1&mask) as u32;
  let h4:u32=(hi<<(4 as u32))&(mask as u32);
  st_u8(dst,db+(2 as u64)+((k as i64) as u64),lo|h4);
  k+=1;
 }
}
// k_set_rows_quant body: one thread per 32-element block, i = nt*bx + tx.
fn fg176r_set_rows_q4_0(src0:u64,src1:u64,dst:u64,ne_total:i64,ne00:i64,ne01:i64,ne02:i64,ne11:i64,ne12:i64,s01:i64,s02:i64,s03:i64,s10:i64,s11:i64,s12:i64,s1:i64,s2:i64,s3:i64,dst_rows:i64,has_cap:bool,explicittype:bool,bx:i32,tx:i32,nt:i32){
 let i:i64=(nt as i64)*(bx as i64)+(tx as i64);
 if i>=ne_total{return;}
 let idx=fg176r_decode((i*(32 as i64)) as u64,ne00,ne01,ne02);
 let i00:i64=idx.i0;
 let i01:i64=idx.i1;
 let i02:i64=idx.i2;
 let i03:i64=idx.i3;
 let i12:i64=i03%ne12;
 let i11:i64=i02%ne11;
 let i10:i64=i01;
 pdl_sync();
 let ioff:i64=i10*s10+i11*s11+i12*s12;
 let dst_row:i64=if explicittype{ld_i64(src1,(ioff*(8 as i64)) as u64)}else{ld_i32(src1,(ioff*(4 as i64)) as u64)};
 if has_cap{if dst_row<(0 as i64){return;}if dst_row>=dst_rows{return;}}
 let blk:i64=(dst_row*s1+i02*s2+i03*s3)/(18 as i64);
 let dptr:i64=(blk+i00/(32 as i64))*(18 as i64);
 let sptr:i64=(i01*s01+i02*s02+i03*s03+i00)*(4 as i64);
 fg176r_quant_q4_0(src0,sptr as u64,dst,dptr as u64);
}
// k_set_rows body with src_t=dst_t=f32: one thread per element, i = nt*bx + tx.
fn fg176r_set_rows_f32(src0:u64,src1:u64,dst:u64,ne_total:i64,ne00:i64,ne01:i64,ne02:i64,ne11:i64,ne12:i64,s01:i64,s02:i64,s03:i64,s10:i64,s11:i64,s12:i64,s1:i64,s2:i64,s3:i64,dst_rows:i64,has_cap:bool,explicittype:bool,bx:i32,tx:i32,nt:i32){
 let i:i64=(nt as i64)*(bx as i64)+(tx as i64);
 if i>=ne_total{return;}
 let idx=fg176r_decode(i as u64,ne00,ne01,ne02);
 let i00:i64=idx.i0;
 let i01:i64=idx.i1;
 let i02:i64=idx.i2;
 let i03:i64=idx.i3;
 let i12:i64=i03%ne12;
 let i11:i64=i02%ne11;
 let i10:i64=i01;
 pdl_sync();
 let ioff:i64=i10*s10+i11*s11+i12*s12;
 let dst_row:i64=if explicittype{ld_i64(src1,(ioff*(8 as i64)) as u64)}else{ld_i32(src1,(ioff*(4 as i64)) as u64)};
 pdl_lc();
 if has_cap{if dst_row<(0 as i64){return;}if dst_row>=dst_rows{return;}}
 let sptr:i64=(i01*s01+i02*s02+i03*s03+i00)*(4 as i64);
 let dptr:i64=(dst_row*s1+i02*s2+i03*s3+i00)*(4 as i64);
 st_f32(dst,dptr as u64,ld_f32(src0,sptr as u64));
}
// Retained entry; the parent never executes main. ne_total=0 makes both bodies
// return before any decode or memory effect.
fn main(){
 fg176r_set_rows_q4_0(0 as u64,0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,false,false,0,0,0);
 fg176r_set_rows_f32(0 as u64,0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,false,false,0,0,0);
}
