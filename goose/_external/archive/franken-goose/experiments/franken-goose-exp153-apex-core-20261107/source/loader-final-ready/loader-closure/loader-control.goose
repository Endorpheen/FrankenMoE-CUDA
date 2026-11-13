// worker-15 / loader-closure: real weight-path control.
//
// Ops:
//   0 init_mappings(call{L,prefetch,mlocks})
//   1 load_all_data(call{L,ctx,bufs,lmlocks,progress_cb,progress_ud})
//   2 load_data_range(call{L,weight,offs,size,buf}) -> pointer, 0 bounds, 2 bad data
//   3 lazy_decide(mode,nbytes,mmap_supported)
//   4 skip_tensor(op_is_none,skip_flag)
//   5 dup_reuse(tensor_ptr)
extern "fg153_lc_call_get" fn call_get(h:i64,k:i64)->i64;
extern "fg153_lc_field" fn field(L:i64,c:i64)->i64;
extern "fg153_lc_add_size_data" fn add_size_data(L:i64,n:i64);
extern "fg153_lc_size_done_add" fn size_done_add(L:i64,n:i64);

extern "fg153_lc_files_count" fn files_count(L:i64)->i64;
extern "fg153_lc_mapping_create" fn mapping_create(L:i64,i:i64,pf:i64,numa:i64)->i64;
extern "fg153_lc_mapping_handle_size" fn map_hsize(m:i64)->i64;
extern "fg153_lc_mapping_handle_free" fn map_hfree(m:i64);
extern "fg153_lc_mappings_reserve" fn mappings_reserve(L:i64,n:i64)->i64;
extern "fg153_lc_mapping_push" fn mapping_push(L:i64,m:i64);
extern "fg153_lc_mappings_reset" fn mappings_reset(L:i64);
extern "fg153_lc_mapping_addr" fn map_addr(L:i64,i:i64)->i64;
extern "fg153_lc_mapping_size_idx" fn map_size(L:i64,i:i64)->i64;
extern "fg153_lc_mapping_unmap_idx" fn map_unmap(L:i64,i:i64,f:i64,l:i64);
extern "fg153_lc_mmaps_used_reserve" fn used_reserve(L:i64,n:i64)->i64;
extern "fg153_lc_mmaps_used_push" fn used_push(L:i64,f:i64,s:i64);
extern "fg153_lc_mmaps_used_get" fn used_get(L:i64,i:i64,w:i64)->i64;
extern "fg153_lc_mmaps_used_update" fn used_update(L:i64,i:i64,o:i64,n:i64);
extern "fg153_lc_cpu_is_numa" fn cpu_is_numa()->i64;
extern "fg153_lc_mlocks_add" fn mlocks_add(ml:i64,m:i64)->i64;
extern "fg153_lc_mlocks_reset" fn mlocks_reset(ml:i64);
extern "fg153_lc_mlock_grow" fn mlock_grow(ml:i64,i:i64,n:i64);

extern "fg153_lc_weights_count" fn weights_count(L:i64)->i64;
extern "fg153_lc_weight_nbytes_at" fn weight_nbytes_at(L:i64,i:i64)->i64;
extern "fg153_lc_weight" fn weight(L:i64,np:i64,nl:i64,w:i64)->i64;
extern "fg153_lc_lazy_has" fn lazy_has(L:i64,t:i64)->i64;
extern "fg153_lc_tw_idx" fn tw_idx(w:i64)->i64;
extern "fg153_lc_tw_offs" fn tw_offs(w:i64)->i64;
extern "fg153_lc_tw_nbytes" fn tw_nbytes(w:i64)->i64;
extern "fg153_lc_tw_type" fn tw_type(w:i64)->i64;

extern "fg153_lc_ctx_first" fn ctx_first(c:i64)->i64;
extern "fg153_lc_ctx_next" fn ctx_next(c:i64,t:i64)->i64;
extern "fg153_lc_tensor_nbytes" fn tensor_nbytes(t:i64)->i64;
extern "fg153_lc_tensor_name" fn tensor_name(t:i64)->i64;
extern "fg153_lc_tensor_name_len" fn tensor_name_len(t:i64)->i64;
extern "fg153_lc_tensor_data" fn tensor_data(t:i64)->i64;
extern "fg153_lc_tensor_type" fn tensor_type(t:i64)->i64;
extern "fg153_lc_tensor_alloc" fn tensor_alloc(b:i64,t:i64,d:i64);
extern "fg153_lc_tensor_set" fn tensor_set(t:i64,src:i64,o:i64,n:i64);
extern "fg153_lc_set_tensor_data" fn set_tensor_data(L:i64,t:i64);

extern "fg153_lc_find_buf" fn find_buf(b:i64,d:i64,n:i64)->i64;

extern "fg153_lc_file_read" fn file_read(L:i64,i:i64,o:i64,dst:i64,n:i64);
extern "fg153_lc_file_read_tensor" fn file_read_tensor(L:i64,i:i64,o:i64,t:i64,n:i64);
extern "fg153_lc_validate_tensor" fn validate_tensor(t:i64)->i64;
extern "fg153_lc_validate_ptr" fn validate_ptr(ty:i64,d:i64,n:i64)->i64;
extern "fg153_lc_progress" fn progress(cb:i64,num:i64,den:i64,ud:i64)->i64;

fn init_mappings(call:i64)->i64 {
 let L=call_get(call,0);
 let prefetch=call_get(call,1);
 let mlocks=call_get(call,2);
 let usemmap=field(L,0);
 let lazyany=field(L,11);
 if usemmap!=0 || lazyany!=0 {
  let n=files_count(L);
  if n<0 { return 0; }
  if mappings_reserve(L,n)==0 { return 0; }
  if used_reserve(L,n)==0 { mappings_reset(L); return 0; }
  var i=0;
  while i<n {
   let numa=cpu_is_numa();
   var pf=0;
   if prefetch!=0 && usemmap!=0 { pf=-1; }
   let mp=mapping_create(L,i,pf,numa);
   if mp==0 { mappings_reset(L); if mlocks!=0 { mlocks_reset(mlocks); } return 0; }
   used_push(L,map_hsize(mp),0);
   if mlocks!=0 {
    if mlocks_add(mlocks,mp)==0 { map_hfree(mp); mappings_reset(L); return 0; }
   }
   mapping_push(L,mp);
   i+=1;
  }
 }
 var j=0;
 let wc=weights_count(L);
 while j<wc {
  let nb=weight_nbytes_at(L,j);
  if nb<0 { return 0; }
  add_size_data(L,nb);
  j+=1;
 }
 return 1;
}

fn load_tensor(L:i64,t:i64,bufs:i64,lmlocks:i64,cb:i64,ud:i64)->i64 {
 let np=tensor_name(t);
 let nl=tensor_name_len(t);
 if weight(L,np,nl,3)==0 { return 1; }
 let n=tensor_nbytes(t);
 if cb!=0 {
  let sd=field(L,4);
  let st=field(L,5);
  if progress(cb,sd,st,ud)==0 { return 0; }
 }
 let lazy=lazy_has(L,t);
 let usemmap=field(L,0);
 let check=field(L,2);
 if usemmap!=0 || lazy!=0 {
  let widx=weight(L,np,nl,0);
  let woffs=weight(L,np,nl,1);
  if widx<0 || woffs<0 { return -1; }
  let base=map_addr(L,widx);
  if base==0 { return -1; }
  let data=base+woffs;
  let buf=find_buf(bufs,data,n);
  if buf!=0 && tensor_data(t)==0 {
   tensor_alloc(buf,t,data);
   if lmlocks!=0 && lazy==0 { mlock_grow(lmlocks,widx,woffs+n); }
   used_update(L,widx,woffs,n);
  } else if tensor_data(t)!=0 || buf!=0 {
   tensor_set(t,data,0,n);
  } else {
   return -1;
  }
  if check!=0 {
   if validate_ptr(tensor_type(t),data,n)==0 { return -1; }
  }
 } else {
  let widx=weight(L,np,nl,0);
  let woffs=weight(L,np,nl,1);
  if widx<0 || woffs<0 { return -1; }
  file_read_tensor(L,widx,woffs,t,n);
  if check!=0 {
   if validate_tensor(t)==0 { return -1; }
  }
 }
 size_done_add(L,n);
 return 1;
}

fn cleanup_mappings(L:i64)->i64 {
 let usemmap=field(L,0);
 if usemmap==0 { return 1; }
 let nm=field(L,7);
 var i=0;
 while i<nm {
  let first=used_get(L,i,0);
  let second=used_get(L,i,1);
  if first<0 { return 0; }
  if first>0 { map_unmap(L,i,0,first); }
  if second!=0 {
   let sz=map_size(L,i);
   if sz<0 { return 0; }
   if second<sz { map_unmap(L,i,second,sz); }
  }
  i+=1;
 }
 return 1;
}

fn load_all(call:i64)->i64 {
 let L=call_get(call,0);
 let ctx=call_get(call,1);
 let bufs=call_get(call,2);
 let lmlocks=call_get(call,3);
 let cb=call_get(call,4);
 let ud=call_get(call,5);
 if field(L,6)==0 {
  var t=ctx_first(ctx);
  while t!=0 {
   set_tensor_data(L,t);
   t=ctx_next(ctx,t);
  }
  return 1;
 }
 var t=ctx_first(ctx);
 while t!=0 {
  let rc=load_tensor(L,t,bufs,lmlocks,cb,ud);
  if rc!=1 { return rc; }
  t=ctx_next(ctx,t);
 }
 let sd=field(L,4);
 let st=field(L,5);
 if sd>=st {
  if cleanup_mappings(L)==0 { return -1; }
  if cb!=0 {
   if progress(cb,1,1,ud)==0 { return 0; }
  }
 }
 return 1;
}

fn load_range(call:i64)->i64 {
 let L=call_get(call,0);
 let w=call_get(call,1);
 let offs=call_get(call,2);
 let size=call_get(call,3);
 let buf=call_get(call,4);
 let n=tw_nbytes(w);
 if n<0 || offs<0 || size<0 || offs>n || size>n-offs { return 0; }
 let widx=tw_idx(w);
 let woffs=tw_offs(w);
 if widx<0 || woffs<0 { return 0; }
 var data=buf;
 if field(L,0)!=0 {
  let base=map_addr(L,widx);
  if base==0 { return 0; }
  data=base+woffs+offs;
 } else {
  if buf==0 { return 0; }
  file_read(L,widx,woffs+offs,buf,size);
 }
 if field(L,2)!=0 {
  if validate_ptr(tw_type(w),data,size)==0 { return 2; }
 }
 return data;
}

fn lc_entry(op:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 if op==99 { return 0; }
 if op==0 { return init_mappings(a); }
 if op==1 { return load_all(a); }
 if op==2 { return load_range(a); }
 if op==3 {
  if a==0 { return 0; }
  if a!=2 { if b<=4294967296 { return 0; } }
  if c==0 { return 0; }
  return 1;
 }
 if op==4 { if a!=0 || b!=0 { return 1; } return 0; }
 if op==5 { if a!=0 { return 1; } return 0; }
 return 0;
}

fn main() { print(lc_entry(99,0,0,0,0,0)); }
