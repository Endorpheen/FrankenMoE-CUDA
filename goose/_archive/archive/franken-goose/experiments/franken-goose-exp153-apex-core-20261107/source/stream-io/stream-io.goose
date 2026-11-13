// stream-io.goose - owned byte/tensor bounds, accounting and ordering for the
// host/dummy/file concrete IO classes. Byte transfers and allocation are typed
// primitives (services.cpp); host destructor flush stays a transfer primitive.
// Source anchors: native/src/llama-context.cpp:2877-3060.
extern "fg153_stream_get" fn sg(kind:i64,s:i64,k:i64,idx:i64)->i64;
extern "fg153_stream_act" fn sa(kind:i64,s:i64,op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg153_dev_get" fn dg(p:i64,kind:i64,k:i64,idx:i64)->i64;
extern "fg153_dev_act" fn da(p:i64,kind:i64,op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_device" fn dv(op:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64;
fn add(a:i64,b:i64)->i64 {return a+b;}
fn dummy_write(s:i64,size:i64)->i64 {let ignored=sa(0,s,13,size,0,0,0);return 1;}
fn dummy_tensor(s:i64,size:i64)->i64 {
 if sg(0,s,2,0)!=0 {return 1;}
 let ignored=sa(0,s,13,size,0,0,0);return 1;
}
fn hostw_write(s:i64,src:i64,size:i64)->i64 {
 let bs=sg(1,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(1,s,0,src,size,0,0); // memcpy at current ptr
 let ignored2=sa(1,s,1,0,size,0,0); // advance ptr/written/buf_size
 return 1;
}
fn hostw_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let bs=sg(1,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(1,s,2,t,size,off,0); // append (ptr/offset recorded)
 let ignored2=sa(1,s,1,0,size,0,0);
 return 1;
}
fn hostr_read(s:i64,dst:i64,size:i64)->i64 {
 let bs=sg(2,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(2,s,3,dst,size,0,0);
 let ignored2=sa(2,s,4,0,size,0,0);
 return 1;
}
fn hostr_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let bs=sg(2,s,0,0);
 if size>bs {return 0;}
 let ignored=sa(2,s,5,t,size,off,0);
 let ignored2=sa(2,s,4,0,size,0,0);
 return 1;
}
fn hostr_discard(s:i64)->i64 {let ignored=sa(2,s,6,0,0,0,0);return 1;}
fn filew_write(s:i64,src:i64,size:i64)->i64 {
 let ignored=sa(3,s,7,src,size,0,0);
 let ignored2=sa(3,s,19,size,0,0,0);
 return 1;
}
fn filew_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let ignored=sa(3,s,18,size,0,0,0);   // temp_buffer.resize(size)
 let ignored2=sa(3,s,14,t,off,size,0); // tensor_get into temp
 let ignored3=sa(3,s,17,size,0,0,0);  // write_raw(temp,size)
 let ignored4=sa(3,s,19,size,0,0,0);  // size_written += size
 return 1;
}
fn filer_read(s:i64,dst:i64,size:i64)->i64 {
 let ignored=sa(4,s,8,dst,size,0,0);
 let ignored2=sa(4,s,20,size,0,0,0);
 return 1;
}
fn filer_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {
 let ignored=sa(4,s,15,size,0,0,0);    // temp resize + read_raw
 let ignored2=sa(4,s,16,t,off,size,0); // tensor_set from temp
 let ignored3=sa(4,s,20,size,0,0,0);
 return 1;
}
fn devw_write(s:i64,src:i64,size:i64)->i64 {
 let bs=dg(s,0,0,0); if size>bs {return 0;}
 let i1=da(s,0,0,src,size,0); let i2=da(s,0,1,0,size,0); return 1;
}
fn devw_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {let i=da(s,0,2,t,size,off);return 1;}
fn devr_read(s:i64,dst:i64,size:i64)->i64 {
 let bs=dg(s,1,0,0); if size>bs {return 0;}
 let i1=da(s,1,0,dst,size,0); let i2=da(s,1,1,0,size,0); return 1;
}
fn devr_tensor(s:i64,t:i64,size:i64,off:i64)->i64 {let i=da(s,1,2,t,size,off);return 1;}
fn devr_discard(s:i64)->i64 {let i=da(s,1,3,0,0,0);return 1;}
fn group_add(gb:i64,buft:i64,sz:i64)->i64 {
 let gc=dv(42,gb,0,0,0,0,0); var idx:i64=-1; var j:i64=0;
 while j<gc {if dv(42,gb,10,j,0,0,0)==buft {idx=j;} j=j+1;}
 if idx<0 {idx=gc; dv(43,gb,10,gc,buft,0,0); dv(43,gb,11,gc,0,0,0); dv(43,gb,12,gc,0,0,0); dv(43,gb,0,0,gc+1,0,0);}
 dv(43,gb,11,idx,dv(42,gb,11,idx,0,0,0)+1,0,0);
 dv(43,gb,12,idx,dv(42,gb,12,idx,0,0,0)+sz,0,0);
 return idx;
}
fn devw_dtor(s:i64)->i64 {
 let n=dg(s,0,3,0); let mb=dg(s,0,8,0); let gb=dv(40,0,0,0,0,0,0); dv(43,gb,0,0,0,0,0);
 var i:i64=0;
 while i<n {let t=dg(s,0,4,i); let ig=group_add(gb,dv(15,t,0,0,0,0,0),dg(s,0,6,i)); i=i+1;}
 let ovh=dv(14,0,0,0,0,0,0);
 var gi:i64=0; let gc=dv(42,gb,0,0,0,0,0);
 while gi<gc {
  let buft=dv(42,gb,10,gi,0,0,0); let cnt=dv(42,gb,11,gi,0,0,0); let tot=dv(42,gb,12,gi,0,0,0);
  let ctx=dv(10,2*cnt*ovh,0,0,0,0,0);
  let org=dv(40,0,0,0,0,0,0); let cpy=dv(40,0,0,0,0,0,0);
  var oi:i64=0; var k:i64=0;
  while k<n {
   let t=dg(s,0,4,k);
   if dv(15,t,0,0,0,0,0)==buft {
    let el=dv(16,t,0,0,0,0,0); let ne=dg(s,0,6,k)/el; let off=dg(s,0,7,k);
    let ov=dv(12,ctx,t,ne,off,0,0);
    let cv=dv(13,ctx,dv(17,t,0,0,0,0,0),ne,0,0,0);
    dv(43,org,10,oi,ov,0,0); dv(43,cpy,10,oi,cv,0,0); oi=oi+1;
   }
   k=k+1;
  }
  let en=dv(26,mb,buft,1,0,0,0);
  let cb=dv(27,en,2,0,0,0,0); let cn=dv(27,en,0,0,0,0,0); let ct=dv(27,en,1,0,0,0,0);
  var need:i64=0;
  if cb==0 {need=1;} if cn!=cnt {need=1;} if ct!=tot {need=1;}
  if need==0 {
   var x:i64=0;
   while x<cnt {
    let co=dv(27,en,6,x,0,0,0); let no=dv(42,org,10,x,0,0,0);
    if dv(19,co,no,0,0,0,0)==0 {need=1;}
    else {if dv(20,co,0,0,0,0,0)!=dv(20,no,0,0,0,0,0) {need=1;} if dv(21,co,0,0,0,0,0)!=dv(21,no,0,0,0,0,0) {need=1;}}
    x=x+1;
   }
  }
  var required:i64=0;let alignment=dv(35,buft,0,0,0,0,0);
  if alignment<=0 {dv(33,8,alignment,0,0,0,0);}
  var ai:i64=0;
  while ai<cnt {
   let bytes=dv(36,buft,dv(42,cpy,10,ai,0,0,0),0,0,0,0);
   if bytes<0||bytes>9223372036854775807-alignment+1 {dv(33,9,bytes,alignment,0,0,0);}
   let padded=((bytes+alignment-1)/alignment)*alignment;
   if required>9223372036854775807-padded {dv(33,10,required,padded,0,0,0);}
   required=required+padded;ai=ai+1;
  }
  var need_new:i64=0;
  if cb==0||ct!=tot {need_new=1;}else{if dv(37,cb,0,0,0,0,0)<required {need_new=1;need=1;}}
  if need!=0 {
   if need_new!=0 {
    let i1=dv(28,en,cnt,tot,ctx,org,cpy); let nb=dv(24,en,buft,0,0,0,0); let i2=dv(29,en,nb,0,0,0,0);
   } else {
    let old=dv(34,en,0,0,0,0,0); let i1=dv(28,en,cnt,tot,ctx,org,cpy); let i2=dv(29,en,old,0,0,0,0);
    let ta=dv(30,old,0,0,0,0,0); var y:i64=0;
    while y<cnt {let i3=dv(22,dv(42,org,10,y,0,0,0),0,0,0,0,0); let i4=dv(31,ta,dv(42,cpy,10,y,0,0,0),0,0,0,0); y=y+1;}
    let i5=dv(32,ta,0,0,0,0,0);
   }
  } else {let i0=dv(11,ctx,0,0,0,0,0);}
  var z:i64=0;
  while z<cnt {let iz=dv(23,dv(27,en,6,z,0,0,0),dv(27,en,7,z,0,0,0),0,0,0,0); z=z+1;}
  let f1=dv(41,org,0,0,0,0,0); let f2=dv(41,cpy,0,0,0,0,0);
  gi=gi+1;
 }
 dv(41,gb,0,0,0,0,0); return 1;
}
fn devr_dtor(s:i64)->i64 {
 let n=dg(s,1,3,0); let mb=dg(s,1,8,0); let gb=dv(40,0,0,0,0,0,0); dv(43,gb,0,0,0,0,0);
 var i:i64=0;
 while i<n {let t=dg(s,1,4,i); let ig=group_add(gb,dv(15,t,0,0,0,0,0),dg(s,1,6,i)); i=i+1;}
 let ovh=dv(14,0,0,0,0,0,0);
 var gi:i64=0; let gc=dv(42,gb,0,0,0,0,0);
 while gi<gc {
  let buft=dv(42,gb,10,gi,0,0,0); let cnt=dv(42,gb,11,gi,0,0,0); let tot=dv(42,gb,12,gi,0,0,0);
  let ctx=dv(10,cnt*ovh,0,0,0,0,0); let org=dv(40,0,0,0,0,0,0);
  var oi:i64=0; var k:i64=0;
  while k<n {
   let t=dg(s,1,4,k);
   if dv(15,t,0,0,0,0,0)==buft {
    let el=dv(16,t,0,0,0,0,0); let ne=dg(s,1,6,k)/el; let off=dg(s,1,7,k);
    let ov=dv(12,ctx,t,ne,off,0,0); let i0=dv(22,ov,0,0,0,0,0);
    dv(43,org,10,oi,ov,0,0); oi=oi+1;
   }
   k=k+1;
  }
  let en=dv(26,mb,buft,0,0,0,0);
  if en==0 {let ab=dv(33,1,buft,0,0,0,0);}
  let cb=dv(27,en,2,0,0,0,0); let ct=dv(27,en,1,0,0,0,0); let cc=dv(27,en,4,0,0,0,0);
  if cb==0||ct!=tot {let ab=dv(33,2,ct,tot,0,0,0);}
  var same:i64=0;
  if dv(27,en,0,0,0,0,0)==cnt {
   same=1; var x:i64=0;
   while x<cnt {
    if dv(18,dv(27,en,7,x,0,0,0),0,0,0,0,0)!=dv(18,dv(42,org,10,x,0,0,0),0,0,0,0,0) {same=0;}
    x=x+1;
   }
  }
  if same!=0 {
   var y:i64=0;
   while y<cnt {let ic=dv(23,dv(27,en,7,y,0,0,0),dv(42,org,10,y,0,0,0),0,0,0,0); y=y+1;}
  } else {
   let sc=dv(10,2*(cc+cnt)*ovh,0,0,0,0,0);
   var sp:i64=0; var dp:i64=0; var sj:i64=0; var di:i64=0; var sb:i64=0; var db:i64=0;
   while sp<tot {
    let st=dv(27,en,7,sj,0,0,0); let dt=dv(42,org,10,di,0,0,0);
    let ss=dv(18,st,0,0,0,0,0); let ds=dv(18,dt,0,0,0,0,0);
    let so=sp-sb; let dof=dp-db;
    var nc:i64=ss-so; let rem:i64=ds-dof; if rem<nc {nc=rem;}
    let el=dv(16,st,0,0,0,0,0); let nel=nc/el;
    let sv=dv(12,sc,st,nel,so,0,0); let i0=dv(22,sv,0,0,0,0,0);
    let dvw=dv(12,sc,dt,nel,dof,0,0); let i1=dv(22,dvw,0,0,0,0,0);
    let i2=dv(23,sv,dvw,0,0,0,0);
    sp=sp+nc; dp=dp+nc;
    if sp-sb==ss {sb=sp; sj=sj+1;}
    if dp-db==ds {db=dp; di=di+1;}
   }
   if sp!=tot {let a1=dv(33,3,sp,tot,0,0,0);}
   if dp!=tot {let a2=dv(33,4,dp,tot,0,0,0);}
   while sj<cc {if dv(18,dv(27,en,7,sj,0,0,0),0,0,0,0,0)!=0 {let a3=dv(33,5,sj,0,0,0,0);} sj=sj+1;}
   while di<cnt {if dv(18,dv(42,org,10,di,0,0,0),0,0,0,0,0)!=0 {let a4=dv(33,6,di,0,0,0,0);} di=di+1;}
   let i3=dv(11,sc,0,0,0,0,0);
  }
  let i4=dv(11,ctx,0,0,0,0,0); let f1=dv(41,org,0,0,0,0,0);
  gi=gi+1;
 }
 if dg(s,1,0,0)!=0 {let ab=dv(33,7,dg(s,1,0,0),0,0,0,0);}
 dv(41,gb,0,0,0,0,0); return 1;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 if mode==0 {return dummy_write(a,b);}
 if mode==1 {return dummy_tensor(a,b);}
 if mode==2 {return hostw_write(a,b,c);}
 if mode==3 {return hostw_tensor(a,b,c,d);}
 if mode==4 {return hostr_read(a,b,c);}
 if mode==5 {return hostr_tensor(a,b,c,d);}
 if mode==6 {return hostr_discard(a);}
 if mode==7 {return filew_write(a,b,c);}
 if mode==8 {return filew_tensor(a,b,c,d);}
 if mode==9 {return filer_read(a,b,c);}
 if mode==10 {return filer_tensor(a,b,c,d);}
 if mode==11 {return devw_write(a,b,c);}
 if mode==12 {return devw_tensor(a,b,c,d);}
 if mode==13 {return devr_read(a,b,c);}
 if mode==14 {return devr_tensor(a,b,c,d);}
 if mode==15 {return devr_discard(a);}
 if mode==16 {return devw_dtor(a);}
 if mode==17 {return devr_dtor(a);}
 if mode==18 {return 1;}
 return 0;
}
fn main(){print(entry(18,0,0,0,0,0));}
