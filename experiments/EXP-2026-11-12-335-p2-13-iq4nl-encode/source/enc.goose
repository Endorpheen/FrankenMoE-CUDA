// IQ4_NL encode in Goose, from ggml-quants.c: quantize_row_iq4_nl_ref through quantize_row_iq4_nl_impl.
//
// For this call the impl runs with super_block_size == block_size == 32 and ntry == -1, so the multi-block
// scale search and the ntry refinement are both inactive; what remains is one block: weights x*x, the signed
// max at the largest magnitude, d = max/values[0], a first level pass, then d = sumqx/sumq2 and the pack.
// best_index_int8 is the reference's binary search with its strict-less tie choice.
extern "fg335q_ld_f32" fn ld_f32(a:u64)->f32;
extern "fg335q_ld_u8"  fn ld_u8(a:u64)->i64;
extern "fg335q_st_u8"  fn st_u8(a:u64,v:i64);
extern "fg335q_st_f16" fn st_f16(a:u64,v:f32);
extern "fg335q_abs"    fn absf(v:f32)->f32;
extern "fg335q_fail"   fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn fg335q_val(i:i64)->i64{
 if i==(0 as i64) {return 0 as i64-(127 as i64);}
 if i==(1 as i64) {return 0 as i64-(104 as i64);}
 if i==(2 as i64) {return 0 as i64-(83 as i64);}
 if i==(3 as i64) {return 0 as i64-(65 as i64);}
 if i==(4 as i64) {return 0 as i64-(49 as i64);}
 if i==(5 as i64) {return 0 as i64-(35 as i64);}
 if i==(6 as i64) {return 0 as i64-(22 as i64);}
 if i==(7 as i64) {return 0 as i64-(10 as i64);}
 if i==(8 as i64) {return 1 as i64;}
 if i==(9 as i64) {return 13 as i64;}
 if i==(10 as i64) {return 25 as i64;}
 if i==(11 as i64) {return 38 as i64;}
 if i==(12 as i64) {return 53 as i64;}
 if i==(13 as i64) {return 69 as i64;}
 if i==(14 as i64) {return 89 as i64;}
 return 113 as i64;
}

fn fg335q_index(x:f32)->i64{
 if x<=(fg335q_val(0) as f32) {return 0 as i64;}
 if x>=(fg335q_val(15) as f32) {return 15 as i64;}
 var ml:i64=0 as i64;
 var mu:i64=15 as i64;
 while mu-ml>(1 as i64) {
  let mav:i64=(ml+mu)/(2 as i64);
  if x<(fg335q_val(mav) as f32) {mu=mav;} else {ml=mav;}
 }
 return if x-(fg335q_val(mu-(1 as i64)) as f32)<(fg335q_val(mu) as f32)-x {mu-(1 as i64)} else {mu};
}

fn fg335q_block(x:u64,y:u64,i:i64,sc:u64)->i64{
 let ui:u64=i as u64;
 let xb:u64=x+ui*(128 as u64);
 let yb:u64=y+ui*(18 as u64);
 var amax:f32=0 as f32;
 var max:f32=0 as f32;
 var j:i64=0 as i64;
 while j<(32 as i64) {
  let v:f32=ld_f32(xb+(j as u64)*(4 as u64));
  let ax:f32=absf(v);
  if ax>amax {amax=ax;max=v;}
  j=j+(1 as i64);
 }
 if amax<(1e-15 as f32) {
  // the reference leaves L unwritten here; the block scale is zero and this path is not gated (see README)
  st_f16(yb,0 as f32);
  return 0 as i64;
 }
 let d0:f32=max/(fg335q_val(0) as f32);
 let id:f32=(1 as f32)/d0;
 var sumqx:f32=0 as f32;
 var sumq2:f32=0 as f32;
 var L:u64=sc;
 j=0 as i64;
 while j<(32 as i64) {
  let xv:f32=ld_f32(xb+(j as u64)*(4 as u64));
  let l:i64=fg335q_index(id*xv);
  st_u8(L+(j as u64),l);
  let q:f32=fg335q_val(l) as f32;
  let w:f32=xv*xv;
  sumqx=sumqx+w*q*xv;
  sumq2=sumq2+w*q*q;
  j=j+(1 as i64);
 }
 let d:f32=if sumq2>(0 as f32) {sumqx/sumq2} else {0 as f32};
 st_f16(yb,d);
 j=0 as i64;
 while j<(16 as i64) {
  let lo:i64=ld_u8(L+(j as u64));
  let hi:i64=ld_u8(L+((j+(16 as i64)) as u64));
  st_u8(yb+(2 as u64)+(j as u64),lo|(hi<<(4 as i64)));
  j=j+(1 as i64);
 }
 return 0 as i64;
}

fn fg335q_encode(x:u64,y:u64,k:i64,sc:u64)->i64{
 ensure(k>=(0 as i64),2500 as i64);ensure(k%(32 as i64)==(0 as i64),2501 as i64);
 var nb:i64=k/(32 as i64);
 var i:i64=0 as i64;
 while i<nb {fg335q_block(x,y,i,sc);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg335q_encode(0 as u64,0 as u64,0 as i64,0 as u64);}
