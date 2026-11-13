// worker-15 / APX026: model-loader control over parsed GGUF shards.
//
// Goose owns the split policy, the duplicate/bounds policy per weight, the
// done_getting_tensors counts and the mmap range merge/subtract algebra. The
// bridge only stores weights/ranges and exposes parsed descriptor reads.

extern "fg153_gg_const" fn kconst(c:i64)->i64;
extern "fg153_gg_type_size" fn tsize(t:i64)->i64;
extern "fg153_gg_blck_size" fn bsize(t:i64)->i64;
extern "fg153_gg_key" fn wkey(c:i64)->i64;
extern "fg153_gg_key_len" fn wkeylen(c:i64)->i64;

extern "fg153_gg_header" fn header(s:i64,c:i64)->i64;
extern "fg153_gg_find_key" fn find_key(s:i64,kp:i64,kl:i64)->i64;
extern "fg153_gg_kv_type" fn kv_type(s:i64,i:i64)->i64;
extern "fg153_gg_kv_is_array" fn kv_is_array(s:i64,i:i64)->i64;
extern "fg153_gg_kv_u16" fn kv_u16(s:i64,i:i64)->i64;
extern "fg153_gg_kv_i32" fn kv_i32(s:i64,i:i64)->i64;
extern "fg153_gg_t_ne" fn t_ne(s:i64,i:i64,j:i64)->i64;
extern "fg153_gg_t_nb" fn t_nb(s:i64,i:i64,j:i64)->i64;
extern "fg153_gg_t_type" fn t_type(s:i64,i:i64)->i64;
extern "fg153_gg_t_off" fn t_off(s:i64,i:i64)->i64;
extern "fg153_gg_t_name" fn t_name(s:i64,i:i64)->i64;
extern "fg153_gg_t_name_len" fn t_name_len(s:i64,i:i64)->i64;
extern "fg153_gg_tensor_count" fn tcount(s:i64)->i64;

extern "fg153_run_len" fn run_len(r:i64)->i64;
extern "fg153_run_store" fn run_store(r:i64,i:i64)->i64;
extern "fg153_run_size" fn run_size(r:i64,i:i64)->i64;
extern "fg153_lw_add" fn lw_add(h:i64,np:i64,nl:i64,idx:i64,offs:i64,nbytes:i64,fz:i64)->i64;
extern "fg153_lw_count" fn lw_count(h:i64)->i64;
extern "fg153_lw_ok" fn lw_ok(h:i64)->i64;
extern "fg153_lr_push" fn lr_push(h:i64,f:i64,l:i64);
extern "fg153_lr_len" fn lr_len(h:i64)->i64;
extern "fg153_lr_first" fn lr_first(h:i64,i:i64)->i64;
extern "fg153_lr_last" fn lr_last(h:i64,i:i64)->i64;
extern "fg153_lr_set_last" fn lr_set_last(h:i64,i:i64,l:i64);
extern "fg153_lr_sort" fn lr_sort(h:i64);

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

fn get_u16_key(s:i64,code:i64,required:i64)->i64 {
 let k=find_key(s,wkey(code),wkeylen(code));
 if k<0 { return -1; }
 if kv_is_array(s,k)!=0 { return -1; }
 if kv_type(s,k)!=2 { return -1; }
 let v=kv_u16(s,k);
 if v<0 { return -1; }
 return v;
}

fn weight_bounds(doff:i64,toff:i64,nbytes:i64,fz:i64)->i64 {
 if doff<0 || toff<0 || nbytes<0 || fz<0 { return 0; }
 if doff > kconst(9)-toff { return 0; }
 let offs=doff+toff;
 if offs > kconst(9)-nbytes { return 0; }
 if offs+nbytes > fz { return 0; }
 return 1;
}

fn add_store(run:i64,lw:i64,idx:i64)->i64 {
 let s=run_store(run,idx);
 if s==0 { return 0; }
 let fz=run_size(run,idx);
 let doff=header(s,4);
 let nt=tcount(s);
 var j=0;
 while j<nt {
  let toff=t_off(s,j);
  let nby=t_nbytes(s,j);
  if weight_bounds(doff,toff,nby,fz)==0 { return 0; }
  let offs=doff+toff;
  if lw_add(lw,t_name(s,j),t_name_len(s,j),idx,offs,nby,fz)==0 { return 0; }
  j+=1;
 }
 return 1;
}

fn build_plan(run:i64,lw:i64)->i64 {
 let nfile=run_len(run);
 if nfile<=0 { return 0; }
 let s0=run_store(run,0);
 if s0==0 { return 0; }
 var nsplit=1;
 let ci=find_key(s0,wkey(1),wkeylen(1));
 if ci>=0 {
  let v=get_u16_key(s0,1,1);
  if v<0 { return 0; }
  nsplit=v;
 }
 if nsplit>1 {
  if nsplit!=nfile { return 0; }
  let mi=get_u16_key(s0,2,1);
  if mi!=0 { return 0; }
 }
 var i=0;
 while i<nfile {
  let s=run_store(run,i);
  if s==0 { return 0; }
  if nsplit>1 && i>0 {
   let si=get_u16_key(s,2,1);
   if si!=i { return 0; }
  }
  if add_store(run,lw,i)==0 { return 0; }
  i+=1;
 }
 if nsplit>1 {
  let k=find_key(s0,wkey(3),wkeylen(3));
  if k<0 { return 0; }
  if kv_is_array(s0,k)!=0 { return 0; }
  if kv_type(s0,k)!=5 { return 0; }
  let want=kv_i32(s0,k);
  if want!=lw_count(lw) { return 0; }
 }
 if lw_ok(lw)==0 { return 0; }
 return 1;
}

fn merge_ranges(inp:i64,out:i64)->i64 {
 lr_sort(inp);
 let n=lr_len(inp);
 var i=0;
 while i<n {
  let f=lr_first(inp,i);
  let l=lr_last(inp,i);
  let m=lr_len(out);
  if m>0 && f<=lr_last(out,m-1) {
   if l>lr_last(out,m-1) { lr_set_last(out,m-1,l); }
  } else {
   lr_push(out,f,l);
  }
  i+=1;
 }
 return 1;
}

fn sub_ranges(a:i64,sub:i64,out:i64)->i64 {
 lr_sort(a);
 lr_sort(sub);
 let na=lr_len(a);
 let ns=lr_len(sub);
 var ia=0;
 while ia<na {
  let first=lr_first(a,ia);
  let xlast=lr_last(a,ia);
  var cur=first;
  var is=0;
  while is<ns {
   let yf=lr_first(sub,is);
   let yl=lr_last(sub,is);
   if yl<=cur || yf>=xlast {
    is+=1;
   } else {
    if yf>cur { lr_push(out,cur,yf); }
    if yl>cur { cur=yl; }
    is+=1;
   }
  }
  if cur<xlast { lr_push(out,cur,xlast); }
  ia+=1;
 }
 return 1;
}

fn mmap_update(first:i64,second:i64,offs:i64,nbytes:i64,out:i64)->i64 {
 if offs<0 || nbytes<0 { return 0; }
 if offs > kconst(9)-nbytes { return 0; }
 let end=offs+nbytes;
 var f=first;
 var s=second;
 if offs<f { f=offs; }
 if end>s { s=end; }
 lr_push(out,f,s);
 return 1;
}

fn loader_entry(op:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 if op==99 { return 0; }
 if op==0 { return build_plan(a,b); }
 if op==1 {
  if a>b { return 1; }
  if a<b { if c==0 { return 2; } }
  return 0;
 }
 if op==2 { return merge_ranges(a,b); }
 if op==3 { return sub_ranges(a,b,c); }
 if op==4 { return mmap_update(b,c,d,e,a); }
 if op==5 { return weight_bounds(a,b,c,d); }
 if op==6 { if a>=b { return 1; } return 0; }
 return 0;
}

fn main() { print(loader_entry(99,0,0,0,0,0)); }
