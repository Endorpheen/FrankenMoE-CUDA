// fg175q: Apex FlashAttention prefill K/V staging device path as Goose, from the generic
// dequantize_block<32,2,dequantize_q4_0,dst_t> in convert.cu with dequantize_q4_0 from
// dequantize.cuh, plus the optimized contiguous dequantize_block_q4_0 from convert.cu.
// Fixed target: CUDA, Apex head D=256, K/V in Q4_0: 18-byte little-endian blocks (half d
// at byte 0, 16 packed qs bytes at byte 2; qs byte b carries element 2b in its low nibble
// and element 2b+1 in its high nibble), strided K/V tensor or contiguous with ne0 multiple
// of 32. All indexing, nibble unpack, scale and multiply arithmetic stays in Goose; only
// raw byte loads/stores, the u16 half load and the raw RN half store are primitive effects.
// No native function is wrapped, no heap/dynamic arrays, no arithmetic bit emulation
// (mask/shift only unpack nibbles). Byte-vs-element: every address is a byte address; half
// outputs are 2 bytes, block_q4_0 stride is 18 = 2 + 16. Parent owns launch policy and
// picks stage (generic, strided-capable) or contiguous per allocation; both are reachable.
struct F2{x:f32,y:f32}
extern "fg175q_ld_u8" fn ld_u8(src:u64,byte:u64)->u32;
extern "fg175q_ld_u16" fn ld_u16(src:u64,byte:u64)->u32;
extern "fg175q_h2f" fn h2f(bits:u32)->f32;
extern "fg175q_st_f16" fn st_f16(dst:u64,byte:u64,v:f32);
// dequantize_q4_0 (dequantize.cuh) on one 18-byte block base: d = half scale bits at byte 0,
// qs byte 2+iqs; v.x=(low nibble - 8.0f)*d, v.y=(high nibble - 8.0f)*d, source order kept.
fn dequant_q4_0(blk:u64,iqs:i64)->F2{let d=h2f(ld_u16(blk,0));let v=ld_u8(blk,(2+iqs) as u64);return F2{((v&(15 as u32)) as f32-8.0)*d,((v>>(4 as u32)) as f32-8.0)*d};}
// Generic dequantize_block (convert.cu) adapted to Apex prefill staging, Q4_0 -> F16.
// Thread x: i00 = 2*(nt*bx+tx) covers two elements (nt = blockDim.x, native 256), return
// when i00>=ne0. Rows i1 start at by and step gy (gridDim.y); flat head-sequence i0203
// starts at bz and steps gz (gridDim.z), i2=i0203%ne2, i3=i0203/ne2 (plain div/mod, native
// fast_div_modulo omitted). Input block base byte = i1*nb1+i2*nb2+i3*nb3+(i00/32)*18.
// Output element iy0 = (i0203*ne1+i1)*ne0+(i00-i00%32)+(i00%32)/2: low nibble half at iy0,
// high at iy0+16 (native iybs+iqs with y_offset=qk/2=16); st_f16 is the RN half store
// matching ggml_cuda_cast<half> for finite values. gx (gridDim.x) is not used inside.
fn stage(src:u64,dst:u64,ne0:i64,ne1:i64,ne2:i64,ne3:i64,nb1:u64,nb2:u64,nb3:u64,bx:i32,by:i32,bz:i32,gx:i32,gy:i32,gz:i32,tx:i32,nt:i32){
 let i00=2*((nt as i64)*(bx as i64)+(tx as i64));
 if i00>=ne0{return;}
 var i1=by as i64;
 while i1<ne1{
  var i0203=bz as i64;
  while i0203<ne2*ne3{
   let i2=i0203%ne2;
   let i3=i0203/ne2;
   let blk=src+(i1 as u64)*nb1+(i2 as u64)*nb2+(i3 as u64)*nb3+(((i00/32)*18) as u64);
   let v=dequant_q4_0(blk,(i00%32)/2);
   let iy0=(i0203*ne1+i1)*ne0+(i00-i00%32)+(i00%32)/2;
   st_f16(dst,(iy0*2) as u64,v.x);
   st_f16(dst,((iy0+16)*2) as u64,v.y);
   i0203+=gz as i64;
  }
  i1+=gy as i64;
 }
}
// Optimized contiguous dequantize_block_q4_0 (convert.cu): one group of 8 q4_0 blocks per
// blockIdx.x with 32 threads; il=tx/8, ir=tx%8, ib=8*bx+ir guarded by total blocks nb32;
// thread (il,ir) reads its four qs bytes 4*il..4*il+3 and writes 4 low + 4 high halves at
// y = 256*bx+32*ir+4*il (+l / +l+16) with the native d*nibble+dm expression, dm = -8*d
// (distinct rounding from the generic (nibble-8)*d form; kept exact per native kernel).
// Parent chooses this path when src/dst are contiguously allocated; not hidden in a primitive.
fn contiguous(src:u64,dst:u64,nb32:i64,bx:i32,tx:i32){
 let bi=bx as i64;
 let tid=tx as i64;
 let il=tid/8;
 let ir=tid%8;
 let ib=8*bi+ir;
 if ib>=nb32{return;}
 let yb=(256*bi+32*ir+4*il)*2;
 let blk=src+((ib*18) as u64);
 let d=h2f(ld_u16(blk,0));
 let dm=0.0-8.0*d;
 var l:i32=0;
 while l<4{
  let q=ld_u8(blk,(2+4*il+(l as i64)) as u64);
  st_f16(dst,(yb+2*(l as i64)) as u64,d*((q&(15 as u32)) as f32)+dm);
  st_f16(dst,(yb+2*((l as i64)+16)) as u64,d*((q>>(4 as u32)) as f32)+dm);
  l+=1;
 }
}
// Retained entries; parent never executes main (dummy zero arguments only).
fn main(){
 stage(0 as u64,0 as u64,0 as i64,0 as i64,0 as i64,0 as i64,0 as u64,0 as u64,0 as u64,0,0,0,0,0,0,0,0);
 contiguous(0 as u64,0 as u64,0 as i64,0,0);
}
