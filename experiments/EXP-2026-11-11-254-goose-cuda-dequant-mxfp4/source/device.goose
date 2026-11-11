// Apex CUDA MXFP4 dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_mxfp4. Third non-K-family path after EXP252 (IQ4_NL) and
// EXP253 (IQ4_XS).
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_MXFP4). The dst_t template also covers half
// and bfloat16 in native; those stay native here.
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the 8-blocks-per-CTA source addressing, the destination base
// i*QK_K + 32*ib + 4*il, the E8M0 exponent conversion, the nibble split, and
// d*kvalues_mxfp4[q]*0.5. kvalues_mxfp4 is kvalues_fp4, reproduced as an
// explicit Goose constant function; the E8M0 conversion is the manual bit path
// used by the native code when CUDART_VERSION < 12080, which is the toolchain
// this profile builds with.
//
// Layout of block_mxfp4 (17 bytes): uint8_t e at 0, uint8_t qs[16] at 1.
// QK_MXFP4 = 32, QK_K = 256, so one CTA covers 8 blocks.
extern "fg254d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg254d_u32_as_f32" fn u32_as_f32(b:u32)->f32;
extern "fg254d_st_f32" fn st_f32(yy:u64,o:u64,v:f32);
extern "fg254d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// kvalues_fp4 = {0,1,2,3,4,6,8,12,0,-1,-2,-3,-4,-6,-8,-12}
fn kv(i:i64)->i64{
 if i==0 as i64{return 0 as i64;}
 if i==1 as i64{return 1 as i64;}
 if i==2 as i64{return 2 as i64;}
 if i==3 as i64{return 3 as i64;}
 if i==4 as i64{return 4 as i64;}
 if i==5 as i64{return 6 as i64;}
 if i==6 as i64{return 8 as i64;}
 if i==7 as i64{return 12 as i64;}
 if i==8 as i64{return 0 as i64;}
 if i==9 as i64{return -1 as i64;}
 if i==10 as i64{return -2 as i64;}
 if i==11 as i64{return -3 as i64;}
 if i==12 as i64{return -4 as i64;}
 if i==13 as i64{return -6 as i64;}
 if i==14 as i64{return -8 as i64;}
 -12 as i64
}

// ggml_cuda_e8m0_to_fp32, manual branch: bits = x ? x << 23 : 0x00400000.
fn e8m0(e:u32)->f32{
 var bits:u32=0;
 let sh:u32=23 as u32;
 if (e==(0 as u32)){
  bits=0x00400000 as u32;
 }else{
  bits=e<<sh;
 }
 u32_as_f32(bits)
}

// One 256-element CTA, 32 threads, F32 destination.
fn mxfp4(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let ib:i64=tid%8;
 let il:i64=tid/8;
 let bidx:i64=i*8+ib;
 let blk:i64=bidx*(17 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+4*il;
 let e:u32=ld_u8(vx,(blk) as u64);
 let d:f32=e8m0(e);
 let qb:i64=blk+1 as i64+4*il;
 var j:i64=0;
 while j<4 as i64{
  let b:u32=ld_u8(vx,((qb+j) as u64));
  let v0:i64=kv((b&15) as i64);
  let v1:i64=kv((b>>4) as i64);
  st_f32(yy,(((ybase+j)*(4 as i64)) as u64),d*((v0) as f32)*(0.5 as f32));
  st_f32(yy,(((ybase+j+16 as i64)*(4 as i64)) as u64),d*((v1) as f32)*(0.5 as f32));
  j=j+1;
 }
}

// Retained entry; the parent never executes main.
fn main(){mxfp4(0,0,0 as i64,0 as i64);}
