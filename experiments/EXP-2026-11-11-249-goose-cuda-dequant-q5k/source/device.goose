// Apex CUDA Q5_K dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_q5_K and its helper get_scale_min_k4. Third dequantisation path
// after EXP182 (Q4_K/F32), EXP188 (Q4_0/F16) and EXP248 (Q6_K/F32).
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_Q5_K). The dst_t template also covers half and
// bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 64-thread decomposition (il = tid/16,
// ir = tid%16, is = 2*il), the destination base 64*il + 2*ir, the packed
// scale/min unpack for both halves, the 4-bit low nibble plus the conditionally
// added high bit from qh, and the affine d*q - m reconstruction. Extern symbols
// are only raw byte loads, two half unpacks and a float store.
//
// Layout of block_q5_K (176 bytes): ggml_half2 dm at 0 (d at 0, dmin at 2),
// uint8_t scales[12] at 4, uint8_t qh[32] at 16, uint8_t qs[128] at 48.
// QK_K = 256, K_SCALE_SIZE = 12.
extern "fg249d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg249d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg249d_st_f32" fn st_f32(yy:u64,o:u64,v:f32);
extern "fg249d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// get_scale_min_k4: returns (d << 8) | m so the caller unpacks without a tuple.
// For j < 4 the values are the low 6 bits of q[j] and q[j+4]; above that the high
// 2 bits come from the top bits of the lower half.
fn scale_min(vx:u64,base:i64,j:i64)->i64{
 var dv:i64=0;
 var mv:i64=0;
 if j<4 as i64{
  dv=(ld_u8(vx,((base+j) as u64))&63) as i64;
  mv=(ld_u8(vx,((base+j+4 as i64) as u64))&63) as i64;
 }else{
  let a:u32=ld_u8(vx,((base+j+4 as i64) as u64));
  let b:u32=ld_u8(vx,((base+j-4 as i64) as u64));
  let c:u32=ld_u8(vx,((base+j) as u64));
  dv=((a&15) as i64)|((((b>>6) as i64)<<4) as i64);
  mv=(((a>>4) as i64))|((((c>>6) as i64)<<4) as i64);
 }
 return (dv<<8)|mv;
}

// `qh` contributes 16 to the value when the mask bit is set, 0 otherwise.
fn hib(b:i64)->i64{
 if b>0 as i64{return 16 as i64;}
 0 as i64
}

// One 256-value super-block, 64 threads, F32 destination.
//   il = tid/16 (0..3) ; ir = tid%16 (0..15) ; is = 2*il (0..6)
//   y  = yy + i*QK_K + 64*il + 2*ir
//   hm = 1 << (2*il), then hm <<= 1 for the high nibbles
fn q5_K(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<64 as i64,10);
 let il:i64=tid/16;
 let ir:i64=tid%16;
 let is:i64=2*il;
 let blk:i64=i*(176 as i64);
 let ybase:i64=i*(256 as i64)+64*il+2*ir;
 let dall:f32=ld_half(vx,(blk) as u64);
 let dmin:f32=ld_half(vx,(blk+2 as i64) as u64);
 let ql:i64=blk+48 as i64+32*il+2*ir;
 let qh:i64=blk+16 as i64+2*ir;
 let s1:i64=scale_min(vx,blk+4 as i64,is);
 let s2:i64=scale_min(vx,blk+4 as i64,is+1);
 let d1:f32=dall*((s1>>8) as f32);
 let m1:f32=dmin*((s1&255) as f32);
 let d2:f32=dall*((s2>>8) as f32);
 let m2:f32=dmin*((s2&255) as f32);
 let hm0:i64=(1 as i64)<<(2*il);
 let hm1:i64=hm0<<1 as i64;
 let ql0:u32=ld_u8(vx,(ql) as u64);
 let ql1:u32=ld_u8(vx,(ql+1 as i64) as u64);
 let qh0:u32=ld_u8(vx,(qh) as u64);
 let qh1:u32=ld_u8(vx,(qh+1 as i64) as u64);
 let a0:i64=((ql0&15) as i64)+hib((qh0 as i64)&hm0);
 let a1:i64=((ql1&15) as i64)+hib((qh1 as i64)&hm0);
 let b0:i64=((ql0>>4) as i64)+hib((qh0 as i64)&hm1);
 let b1:i64=((ql1>>4) as i64)+hib((qh1 as i64)&hm1);
 st_f32(yy,((ybase)*(4 as i64)) as u64,d1*((a0) as f32)-m1);
 st_f32(yy,((ybase+1 as i64)*(4 as i64)) as u64,d1*((a1) as f32)-m1);
 st_f32(yy,((ybase+32 as i64)*(4 as i64)) as u64,d2*((b0) as f32)-m2);
 st_f32(yy,((ybase+33 as i64)*(4 as i64)) as u64,d2*((b1) as f32)-m2);
}

// Retained entry; the parent never executes main.
fn main(){q5_K(0,0,0 as i64,0 as i64);}
