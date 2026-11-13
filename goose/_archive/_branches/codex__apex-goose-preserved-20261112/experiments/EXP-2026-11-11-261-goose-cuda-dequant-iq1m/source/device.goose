// Apex CUDA IQ1_M dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq1_m. Last dequantize.cuh path; closes the family with
// EXP182/188/248-260.
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ1_M).
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the ib16 sub-block index, the half scale pattern assembled from
// four uint16 words, the 3-bit scale extraction from that same word, the delta
// bit and its il-dependent mask, the 11-bit grid index, the nibble split of the
// grid word, destination i*QK_K + 32*ib + 8*il and d*(q+delta). Extern symbols
// are raw loads, the half-bit-pattern conversion and the grid table read.
//
// Layout of block_iq1_m (56 bytes): uint8_t qs[32] at 0, uint8_t qh[16] at 32,
// uint8_t scales[8] at 48. There is NO d field in this block; the scale is
// packed inside the scales bytes. QK_K = 256.
extern "fg261d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg261d_ld_u16" fn ld_u16(vx:u64,o:u64)->u32;
extern "fg261d_f16" fn f16(u:u32)->f32;
extern "fg261d_grid" fn grid(i:u32)->u32;
extern "fg261d_st_f32" fn st_f32(yy:u64,o:u64,v:f32);
extern "fg261d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// One 256-value super-block, 32 threads, F32 destination.
fn iq1_m(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(56 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let s0:u32=ld_u16(vx,((blk+48 as i64) as u64));
 let s1:u32=ld_u16(vx,((blk+50 as i64) as u64));
 let s2:u32=ld_u16(vx,((blk+52 as i64) as u64));
 let s3:u32=ld_u16(vx,((blk+54 as i64) as u64));
 let pat:u32=(s0>>(12 as u32))|((s1>>(8 as u32))&(0x00f0 as u32))
           |((s2>>(4 as u32))&(0x0f00 as u32))|(s3&(0xf000 as u32));
 let dbase:f32=f16(pat);
 let ib16:i64=2*ib+il/2;
 let sco:i64=blk+48 as i64+2*(ib16/4);
 let scw:u32=ld_u16(vx,(sco) as u64);
 let shs:u32=(3*(ib16%4)) as u32;
 let sc3:i64=((scw>>shs)&(7 as u32)) as i64;
 let d:f32=dbase*(((2*sc3)+(1 as i64)) as f32);
 let qhb:u32=ld_u8(vx,((blk+32 as i64+2*ib+il/2) as u64));
 let shg:u32=(4*(il%2)) as u32;
 let deltam:u32=(0x08 as u32)<<shg;
 var delta:f32=-0.875 as f32;
 if (qhb&deltam)>(0 as u32){
  delta=-1.125 as f32;
 }
 let idx:u32=ld_u8(vx,((blk+4*ib+il) as u64))|(((qhb>>shg)&(7 as u32))<<(8 as u32));
 let g:u32=grid(idx);
 let g0:u32=g&(0x0f0f0f0f as u32);
 let g1:u32=(g>>(4 as u32))&(0x0f0f0f0f as u32);
 var j:i64=0;
 while j<8 as i64{
  var w:u32=0;
  if j<4 as i64{
   let sj:u32=(j*8) as u32;
   w=g0>>sj;
  }else{
   let sj:u32=((j-4 as i64)*8) as u32;
   w=g1>>sj;
  }
  let qb:i64=(w&(255 as u32)) as i64;
  st_f32(yy,(((ybase+j)*(4 as i64)) as u64),d*(((qb) as f32)+delta));
  j=j+1;
 }
}

// Retained entry; the parent never executes main.
fn main(){iq1_m(0,0,0 as i64,0 as i64);}
