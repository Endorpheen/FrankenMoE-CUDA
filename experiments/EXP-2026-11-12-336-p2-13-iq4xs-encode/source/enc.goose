// IQ4_XS encode in Goose, from ggml-quants.c: quantize_row_iq4_xs_ref through quantize_row_iq4_nl_impl.
//
// Here super_block_size = 256, block_size = 32 and ntry = 8, so the multi-block branch is live: eight per-block
// scale searches including the -8..8 refinement, then one fp16 super-scale with 6-bit per-block scales split
// across scales_l and scales_h, and a final level pass at the quantised scale.
extern "fg336q_ld_f32" fn ld_f32(a:u64)->f32;
extern "fg336q_ld_u8"  fn ld_u8(a:u64)->i64;
extern "fg336q_st_f32" fn st_f32(a:u64,v:f32);
extern "fg336q_st_u8"  fn st_u8(a:u64,v:i64);
extern "fg336q_st_u16" fn st_u16(a:u64,v:i64);
extern "fg336q_st_f16" fn st_f16(a:u64,v:f32);
extern "fg336q_abs"    fn absf(v:f32)->f32;
extern "fg336q_nearest" fn nearest(v:f32)->i64;
extern "fg336q_fail"   fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn mx_i(a:i64,b:i64)->i64{if a>b {return a;}return b;}
fn mn_i(a:i64,b:i64)->i64{if a<b {return a;}return b;}

fn fg336q_val(i:i64)->i64{
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

fn fg336q_index(x:f32)->i64{
 if x<=(fg336q_val(0) as f32) {return 0 as i64;}
 if x>=(fg336q_val(15) as f32) {return 15 as i64;}
 var ml:i64=0 as i64;
 var mu:i64=15 as i64;
 while mu-ml>(1 as i64) {
  let mav:i64=(ml+mu)/(2 as i64);
  if x<(fg336q_val(mav) as f32) {mu=mav;} else {ml=mav;}
 }
 return if x-(fg336q_val(mu-(1 as i64)) as f32)<(fg336q_val(mu) as f32)-x {mu-(1 as i64)} else {mu};
}

// block search for one 32-element group; returns d and writes the eight bits of levels to sc
fn fg336q_scale(xb:u64,L:u64,scales_out:u64,ib:i64)->i64{
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
  // the reference stores scale 0 and leaves the levels unwritten; not gated, see README
  st_f32(scales_out+(ib as u64)*4 as u64,0 as f32);
  return 0 as i64;
 }
 let d0:f32=(0 as f32-max)/(fg336q_val(0) as f32);
 let id0:f32=(1 as f32)/d0;
 var sumqx:f32=0 as f32;
 var sumq2:f32=0 as f32;
 j=0 as i64;
 while j<(32 as i64) {
  let xv:f32=ld_f32(xb+(j as u64)*(4 as u64));
  let l:i64=fg336q_index(id0*xv);
  st_u8(L+(ib as u64)*(32 as u64)+(j as u64),l);
  let q:f32=fg336q_val(l) as f32;
  let w:f32=xv*xv;
  sumqx=sumqx+w*q*xv;
  sumq2=sumq2+w*q*q;
  j=j+(1 as i64);
 }
 var d:f32=if sumq2>(0 as f32) {sumqx/sumq2} else {0 as f32};
 var best:f32=d*sumqx;
 var itry:i64=0 as i64-(8 as i64);
 while itry<=(8 as i64) {
  let id:f32=((itry as f32)+(fg336q_val(0) as f32))/max;
  sumqx=0 as f32;sumq2=0 as f32;
  j=0 as i64;
  while j<(32 as i64) {
   let xv:f32=ld_f32(xb+(j as u64)*(4 as u64));
   let l:i64=fg336q_index(id*xv);
   let q:f32=fg336q_val(l) as f32;
   let w:f32=xv*xv;
   sumqx=sumqx+w*q*xv;
   sumq2=sumq2+w*q*q;
   j=j+(1 as i64);
  }
  if sumq2>(0 as f32)&&sumqx*sumqx>best*sumq2 {
   d=sumqx/sumq2;
   best=d*sumqx;
  }
  itry=itry+(1 as i64);
 }
 st_f32(scales_out+(ib as u64)*4 as u64,d);
 return 1 as i64;
}

fn fg336q_block(x:u64,y:u64,i:i64,scr:u64)->i64{
 let ui:u64=i as u64;
 let xb:u64=x+ui*(1024 as u64);
 let yb:u64=y+ui*(136 as u64);
 let L:u64=scr;                 // 256 levels
 let scales:u64=scr+(256 as u64); // 8 floats
 var amax_scale:f32=0 as f32;
 var max_scale:f32=0 as f32;
 var shacc:i64=0 as i64;
 var ib:i64=0 as i64;
 while ib<(8 as i64) {
  let ok:i64=fg336q_scale(xb+(ib as u64)*(128 as u64),L,scales,ib);
  if ok!=(0 as i64) {
   let d:f32=ld_f32(scales+(ib as u64)*(4 as u64));
   let ad:f32=absf(d);
   if ad>amax_scale {amax_scale=ad;max_scale=d;}
  }
  ib=ib+(1 as i64);
 }
 // super-scale and the six-bit per-block scales
 let dsup:f32=(0 as f32-max_scale)/(32 as f32);
 st_f16(yb,dsup);
 let idsup:f32=if dsup!=(0 as f32) {(1 as f32)/dsup} else {0 as f32};
 ib=0 as i64;
 while ib<(8 as i64) {
  let l0:i64=mn_i(31 as i64,mx_i(0 as i64-(32 as i64),nearest(idsup*ld_f32(scales+(ib as u64)*(4 as u64)))));
  let dl:f32=dsup*(l0 as f32);
  let idl:f32=if dl!=(0 as f32) {(1 as f32)/dl} else {0 as f32};
  var j:i64=0 as i64;
  while j<(32 as i64) {
   st_u8(L+(ib as u64)*(32 as u64)+(j as u64),fg336q_index(idl*ld_f32(xb+(ib as u64)*(128 as u64)+(j as u64)*(4 as u64))));
   j=j+(1 as i64);
  }
  let l:i64=l0+(32 as i64);
  let ll:i64=l&(15 as i64);
  let lh:i64=(l>>(4 as i64))&(3 as i64);
  let sl:u64=yb+(4 as u64)+((ib/(2 as i64)) as u64);
  if (ib%(2 as i64))==(0 as i64) {st_u8(sl,ll);} else {st_u8(sl,ld_u8(sl)|(ll<<(4 as i64)));}
  shacc=shacc|(lh<<((2 as i64)*(ib%(8 as i64))));
  ib=ib+(1 as i64);
 }
 st_u16(yb+(2 as u64),shacc);
 // pack the levels
 ib=0 as i64;
 while ib<(8 as i64) {
  var j:i64=0 as i64;
  while j<(16 as i64) {
   let lo:i64=ld_u8(L+(ib as u64)*(32 as u64)+(j as u64));
   let hi:i64=ld_u8(L+(ib as u64)*(32 as u64)+((j+(16 as i64)) as u64));
   st_u8(yb+(8 as u64)+((ib*(16 as i64)+j) as u64),lo|(hi<<(4 as i64)));
   j=j+(1 as i64);
  }
  ib=ib+(1 as i64);
 }
 return 0 as i64;
}

fn fg336q_encode(x:u64,y:u64,k:i64,scr:u64)->i64{
 ensure(k>=(0 as i64),2600 as i64);ensure(k%(256 as i64)==(0 as i64),2601 as i64);
 var nb:i64=k/(256 as i64);
 var i:i64=0 as i64;
 while i<nb {fg336q_block(x,y,i,scr);i=i+(1 as i64);}
 return 0 as i64;
}

fn main(){fg336q_encode(0 as u64,0 as u64,0 as i64,0 as u64);}
