// Apex CUDA IQ3_S and IQ1_S dequantization to half and bfloat16 (Goose).
// Extends EXP259/EXP260, which covered the F32 destinations.
//
// Arithmetic is the F32 modules': IQ3_S's two 9-bit grid indices from qs bytes
// and qh[ib] bits, per-32 scale 1+2*((scales[ib/2]>>4*(ib%2))&0xf), sign byte
// and two 4-value groups; IQ1_S's 11-bit grid index, delta from qh bit 15, scale
// 2*((qh>>12)&7)+1 and the nibble split. iq3s_grid, iq1s_grid_gpu are explicit
// device constant imports. Only the store differs.
//
// Layouts: block_iq3_s 110 bytes (d 0, qs[64] 2, qh[8] 66, signs[32] 74,
// scales[4] 106); block_iq1_s 50 bytes (d 0, qs[32] 2, qh[8] u16 at 34).
extern "fg271d_ld_u8" fn ld_u8(vx:u64,o:u64)->u32;
extern "fg271d_ld_u16" fn ld_u16(vx:u64,o:u64)->u32;
extern "fg271d_ld_half" fn ld_half(vx:u64,o:u64)->f32;
extern "fg271d_grid_s" fn grid_s(i:u32)->u32;
extern "fg271d_grid_1" fn grid_1(i:u32)->u32;
extern "fg271d_st_half" fn st_half(yy:u64,o:u64,v:f32);
extern "fg271d_st_bf16" fn st_bf16(yy:u64,o:u64,v:f32);
extern "fg271d_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn sgn(b:i64)->f32{
 if b>0 as i64{return -1.0 as f32;}
 1.0 as f32
}

fn q3_s_f16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(110 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let qs:i64=blk+2 as i64+8*ib;
 let qhb:u32=ld_u8(vx,((blk+66 as i64+ib) as u64));
 let sh1:u32=(8-2*il) as u32;
 let sh2:u32=(7-2*il) as u32;
 let h1:u32=(qhb<<sh1)&(256 as u32);
 let h2:u32=(qhb<<sh2)&(256 as u32);
 let i1:u32=ld_u8(vx,((qs+2*il) as u64))|h1;
 let i2:u32=ld_u8(vx,((qs+2*il+1 as i64) as u64))|h2;
 let scb:u32=ld_u8(vx,((blk+106 as i64+ib/2) as u64));
 let shs:u32=(4*(ib%2)) as u32;
 let scv:i64=((scb>>shs)&(15 as u32)) as i64;
 let d:f32=ld_half(vx,(blk) as u64)*(((1 as i64)+(2*scv)) as f32);
 let sgnb:u32=ld_u8(vx,((blk+74 as i64+4*ib+il) as u64));
 let G1:u32=grid_s(i1);
 let G2:u32=grid_s(i2);
 var j:i64=0;
 while j<4 as i64{
  let shk:u64=(j*8) as u64;
  let b1:i64=((G1>>shk)&(255 as u64)) as i64;
  let b2:i64=((G2>>shk)&(255 as u64)) as i64;
  let m1:i64=(1 as i64)<<j;
  let m2:i64=(1 as i64)<<(j+4);
  let q1:i64=(sgnb as i64)&m1;
  let q2:i64=(sgnb as i64)&m2;
  st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*((b1) as f32)*sgn(q1));
  st_half(yy,(((ybase+j+4 as i64)*(2 as i64)) as u64),d*((b2) as f32)*sgn(q2));
  j=j+1;
 }
}
fn q3_s_bf16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(110 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let qs:i64=blk+2 as i64+8*ib;
 let qhb:u32=ld_u8(vx,((blk+66 as i64+ib) as u64));
 let sh1:u32=(8-2*il) as u32;
 let sh2:u32=(7-2*il) as u32;
 let h1:u32=(qhb<<sh1)&(256 as u32);
 let h2:u32=(qhb<<sh2)&(256 as u32);
 let i1:u32=ld_u8(vx,((qs+2*il) as u64))|h1;
 let i2:u32=ld_u8(vx,((qs+2*il+1 as i64) as u64))|h2;
 let scb:u32=ld_u8(vx,((blk+106 as i64+ib/2) as u64));
 let shs:u32=(4*(ib%2)) as u32;
 let scv:i64=((scb>>shs)&(15 as u32)) as i64;
 let d:f32=ld_half(vx,(blk) as u64)*(((1 as i64)+(2*scv)) as f32);
 let sgnb:u32=ld_u8(vx,((blk+74 as i64+4*ib+il) as u64));
 let G1:u32=grid_s(i1);
 let G2:u32=grid_s(i2);
 var j:i64=0;
 while j<4 as i64{
  let shk:u64=(j*8) as u64;
  let b1:i64=((G1>>shk)&(255 as u64)) as i64;
  let b2:i64=((G2>>shk)&(255 as u64)) as i64;
  let m1:i64=(1 as i64)<<j;
  let m2:i64=(1 as i64)<<(j+4);
  let q1:i64=(sgnb as i64)&m1;
  let q2:i64=(sgnb as i64)&m2;
  st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*((b1) as f32)*sgn(q1));
  st_bf16(yy,(((ybase+j+4 as i64)*(2 as i64)) as u64),d*((b2) as f32)*sgn(q2));
  j=j+1;
 }
}

fn q1_s_f16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(50 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let qh:u32=ld_u16(vx,((blk+34 as i64+2*ib) as u64));
 let qs:u32=ld_u8(vx,((blk+2 as i64+4*ib+il) as u64));
 let s3:u32=(3*il) as u32;
 let idx:u32=qs|(((qh>>s3)&(7 as u32))<<(8 as u32));
 let hb:u32=qh&(0x8000 as u32);
 var delta:f32=-0.875 as f32;
 if hb>(0 as u32){ delta=-1.125 as f32; }
 let sc7:i64=((qh>>(12 as u32))&(7 as u32)) as i64;
 let d:f32=ld_half(vx,(blk) as u64)*(((2*sc7)+(1 as i64)) as f32);
 let g:u32=grid_1(idx);
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
  st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*(((qb) as f32)+delta));
  j=j+1;
 }
}
fn q1_s_bf16(vx:u64,yy:u64,i:i64,tid:i64){
 ensure(tid>=0&&tid<32 as i64,10);
 let il:i64=tid/8;
 let ib:i64=tid%8;
 let blk:i64=i*(50 as i64);
 let ybase:i64=i*(256 as i64)+32*ib+8*il;
 let qh:u32=ld_u16(vx,((blk+34 as i64+2*ib) as u64));
 let qs:u32=ld_u8(vx,((blk+2 as i64+4*ib+il) as u64));
 let s3:u32=(3*il) as u32;
 let idx:u32=qs|(((qh>>s3)&(7 as u32))<<(8 as u32));
 let hb:u32=qh&(0x8000 as u32);
 var delta:f32=-0.875 as f32;
 if hb>(0 as u32){ delta=-1.125 as f32; }
 let sc7:i64=((qh>>(12 as u32))&(7 as u32)) as i64;
 let d:f32=ld_half(vx,(blk) as u64)*(((2*sc7)+(1 as i64)) as f32);
 let g:u32=grid_1(idx);
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
  st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*(((qb) as f32)+delta));
  j=j+1;
 }
}

fn main(){
 q3_s_f16(0,0,0 as i64,0 as i64);
 q3_s_bf16(0,0,0 as i64,0 as i64);
 q1_s_f16(0,0,0 as i64,0 as i64);
 q1_s_bf16(0,0,0 as i64,0 as i64);
}
