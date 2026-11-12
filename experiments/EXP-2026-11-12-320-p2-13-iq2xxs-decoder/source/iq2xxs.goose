// IQ2_XXS decode in Goose, from ggml-quants.c: dequantize_row_iq2_xxs.
//
// block_iq2_xxs is 66 bytes in this fork, verified with offsetof: d (f16) 0, qs[64] 2. QK_K = 256 in
// eight groups of 32. Each group reads two words; the low word carries four 8-bit grid codes, the high
// word carries four 7-bit sign indices and the 4-bit group scale in its top nibble. The grid and sign
// tables live in the host (ggml-common.h) and are read through externs rather than duplicated here.
extern "fg320q_ld_u32"  fn ld_u32(a:u64)->i64;
extern "fg320q_ld_f16"  fn ld_f16(a:u64)->f32;
extern "fg320q_st_f32"  fn st_f32(a:u64,v:f32);
extern "fg320q_grid"    fn grid(code:i64)->i64;
extern "fg320q_signs"   fn signs(idx:i64)->i64;
extern "fg320q_kmask"   fn kmask(j:i64)->i64;
extern "fg320q_fail"    fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn fg320q_block(src:u64,dst:u64,i:i64)->i64{
 let ui:u64=i as u64;
 let base:u64=src+ui*(66 as u64);
 let d:f32=ld_f16(base);
 let qs:u64=base+(2 as u64);
 let y0:u64=dst+ui*(1024 as u64);
 var ib:i64=0 as i64;
 while ib<(8 as i64) {
  // the executed library advances eight bytes per group; the checked-in source shows four (see README)
  let qb:u64=qs+(ib as u64)*(8 as u64);
  let a0:i64=ld_u32(qb);
  let a1:i64=ld_u32(qb+(4 as u64));
  let db:f32=d*(0.5 as f32+((a1>>(28 as i64)) as f32))*(0.25 as f32);
  var l:i64=0 as i64;
  while l<(4 as i64) {
   let code:i64=(a0>>(l*(8 as i64)))&(255 as i64);
   let g:i64=grid(code);
   let sg:i64=signs((a1>>(l*(7 as i64)))&(127 as i64));
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

fn fg320q_decode(src:u64,dst:u64,k:i64)->i64{
 ensure(k>=(0 as i64),1300 as i64);ensure(k%(256 as i64)==(0 as i64),1301 as i64);
 var nb:i64=k/(256 as i64);
 var i:i64=0 as i64;
 while i<nb {fg320q_block(src,dst,i);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg320q_decode(0 as u64,0 as u64,0 as i64);}
