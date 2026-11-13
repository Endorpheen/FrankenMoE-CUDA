// Apex CUDA Q5_K dequantization to half and bfloat16 (Goose), from
// ggml-cuda/dequantize.cuh: dequantize_q5_K<dst_t> for half and nv_bfloat16.
// EXP249 covered the F32 destination.
//
// Arithmetic is the F32 module's: 64-thread (il = tid/16, ir = tid%16,
// is = 2*il), base i*QK_K + 64*il + 2*ir, packed dm scale/min, both 6-bit pairs,
// the qh high-bit plane with hm = 1<<2*il then hm<<1, and d*q - m. Only the
// store differs (__float2half / __float2bfloat16).
//
// Layout of block_q5_K (176 bytes): ggml_half2 dm at 0 (d at 0, dmin at 2),
// uint8_t scales[12] at 4, uint8_t qh[32] at 16, uint8_t qs[128] at 48.
extern "fg265d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg265d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg265d_st_half" fn st_half(yy:u64,o:u64,v:f32);
extern "fg265d_st_bf16" fn st_bf16(yy:u64,o:u64,v:f32);
extern "fg265d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

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
 (dv<<8)|mv
}

fn hib(b:i64)->i64{
 if b>0 as i64{return 16 as i64;}
 0 as i64
}

fn q5_K_f16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<64 as i64,10);
 let il:i64=tid/16;
 let ir:i64=tid%16;
 let is:i64=2*il;
 let blk:i64=i*(176 as i64);
 let ybase:i64=i*(256 as i64)+64*il+2*ir;
 let dall:f32=ld_half(vx,(blk) as u64);
 let dmin:f32=ld_half(vx,((blk+2 as i64) as u64));
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
 let ql1:u32=ld_u8(vx,((ql+1 as i64) as u64));
 let qh0:u32=ld_u8(vx,(qh) as u64);
 let qh1:u32=ld_u8(vx,((qh+1 as i64) as u64));
 let a0:i64=((ql0&15) as i64)+hib((qh0 as i64)&hm0);
 let a1:i64=((ql1&15) as i64)+hib((qh1 as i64)&hm0);
 let b0:i64=((ql0>>4) as i64)+hib((qh0 as i64)&hm1);
 let b1:i64=((ql1>>4) as i64)+hib((qh1 as i64)&hm1);
 st_half(yy,((ybase)*(2 as i64)) as u64,d1*((a0) as f32)-m1);
 st_half(yy,((ybase+1 as i64)*(2 as i64)) as u64,d1*((a1) as f32)-m1);
 st_half(yy,((ybase+32 as i64)*(2 as i64)) as u64,d2*((b0) as f32)-m2);
 st_half(yy,((ybase+33 as i64)*(2 as i64)) as u64,d2*((b1) as f32)-m2);
}

fn q5_K_bf16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<64 as i64,10);
 let il:i64=tid/16;
 let ir:i64=tid%16;
 let is:i64=2*il;
 let blk:i64=i*(176 as i64);
 let ybase:i64=i*(256 as i64)+64*il+2*ir;
 let dall:f32=ld_half(vx,(blk) as u64);
 let dmin:f32=ld_half(vx,((blk+2 as i64) as u64));
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
 let ql1:u32=ld_u8(vx,((ql+1 as i64) as u64));
 let qh0:u32=ld_u8(vx,(qh) as u64);
 let qh1:u32=ld_u8(vx,((qh+1 as i64) as u64));
 let a0:i64=((ql0&15) as i64)+hib((qh0 as i64)&hm0);
 let a1:i64=((ql1&15) as i64)+hib((qh1 as i64)&hm0);
 let b0:i64=((ql0>>4) as i64)+hib((qh0 as i64)&hm1);
 let b1:i64=((ql1>>4) as i64)+hib((qh1 as i64)&hm1);
 st_bf16(yy,((ybase)*(2 as i64)) as u64,d1*((a0) as f32)-m1);
 st_bf16(yy,((ybase+1 as i64)*(2 as i64)) as u64,d1*((a1) as f32)-m1);
 st_bf16(yy,((ybase+32 as i64)*(2 as i64)) as u64,d2*((b0) as f32)-m2);
 st_bf16(yy,((ybase+33 as i64)*(2 as i64)) as u64,d2*((b1) as f32)-m2);
}

fn main(){q5_K_f16(0,0,0 as i64,0 as i64);q5_K_bf16(0,0,0 as i64,0 as i64);}
