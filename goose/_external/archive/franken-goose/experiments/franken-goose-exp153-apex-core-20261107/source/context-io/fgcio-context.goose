// fgcio-context.goose - bind + context-aware persistent device writer cache.
// Consumes the qualified canonical fgcio_* ABI; no std containers or IO policy
// duplication. Registry/ctx records are canonical storage records.
extern "fgc_record_new" fn rn(k:i64)->i64;
extern "fgc_record_get" fn rg(h:i64,k:i64)->i64;
extern "fgc_record_set" fn rs(h:i64,k:i64,v:i64)->i64;
extern "fgc_i64_record_free" fn rf(h:i64)->i64;
extern "fgc_vec_new" fn vn(ib:i64)->i64;
extern "fgc_vec_len" fn vl(h:i64)->i64;
extern "fgc_i64_vec_data" fn vd(h:i64)->i64;
extern "fgc_vec_resize" fn vr(h:i64,n:i64)->i64;
extern "fgc_i64_vec_free" fn vf(h:i64)->i64;
extern "fgc_map_new" fn mn()->i64;
extern "fgc_map_put" fn mput(h:i64,k:i64,v:i64)->i64;
extern "fgc_i64_map_get" fn mget(h:i64,k:i64,out:i64)->i64;
extern "fgc_map_erase" fn merase(h:i64,k:i64)->i64;
extern "fgc_i64_map_free" fn mf(h:i64)->i64;
extern "fgcio_create" fn io_create(ty:i64,ptr:i64,buf:i64,skip:i64)->i64;
extern "fgcio_dispose" fn io_dispose(h:i64)->i64;
extern "fgcio_raw" fn raw(op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
fn vpush(v:i64,x:i64)->i64 {let n=vl(v); let s=vr(v,n+1); if s!=0 {return -1;} return raw(8,vd(v)+8*n,x,0,0);}
fn vat(v:i64,i:i64)->i64 {return raw(7,vd(v)+8*i,0,0,0);}
extern "fgcio_map_value" fn map_get(m:i64,k:i64)->i64;
fn bind(h:i64,ptr:i64,size:i64)->i64 {
 if h==0 {return -1;} if ptr<=0||size<0 {return -1;}
 let l=rg(h,6); if l!=0 {if vl(l)!=0 {return -1;}} // pending tensor entries
 let i0=rs(h,1,ptr); let i1=rs(h,2,size); let i2=rs(h,3,0);
 return 0;
}
fn reg_new()->i64 {
 let r=rn(3200); if r==0 {return 0;}
 let m=mn(); if m==0 {rf(r);return 0;}
 if rs(r,0,m)!=0||rs(r,1,0)!=0 {mf(m);rf(r);return 0;}
 return r;
}
fn ctx_get(reg:i64,ctx:i64,seq:i64,dst:i64,size:i64)->i64 {
 if reg==0||ctx==0||dst<=0||size<0 {return 0;}
 let m=rg(reg,0); var cr=map_get(m,ctx);
 if cr==0 {
  cr=rn(3201); if cr==0 {return 0;}
  let sm=mn();if sm==0 {rf(cr);return 0;}
  let sv=vn(8);if sv==0 {mf(sm);rf(cr);return 0;}
  if rs(cr,0,sm)!=0||rs(cr,1,sv)!=0 {vf(sv);mf(sm);rf(cr);return 0;}
  if mput(m,ctx,cr)!=0 {vf(sv);mf(sm);rf(cr);return 0;}
  let c=rs(reg,1,rg(reg,1)+1);
 }
 let sm=rg(cr,0);var w=map_get(sm,seq);
 if w==0 {
  let sv=rg(cr,1);let n=vl(sv);if vr(sv,n+1)!=0 {return 0;}
  w=io_create(5,dst,size,0);
  if w==0 {vr(sv,n);return 0;}
  if mput(sm,seq,w)!=0 {io_dispose(w);vr(sv,n);return 0;}
  raw(8,vd(sv)+8*n,w,0,0);return w;
 }
 if bind(w,dst,size)!=0 {return 0;}
 return w;
}
fn ctx_has(reg:i64,ctx:i64,seq:i64)->i64 {
 if reg==0 {return 0;}let cr=map_get(rg(reg,0),ctx);
 if cr==0 {return 0;}if map_get(rg(cr,0),seq)!=0 {return 1;}return 0;
}
fn ctx_read(reg:i64,ctx:i64,seq:i64,src:i64,size:i64)->i64 {
 if reg==0 {return 0;}
 let cr=map_get(rg(reg,0),ctx); if cr==0 {return 0;}
 let w=map_get(rg(cr,0),seq); if w==0 {return 0;}
 let r=io_create(6,0,0,w); if r==0 {return 0;}
 let b=bind(r,src,size); if b!=0 {let d=io_dispose(r); return 0;}
 return r;
}
fn ctx_drop(reg:i64,ctx:i64)->i64 {
 if reg==0 {return 0;}
 let m=rg(reg,0); let cr=map_get(m,ctx); if cr==0 {return 0;}
 let sv=rg(cr,1); let n=vl(sv); var i=0;
 while i<n {let w=vat(sv,i); if w!=0 {let d=io_dispose(w);} i=i+1;}
 let sm=rg(cr,0); let f1=mf(sm); let f2=vf(sv); let f3=rf(cr); let e=merase(m,ctx);
 let cnt=rg(reg,1)-1;
 if cnt<=0 {let m2=rg(reg,0); let f4=mf(m2); let f5=rf(reg); return 1;}
 let c=rs(reg,1,cnt); return 0;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 if mode==0 {return bind(a,b,c);}
 if mode==1 {return ctx_get(a,b,c,d,e);}
 if mode==2 {return ctx_read(a,b,c,d,e);}
 if mode==3 {return ctx_drop(a,b);}
 if mode==4 {return reg_new();}
 if mode==5 {return ctx_has(a,b,c);}
 if mode==9 {return 1;}
 raw(0,0,0,0,0);return -1;
}
fn main(){print(entry(9,0,0,0,0,0));}
