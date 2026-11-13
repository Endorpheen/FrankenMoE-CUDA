// worker-15 / APX026: GGUF v1..v3 metadata and tensor-descriptor control.
//
// Every read loop and every accept/reject decision lives here. The bridge only
// provides byte reads, ggml type traits and typed record stores.

extern "fg153_gg_const" fn kconst(c:i64)->i64;
extern "fg153_gg_type_size" fn tsize(t:i64)->i64;
extern "fg153_gg_kv_tsize" fn kvtsize(t:i64)->i64;
extern "fg153_gg_blck_size" fn bsize(t:i64)->i64;
extern "fg153_gg_key" fn wkey(c:i64)->i64;
extern "fg153_gg_key_len" fn wkeylen(c:i64)->i64;

extern "fg153_gg_u8" fn u8(h:i64)->i64;
extern "fg153_gg_u16" fn u16(h:i64)->i64;
extern "fg153_gg_i16" fn i16(h:i64)->i64;
extern "fg153_gg_u32" fn u32(h:i64)->i64;
extern "fg153_gg_i32" fn i32(h:i64)->i64;
extern "fg153_gg_u64" fn u64(h:i64)->i64;
extern "fg153_gg_i64" fn i64v(h:i64)->i64;
extern "fg153_gg_f32bits" fn f32b(h:i64)->i64;
extern "fg153_gg_f64bits" fn f64b(h:i64)->i64;
extern "fg153_gg_str" fn rdstr(h:i64,max:i64)->i64;
extern "fg153_gg_kstr" fn kstr(h:i64,max:i64)->i64;
extern "fg153_gg_kscratch" fn kscratch(h:i64)->i64;
extern "fg153_gg_scratch" fn scratch(h:i64)->i64;
extern "fg153_gg_rd" fn rdbytes(h:i64,n:i64)->i64;
extern "fg153_gg_tell" fn tell(h:i64)->i64;
extern "fg153_gg_seek" fn seek(h:i64,off:i64)->i64;
extern "fg153_gg_start" fn start(h:i64)->i64;
extern "fg153_gg_failed" fn failed(h:i64)->i64;

extern "fg153_gg_set_version" fn set_version(s:i64,v:i64);
extern "fg153_gg_set_counts" fn set_counts(s:i64,nkv:i64,nt:i64);
extern "fg153_gg_set_alignment" fn set_alignment(s:i64,a:i64);
extern "fg153_gg_set_data" fn set_data(s:i64,off:i64,size:i64);
extern "fg153_gg_dup_key" fn dup_key(s:i64,kp:i64,kl:i64)->i64;
extern "fg153_gg_dup_tensor" fn dup_tensor(s:i64,np:i64,nl:i64)->i64;
extern "fg153_gg_kv_scalar" fn kv_scalar(s:i64,kp:i64,kl:i64,t:i64,bits:i64);
extern "fg153_gg_kv_string" fn kv_string(s:i64,kp:i64,kl:i64,sp:i64,sl:i64);
extern "fg153_gg_kv_arr" fn kv_arr(s:i64,kp:i64,kl:i64,et:i64,dp:i64,n:i64);
extern "fg153_gg_kv_arr_str_begin" fn kv_arr_str_begin(s:i64,kp:i64,kl:i64,n:i64);
extern "fg153_gg_kv_arr_str_push" fn kv_arr_str_push(s:i64,sp:i64,sl:i64);
extern "fg153_gg_tensor" fn tensor(s:i64,np:i64,nl:i64,nd:i64,ne0:i64,ne1:i64,ne2:i64,ne3:i64,
                                   t:i64,nb0:i64,nb1:i64,nb2:i64,nb3:i64,off:i64);
extern "fg153_gg_find_key" fn find_key(s:i64,kp:i64,kl:i64)->i64;
extern "fg153_gg_kv_type" fn kv_type(s:i64,i:i64)->i64;
extern "fg153_gg_kv_is_array" fn kv_is_array(s:i64,i:i64)->i64;
extern "fg153_gg_kv_u32" fn kv_u32(s:i64,i:i64)->i64;
extern "fg153_gg_t_ne" fn t_ne(s:i64,i:i64,j:i64)->i64;
extern "fg153_gg_t_nb" fn t_nb(s:i64,i:i64,j:i64)->i64;
extern "fg153_gg_t_type" fn t_type(s:i64,i:i64)->i64;
extern "fg153_gg_t_off" fn t_off(s:i64,i:i64)->i64;

fn is_pow2(a:i64)->bool {
 if a<=0 { return false; }
 var x=a;
 while x%2==0 { x=x/2; }
 return x==1;
}

fn read_val(h:i64,t:i64)->i64 {
 if t==0 { return u8(h); }
 if t==1 { return u8(h); }
 if t==2 { return u16(h); }
 if t==3 { return i16(h); }
 if t==4 { return u32(h); }
 if t==5 { return i32(h); }
 if t==6 { return f32b(h); }
 if t==7 { return u8(h); }
 if t==10 { return u64(h); }
 if t==11 { return i64v(h); }
 if t==12 { return f64b(h); }
 return 0;
}

fn t_nbytes(s:i64,i:i64)->i64 {
 let ty=t_type(s,i);
 let blck=bsize(ty);
 let ne0=t_ne(s,i,0); let ne1=t_ne(s,i,1);
 let ne2=t_ne(s,i,2); let ne3=t_ne(s,i,3);
 if ne0<=0 || ne1<=0 || ne2<=0 || ne3<=0 { return 0; }
 let nb0=t_nb(s,i,0); let nb1=t_nb(s,i,1);
 let nb2=t_nb(s,i,2); let nb3=t_nb(s,i,3);
 if blck==1 {
  return tsize(ty) + (ne0-1)*nb0 + (ne1-1)*nb1 + (ne2-1)*nb2 + (ne3-1)*nb3;
 }
 return ne0*nb0/blck + (ne1-1)*nb1 + (ne2-1)*nb2 + (ne3-1)*nb3;
}

fn parse_kv(h:i64,s:i64,nkv:i64)->i64 {
 var i=0;
 while i<nkv {
  let kl=kstr(h,kconst(7));
  if kl<0 { return 0; }
  if kl==0 { return 0; }
  let kp=kscratch(h);
  if dup_key(s,kp,kl)==0 { return 0; }
  let ty=i32(h);
  if failed(h)!=0 { return 0; }
  if ty<0 || ty>=kconst(10) { return 0; }
  var isarr=0;
  var elem=ty;
  var n=1;
  if ty==9 {
   isarr=1;
   elem=i32(h);
   if failed(h)!=0 { return 0; }
   if elem<0 || elem>=kconst(10) { return 0; }
   n=i64v(h);
   if failed(h)!=0 { return 0; }
   if n<0 || n>kconst(8) { return 0; }
  }
  if isarr==0 {
   if ty==8 {
    let sl=rdstr(h,kconst(7));
    if sl<0 { return 0; }
    kv_string(s,kp,kl,scratch(h),sl);
   } else {
    let sz=kvtsize(ty);
    if sz<=0 { return 0; }
    let bits=read_val(h,ty);
    if failed(h)!=0 { return 0; }
    kv_scalar(s,kp,kl,ty,bits);
   }
  } else {
   if elem==8 {
    kv_arr_str_begin(s,kp,kl,n);
    var j=0;
    while j<n {
     let sl=rdstr(h,kconst(7));
     if sl<0 { return 0; }
     kv_arr_str_push(s,scratch(h),sl);
     j+=1;
    }
   } else {
    let esz=kvtsize(elem);
    if esz<=0 { return 0; }
    if n > kconst(9)/esz { return 0; }
    let bytes=n*esz;
    let dp=rdbytes(h,bytes);
    if failed(h)!=0 { return 0; }
    kv_arr(s,kp,kl,elem,dp,n);
   }
  }
  i+=1;
 }
 return 1;
}

fn parse_tensors(h:i64,s:i64,nt:i64)->i64 {
 var i=0;
 while i<nt {
  let nl=kstr(h,kconst(7));
  if nl<0 { return 0; }
  if nl>=kconst(3) { return 0; }
  let np=kscratch(h);
  if dup_tensor(s,np,nl)==0 { return 0; }
  let nd=u32(h);
  if failed(h)!=0 { return 0; }
  if nd>kconst(2) { return 0; }
  var ne0=1; var ne1=1; var ne2=1; var ne3=1;
  if nd>0 { ne0=i64v(h); }
  if nd>1 { ne1=i64v(h); }
  if nd>2 { ne2=i64v(h); }
  if nd>3 { ne3=i64v(h); }
  if failed(h)!=0 { return 0; }
  if ne0<0 || ne1<0 || ne2<0 || ne3<0 { return 0; }
  var total=ne0;
  if ne1>1 { if kconst(9)/ne1 < total { return 0; } total=total*ne1; } else { total=total*ne1; }
  if ne2>1 { if kconst(9)/ne2 < total { return 0; } total=total*ne2; } else { total=total*ne2; }
  if ne3>1 { if kconst(9)/ne3 < total { return 0; } total=total*ne3; } else { total=total*ne3; }
  let ty=i32(h);
  if failed(h)!=0 { return 0; }
  if ty<0 || ty>=kconst(4) { return 0; }
  let sz=tsize(ty);
  let blck=bsize(ty);
  if sz<=0 || blck==0 { return 0; }
  if ne0%blck!=0 { return 0; }
  if total/blck > kconst(9)/sz { return 0; }
  let nb0=sz;
  let nb1=nb0*(ne0/blck);
  let nb2=nb1*ne1;
  let nb3=nb2*ne2;
  let off=i64v(h);
  if failed(h)!=0 { return 0; }
  if off<0 { return 0; }
  tensor(s,np,nl,nd,ne0,ne1,ne2,ne3,ty,nb0,nb1,nb2,nb3,off);
  i+=1;
 }
 return 1;
}

fn parse_data_size(s:i64,nt:i64,align:i64)->i64 {
 var sz=0;
 var i=0;
 while i<nt {
  if t_off(s,i)!=sz { return 0; }
  let nby=t_nbytes(s,i);
  if nby > kconst(9) - (align-1) { return 0; }
  let padded=((nby+align-1)/align)*align;
  if padded < nby { return 0; }
  if sz > kconst(9) - padded { return 0; }
  sz+=padded;
  i+=1;
 }
 return sz;
}

fn parse(h:i64,s:i64)->i64 {
 let m0=u8(h); let m1=u8(h); let m2=u8(h); let m3=u8(h);
 if failed(h)!=0 { return 0; }
 if m0!=71 || m1!=71 || m2!=85 || m3!=70 { return 0; }

 let version=u32(h);
 if failed(h)!=0 { return 0; }
 if version==0 { return 0; }
 if version%65536==0 { return 0; }
 if version==1 { return 0; }
 if version>kconst(0) { return 0; }

 let nt=i64v(h);
 if failed(h)!=0 { return 0; }
 if nt<0 || nt>kconst(6) { return 0; }
 let nkv=i64v(h);
 if failed(h)!=0 { return 0; }
 if nkv<0 || nkv>kconst(5) { return 0; }

 set_version(s,version);
 set_counts(s,nkv,nt);

 if parse_kv(h,s,nkv)==0 { return 0; }
 if failed(h)!=0 { return 0; }

 var align=kconst(1);
 let aidx=find_key(s,wkey(0),wkeylen(0));
 if aidx>=0 {
  if kv_is_array(s,aidx)!=0 { return 0; }
  if kv_type(s,aidx)!=4 { return 0; }
  align=kv_u32(s,aidx);
 }
 if align<=0 { return 0; }
 if !is_pow2(align) { return 0; }
 set_alignment(s,align);

 if parse_tensors(h,s,nt)==0 { return 0; }
 if failed(h)!=0 { return 0; }

 if nt>0 {
  let pos=tell(h); let st=start(h);
  if pos<0 || st<0 { return 0; }
  let rel=pos-st;
  if rel<0 { return 0; }
  if rel > kconst(9) - (align-1) { return 0; }
  let padlen=((rel+align-1)/align)*align;
  if seek(h,st+padlen)!=0 { return 0; }
 }
 let doff=tell(h);
 if doff<0 { return 0; }
 let dsz=parse_data_size(s,nt,align);
 if dsz<0 { return 0; }
 set_data(s,doff,dsz);
 return 1;
}

fn parse_entry(op:i64,h:i64,s:i64,a:i64,b:i64,c:i64)->i64 {
 if op==99 { return 0; }
 if op==0 { return parse(h,s); }
 return 0;
}

fn main() { print(parse_entry(99,0,0,0,0,0)); }
