// fg176r: Apex cache/state SET_ROWS device path (FriedGoose self-hosted stage 3).
// Bounded literal port of ggml-cuda/set-rows.cu; no C++ or original math is called:
//   k_set_rows_quant<idx_t, block_q4_0, 32, quantize_f32_q4_0_block>  (src0 F32 -> dst Q4_0)
//   k_set_rows<src_t, idx_t, dst_t> instanced as F32 -> F32 only.
// Grid/thread mapping, 32-bit index decode, src1 row-index load, Q4_0 signed
// absmax selection, d = vmax/-8, id = 1/d, (int8_t)(x*id + 8.5), min(15,..)
// clipping and nibble packing all live in Goose. Only the raw scalar effects
// declared below are native. No quantizer, cast helper, math callback, fastdiv
// table or C++ function is invoked on this path.
//
// Q4_0 block layout (ggml-common.h): 18 bytes = half d at byte 0 (RN store),
// qs[16] at bytes 2..17. qs[j] low nibble = element j, high nibble = element
// j+16, exactly as quantize_f32_q4_0_block (cpy-utils.cuh) packs it.
//
// Raw ABI: pointers are u64 global byte addresses; every offset is a byte
// offset from its pointer. No state, no shared memory, no heap, no arrays.
//
// No GPU-index safety is claimed. has_cap + dst_rows is the only bound: when
// has_cap is true a block with dst_row<0 or dst_row>=dst_rows returns before any
// store, with unchanged kernel topology. Caller must pass real storage spans,
// nonnegative whole-element strides, ne00%32==0 for Q4_0, and in-range src1
// values when has_cap is false. Unsupported element types are rejected by the
// host bridge; see contract.json.
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
// Native index decode: tmp=(u32)base; i0=tmp%ne00; tmp/=ne00; i1=tmp%ne01;
// tmp/=ne01; i2=tmp%ne02; i3=tmp/ne02. fast_div_modulo/fastmodulo replaced by
// logical / and % (identical for ne00,ne01,ne02 in 1..UINT32_MAX).
fn fg176r_decode(base:u32,ne00:i64,ne01:i64,ne02:i64)->Fg176rIdx{
 var tmp:u32=base;
 let n0=(ne00 as u64) as u32;
 let i0=tmp%n0;
 tmp=tmp/n0;
 let n1=(ne01 as u64) as u32;
 let i1=tmp%n1;
 tmp=tmp/n1;
 let n2=(ne02 as u64) as u32;
 let i2=tmp%n2;
 let i3=tmp/n2;
 return Fg176rIdx{i0 as i64,i1 as i64,i2 as i64,i3 as i64};
}
// quantize_f32_q4_0_block: sb = byte offset of 32 source floats from src;
// db = byte offset of the 18-byte destination block from dst.
fn fg176r_quant_q4_0(src:u64,sb:u64,dst:u64,db:u64){
 var amax:f32=0.0;
 var vmax:f32=0.0;
 var j:i32=0;
 while j<32{
  let v=ld_f32(src,sb+((j as i64) as u64)*(4 as u64));
  let a=fabsv(v);
  if amax<a{amax=a;vmax=v;}
  j+=1;
 }
 let d=vmax/(-8.0);
 var id:f32=0.0;
 if d!=0.0{id=1.0/d;}
 st_f16(dst,db,d);
 var k:i32=0;
 while k<16{
  let x0=ld_f32(src,sb+((k as i64) as u64)*(4 as u64))*id;
  let x1=ld_f32(src,sb+(((k+16) as i64) as u64)*(4 as u64))*id;
  let t0=i8trunc(x0+8.5);
  let t1=i8trunc(x1+8.5);
  let q0=if t0>15{15}else{t0};
  let q1=if t1>15{15}else{t1};
  let lo=(q0&(255 as i32)) as u32;
  let hi=(q1&(255 as i32)) as u32;
  let h4=(hi<<(4 as u32))&(255 as u32);
  st_u8(dst,db+(2 as u64)+((k as i64) as u64),lo+h4-(lo&h4));
  k+=1;
 }
}
// k_set_rows_quant body: one thread per 32-element block, i=nt*bx+tx.
fn fg176r_set_rows_q4_0(src0:u64,src1:u64,dst:u64,ne_total:i64,ne00:i64,ne01:i64,ne02:i64,ne11:i64,ne12:i64,s01:i64,s02:i64,s03:i64,s10:i64,s11:i64,s12:i64,s1:i64,s2:i64,s3:i64,dst_rows:i64,has_cap:bool,explicittype:bool,bx:i32,tx:i32,nt:i32){
 let i=(nt as i64)*(bx as i64)+(tx as i64);
 if i>=ne_total{return;}
 let idx=fg176r_decode(((i*(32 as i64)) as u64) as! u32,ne00,ne01,ne02);
 let i00=idx.i0;
 let i01=idx.i1;
 let i02=idx.i2;
 let i03=idx.i3;
 let i12=i03%ne12;
 let i11=i02%ne11;
 let i10=i01;
 pdl_sync();
 let ioff=i10*s10+i11*s11+i12*s12;
 let dst_row=if explicittype{ld_i64(src1,(ioff as u64)*(8 as u64))}else{ld_i32(src1,(ioff as u64)*(4 as u64))};
 if has_cap{if dst_row<(0 as i64){return;}if dst_row>=dst_rows{return;}}
 let dblk=(dst_row*s1+i02*s2+i03*s3)/(18 as i64);
 let dptr=(dblk+i00/(32 as i64))*(18 as i64);
 let sptr=(i01*s01+i02*s02+i03*s03+i00)*(4 as i64);
 fg176r_quant_q4_0(src0,sptr as u64,dst,dptr as u64);
}
// k_set_rows body instanced F32->F32: one thread per element, i=nt*bx+tx.
fn fg176r_set_rows_f32(src0:u64,src1:u64,dst:u64,ne_total:i64,ne00:i64,ne01:i64,ne02:i64,ne11:i64,ne12:i64,s01:i64,s02:i64,s03:i64,s10:i64,s11:i64,s12:i64,s1:i64,s2:i64,s3:i64,dst_rows:i64,has_cap:bool,explicittype:bool,bx:i32,tx:i32,nt:i32){
 let i=(nt as i64)*(bx as i64)+(tx as i64);
 if i>=ne_total{return;}
 let idx=fg176r_decode((i as u64) as! u32,ne00,ne01,ne02);
 let i00=idx.i0;
 let i01=idx.i1;
 let i02=idx.i2;
 let i03=idx.i3;
 let i12=i03%ne12;
 let i11=i02%ne11;
 let i10=i01;
 pdl_sync();
 let ioff=i10*s10+i11*s11+i12*s12;
 let dst_row=if explicittype{ld_i64(src1,(ioff as u64)*(8 as u64))}else{ld_i32(src1,(ioff as u64)*(4 as u64))};
 pdl_lc();
 if has_cap{if dst_row<(0 as i64){return;}if dst_row>=dst_rows{return;}}
 let sptr=(i01*s01+i02*s02+i03*s03+i00)*(4 as i64);
 let dptr=(dst_row*s1+i02*s2+i03*s3+i00)*(4 as i64);
 st_f32(dst,dptr as u64,ld_f32(src0,sptr as u64));
}
// Retained entry; parent never executes main (zero arguments only).
fn main(){
 fg176r_set_rows_q4_0(0 as u64,0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,false,false,0,0,0);
 fg176r_set_rows_f32(0 as u64,0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as i64,false,false,0,0,0);
}
