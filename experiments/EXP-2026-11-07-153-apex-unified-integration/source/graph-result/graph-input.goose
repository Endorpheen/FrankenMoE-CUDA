extern "fg153_gi_get" fn gi_get(inp:i64,kind:i64,k:i64,i:i64)->i64;
extern "fg153_gi_fget" fn gi_fget(inp:i64,kind:i64,k:i64)->f32;
extern "fg153_gu_get" fn gu_get(u:i64,k:i64,i:i64)->i64;
extern "fg153_gp_get" fn gp_get(p:i64,k:i64,i:i64)->i64;
extern "fg153_gp_ubatch" fn gp_ubatch(p:i64)->i64;
extern "fg153_gx_ne" fn gx_ne(t:i64,i:i64)->i64;
extern "fg153_gx_elem" fn gx_elem(t:i64)->i64;
extern "fg153_gx_nbytes" fn gx_nbytes(t:i64)->i64;
extern "fg153_gx_is_host" fn gx_is_host(t:i64)->i64;
extern "fg153_gx_store_i32" fn gx_store_i32(t:i64,idx:i64,v:i64);
extern "fg153_gx_store_i64" fn gx_store_i64(t:i64,idx:i64,v:i64);
extern "fg153_gx_store_f32" fn gx_store_f32(t:i64,idx:i64,v:f32);
extern "fg153_gx_set_i32" fn gx_set_i32(t:i64,idx:i64,v:i64);
extern "fg153_gx_set_bytes" fn gx_set_bytes(t:i64,src:i64,off:i64,size:i64);
extern "fg153_gx_assert" fn gx_assert(ok:i64);

fn ensure(ok:bool) { if !ok { gx_assert(0); } }

fn n_tok_rows(u:i64)->i64 {
 if gu_get(u,6,0)==0 { return gu_get(u,0,0); }
 var n=0; var i=0;
 while i<gu_get(u,0,0) { if gu_get(u,14,i)==0 { n+=1; } i+=1; }
 return n;
}

fn embd_set(inp:i64,u:i64)->i64 {
 let n=gu_get(u,0,0);
 let tokens=gi_get(inp,0,0,0);
 if gu_get(u,7,0)!=0 {
  ensure(tokens!=0);
  gx_set_bytes(tokens,gu_get(u,19,0),0,n*gx_elem(tokens));
 }
 let embd=gi_get(inp,0,1,0);
 if gu_get(u,8,0)!=0 && embd!=0 && gu_get(u,6,0)==0 {
  ensure(gi_get(inp,0,6,0)==gx_ne(embd,0));
  gx_set_bytes(embd,gu_get(u,20,0),0,n*gi_get(inp,0,6,0)*gx_elem(embd));
 }
 if gu_get(u,6,0)!=0 && embd!=0 {
  let mt=gi_get(inp,0,2,0); let ms=gi_get(inp,0,3,0); let me=gi_get(inp,0,4,0);
  ensure(mt!=0); ensure(ms!=0); ensure(me!=0);
  var rows=0; var i=0;
  while i<n {
   if gu_get(u,14,i)==0 {
    gx_store_i32(mt,rows,gu_get(u,13,i));
    gx_store_i64(ms,rows,i);
    rows+=1;
   }
   i+=1;
  }
  ensure(rows==gx_ne(mt,0));
  ensure(gi_get(inp,0,6,0)==gx_ne(me,0));
  gx_set_bytes(me,gu_get(u,20,0),0,gx_nbytes(me));
 }
 let sr=gi_get(inp,0,5,0);
 if sr!=0 {
  let scale=gi_fget(inp,0,7);
  var i=0;
  while i<n {
   var is_embd=0;
   if gu_get(u,7,0)==0 { is_embd=1; }
   if gu_get(u,6,0)!=0 && gu_get(u,14,i)!=0 { is_embd=1; }
   var v=scale; if is_embd!=0 { v=1.0; }
   gx_store_f32(sr,i,v);
   i+=1;
  }
 }
 return 0;
}

fn embd_reuse(inp:i64,p:i64)->bool {
 let u=gp_ubatch(p);
 let nt=gu_get(u,0,0);
 let tokens=gi_get(inp,0,0,0);
 if gu_get(u,7,0)!=0 { if tokens==0||gx_ne(tokens,0)!=nt {return false;} }
 let embd=gi_get(inp,0,1,0);
 if gu_get(u,8,0)!=0 { if embd==0||gx_ne(embd,1)!=nt {return false;} }
 let mt=gi_get(inp,0,2,0);
 if mt!=0 { if gx_ne(mt,0)!=n_tok_rows(u) {return false;} }
 let me=gi_get(inp,0,4,0);
 if me!=0 { if gx_ne(me,1)!=nt {return false;} }
 let sr=gi_get(inp,0,5,0);
 if sr!=0 { if gx_ne(sr,1)!=nt {return false;} }
 return true;
}

fn embd_h_set(inp:i64,u:i64)->i64 {
 ensure(gu_get(u,6,0)==0);
 let n=gu_get(u,0,0);
 let tokens=gi_get(inp,1,0,0);
 if gu_get(u,7,0)!=0 {
  ensure(tokens!=0);
  gx_set_bytes(tokens,gu_get(u,19,0),0,n*gx_elem(tokens));
 } else {
  ensure(gu_get(u,8,0)!=0);
  let embd=gi_get(inp,1,1,0); ensure(embd!=0);
  ensure(gi_get(inp,1,3,0)==gx_ne(embd,0));
  gx_set_bytes(embd,gu_get(u,20,0),0,n*gi_get(inp,1,3,0)*gx_elem(embd));
 }
 ensure(gu_get(u,9,0)!=0);
 let h=gi_get(inp,1,2,0); ensure(h!=0);
 ensure(gi_get(inp,1,4,0)==gx_ne(h,0));
 gx_set_bytes(h,gu_get(u,21,0),0,n*gi_get(inp,1,4,0)*gx_elem(h));
 return 0;
}

fn embd_h_reuse(inp:i64,p:i64)->bool {
 let u=gp_ubatch(p);
 let nt=gu_get(u,0,0);
 let tokens=gi_get(inp,1,0,0);
 if gu_get(u,7,0)!=0 { if tokens==0||gx_ne(tokens,0)!=nt {return false;} }
 let embd=gi_get(inp,1,1,0);
 if gu_get(u,8,0)!=0 { if embd==0||gx_ne(embd,1)!=nt {return false;} }
 let h=gi_get(inp,1,2,0);
 if gu_get(u,9,0)!=0 { if h==0||gx_ne(h,1)!=nt {return false;} }
 return true;
}

fn pos_set(inp:i64,u:i64)->i64 {
 let pos=gi_get(inp,2,0,0);
 if gu_get(u,10,0)==0 { return 0; }
 ensure(pos!=0);
 let n=gu_get(u,0,0);
 let npe=gi_get(inp,2,1,0);
 if npe==0 { return 0; }
 let order=gi_get(inp,2,2,0);
 let has_embd=gu_get(u,6,0)!=0 || gu_get(u,7,0)==0;
 if order==0 || !has_embd {
  gx_set_bytes(pos,gu_get(u,22,0),0,n*npe*gx_elem(pos));
  return 0;
 }
 ensure(npe==4);
 var slot_of_section:i64[4]=default<i64[4]>();
 slot_of_section[0]=0; slot_of_section[1]=1; slot_of_section[2]=2; slot_of_section[3]=3;
 if order==1 {
  slot_of_section[0]=3; slot_of_section[1]=1; slot_of_section[2]=2; slot_of_section[3]=0;
 } else {
  ensure(false);
 }
 var i=0;
 while i<n {
  var is_embd=0;
  if gu_get(u,6,0)!=0 {
   if gu_get(u,14,i)!=0 { is_embd=1; }
  } else {
   is_embd=1;
  }
  var s=0;
  while s<4 {
   var slot=s;
   if is_embd!=0 { slot=slot_of_section[s]; }
   gx_set_i32(pos,s*n+i,gu_get(u,15,slot*n+i));
   s+=1;
  }
  i+=1;
 }
 return 0;
}

fn pos_reuse(inp:i64,p:i64)->bool {
 let pos=gi_get(inp,2,0,0);
 if pos==0 { return false; }
 let u=gp_ubatch(p);
 return gx_ne(pos,0)==gu_get(u,0,0)*gi_get(inp,2,1,0);
}

fn out_ids_set(inp:i64,u:i64)->i64 {
 let out=gi_get(inp,3,0,0); ensure(out!=0);
 let n=gu_get(u,0,0);
 let nout=gi_get(inp,3,1,0);
 ensure(gx_is_host(out)!=0);
 if nout==n {
  var i=0; while i<n { gx_store_i32(out,i,i); i+=1; }
  return 0;
 }
 ensure(gu_get(u,11,0)!=0);
 var k=0; var i=0;
 while i<n {
  if gu_get(u,16,i)!=0 { gx_store_i32(out,k,i); k+=1; }
  i+=1;
 }
 return 0;
}

fn out_ids_reuse(inp:i64,p:i64)->bool {
 return gi_get(inp,3,1,0)==gp_get(p,1,0);
}

fn inp_entry(op:i64,inp:i64,x:i64,y:i64,z:i64,w:i64)->i64 {
 if op==99 {return 0;}
 if op==0 {return embd_set(inp,x);}
 if op==1 {if embd_reuse(inp,x) {return 1;} return 0;}
 if op==2 {return embd_h_set(inp,x);}
 if op==3 {if embd_h_reuse(inp,x) {return 1;} return 0;}
 if op==4 {return pos_set(inp,x);}
 if op==5 {if pos_reuse(inp,x) {return 1;} return 0;}
 if op==6 {return out_ids_set(inp,x);}
 if op==7 {if out_ids_reuse(inp,x) {return 1;} return 0;}
 ensure(false);
 return 0;
}

fn main() { print(inp_entry(99,0,0,0,0,0)); }
