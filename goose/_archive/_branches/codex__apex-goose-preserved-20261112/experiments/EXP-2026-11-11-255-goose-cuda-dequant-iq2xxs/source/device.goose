// Apex CUDA IQ2_XXS dequantization (Goose), from ggml-cuda/dequantize.cuh:
// dequantize_iq2_xxs. First grid-table IQ path after the lattice-free
// EXP252/253/254 modules.
//
// Scope: the F32 destination case reached through
// ggml_get_to_fp32_cuda(GGML_TYPE_IQ2_XXS).
//
// Everything algorithmic is Goose: the 32-thread decomposition (il = tid/8,
// ib = tid%8), the q2 word layout, the aux8 index, the aux32 exponent and
// per-il sign index, the destination base i*QK_K + 32*ib + 8*il, the
// per-element grid byte extraction and the conditional sign. The two shared
// tables (iq2xxs_grid, ksigns_iq2xs) are DATA, not code: they are imported as
// explicit constant reads through the device primitives, exactly like ld_u8.
// No native arithmetic, index computation or branch is called.
//
// Layout of block_iq2_xxs (66 bytes): ggml_half d at 0, uint16_t qs[32] at 2.
// QK_K = 256.
extern "fg255d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg255d_ld_u16" fn ld_u16(vx:u64,o:u64)->u32;
extern "fg255d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg255d_grid" fn grid(i:u32)->u64;
extern "fg255d_ksign" fn ksign(i:u32)->u32;
extern "fg255d_st_f32" fn st_f32(yy:u64,o:u64,v:f32);
extern "fg255d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn sgn(b:i64)->f32{
 if b>0 as i64{return -1.0 as f32;}
 1.0 as f32
}

// One 256-value super-block, 32 threads, F32 destination. Each thread writes 8
// consecutive values at ybase..ybase+7.
fn iq2_xxs(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(66 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let q2:i64=blk+2 as i64+8*ib;
 let dhalf:f32=ld_half(vx,(blk) as u64);
 let gi:u32=ld_u8(vx,((q2+il) as u64));
 let a2:u32=ld_u16(vx,((q2+4 as i64) as u64));
 let a3:u32=ld_u16(vx,((q2+6 as i64) as u64));
 let aux:u32=a2|(a3<<(16 as u32));
 let e:u32=aux>>(28 as u32);
 let d:f32=dhalf*((0.5 as f32)+((e) as f32))*(0.25 as f32);
 let sh7:u32=(7*il) as u32;
 let sg:u32=ksign((aux>>sh7)&(127 as u32));
 let g:u64=grid(gi);
 var j:i64=0;
 while j<8 as i64{
  let shk:u64=(j*8) as u64;
  let gb:i64=((g>>shk)&(255 as u64)) as i64;
  let m:i64=(1 as i64)<<j;
  let bit:i64=(sg as i64)&m;
  st_f32(yy,(((ybase+j)*(4 as i64)) as u64),d*((gb) as f32)*sgn(bit));
  j=j+1;
 }
}

// Retained entry; the parent never executes main.
fn main(){iq2_xxs(0,0,0 as i64,0 as i64);}
