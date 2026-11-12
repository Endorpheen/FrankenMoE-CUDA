// TQ1_0 encode in Goose, from ggml-quants.c: quantize_row_tq1_0_ref.
//
// block_tq1_0 is 54 bytes: qs[48] 0, qh[4] 48, d (f16) 52. QK_K = 256 in three segments: 32 bytes over 160
// elements, 16 bytes over 80, then the four qh bytes over the last 16. Each byte packs five trits in base 3
// (the qh byte packs four and multiplies once more), then a ceiling division by 243 maps it into a byte.
extern "fg334q_ld_f32" fn ld_f32(a:u64)->f32;
extern "fg334q_st_u8"  fn st_u8(a:u64,v:i64);
extern "fg334q_st_f16" fn st_f16(a:u64,v:f32);
extern "fg334q_abs"    fn absf(v:f32)->f32;
extern "fg334q_lround" fn lroundf(v:f32)->i64;
extern "fg334q_fail"   fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn mx(a:f32,b:f32)->f32{if a>b {return a;}return b;}

fn fg334q_at(x:u64,i:i64)->f32{return ld_f32(x+(i as u64)*(4 as u64));}

fn fg334q_block(x:u64,y:u64,i:i64)->i64{
 let ui:u64=i as u64;
 let xb:u64=x+ui*(1024 as u64);
 let yb:u64=y+ui*(54 as u64);
 var amax:f32=0 as f32;
 var j:i64=0 as i64;
 while j<(256 as i64) {amax=mx(amax,absf(fg334q_at(xb,j)));j=j+(1 as i64);}
 let d:f32=amax;
 let id:f32=if d!=(0 as f32) {(1 as f32)/d} else {0 as f32};
 st_f16(yb+(52 as u64),d);
 // segment A: 32 bytes, five trits each, elements 0..159
 var m:i64=0 as i64;
 while m<(32 as i64) {
  var q:i64=0 as i64;
  var n:i64=0 as i64;
  while n<(5 as i64) {
   let xi:i64=lroundf(fg334q_at(xb,m+n*(32 as i64))*id)+(1 as i64);
   q=q*(3 as i64)+xi;
   n=n+(1 as i64);
  }
  st_u8(yb+(m as u64),((q*(256 as i64))+(242 as i64))/(243 as i64));
  m=m+(1 as i64);
 }
 // segment B: 16 bytes starting at qs[32], elements 160..239
 m=0 as i64;
 while m<(16 as i64) {
  var q:i64=0 as i64;
  var n:i64=0 as i64;
  while n<(5 as i64) {
   let xi:i64=lroundf(fg334q_at(xb,(160 as i64)+m+n*(16 as i64))*id)+(1 as i64);
   q=q*(3 as i64)+xi;
   n=n+(1 as i64);
  }
  st_u8(yb+((32 as i64)+m) as u64,((q*(256 as i64))+(242 as i64))/(243 as i64));
  m=m+(1 as i64);
 }
 // segment C: four qh bytes over elements 240..255
 j=0 as i64;
 while j<(4 as i64) {
  var q:i64=0 as i64;
  var mm:i64=0 as i64;
  while mm<(4 as i64) {
   let xi:i64=lroundf(fg334q_at(xb,(240 as i64)+j+mm*(4 as i64))*id)+(1 as i64);
   q=q*(3 as i64)+xi;
   mm=mm+(1 as i64);
  }
  q=q*(3 as i64);
  st_u8(yb+((48 as i64)+j) as u64,((q*(256 as i64))+(242 as i64))/(243 as i64));
  j=j+(1 as i64);
 }
 return 0 as i64;
}

fn fg334q_encode(x:u64,y:u64,k:i64)->i64{
 ensure(k>=(0 as i64),2400 as i64);ensure(k%(256 as i64)==(0 as i64),2401 as i64);
 var nb:i64=k/(256 as i64);
 var i:i64=0 as i64;
 while i<nb {fg334q_block(x,y,i);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg334q_encode(0 as u64,0 as u64,0 as i64);}
