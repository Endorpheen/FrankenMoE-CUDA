// Apex CUDA 32-element legacy dequantization callbacks in Goose, from
// ggml-cuda/dequantize.cuh: dequantize_q1_0, q2_0, q4_0, q4_1, q5_0, q5_1,
// q8_0. These are the two-value callbacks consumed by the generic
// dequantize_block<qk, qr, fn> kernel; the generic kernel itself is a shared
// template and is not part of this module.
//
// Everything algorithmic is Goose: the bit/byte index arithmetic, the nibble
// and 2-bit splits, the high-bit plane for q5, the signed-byte interpretation
// for q8_0, the affine reconstructions and the two destination stores. Extern
// symbols are raw byte/word/half loads and a float store. The output pair is
// written through the same float2 interface the native callbacks use.
//
// Block layouts:
//   q1_0 18 bytes: d at 0, qs[16] at 2   (QK1_0 = 128, QR1_0 = 1)
//   q2_0 18 bytes: d at 0, qs[16] at 2   (QK2_0 = 64,  QR2_0 = 1)
//   q4_0 18 bytes: d at 0, qs[16] at 2   (QK4_0 = 32,  QR4_0 = 2)
//   q4_1 20 bytes: dm at 0, qs[16] at 4  (d at 0, m at 2)
//   q5_0 22 bytes: d at 0, qh[4] at 2, qs[16] at 6
//   q5_1 24 bytes: dm at 0, qh[4] at 4, qs[16] at 8
//   q8_0 34 bytes: d at 0, int8 qs[32] at 2
extern "fg262d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg262d_ld_i8" fn ld_i8(vx:u64,o:u64)->i32;
extern "fg262d_ld_u32" fn ld_u32(vx:u64,o:u64)->u32;
extern "fg262d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg262d_st_f32" fn st_f32(out:u64,o:u64,v:f32);
extern "fg262d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn q1_0(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(18 as i64);
 let d:f32=ld_half(vx,(blk) as u64);
 let b0:i64=iqs;
 let b1:i64=iqs+1 as i64;
 let by0:i64=b0/8;
 let of0:i64=b0%8;
 let by1:i64=b1/8;
 let of1:i64=b1%8;
 let k0:u32=(ld_u8(vx,((blk+2 as i64+by0) as u64))>>((of0) as u32))&(1 as u32);
 let k1:u32=(ld_u8(vx,((blk+2 as i64+by1) as u64))>>((of1) as u32))&(1 as u32);
 st_f32(out,(0 as u64),(((2*(k0 as i64))-(1 as i64)) as f32)*d);
 st_f32(out,(4 as u64),(((2*(k1 as i64))-(1 as i64)) as f32)*d);
}

fn q2_0(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(18 as i64);
 let d:f32=ld_half(vx,(blk) as u64);
 let b0:i64=iqs;
 let b1:i64=iqs+1 as i64;
 let by0:i64=b0/4;
 let of0:i64=(b0%4)*2;
 let by1:i64=b1/4;
 let of1:i64=(b1%4)*2;
 let c0:u32=(ld_u8(vx,((blk+2 as i64+by0) as u64))>>((of0) as u32))&(3 as u32);
 let c1:u32=(ld_u8(vx,((blk+2 as i64+by1) as u64))>>((of1) as u32))&(3 as u32);
 st_f32(out,(0 as u64),(((c0 as i64)-(1 as i64)) as f32)*d);
 st_f32(out,(4 as u64),(((c1 as i64)-(1 as i64)) as f32)*d);
}

fn q4_0(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(18 as i64);
 let d:f32=ld_half(vx,(blk) as u64);
 let v:u32=ld_u8(vx,((blk+2 as i64+iqs) as u64));
 let lo:i64=(v&(15 as u32)) as i64;
 let hi:i64=(v>>(4 as u32)) as i64;
 st_f32(out,(0 as u64),(((lo) as f32)-(8.0 as f32))*d);
 st_f32(out,(4 as u64),(((hi) as f32)-(8.0 as f32))*d);
}

fn q4_1(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(20 as i64);
 let dx:f32=ld_half(vx,(blk) as u64);
 let dy:f32=ld_half(vx,(blk+2 as i64) as u64);
 let v:u32=ld_u8(vx,((blk+4 as i64+iqs) as u64));
 let lo:i64=(v&(15 as u32)) as i64;
 let hi:i64=(v>>(4 as u32)) as i64;
 st_f32(out,(0 as u64),((lo) as f32)*dx+dy);
 st_f32(out,(4 as u64),((hi) as f32)*dx+dy);
}

fn q5_0(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(22 as i64);
 let d:f32=ld_half(vx,(blk) as u64);
 let qh:u32=ld_u32(vx,(blk+2 as i64) as u64);
 let s0:u32=(iqs) as u32;
 let s1:u32=(iqs+12 as i64) as u32;
 let xh0:u32=((qh>>s0)<<(4 as u32))&(0x10 as u32);
 let xh1:u32=(qh>>s1)&(0x10 as u32);
 let v:u32=ld_u8(vx,((blk+6 as i64+iqs) as u64));
 let lo:i64=((v&(15 as u32))|xh0) as i64;
 let hi:i64=((v>>(4 as u32))|xh1) as i64;
 st_f32(out,(0 as u64),(((lo) as f32)-(16.0 as f32))*d);
 st_f32(out,(4 as u64),(((hi) as f32)-(16.0 as f32))*d);
}

fn q5_1(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(24 as i64);
 let dx:f32=ld_half(vx,(blk) as u64);
 let dy:f32=ld_half(vx,(blk+2 as i64) as u64);
 let qh:u32=ld_u32(vx,(blk+4 as i64) as u64);
 let s0:u32=(iqs) as u32;
 let s1:u32=(iqs+12 as i64) as u32;
 let xh0:u32=((qh>>s0)<<(4 as u32))&(0x10 as u32);
 let xh1:u32=(qh>>s1)&(0x10 as u32);
 let v:u32=ld_u8(vx,((blk+8 as i64+iqs) as u64));
 let lo:i64=((v&(15 as u32))|xh0) as i64;
 let hi:i64=((v>>(4 as u32))|xh1) as i64;
 st_f32(out,(0 as u64),((lo) as f32)*dx+dy);
 st_f32(out,(4 as u64),((hi) as f32)*dx+dy);
}

fn q8_0(vx:u64,ib:i64,iqs:i64,out:u64){
 ensure(iqs>=0 as i64,10);
 let blk:i64=ib*(34 as i64);
 let d:f32=ld_half(vx,(blk) as u64);
 let x0:i64=(ld_i8(vx,((blk+2 as i64+iqs) as u64))) as i64;
 let x1:i64=(ld_i8(vx,((blk+3 as i64+iqs) as u64))) as i64;
 st_f32(out,(0 as u64),((x0) as f32)*d);
 st_f32(out,(4 as u64),((x1) as f32)*d);
}

// Call order defines the entry mapping emitted into entries.h.
fn main(){
 q1_0(0,0,0,0);q2_0(0,0,0,0);q4_0(0,0,0,0);q4_1(0,0,0,0);
 q5_0(0,0,0,0);q5_1(0,0,0,0);q8_0(0,0,0,0);
}
