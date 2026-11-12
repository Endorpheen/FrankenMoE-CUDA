// IQ2_XS decode in Goose, from ggml-quants.c: dequantize_row_iq2_xs.
//
// block_iq2_xs is 74 bytes in this fork, verified with offsetof: d (f16) 0, qs (uint16[32]) 2, scales[8] 66.
// Each of the eight groups reads its scale byte once (low nibble for the first two l, high nibble for the
// last two) and four 16-bit words; the low nine bits index iq2xs_grid and the high seven index ksigns_iq2xs.
// Note this function's own stride is 4 uint16 per group = 8 bytes, matching the executed IQ2_XXS behaviour
// and not the 4-byte stride its checked-in source still shows.
extern "fg321q_ld_u8"  fn ld_u8(a:u64)->i64;
extern "fg321q_ld_u16" fn ld_u16(a:u64)->i64;
extern "fg321q_ld_f16" fn ld_f16(a:u64)->f32;
extern "fg321q_st_f32" fn st_f32(a:u64,v:f32);
extern "fg321q_grid"   fn grid(code:i64)->i64;
extern "fg321q_signs"  fn signs(idx:i64)->i64;
extern "fg321q_kmask"  fn kmask(j:i64)->i64;
extern "fg321q_fail"   fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn fg321q_block(src:u64,dst:u64,i:i64)->i64{
 let ui:u64=i as u64;
 let base:u64=src+ui*(74 as u64);
 let d:f32=ld_f16(base);
 let qs:u64=base+(2 as u64);
 let scp:u64=base+(66 as u64);
 let y0:u64=dst+ui*(1024 as u64);
 var ib:i64=0 as i64;
 while ib<(8 as i64) {
  let sc:i64=ld_u8(scp+(ib as u64));
  let db0:f32=d*(0.5 as f32+((sc&(15 as i64)) as f32))*(0.25 as f32);
  let db1:f32=d*(0.5 as f32+((sc>>(4 as i64)) as f32))*(0.25 as f32);
  var l:i64=0 as i64;
  while l<(4 as i64) {
   let w:i64=ld_u16(qs+(((ib*(4 as i64)+l)*(2 as i64)) as u64));
   let code:i64=w&(511 as i64);
   let sg:i64=signs(w>>(9 as i64));
   let g:i64=grid(code);
   let db:f32=if l<(2 as i64) {db0} else {db1};
   let y:u64=y0+(ib as u64)*(128 as u64)+(l as u64)*(32 as u64);
   var j:i64=0 as i64;
   while j<(8 as i64) {
    let gv:i64=(g>>(j*(8 as i64)))&(255 as i64);
    let neg:bool=(sg&kmask(j))!=(0 as i64);
    let s:f32=if neg {0 as f32-(1 as f32)} else {1 as f32};
    st_f32(y+(j as u64)*(4 as u64),db*(gv as f32)*s);
    j=j+(1 as i64);
   }
   l=l+(1 as i64);
  }
  ib=ib+(1 as i64);
 }
 return 1 as i64;
}

fn fg321q_decode(src:u64,dst:u64,k:i64)->i64{
 ensure(k>=(0 as i64),1400 as i64);ensure(k%(256 as i64)==(0 as i64),1401 as i64);
 var nb:i64=k/(256 as i64);
 var i:i64=0 as i64;
 while i<nb {fg321q_block(src,dst,i);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg321q_decode(0 as u64,0 as u64,0 as i64);}
