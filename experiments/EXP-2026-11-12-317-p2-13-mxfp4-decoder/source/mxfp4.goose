// MXFP4 decode in Goose, from ggml-quants.c: dequantize_row_mxfp4.
//
// block_mxfp4 is 17 bytes in this fork, verified with offsetof: e (u8) 0, qs[16] 1. QK_MXFP4 = 32.
// The scale is an E8M0 exponent converted by ggml_e8m0_to_fp32_half: x<2 uses denormal patterns
// 0x00200000 << x, otherwise exponent (x-1) in the top bits. The values are the doubled e2m1 table
// kvalues_fp4, embedded here. Low nibbles are outputs 0..15, high nibbles 16..31.
extern "fg317q_ld_u8"    fn ld_u8(a:u64)->i64;
extern "fg317q_bits_f32" fn bits_f32(v:u64)->f32;
extern "fg317q_st_f32"   fn st_f32(a:u64,v:f32);
extern "fg317q_fail"     fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn fg317q_val(i:i64)->i64{
 if i==(0 as i64) {return 0 as i64;}
 if i==(1 as i64) {return 1 as i64;}
 if i==(2 as i64) {return 2 as i64;}
 if i==(3 as i64) {return 3 as i64;}
 if i==(4 as i64) {return 4 as i64;}
 if i==(5 as i64) {return 6 as i64;}
 if i==(6 as i64) {return 8 as i64;}
 if i==(7 as i64) {return 12 as i64;}
 if i==(8 as i64) {return 0 as i64;}
 if i==(9 as i64) {return 0 as i64-(1 as i64);}
 if i==(10 as i64) {return 0 as i64-(2 as i64);}
 if i==(11 as i64) {return 0 as i64-(3 as i64);}
 if i==(12 as i64) {return 0 as i64-(4 as i64);}
 if i==(13 as i64) {return 0 as i64-(6 as i64);}
 if i==(14 as i64) {return 0 as i64-(8 as i64);}
 return 0 as i64-(12 as i64);
}

fn fg317q_scale(e:i64)->f32{
 let bits:u64=if e<(2 as i64) {((0x00200000 as u64)<<(e as u64))} else {((e-(1 as i64)) as u64)<<(23 as u64)};
 return bits_f32(bits);
}

fn fg317q_block(src:u64,dst:u64,i:i64)->i64{
 let ui:u64=i as u64;
 let base:u64=src+ui*(17 as u64);
 let d:f32=fg317q_scale(ld_u8(base));
 let qs:u64=base+(1 as u64);
 let y:u64=dst+ui*(128 as u64);
 var j:i64=0 as i64;
 while j<(16 as i64) {
  let uj:u64=j as u64;
  let b:i64=ld_u8(qs+uj);
  st_f32(y+uj*(4 as u64),d*(fg317q_val(b&(15 as i64)) as f32));
  st_f32(y+(uj+(16 as u64))*(4 as u64),d*(fg317q_val(b>>(4 as i64)) as f32));
  j=j+(1 as i64);
 }
 return 1 as i64;
}

fn fg317q_decode(src:u64,dst:u64,k:i64)->i64{
 ensure(k>=(0 as i64),1000 as i64);ensure(k%(32 as i64)==(0 as i64),1001 as i64);
 var nb:i64=k/(32 as i64);
 var i:i64=0 as i64;
 while i<nb {fg317q_block(src,dst,i);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg317q_decode(0 as u64,0 as u64,0 as i64);}
