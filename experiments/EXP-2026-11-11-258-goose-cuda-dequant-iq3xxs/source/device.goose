// Apex CUDA IQ3_XXS dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq3_xxs. First 3-bit grid path after the IQ2 family EXP255-257.
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ3_XXS).
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the two grid indices per thread, the packed gas words at
// qs + QK_K/4, the exponent aux32>>28, the per-il sign index, the destination
// base i*QK_K + 32*ib + 8*il, the 4-byte grid extraction and the conditional
// sign. The shared tables iq3xxs_grid and ksigns_iq2xs are DATA, imported
// through explicit constant primitives.
//
// Layout of block_iq3_xxs (98 bytes): ggml_half d at 0, uint8_t qs[96] at 2.
// QK_K = 256.
extern "fg258d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg258d_ld_u16" fn ld_u16(vx:u64,o:u64)->u32;
extern "fg258d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg258d_grid" fn grid(i:u32)->u32;
extern "fg258d_ksign" fn ksign(i:u32)->u32;
extern "fg258d_st_f32" fn st_f32(yy:u64,o:u64,v:f32);
extern "fg258d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn sgn(b:i64)->f32{
 if b>0 as i64{return -1.0 as f32;}
 1.0 as f32
}

// One 256-value super-block, 32 threads, F32 destination. Each thread writes
// four values from grid1 at ybase..ybase+3 and four from grid2 at +4..+7.
fn iq3_xxs(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(98 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let q3:i64=blk+2 as i64+8*ib;
 let i1:u32=ld_u8(vx,((q3+2*il) as u64));
 let i2:u32=ld_u8(vx,((q3+2*il+1 as i64) as u64));
 let gas:i64=blk+66 as i64+4*ib;
 let g0:u32=ld_u16(vx,(gas) as u64);
 let g1w:u32=ld_u16(vx,((gas+2 as i64) as u64));
 let aux:u32=g0|(g1w<<(16 as u32));
 let e:u32=aux>>(28 as u32);
 let d:f32=ld_half(vx,(blk) as u64)*((0.5 as f32)+((e) as f32))*(0.5 as f32);
 let sh7:u32=(7*il) as u32;
 let sg:u32=ksign((aux>>sh7)&(127 as u32));
 let G1:u32=grid(i1);
 let G2:u32=grid(i2);
 var j:i64=0;
 while j<4 as i64{
  let shk:u64=(j*8) as u64;
  let b1:i64=((G1>>shk)&(255 as u64)) as i64;
  let b2:i64=((G2>>shk)&(255 as u64)) as i64;
  let m1:i64=(1 as i64)<<j;
  let m2:i64=(1 as i64)<<(j+4);
  let q1:i64=(sg as i64)&m1;
  let q2:i64=(sg as i64)&m2;
  st_f32(yy,(((ybase+j)*(4 as i64)) as u64),d*((b1) as f32)*sgn(q1));
  st_f32(yy,(((ybase+j+4 as i64)*(4 as i64)) as u64),d*((b2) as f32)*sgn(q2));
  j=j+1;
 }
}

// Retained entry; the parent never executes main.
fn main(){iq3_xxs(0,0,0 as i64,0 as i64);}
