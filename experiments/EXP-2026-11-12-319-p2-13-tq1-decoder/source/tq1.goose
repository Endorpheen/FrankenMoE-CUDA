// TQ1_0 decode in Goose, from ggml-quants.c: dequantize_row_tq1_0.
//
// block_tq1_0 is 54 bytes in this fork, verified with offsetof: qs[48] 0, qh[4] 48, d (f16) 52.
// Three segments feed 256 outputs in order: qs[0..31] five times (160), qs[32..47] five times (80),
// then qh[0..3] four times (16). The product is truncated to a byte before the x3>>8 step, exactly as
// the reference's uint8_t does; that truncation is the part a "clean" rewrite would lose.
extern "fg319q_ld_u8"  fn ld_u8(a:u64)->i64;
extern "fg319q_ld_f16" fn ld_f16(a:u64)->f32;
extern "fg319q_st_f32" fn st_f32(a:u64,v:f32);
extern "fg319q_fail"   fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn fg319q_pow3(n:i64)->i64{
 if n==(0 as i64) {return 1 as i64;}
 if n==(1 as i64) {return 3 as i64;}
 if n==(2 as i64) {return 9 as i64;}
 if n==(3 as i64) {return 27 as i64;}
 if n==(4 as i64) {return 81 as i64;}
 return 243 as i64;
}

// one source byte at a given power, returned as (xi - 1) * d written to y
fn fg319q_emit(b:i64,n:i64,d:f32,y:u64){
 let q:i64=(b*(fg319q_pow3(n)))&(255 as i64);
 let xi:i64=(q*(3 as i64))>>(8 as i64);
 st_f32(y,d*((xi-(1 as i64)) as f32));
}

fn fg319q_block(src:u64,dst:u64,i:i64)->i64{
 let ui:u64=i as u64;
 let base:u64=src+ui*(54 as u64);
 let d:f32=ld_f16(base+(52 as u64));
 let qh:u64=base+(48 as u64);
 let y0:u64=dst+ui*(1024 as u64);
 var y:u64=y0;
 var n:i64=0 as i64;
 while n<(5 as i64) {
  var m:i64=0 as i64;
  while m<(32 as i64) {
   fg319q_emit(ld_u8(base+(m as u64)),n,d,y);
   y=y+(4 as u64); m=m+(1 as i64);
  }
  n=n+(1 as i64);
 }
 n=0 as i64;
 while n<(5 as i64) {
  var m:i64=0 as i64;
  while m<(16 as i64) {
   fg319q_emit(ld_u8(base+((32 as i64)+(m as i64)) as u64),n,d,y);
   y=y+(4 as u64); m=m+(1 as i64);
  }
  n=n+(1 as i64);
 }
 n=0 as i64;
 while n<(4 as i64) {
  var m:i64=0 as i64;
  while m<(4 as i64) {
   fg319q_emit(ld_u8(qh+(m as u64)),n,d,y);
   y=y+(4 as u64); m=m+(1 as i64);
  }
  n=n+(1 as i64);
 }
 return 1 as i64;
}

fn fg319q_decode(src:u64,dst:u64,k:i64)->i64{
 ensure(k>=(0 as i64),1200 as i64);ensure(k%(256 as i64)==(0 as i64),1201 as i64);
 var nb:i64=k/(256 as i64);
 var i:i64=0 as i64;
 while i<nb {fg319q_block(src,dst,i);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg319q_decode(0 as u64,0 as u64,0 as i64);}
