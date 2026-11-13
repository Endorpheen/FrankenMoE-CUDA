// Apex CUDA IQ2_S and IQ3_XXS dequantization to half and bfloat16 (Goose).
// Extends EXP257/EXP258, which covered the F32 destinations.
//
// Arithmetic is the F32 modules': IQ2_S's 10-bit grid index assembled from a qs
// byte and qh[ib] bits, per-sub-block scale, sign byte and 8-value loop;
// IQ3_XXS's two grid indices, packed gas words, exponent, per-il sign index,
// two 4-value groups. iq2s_grid, iq3xxs_grid and ksigns_iq2xs are explicit
// device constant imports. Only the store differs.
//
// Layouts: block_iq2_s 82 bytes (d 0, qs[64] 2, qh[8] 66, scales[8] 74);
// block_iq3_xxs 98 bytes (d 0, qs[96] 2).
extern "fg270d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg270d_ld_u16" fn ld_u16(vx:u64,o:u64)->u32;
extern "fg270d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg270d_grid_s" fn grid_s(i:u32)->u64;
extern "fg270d_grid3" fn grid3(i:u32)->u32;
extern "fg270d_ksign" fn ksign(i:u32)->u32;
extern "fg270d_st_half" fn st_half(yy:u64,o:u64,v:f32);
extern "fg270d_st_bf16" fn st_bf16(yy:u64,o:u64,v:f32);
extern "fg270d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn sgn(b:i64)->f32{
 if b>0 as i64{return -1.0 as f32;}
 1.0 as f32
}

fn q2_s_f16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(82 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let qs:i64=blk+2 as i64;
 let lo:u32=ld_u8(vx,((qs+4*ib+il) as u64));
 let qhb:u32=ld_u8(vx,((blk+66 as i64+ib) as u64));
 let shi:u32=(8-2*il) as u32;
 let hb:u32=(qhb<<shi)&(0x300 as u32);
 let gi:u32=lo|hb;
 let sc:u32=ld_u8(vx,((blk+74 as i64+ib) as u64));
 let sh:u32=(4*(il/2)) as u32;
 let d:f32=ld_half(vx,(blk) as u64)*((0.5 as f32)+(((sc>>sh)&(15 as u32)) as f32))*(0.25 as f32);
 let sgnb:u32=ld_u8(vx,((qs+32 as i64+4*ib+il) as u64));
 let g:u64=grid_s(gi);
 var j:i64=0;
 while j<8 as i64{
  let shk:u64=(j*8) as u64;
  let gb:i64=((g>>shk)&(255 as u64)) as i64;
  let m:i64=(1 as i64)<<j;
  let bit:i64=(sgnb as i64)&m;
  st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*((gb) as f32)*sgn(bit));
  j=j+1;
 }
}
fn q2_s_bf16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(82 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let qs:i64=blk+2 as i64;
 let lo:u32=ld_u8(vx,((qs+4*ib+il) as u64));
 let qhb:u32=ld_u8(vx,((blk+66 as i64+ib) as u64));
 let shi:u32=(8-2*il) as u32;
 let hb:u32=(qhb<<shi)&(0x300 as u32);
 let gi:u32=lo|hb;
 let sc:u32=ld_u8(vx,((blk+74 as i64+ib) as u64));
 let sh:u32=(4*(il/2)) as u32;
 let d:f32=ld_half(vx,(blk) as u64)*((0.5 as f32)+(((sc>>sh)&(15 as u32)) as f32))*(0.25 as f32);
 let sgnb:u32=ld_u8(vx,((qs+32 as i64+4*ib+il) as u64));
 let g:u64=grid_s(gi);
 var j:i64=0;
 while j<8 as i64{
  let shk:u64=(j*8) as u64;
  let gb:i64=((g>>shk)&(255 as u64)) as i64;
  let m:i64=(1 as i64)<<j;
  let bit:i64=(sgnb as i64)&m;
  st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*((gb) as f32)*sgn(bit));
  j=j+1;
 }
}

fn q3_xxs_f16(vx:u64,yy:u64,i:i64,tid:i64){
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
 let G1:u32=grid3(i1);
 let G2:u32=grid3(i2);
 var j:i64=0;
 while j<4 as i64{
  let shk:u64=(j*8) as u64;
  let b1:i64=((G1>>shk)&(255 as u64)) as i64;
  let b2:i64=((G2>>shk)&(255 as u64)) as i64;
  let m1:i64=(1 as i64)<<j;
  let m2:i64=(1 as i64)<<(j+4);
  let q1:i64=(sg as i64)&m1;
  let q2:i64=(sg as i64)&m2;
  st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*((b1) as f32)*sgn(q1));
  st_half(yy,(((ybase+j+4 as i64)*(2 as i64)) as u64),d*((b2) as f32)*sgn(q2));
  j=j+1;
 }
}
fn q3_xxs_bf16(vx:u64,yy:u64,i:i64,tid:i64){
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
 let G1:u32=grid3(i1);
 let G2:u32=grid3(i2);
 var j:i64=0;
 while j<4 as i64{
  let shk:u64=(j*8) as u64;
  let b1:i64=((G1>>shk)&(255 as u64)) as i64;
  let b2:i64=((G2>>shk)&(255 as u64)) as i64;
  let m1:i64=(1 as i64)<<j;
  let m2:i64=(1 as i64)<<(j+4);
  let q1:i64=(sg as i64)&m1;
  let q2:i64=(sg as i64)&m2;
  st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*((b1) as f32)*sgn(q1));
  st_bf16(yy,(((ybase+j+4 as i64)*(2 as i64)) as u64),d*((b2) as f32)*sgn(q2));
  j=j+1;
 }
}

fn main(){
 q2_s_f16(0,0,0 as i64,0 as i64);
 q2_s_bf16(0,0,0 as i64,0 as i64);
 q3_xxs_f16(0,0,0 as i64,0 as i64);
 q3_xxs_bf16(0,0,0 as i64,0 as i64);
}
