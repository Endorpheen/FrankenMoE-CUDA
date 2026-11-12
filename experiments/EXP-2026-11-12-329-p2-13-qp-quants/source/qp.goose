// make_qp_quants in Goose, from ggml-quants.c (the helper used for the Q4_K block scale and min).
//
// Exactness matters more than structure here: MAX and MIN are the native macros (a > b ? a : b), the
// nearest-integer rounding is the reference's 12582912 trick (round half to even, not roundf), and the two
// refinement loops keep the reference's order, because the result feeds a byte-exact gate.
extern "fg329q_ld_f32"  fn ld_f32(a:u64)->f32;
extern "fg329q_ld_u8"   fn ld_u8(a:u64)->i64;
extern "fg329q_st_u8"   fn st_u8(a:u64,v:i64);
extern "fg329q_nearest" fn nearest(v:f32)->i64;
extern "fg329q_fail"    fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn mx(a:f32,b:f32)->f32{if a>b {return a;}return b;}
fn mn_i(a:i64,b:i64)->i64{if a<b {return a;}return b;}

fn fg329q_make_qp(n:i64,nmax:i64,x:u64,L:u64,w:u64)->f32{
 var max:f32=0 as f32;
 var i:i64=0 as i64;
 while i<n {
  max=mx(max,ld_f32(x+(i as u64)*(4 as u64)));
  i=i+(1 as i64);
 }
 if max<(1e-15 as f32) {
  i=0 as i64;
  while i<n {st_u8(L+(i as u64),0);i=i+(1 as i64);}
  return 0 as f32;
 }
 var iscale:f32=(nmax as f32)/max;
 i=0 as i64;
 while i<n {
  st_u8(L+(i as u64),nearest(iscale*ld_f32(x+(i as u64)*(4 as u64))));
  i=i+(1 as i64);
 }
 var scale:f32=(1 as f32)/iscale;
 var best_mse:f32=0 as f32;
 i=0 as i64;
 while i<n {
  let xi:f32=ld_f32(x+(i as u64)*(4 as u64));
  let li:f32=(ld_u8(L+(i as u64)) as f32);
  let diff:f32=xi-scale*li;
  let wi:f32=ld_f32(w+(i as u64)*(4 as u64));
  best_mse=best_mse+wi*diff*diff;
  i=i+(1 as i64);
 }
 var js:i64=0-4 as i64;
 while js<=4 as i64 {
  if js!=0 as i64 {
   let iscale_is:f32=((0.1 as f32)*(js as f32)+(nmax as f32))/max;
   let scale_is:f32=(1 as f32)/iscale_is;
   var mse:f32=0 as f32;
   i=0 as i64;
   while i<n {
    let xi:f32=ld_f32(x+(i as u64)*(4 as u64));
    let l:i64=mn_i(nmax,nearest(iscale_is*xi));
    let diff:f32=xi-scale_is*(l as f32);
    let wi:f32=ld_f32(w+(i as u64)*(4 as u64));
    mse=mse+wi*diff*diff;
    i=i+(1 as i64);
   }
   if mse<best_mse {best_mse=mse;iscale=iscale_is;}
  }
  js=js+(1 as i64);
 }
 var sumlx:f32=0 as f32;
 var suml2:f32=0 as f32;
 i=0 as i64;
 while i<n {
  let xi:f32=ld_f32(x+(i as u64)*(4 as u64));
  let l:i64=mn_i(nmax,nearest(iscale*xi));
  st_u8(L+(i as u64),l);
  let wi:f32=ld_f32(w+(i as u64)*(4 as u64));
  sumlx=sumlx+wi*xi*(l as f32);
  suml2=suml2+wi*(l as f32)*(l as f32);
  i=i+(1 as i64);
 }
 var itry:i64=0 as i64;
 while itry<(5 as i64) {
  var n_changed:i64=0 as i64;
  i=0 as i64;
  while i<n {
   let wi:f32=ld_f32(w+(i as u64)*(4 as u64));
   let xi:f32=ld_f32(x+(i as u64)*(4 as u64));
   let li:i64=ld_u8(L+(i as u64));
   var slx:f32=sumlx-wi*xi*(li as f32);
   var sl2:f32=suml2-wi*(li as f32)*(li as f32);
   if slx>(0 as f32)&&sl2>(0 as f32) {
    var new_l:i64=mn_i(nmax,nearest(xi*sl2/slx));
    if new_l!=li {
     let nl:f32=(new_l as f32);
     slx=slx+wi*xi*nl;
     sl2=sl2+wi*nl*nl;
     if slx*slx*suml2>sumlx*sumlx*sl2 {
      st_u8(L+(i as u64),new_l);
      sumlx=slx;suml2=sl2;n_changed=n_changed+(1 as i64);
     }
    }
   }
   i=i+(1 as i64);
  }
  if n_changed==(0 as i64) {itry=5 as i64;} else {itry=itry+(1 as i64);}
 }
 return if suml2>(0 as f32) {sumlx/suml2} else {0 as f32};
}

fn main(){fg329q_make_qp(0 as i64,0 as i64,0 as u64,0 as u64,0 as u64);}
