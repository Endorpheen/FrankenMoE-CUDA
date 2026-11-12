// make_qkx3_quants in Goose, from ggml-quants.c. Used by the Q4_K encoder for the per-group scale/min search.
//
// Exactness rules kept: MAX/MIN are the native macros; the initial min/max scan starts from x[0]; the
// weighted moments use the same accumulation order; the is-loop evaluates nstep+1 candidates from
// rmin + rdelta*is; the minimum is clamped by "if (min > 0) min = 0" and, once a candidate fits better, by
// this_min which may be positive. The in-loop clamp "v > 0 ? nearest_int(MIN(v, nmax)) : 0" is kept
// verbatim because v can be inf or nan when min sits near max.
extern "fg330q_ld_f32"  fn ld_f32(a:u64)->f32;
extern "fg330q_ld_u8"   fn ld_u8(a:u64)->i64;
extern "fg330q_st_u8"   fn st_u8(a:u64,v:i64);
extern "fg330q_st_f32"  fn st_f32(a:u64,v:f32);
extern "fg330q_nearest" fn nearest(v:f32)->i64;
extern "fg330q_abs"     fn absf(v:f32)->f32;
extern "fg330q_neg"     fn neg(v:f32)->f32;
extern "fg330q_fail"    fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}
fn mx(a:f32,b:f32)->f32{if a>b {return a;}return b;}
fn mnf(a:f32,b:f32)->f32{if a<b {return a;}return b;}
fn mn_i(a:i64,b:i64)->i64{if a<b {return a;}return b;}
fn mx_i(a:i64,b:i64)->i64{if a>b {return a;}return b;}
fn at(x:u64,i:i64)->f32{return ld_f32(x+(i as u64)*(4 as u64));}

// hasw: 1 when the weights array is supplied, 0 when weights are x*x
fn fg330q_make_qkx3(n:i64,nmax:i64,x:u64,w:u64,hasw:i64,L:u64,the_min:u64,Laux:u64,
                    rmin:f32,rdelta:f32,nstep:i64,use_mad:i64)->f32{
 var min:f32=at(x,0 as i64);
 var max:f32=at(x,0 as i64);
 var w0:f32=if hasw!=(0 as i64) {at(w,0 as i64)} else {at(x,0 as i64)*at(x,0 as i64)};
 var sum_w:f32=w0;
 var sum_x:f32=w0*at(x,0 as i64);
 var i:i64=1 as i64;
 while i<n {
  let xi:f32=at(x,i);
  if xi<min {min=xi;}
  if xi>max {max=xi;}
  let wi:f32=if hasw!=(0 as i64) {at(w,i)} else {xi*xi};
  sum_w=sum_w+wi;
  sum_x=sum_x+wi*xi;
  i=i+(1 as i64);
 }
 if min>(0 as f32) {min=0 as f32;}
 if max<=min {
  i=0 as i64;
  while i<n {st_u8(L+(i as u64),0);i=i+(1 as i64);}
  st_f32(the_min,neg(min));
  return 0 as f32;
 }
 var iscale:f32=(nmax as f32)/(max-min);
 var scale:f32=(1 as f32)/iscale;
 var best_mad:f32=0 as f32;
 i=0 as i64;
 while i<n {
  let l:i64=nearest(iscale*(at(x,i)-min));
  st_u8(L+(i as u64),mx_i(0 as i64,mn_i(nmax,l)));
  var diff:f32=scale*(ld_u8(L+(i as u64)) as f32)+min-at(x,i);
  diff=if use_mad!=(0 as i64) {absf(diff)} else {diff*diff};
  let wi:f32=if hasw!=(0 as i64) {at(w,i)} else {at(x,i)*at(x,i)};
  best_mad=best_mad+wi*diff;
  i=i+(1 as i64);
 }
 if nstep<(1 as i64) {
  st_f32(the_min,neg(min));
  return scale;
 }
 var is:i64=0 as i64;
 while is<=nstep {
  iscale=(rmin+rdelta*(is as f32)+(nmax as f32))/(max-min);
  var sum_l:f32=0 as f32;
  var sum_l2:f32=0 as f32;
  var sum_xl:f32=0 as f32;
  i=0 as i64;
  while i<n {
   let v:f32=iscale*(at(x,i)-min);
   let l:i64=if v>(0 as f32) {nearest(mnf(v,nmax as f32))} else {0 as i64};
   st_u8(Laux+(i as u64),l);
   let wi:f32=if hasw!=(0 as i64) {at(w,i)} else {at(x,i)*at(x,i)};
   sum_l=sum_l+wi*(l as f32);
   sum_l2=sum_l2+wi*(l as f32)*(l as f32);
   sum_xl=sum_xl+wi*(l as f32)*at(x,i);
   i=i+(1 as i64);
  }
  let D:f32=sum_w*sum_l2-sum_l*sum_l;
  if D>(0 as f32) {
   var this_scale:f32=(sum_w*sum_xl-sum_x*sum_l)/D;
   var this_min:f32=(sum_l2*sum_x-sum_l*sum_xl)/D;
   if this_min>(0 as f32) {this_min=0 as f32;this_scale=sum_xl/sum_l2;}
   var mad:f32=0 as f32;
   i=0 as i64;
   while i<n {
    var diff:f32=this_scale*(ld_u8(Laux+(i as u64)) as f32)+this_min-at(x,i);
    diff=if use_mad!=(0 as i64) {absf(diff)} else {diff*diff};
    let wi:f32=if hasw!=(0 as i64) {at(w,i)} else {at(x,i)*at(x,i)};
    mad=mad+wi*diff;
    i=i+(1 as i64);
   }
   if mad<best_mad {
    i=0 as i64;
    while i<n {st_u8(L+(i as u64),ld_u8(Laux+(i as u64)));i=i+(1 as i64);}
    best_mad=mad;scale=this_scale;min=this_min;
   }
  }
  is=is+(1 as i64);
 }
 st_f32(the_min,neg(min));
 return scale;
}

fn main(){fg330q_make_qkx3(0 as i64,0 as i64,0 as u64,0 as u64,0 as i64,0 as u64,0 as u64,0 as u64,0 as f32,0 as f32,0 as i64,0 as i64);}
