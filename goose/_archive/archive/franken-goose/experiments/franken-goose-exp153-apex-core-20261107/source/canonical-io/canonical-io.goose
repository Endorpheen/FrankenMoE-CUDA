// canonical-io.goose - APX030 canonical IO representation/dispatch.
// Owned state lives in canonical records/vecs/maps; no C++ containers or
// virtual IO classes. Raw memcpy/file/tensor effects are primitives.
extern "fgc_record_new" fn rn(kind:i64)->i64;
extern "fgc_record_get" fn rg(h:i64,k:i64)->i64;
extern "fgc_record_set" fn rs(h:i64,k:i64,v:i64)->i64;
extern "fgc_i64_record_free" fn rf(h:i64)->i64;
extern "fgc_vec_new" fn vn(ib:i64)->i64;
extern "fgc_vec_len" fn vl(h:i64)->i64;
extern "fgc_i64_vec_data" fn vd(h:i64)->i64;
extern "fgc_vec_resize" fn vr(h:i64,n:i64)->i64;
extern "fgc_i64_vec_free" fn vf(h:i64)->i64;
extern "fgc_map_new" fn mn()->i64;
extern "fgc_i64_map_free" fn mf(h:i64)->i64;
extern "fgc_set_new" fn sn()->i64;
extern "fgc_set_insert" fn si(h:i64,k:i64)->i64;
extern "fgc_set_contains" fn sc(h:i64,k:i64)->i64;
extern "fgc_i64_set_free" fn sf(h:i64)->i64;
extern "fgcio_raw" fn raw(op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
fn ensure(ok:bool){if !ok {let i=raw(0,0,0,0,0);assert(false);}}
fn create(ty:i64,ptr:i64,buf:i64,skip:i64)->i64 {
 let r=rn(3101); if r==0 {return 0;}
 let l=vn(8); var g:i64=0; var shared:i64=0;
 if ty==6 {if skip!=0 {g=rg(skip,7);shared=1;} else {g=vn(8);}}
 if g==0 {g=vn(8);}
 let st=sn();
 let i0=rs(r,0,ty); let i1=rs(r,1,ptr); let i2=rs(r,2,buf); let i3=rs(r,3,0);
 let i4=rs(r,4,skip); let i5=rs(r,5,ptr); let i6=rs(r,6,l); let i7=rs(r,7,g); let i8=rs(r,8,st);
 let i9=rs(r,10,shared);
 return r;
}
fn push_entry(r:i64,t:i64,ptr:i64,size:i64,off:i64)->i64 {
 let e=rn(3102); if e==0 {return -1;}
 let i0=rs(e,0,t); let i1=rs(e,1,ptr); let i2=rs(e,2,size); let i3=rs(e,3,off);
 let l=rg(r,6); let n=vl(l); let s=vr(l,n+1); if s!=0 {let i=rf(e);return -1;}
 let d=vd(l); let i4=raw(8,d+8*n,e,0,0);
 return 0;
}
fn addsz(h:i64,size:i64)->i64 {let i=rs(h,3,rg(h,3)+size);return 0;}
fn write(h:i64,src:i64,size:i64)->i64 {
 let ty=rg(h,0);
 if ty==0 {let i=addsz(h,size);return 0;}
 if ty==3 {let f=rg(h,5); let st=raw(3,f,src,size,0); if st<0 {return -2;} let i=addsz(h,size); return 0;}
 let bs=rg(h,2); if size>bs {return -1;}
 let ptr=rg(h,1); let i0=raw(1,ptr,src,size,0);
 let i1=addsz(h,size); let i2=rs(h,1,ptr+size); let i3=rs(h,2,bs-size);
 return 0;
}
fn read(h:i64,dst:i64,size:i64)->i64 {
 let ty=rg(h,0);
 if ty==4 {let f=rg(h,5); let st=raw(4,f,dst,size,0); if st==0 {return -1;} let i=addsz(h,size); return 0;}
 let bs=rg(h,2); if size>bs {return -1;}
 let ptr=rg(h,1); let i0=raw(2,dst,ptr,size,0);
 let i1=addsz(h,size); let i2=rs(h,1,ptr+size); let i3=rs(h,2,bs-size);
 return 0;
}
fn wtensor(h:i64,t:i64,size:i64,off:i64)->i64 {
 let ty=rg(h,0);
 if ty==0 {if rg(h,4)!=0 {return 0;} let i=addsz(h,size); return 0;}
 if ty==3 {let f=rg(h,5); let st=raw(10,f,t,size,off); if st<0 {return -2;} let i=addsz(h,size); return 0;}
 if ty==5 {let e=push_entry(h,t,rg(h,1),size,off); return e;}
 let bs=rg(h,2); if size>bs {return -1;}
 let ptr=rg(h,1); let e=push_entry(h,t,ptr,size,off); if e!=0 {return e;}
 let i1=addsz(h,size); let i2=rs(h,1,ptr+size); let i3=rs(h,2,bs-size);
 return 0;
}
fn rtensor(h:i64,t:i64,size:i64,off:i64)->i64 {
 let ty=rg(h,0);
 if ty==4 {let f=rg(h,5); let st=raw(11,f,t,size,off); if st==0 {return -1;} let i=addsz(h,size); return 0;}
 if ty==6 {let e=push_entry(h,t,rg(h,1),size,off); return e;}
 let bs=rg(h,2); if size>bs {return -1;}
 let ptr=rg(h,1); let e=push_entry(h,t,ptr,size,off); if e!=0 {return e;}
 let i1=addsz(h,size); let i2=rs(h,1,ptr+size); let i3=rs(h,2,bs-size);
 return 0;
}
fn discard(h:i64)->i64 {
 let l=rg(h,6); let n=vl(l); var i=0;
 while i<n {let e=raw(7,vd(l)+8*i,0,0,0); if e!=0 {let f=rf(e);} i=i+1;}
 let s=vr(l,0);
 if rg(h,0)==6 {let z=rs(h,2,0);}
 return 0;
}
fn vpush(v:i64,x:i64)->i64 {
 let n=vl(v); let s=vr(v,n+1); if s!=0 {return -1;}
 let i=raw(8,vd(v)+8*n,x,0,0); return 0;
}
fn vat(v:i64,i:i64)->i64 {return raw(7,vd(v)+8*i,0,0,0);}
fn dev_group_add(h:i64,t:i64,size:i64)->i64 {
 let buft=raw(12,t,0,0,0); let st=rg(h,8); let gv=rg(h,7);
 if sc(st,buft)==0 {let x=si(st,buft); let g=rn(3103);
  let a=rs(g,0,buft); let b=rs(g,1,0); let c=rs(g,2,0); let d=rs(g,3,0); let e=rs(g,4,0);
  let f=vn(8); let c2=vn(8); let g1=rs(g,5,f); let g2=rs(g,6,c2); let z=vpush(gv,g);}
 let n=vl(gv); var idx:i64=0; var i:i64=0;
 while i<n {let g=vat(gv,i); if rg(g,0)==buft {idx=i;} i=i+1;}
 let g=vat(gv,idx); let a=rs(g,7,rg(g,7)+1); let b=rs(g,8,rg(g,8)+size); return g;
}
fn devw_flush(h:i64)->i64 {
 let l=rg(h,6); let n=vl(l); let gv=rg(h,7);
 var ri:i64=0; while ri<vl(gv) {let g=vat(gv,ri);rs(g,7,0);rs(g,8,0);ri=ri+1;}
 var i:i64=0;
 while i<n {let e=vat(l,i); let a=dev_group_add(h,rg(e,0),rg(e,2)); i=i+1;}
 let ovh=raw(25,0,0,0,0); let gl=vl(gv); var gi:i64=0;
 while gi<gl {
  let g=vat(gv,gi); let buft=rg(g,0); let cnt=rg(g,7); let tot=rg(g,8);
  if cnt>0 {
  let ctx=raw(21,2*cnt*ovh,0,0,0); let org=vn(8); let cpy=vn(8);
  var k:i64=0;
  while k<n {let e=vat(l,k); let t=rg(e,0);
   if raw(12,t,0,0,0)==buft {let sz=rg(e,2); let el=raw(13,t,0,0,0); let ne=sz/el; let off=rg(e,3);
    let ov=raw(23,ctx,t,ne,off); let cv=raw(24,ctx,raw(14,t,0,0,0),ne,0);
    let p1=vpush(org,ov); let p2=vpush(cpy,cv);}
   k=k+1;}
  let cb=rg(g,4); let cn=rg(g,1); let ct=rg(g,2); var need:i64=0;
  if cb==0 {need=1;} if cn!=cnt {need=1;} if ct!=tot {need=1;}
  if need==0 {let oo=rg(g,5); var x:i64=0;
   while x<cnt {let co=vat(oo,x); let no=vat(org,x);
    if raw(16,co,no,0,0)==0 {need=1;} else {
     if raw(17,co,0,0,0)!=raw(17,no,0,0,0) {need=1;}
     if raw(18,co,0,0,0)!=raw(18,no,0,0,0) {need=1;}}
    x=x+1;}}
  let align=raw(27,buft,0,0,0); var required:i64=0; var ai:i64=0;
  while ai<cnt {let bytes=raw(28,buft,vat(cpy,ai),0,0);
   if bytes<0 {raw(34,9,bytes,0,0);}
   let padded=((bytes+align-1)/align)*align; required=required+padded; ai=ai+1;}
  var need_new:i64=0;
  if cb==0||ct!=tot {need_new=1;} else {if raw(29,cb,0,0,0)<required {need_new=1;need=1;}}
  if need!=0 {
   if need_new!=0 {
    let oldbuf=rg(g,4); if oldbuf!=0 {raw(38,oldbuf,0,0,0);} let oc=rg(g,3); if oc!=0 {let f=raw(22,oc,0,0,0);}
    let a1=rs(g,3,ctx); let nb=raw(26,ctx,buft,0,0); let a2=rs(g,4,nb);
    let oo=rg(g,5); let ocp=rg(g,6); if oo!=0 {let f1=vf(oo);} if ocp!=0 {let f2=vf(ocp);}
    let a3=rs(g,5,org); let a4=rs(g,6,cpy); let a5=rs(g,1,cnt); let a6=rs(g,2,tot);
   } else {
    let oldbuf=rg(g,4); let oc=rg(g,3); if oc!=0 {let f=raw(22,oc,0,0,0);}
    let a1=rs(g,3,ctx); let a2=rs(g,4,oldbuf);
    let oo=rg(g,5); let ocp=rg(g,6); if oo!=0 {let f1=vf(oo);} if ocp!=0 {let f2=vf(ocp);}
    let a3=rs(g,5,org); let a4=rs(g,6,cpy); let a5=rs(g,1,cnt); let a6=rs(g,2,tot);
    let ta=raw(31,oldbuf,0,0,0); var y:i64=0;
    while y<cnt {let o=vat(org,y); let c=vat(cpy,y);
     let i3=raw(19,o,0,0,0); let i4=raw(32,ta,c,0,0); y=y+1;}
    let i5=raw(33,ta,0,0,0);
   }
  } else {let f=raw(22,ctx,0,0,0); let f1=vf(org); let f2=vf(cpy);}
  let so=rg(g,5); let sp=rg(g,6); var z:i64=0;
  while z<cnt {let ic=raw(20,vat(so,z),vat(sp,z),0,0); z=z+1;}
  }
  gi=gi+1;
 }
 var fi:i64=0;while fi<n {let e=vat(l,fi);rf(e);fi=fi+1;}
 let c1=vr(l,0); return 0;
}
fn find_group(h:i64,buft:i64)->i64 {
 let gv=rg(h,7); let n=vl(gv); var i:i64=0; var r:i64=0;
 while i<n {let g=vat(gv,i); if rg(g,0)==buft {r=g;} i=i+1;} return r;
}
fn buft_push(tv:i64,buft:i64)->i64 {
 let n=vl(tv); var i:i64=0; while i<n {if vat(tv,i)==buft {return 0;} i=i+1;}
 return vpush(tv,buft);
}
fn devr_flush(h:i64)->i64 {
 let l=rg(h,6); let n=vl(l); let ovh=raw(25,0,0,0,0); let tv=vn(8);
 var i:i64=0;
 while i<n {let e=vat(l,i); let q=buft_push(tv,raw(12,rg(e,0),0,0,0)); i=i+1;}
 let tl=vl(tv); var ti:i64=0;
 while ti<tl {
  let buft=vat(tv,ti); let p=find_group(h,buft); if p==0 {raw(34,1,buft,0,0);}
  let cntp=rg(p,1); let tot=rg(p,2);
  var read_count:i64=0;var ri:i64=0;
  while ri<n {if raw(12,rg(vat(l,ri),0),0,0,0)==buft {read_count=read_count+1;}ri=ri+1;}
  let ctx=raw(21,read_count*ovh,0,0,0); let org=vn(8); var k:i64=0;
  while k<n {let e=vat(l,k); let t=rg(e,0);
   if raw(12,t,0,0,0)==buft {let sz=rg(e,2); let el=raw(13,t,0,0,0); let ne=sz/el; let off=rg(e,3);
    let ov=raw(23,ctx,t,ne,off); let vi=raw(19,ov,0,0,0); let z=vpush(org,ov);}
   k=k+1;}
  let cb=rg(p,4); if cb==0 {raw(34,2,0,0,0);}
  let cpyv=rg(p,6); let cc=vl(cpyv); let cnt=vl(org); var same:i64=0;
  if cntp==cnt {same=1; var y:i64=0;
   while y<cnt {if raw(15,vat(cpyv,y),0,0,0)!=raw(15,vat(org,y),0,0,0) {same=0;} y=y+1;}}
  if same!=0 {var y:i64=0;
   while y<cnt {let ic=raw(20,vat(cpyv,y),vat(org,y),0,0); y=y+1;}}
  else {
   let sctx=raw(21,2*(cc+cnt)*ovh,0,0,0);
   var ps:i64=0; var pd:i64=0; var sj:i64=0; var di:i64=0; var sb:i64=0; var db:i64=0;
   while ps<tot {let st=vat(cpyv,sj); let dt=vat(org,di);
    let ss=raw(15,st,0,0,0); let ds=raw(15,dt,0,0,0);
    let so=ps-sb; let dof=pd-db; var nc:i64=ss-so; let rem:i64=ds-dof; if rem<nc {nc=rem;}
    let el=raw(13,st,0,0,0); let nel=nc/el;
    let sv=raw(23,sctx,st,nel,so); let i0=raw(19,sv,0,0,0);
    let dvw=raw(23,sctx,dt,nel,dof); let i1=raw(19,dvw,0,0,0);
    let i2=raw(20,sv,dvw,0,0);
    ps=ps+nc; pd=pd+nc;
    if ps-sb==ss {sb=ps; sj=sj+1;} if pd-db==ds {db=pd; di=di+1;}}
   if ps!=tot {raw(34,3,ps,tot,0);} if pd!=tot {raw(34,4,pd,tot,0);}
   while sj<cc {if raw(15,vat(cpyv,sj),0,0,0)!=0 {raw(34,5,sj,0,0);} sj=sj+1;}
   while di<cnt {if raw(15,vat(org,di),0,0,0)!=0 {raw(34,6,di,0,0);} di=di+1;}
   let f=raw(22,sctx,0,0,0);
  }
  let f2=raw(22,ctx,0,0,0); let f3=vf(org); ti=ti+1;
 }
 if rg(h,2)!=0 {raw(34,7,rg(h,2),0,0);}
 var fi:i64=0; while fi<n {let e=vat(l,fi); let f=rf(e); fi=fi+1;}
 let c1=vr(l,0);vf(tv);return 0;
}
fn flush(h:i64)->i64 {
 let ty=rg(h,0);
 if ty==5 {return devw_flush(h);}
 if ty==6 {return devr_flush(h);}
 if ty==3||ty==4 {discard(h); return 0;}
 let l=rg(h,6); let n=vl(l); var i=0;
 while i<n {
  let ep=vd(l)+8*i; let e=raw(7,ep,0,0,0); if e!=0 {
   let t=rg(e,0); let ptr=rg(e,1); let size=rg(e,2); let off=rg(e,3);
   if ty==1 {let x=raw(5,t,ptr,off,size);} else {let x=raw(6,t,ptr,off,size);}
   let f=rf(e);
  }
  i=i+1;
 }
 let s=vr(l,0);
 return 0;
}
fn dispose(h:i64)->i64 {
 if h==0 {return 0;}
 let ty=rg(h,0);
 if ty==5||ty==6 {let i0=discard(h);} else {let i0=flush(h);}
 let l=rg(h,6); let i1=discard(h); let vf0=vf(l);
 if rg(h,10)==0 {let g=rg(h,7); let gn=vl(g); var gi:i64=0;
  while gi<gn {let q=vat(g,gi); let cb=rg(q,4); if cb!=0 {let f0=raw(38,cb,0,0,0);}
   let oc=rg(q,3); if oc!=0 {let f1=raw(22,oc,0,0,0);}
   let oo=rg(q,5); if oo!=0 {let f2=vf(oo);} let cp=rg(q,6); if cp!=0 {let f3=vf(cp);}
   let f4=rf(q); gi=gi+1;}
  let f5=vf(g);}
 let st=rg(h,8); if st!=0 {let f6=sf(st);}
 let f=rf(h); return 0;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 if mode==0 {return create(a,b,c,d);}
 if mode==1 {return write(a,b,c);}
 if mode==2 {return read(a,b,c);}
 if mode==3 {return wtensor(a,b,c,d);}
 if mode==4 {return rtensor(a,b,c,d);}
 if mode==5 {return rg(a,3);}
 if mode==6 {return discard(a);}
 if mode==7 {return flush(a);}
 if mode==8 {return dispose(a);}
 if mode==9 {return 2147483647;}
 raw(0,0,0,0,0);return -1;
}
fn main(){print(entry(9,0,0,0,0,0));}
