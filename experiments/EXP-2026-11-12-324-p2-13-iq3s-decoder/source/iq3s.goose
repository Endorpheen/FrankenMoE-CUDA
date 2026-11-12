// IQ3_S decode in Goose, from ggml-quants.c: dequantize_row_iq3_s.
//
// block_iq3_s is 110 bytes in this fork, verified with offsetof: d (f16) 0, qs[64] 2, qh[8] 66, signs[32] 74,
// scales[4] 106. Four iterations of 64 outputs: each iteration reads one scale byte (low nibble for the first
// half, high for the second), two qh bytes (one per half) that widen the nine-bit grid codes with a per-l
// shift of 8-2*l or 7-2*l, eight code bytes and eight sign bytes.
extern "fg324q_ld_u8"  fn ld_u8(a:u64)->i64;
extern "fg324q_ld_f16" fn ld_f16(a:u64)->f32;
extern "fg324q_st_f32" fn st_f32(a:u64,v:f32);
extern "fg324q_grid"   fn grid(code:i64)->i64;
extern "fg324q_kmask"  fn kmask(j:i64)->i64;
extern "fg324q_fail"   fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn fg324q_half(qb:u64,sb:u64,qhb:i64,db:f32,y:u64){
 var l:i64=0 as i64;
 while l<(4 as i64) {
  let ul:u64=l as u64;
  let c1:i64=(ld_u8(qb+ul*(2 as u64)))|((qhb<<((8 as i64)-(2 as i64)*(l as i64)))&(256 as i64));
  let c2:i64=(ld_u8(qb+ul*(2 as u64)+(1 as u64)))|((qhb<<((7 as i64)-(2 as i64)*(l as i64)))&(256 as i64));
  let sg:i64=ld_u8(sb+ul);
  let g1:i64=grid(c1);
  let g2:i64=grid(c2);
  var j:i64=0 as i64;
  while j<(4 as i64) {
   let v1:i64=(g1>>(j*(8 as i64)))&(255 as i64);
   let v2:i64=(g2>>(j*(8 as i64)))&(255 as i64);
   let n1:bool=(sg&kmask(j))!=(0 as i64);
   let n2:bool=(sg&kmask(j+(4 as i64)))!=(0 as i64);
   let s1:f32=if n1 {0 as f32-(1 as f32)} else {1 as f32};
   let s2:f32=if n2 {0 as f32-(1 as f32)} else {1 as f32};
   let yl:u64=y+ul*(32 as u64);
   st_f32(yl+(j as u64)*(4 as u64),db*(v1 as f32)*s1);
   st_f32(yl+(j as u64)*(4 as u64)+(16 as u64),db*(v2 as f32)*s2);
   j=j+(1 as i64);
  }
  l=l+(1 as i64);
 }
}

fn fg324q_block(src:u64,dst:u64,i:i64)->i64{
 let ui:u64=i as u64;
 let base:u64=src+ui*(110 as u64);
 let d:f32=ld_f16(base);
 let qs:u64=base+(2 as u64);
 let qh:u64=base+(66 as u64);
 let sg:u64=base+(74 as u64);
 let scp:u64=base+(106 as u64);
 let y0:u64=dst+ui*(1024 as u64);
 var it:i64=0 as i64;
 while it<(4 as i64) {
  let sc:i64=ld_u8(scp+(it as u64));
  let db1:f32=d*(1 as f32+(2 as f32)*((sc&(15 as i64)) as f32));
  let db2:f32=d*(1 as f32+(2 as f32)*((sc>>(4 as i64)) as f32));
  let qh0:i64=ld_u8(qh+(it as u64)*(2 as u64));
  let qh1:i64=ld_u8(qh+(it as u64)*(2 as u64)+(1 as u64));
  let qb:u64=qs+(it as u64)*(16 as u64);
  let sb:u64=sg+(it as u64)*(8 as u64);
  let yb:u64=y0+(it as u64)*(256 as u64);
  fg324q_half(qb,sb,qh0,db1,yb);
  fg324q_half(qb+(8 as u64),sb+(4 as u64),qh1,db2,yb+(128 as u64));
  it=it+(1 as i64);
 }
 return 1 as i64;
}

fn fg324q_decode(src:u64,dst:u64,k:i64)->i64{
 ensure(k>=(0 as i64),1700 as i64);ensure(k%(256 as i64)==(0 as i64),1701 as i64);
 var nb:i64=k/(256 as i64);
 var i:i64=0 as i64;
 while i<nb {fg324q_block(src,dst,i);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg324q_decode(0 as u64,0 as u64,0 as i64);}
