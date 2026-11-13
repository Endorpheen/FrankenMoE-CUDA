extern "fg153_gh_get" fn gh_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_gh_fget" fn gh_fget(p:i64,k:i64)->f32;
extern "fg153_gc_clear" fn gc_clear(out:i64,n:i64);
extern "fg153_gc_put" fn gc_put(out:i64,index:i64,value:i64);
extern "fg153_gc_putf" fn gc_putf(out:i64,index:i64,value:f32);
extern "fg153_gx_assert" fn gx_assert(ok:i64);

fn ensure(ok:bool) { if !ok { gx_assert(0); } }

fn ctx_meta(p:i64,out:i64)->i64 {
 gc_clear(out,29*8+8*4);
 gc_put(out,0,gh_get(p,0,0));
 gc_put(out,1,gh_get(p,1,0));
 gc_put(out,2,gh_get(p,2,0));
 gc_put(out,3,gh_get(p,3,0));
 gc_put(out,4,gh_get(p,4,0));
 gc_put(out,5,gh_get(p,16,0));
 gc_put(out,6,gh_get(p,5,0));
 gc_put(out,7,gh_get(p,6,0));
 gc_put(out,8,gh_get(p,7,0));
 gc_put(out,9,gh_get(p,8,0));
 gc_put(out,10,gh_get(p,9,0));
 gc_put(out,11,gh_get(p,10,0));
 gc_put(out,12,gh_get(p,11,0));
 gc_put(out,13,if gh_get(p,25,0)!=0 {gh_get(p,11,0)} else {gh_get(p,12,0)});
 gc_put(out,14,gh_get(p,26,0));
 gc_put(out,15,gh_get(p,27,0));
 gc_put(out,16,gh_get(p,23,0));
 gc_put(out,17,gh_get(p,24,0));
 gc_put(out,18,gh_get(p,15,0));
 gc_put(out,19,gh_get(p,28,0));
 gc_put(out,20,gh_get(p,29,0));
 gc_put(out,21,gh_get(p,30,0));
 gc_put(out,22,gh_get(p,31,0));
 gc_put(out,23,gh_get(p,32,0));
 gc_put(out,24,gh_get(p,33,0));
 gc_put(out,25,gh_get(p,34,0));
 gc_put(out,26,gh_get(p,35,0));
 gc_put(out,27,gh_get(p,36,0));
 gc_put(out,28,gh_get(p,37,0));
 gc_putf(out,0,gh_fget(p,0));
 gc_putf(out,1,gh_fget(p,1));
 gc_putf(out,2,gh_fget(p,2));
 gc_putf(out,3,gh_fget(p,3));
 gc_putf(out,4,gh_fget(p,4));
 gc_putf(out,5,gh_fget(p,5));
 gc_putf(out,6,gh_fget(p,6));
 gc_putf(out,7,gh_fget(p,7));
 return 0;
}

fn ctx_valid(p:i64)->i64 {
 if gh_get(p,37,0)==0 { return 0; }
 if gh_get(p,1,0)<0 { return 0; }
 if gh_get(p,2,0)<0 { return 0; }
 if gh_get(p,26,0)<0 { return 0; }
 return 1;
}

fn ctx_entry(op:i64,p:i64,out:i64,a:i64,b:i64,c:i64)->i64 {
 if op==99 {return 0;}
 if op==0 {return ctx_meta(p,out);}
 if op==1 {
  if a==0 {return 0;}
  if gh_get(p,9,0)==0 {return 1;}
  if gh_get(p,10,0)!=0 {return 1;}
  return 0;
 }
 if op==2 {
  if a==0 {return 0;}
  if gh_get(p,9,0)!=0 && gh_get(p,10,0)==0 {return 1;}
  return 0;
 }
 if op==3 {return ctx_valid(p);}
 ensure(false);
 return 0;
}

fn main() { print(ctx_entry(99,0,0,0,0,0)); }
