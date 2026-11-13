// recurrent-lifecycle.goose - Goose proposal for recurrent memory lifecycle.
// Source: inputs/llama-memory-recurrent.cpp/.h, inputs/llama-memory.h.
//
// Owned methods ported as real Goose control logic (only typed primitives are opaque):
//   ctor allocation ownership: r_l/s_l/p_l per layer, ctxs_bufs, is_empty,
//     n_rs_seq disable-on-empty, per-layer size accounting + logging
//   is_empty / total_size / size_r_bytes / size_s_bytes / size_p_bytes / memory_breakdown
//   init_batch : split loop, n_used<n_tokens guard, prepare snapshot/rollback, context status
//   init_full / init_update
//   context: next / apply / get_status / get_ubatch / get_n_rs / get_head / get_rs_z /
//     get_size / get_r_l / get_s_l / get_p_l (is_full branch)
// Reused from recurrent-cells.goose via the fg153_lc seam (NOT reimplemented):
//   create, find_slot, prepare (snapshot/restore), s_copy, can_shift, shared header keys.
// OUT OF SCOPE / blocked elsewhere: state_write/state_read tensor rows, graph build,
//   hybrid forwarding, full KV, and the batch split algorithms (llama-batch-allocr).
//
// The service calls below never contain the init_batch/update/context algorithm:
// they are typed storage, tensor alloc/free/read/write, buft/dev introspection,
// logging sinks and batch-allocr split primitives. All ordered decisions are Goose.
//
// Shared header keys (identical to recurrent-cells.goose, documented seam, not a new ABI):
//   0 size, 1 used, 2 head, 3 n, 4 n_seq_max, 5 n_rs_seq, 6 rs_z, 7 empty,
//   10 pos, 11 src, 12 src0, 13 tail, 14 seq handle, 15 rs_idx.
// Lifecycle-owned extension keys on the same mem handle (cells never reads them):
//   70 r_l[il], 71 s_l[il], 72 p_l[il], 80 buf[k], 81 buft[k], 83 ctx[k], 84 buf_count.
extern "fg153_lg" fn lget(p:i64,k:i64,i:i64)->i64;
extern "fg153_lp" fn lput(p:i64,k:i64,i:i64,v:i64);
extern "fg153_ls" fn lsvc(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_lc" fn lcell(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_lw" fn lwit(mode:i64);
fn fail(reason:i64) {let ignored=lsvc(0,reason,0,0);assert(false);}
fn ensure(ok:bool) {if !ok {fail(1);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
// faithful to inputs/llama-memory.cpp:50-65 (status_is_fail)
fn status_is_fail(s:i64)->i64 {if s==0||s==1 {return 0;}if s==2||s==3 {return 1;}return 0;}
// faithful to inputs/llama-memory.cpp:9-48 (status_combine)
fn status_combine(s0:i64,s1:i64)->i64 {
 var has_update=false;
 if s0==2||s0==3 {return s0;}
 if s0==0 {has_update=true;}
 if s1==2||s1==3 {return s1;}
 if s1==0 {has_update=true;}
 return if has_update {0}else{1};
}
fn status_fail(s:i64)->i64 {return status_is_fail(s);}
fn size_of(mp:i64)->i64 {return lget(mp,0,0);}
fn used_of(mp:i64)->i64 {return lget(mp,1,0);}
fn head_of(mp:i64)->i64 {return lget(mp,2,0);}
fn n_of(mp:i64)->i64 {return lget(mp,3,0);}
fn n_rs_seq_of(mp:i64)->i64 {return lget(mp,5,0);}
fn rs_z_of(mp:i64)->i64 {return lget(mp,6,0);}
fn empty_of(mp:i64)->i64 {return lget(mp,7,0);}
fn hnew(bytes:i64)->i64 {return lsvc(1,bytes,0,0);}
fn hfree(h:i64) {let ignored=lsvc(2,h,0,0);}
fn list_count(l:i64)->i64 {return lget(l,11,0);}
fn list_item(l:i64,i:i64)->i64 {return lget(l,10,i);}
fn list_push(l:i64,v:i64) {let n=list_count(l);lput(l,10,n,v);lput(l,11,0,add(n,1));}
fn ctx_make(status:i64,mem:i64,is_full:i64)->i64 {
 let c=hnew(64);if c==0 {return 0;}
 lput(c,0,0,status);lput(c,1,0,mem);lput(c,2,0,0);lput(c,3,0,0);lput(c,4,0,0);lput(c,5,0,is_full);
 return c;
}
fn ctx_for_buft(map:i64,buft:i64,mp:i64)->i64 {
 let count=lget(map,0,0);var i:i64=0;
 while i<count {
  let have=lget(map,1,i);
  if lsvc(14,lsvc(13,have,0,0),lsvc(13,buft,0,0),0)!=0 {return lget(map,2,i);}
  i=add(i,1);
 }
 let ctx=lsvc(15,buft,mp,0);if ctx==0 {return 0;}
 lput(map,1,count,buft);lput(map,2,count,ctx);lput(map,0,0,add(count,1));return ctx;
}
// Filtered hybrid layers and PLE==0 leave NULL tensors; native skips them.
fn size_r_bytes(mp:i64)->i64 {
 var total:i64=0;var i:i64=0;let n=lsvc(3,0,mp,0);
 while i<n {let t=lget(mp,70,i);if t!=0 {total=add(total,lsvc(19,t,0,0));}i=add(i,1);}
 return total;
}
fn size_s_bytes(mp:i64)->i64 {
 var total:i64=0;var i:i64=0;let n=lsvc(3,0,mp,0);
 while i<n {let t=lget(mp,71,i);if t!=0 {total=add(total,lsvc(19,t,0,0));}i=add(i,1);}
 return total;
}
fn size_p_bytes(mp:i64)->i64 {
 var total:i64=0;var i:i64=0;let n=lsvc(3,0,mp,0);
 while i<n {let t=lget(mp,72,i);if t!=0 {total=add(total,lsvc(19,t,0,0));}i=add(i,1);}
 return total;
}
fn total_size(mp:i64)->i64 {
 var total:i64=0;var k:i64=0;let count=lget(mp,84,0);
 while k<count {total=add(total,lsvc(24,lget(mp,80,k),0,0));k=add(k,1);}
 return total;
}
fn is_empty(mp:i64)->i64 {
 let res=if lget(mp,84,0)==0 {1}else{0};
 ensure(res==0||total_size(mp)==0);
 return res;
}
// Free Goose-created buffers + every context in the dedup map exactly once.
// Side ctx entries share the map pointers, so they are nulled, not freed twice.
fn alloc_cleanup(mp:i64,map:i64) {
 var k:i64=0;let sc=lget(mp,84,0);
 while k<sc {
  let b=lget(mp,80,k);
  if b!=0 {lsvc(27,b,0,0);lput(mp,80,k,0);}
  lput(mp,83,k,0);
  k=add(k,1);
 }
 k=0;let mc=lget(map,0,0);
 while k<mc {lsvc(16,lget(map,2,k),0,0);k=add(k,1);}
 lput(mp,84,0,0);hfree(map);
}
fn create_owned(mp:i64,cfg:i64)->i64 {
 let size=lget(cfg,0,0);let n_seq_max=lget(cfg,1,0);let n_rs_seq=lget(cfg,2,0);
 let offload=lget(cfg,3,0);let type_r=lget(cfg,4,0);let type_s=lget(cfg,5,0);
 // metadata init is delegated to cells.create (same control code, no duplication)
 let ccfg=hnew(32);if ccfg==0 {return -2;}
 lput(ccfg,0,0,size);lput(ccfg,1,0,n_seq_max);lput(ccfg,2,0,n_rs_seq);lput(ccfg,3,0,0);
 let created=lcell(0,mp,ccfg,0);hfree(ccfg);
 if created!=0 {return -2;}
 let n_layer=lsvc(3,0,mp,0);
 var i:i64=0;
 while i<n_layer {lput(mp,70,i,0);lput(mp,71,i,0);lput(mp,72,i,0);i=add(i,1);}
 let map=hnew(64);if map==0 {return -2;}
 lput(map,0,0,0);
 let n_rs=n_rs_seq;
 let ple=lsvc(6,0,mp,0);
 i=0;
 while i<n_layer {
  if lsvc(8,i,mp,0)==0 {lsvc(29,i,0,0);i=add(i,1);continue;}
  var buft=lsvc(12,0,0,0);
  if offload!=0 {let dev=lsvc(9,i,mp,0);buft=lsvc(10,dev,0,0);lsvc(29,i,0,0);}
  let ctx=ctx_for_buft(map,buft,mp);
  if ctx==0 {alloc_cleanup(mp,map);return -3;}
  let n_rows=mul(size,add(1,n_rs));
  let tcfg=hnew(32);if tcfg==0 {alloc_cleanup(mp,map);return -2;}
  lput(tcfg,0,0,type_r);lput(tcfg,1,0,lsvc(4,0,mp,0));lput(tcfg,2,0,n_rows);
  let r=lsvc(17,ctx,tcfg,0);
  lput(tcfg,0,0,type_s);lput(tcfg,1,0,lsvc(5,0,mp,0));lput(tcfg,2,0,n_rows);
  let s=lsvc(17,ctx,tcfg,0);hfree(tcfg);
  lsvc(18,r,i,1);lsvc(18,s,i,2);
  lput(mp,70,i,r);lput(mp,71,i,s);
  if ple>0&&lsvc(7,i,mp,0)!=0 {
   let pcfg=hnew(32);if pcfg==0 {alloc_cleanup(mp,map);return -2;}
   lput(pcfg,0,0,type_r);lput(pcfg,1,0,ple);lput(pcfg,2,0,n_rows);
   let p=lsvc(17,ctx,pcfg,0);hfree(pcfg);lsvc(18,p,i,3);lput(mp,72,i,p);
  }
  i=add(i,1);
 }
 let count=lget(map,0,0);
 var k:i64=0;
 while k<count {
  let buft=lget(map,1,k);let ctx=lget(map,2,k);
  let buf=lsvc(22,ctx,buft,0);
  if buf==0 {alloc_cleanup(mp,map);return -4;}
  lsvc(23,buf,0,0);lsvc(28,1,lsvc(24,buf,0,0),0);
  lput(mp,80,k,buf);lput(mp,81,k,buft);lput(mp,83,k,ctx);
  lput(mp,84,0,add(k,1)); // incremental so failure cleanup sees created pairs
  k=add(k,1);
 }
 lput(mp,84,0,count);
 let empty=if count==0 {1}else{0};lput(mp,7,0,empty);
 if empty!=0 {if n_rs_seq_of(mp)>0 {lput(mp,5,0,0);lsvc(28,2,0,0);}}
 lsvc(28,3,add(add(size_r_bytes(mp),size_s_bytes(mp)),size_p_bytes(mp)),count);
 // hand buffer ownership to the real class (ctxs_bufs) BEFORE freeing the map,
 // so the failure cleanup still has valid map storage (no use-after-free).
 let adopted=lsvc(41,mp,0,0);
 if adopted!=0 {alloc_cleanup(mp,map);return -5;} // cleanup frees map
 hfree(map);
 return 0;
}
fn destroy_owned(mp:i64) {
 // post-adopt: the real class owns ctxs_bufs and frees them in its dtor
 if lsvc(42,mp,0,0)!=0 {lput(mp,84,0,0);return;}
 var k:i64=0;let count=lget(mp,84,0);
 while k<count {lsvc(27,lget(mp,80,k),0,0);lsvc(16,lget(mp,83,k),0,0);k=add(k,1);}
 lput(mp,84,0,0);lput(mp,7,0,1);
}
fn breakdown(mp:i64)->i64 {
 let out=hnew(64);if out==0 {return 0;}
 lput(out,11,0,0);
 var k:i64=0;let count=lget(mp,84,0);
 while k<count {
  let buf=lget(mp,80,k);let buft=lsvc(25,buf,0,0);let bytes=lsvc(24,buf,0,0);
  var found:i64=-1;var i:i64=0;let n=list_count(out);
  while i<n {if lget(out,10,i)==buft {found=i;}i=add(i,1);}
  if found>=0 {lput(out,12,found,add(lget(out,12,found),bytes));}
  else {lput(out,10,n,buft);lput(out,12,n,bytes);lput(out,11,0,add(n,1));}
  k=add(k,1);
 }
 return out;
}
fn build_desc(ub:i64)->i64 {
 let d=hnew(64);if d==0 {return 0;}
 let n_seqs=lsvc(37,ub,0,0);let n_tok=lsvc(36,ub,0,0);
 lput(d,20,0,n_seqs);lput(d,21,0,n_tok);
 var s:i64=0;
 while s<n_seqs {lput(d,22,s,lsvc(38,ub,s,0));lput(d,23,s,lsvc(39,ub,s,0));s=add(s,1);}
 return d;
}
fn init_batch(mp:i64,balloc:i64,n_ubatch:i64,embd_all:i64)->i64 {
 lsvc(30,balloc,0,0);
 let list=hnew(128);if list==0 {return 0;}
 lput(list,11,0,0);
 let tail=if n_rs_seq_of(mp)>0 {add(n_rs_seq_of(mp),1)}else{0};
 var done=false;
 while !done {
  var ub:i64=0;
  if embd_all!=0 {ub=lsvc(31,balloc,n_ubatch,0);}
  else {
   let ecfg=hnew(32);if ecfg==0 {hfree(list);return 0;}
   lput(ecfg,0,0,n_ubatch);lput(ecfg,1,0,1);lput(ecfg,2,0,tail);
   ub=lsvc(32,balloc,ecfg,0);hfree(ecfg);
  }
  if lsvc(35,ub,0,0)==0 {done=true;}else {list_push(list,ub);}
 }
 var ok:i64=1;
 if lsvc(33,balloc,0,0)<lsvc(34,balloc,0,0) {ok=0;}
 if ok!=0 {
  let count=list_count(list);
  let batch=hnew(128);if batch==0 {hfree(list);return 0;}
  lput(batch,24,0,count);
  var k:i64=0;
  while k<count {lput(batch,25,k,build_desc(list_item(list,k)));k=add(k,1);}
  let prepared=lcell(11,mp,batch,0);
  k=0;
  while k<count {hfree(lget(batch,25,k));k=add(k,1);}
  hfree(batch);
  if prepared==0 {ok=0;}
 }
 if ok==0 {
  // native returns a FAILED_PREPARE context; snapshot/rollback already done by prepare
  hfree(list);
  return ctx_make(2,mp,0);
 }
 let c=ctx_make(0,mp,0);if c==0 {hfree(list);return 0;}
 lput(c,3,0,list);lput(c,4,0,list_count(list));
 return c;
}
fn init_full(mp:i64)->i64 {return ctx_make(0,mp,1);}
fn init_update(mp:i64)->i64 {return ctx_make(1,mp,0);}
fn ctx_next(c:i64)->i64 {
 ensure(lget(c,0,0)==0);
 let nn=add(lget(c,2,0),1);lput(c,2,0,nn);
 if nn>=lget(c,4,0) {return 0;}
 return 1;
}
fn ctx_apply(c:i64)->i64 {
 ensure(status_fail(lget(c,0,0))==0);
 if lget(c,4,0)==0 {ensure(lget(c,0,0)==1);return 1;}
 let d=build_desc(list_item(lget(c,3,0),lget(c,2,0)));
 let ignored=lcell(10,lget(c,1,0),d,0);
 hfree(d);
 return 1;
}
fn ctx_status(c:i64)->i64 {return lget(c,0,0);}
fn ctx_ubatch(c:i64)->i64 {ensure(lget(c,0,0)==0);return list_item(lget(c,3,0),lget(c,2,0));}
fn ctx_is_full(c:i64)->i64 {return lget(c,5,0);}
fn ctx_i_next(c:i64)->i64 {return lget(c,2,0);}
fn ctx_count(c:i64)->i64 {return lget(c,4,0);}
fn ctx_n_rs(c:i64)->i64 {if lget(c,5,0)!=0 {return size_of(lget(c,1,0));}return n_of(lget(c,1,0));}
fn ctx_head(c:i64)->i64 {if lget(c,5,0)!=0 {return 0;}return head_of(lget(c,1,0));}
fn ctx_rs_z(c:i64)->i64 {if lget(c,5,0)!=0 {return 0;}return rs_z_of(lget(c,1,0));}
fn ctx_size(c:i64)->i64 {return size_of(lget(c,1,0));}
fn ctx_r_l(c:i64,il:i64)->i64 {return lget(lget(c,1,0),70,il);}
fn ctx_s_l(c:i64,il:i64)->i64 {return lget(lget(c,1,0),71,il);}
fn ctx_p_l(c:i64,il:i64)->i64 {return lget(lget(c,1,0),72,il);}
fn destroy_ctx(c:i64) {
 if c!=0 {
  if lget(c,3,0)!=0 {
   let l=lget(c,3,0);var i:i64=0;let n=list_count(l);
   while i<n {lsvc(40,list_item(l,i),0,0);i=add(i,1);}
   hfree(l);
  }
  hfree(c);
 }
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 lwit(mode);
 if mode==0 {return status_is_fail(a);}
 if mode==1 {return create_owned(a,b);}
 if mode==2 {destroy_owned(a);return 0;}
 if mode==3 {return init_batch(a,b,c,d);}
 if mode==4 {return init_full(a);}
 if mode==5 {return init_update(a);}
 if mode==6 {return ctx_next(a);}
 if mode==7 {return ctx_apply(a);}
 if mode==8 {return ctx_status(a);}
 if mode==9 {return ctx_ubatch(a);}
 if mode==10 {return ctx_n_rs(a);}
 if mode==11 {return ctx_head(a);}
 if mode==12 {return ctx_rs_z(a);}
 if mode==13 {return ctx_size(a);}
 if mode==14 {return ctx_r_l(a,b);}
 if mode==15 {return ctx_s_l(a,b);}
 if mode==16 {return ctx_p_l(a,b);}
 if mode==17 {return ctx_is_full(a);}
 if mode==18 {return ctx_i_next(a);}
 if mode==19 {return is_empty(a);}
 if mode==20 {return total_size(a);}
 if mode==21 {return size_r_bytes(a);}
 if mode==22 {return size_s_bytes(a);}
 if mode==23 {return size_p_bytes(a);}
 if mode==24 {return breakdown(a);}
 if mode==25 {return list_count(a);}
 if mode==26 {return lget(a,10,b);}
 if mode==27 {return lget(a,12,b);}
 if mode==28 {return ctx_count(a);}
 if mode==29 {destroy_ctx(a);return 0;}
 if mode==30 {return list_item(a,b);}
 if mode==31 {return status_combine(a,b);}
 // context built from an existing ubatch list (direct concrete ctor path)
 if mode==32 {
  let c=ctx_make(0,a,0);if c==0 {return 0;}
  lput(c,3,0,b);lput(c,4,0,list_count(b));
  return c;
 }
 // status-only context (NO_UPDATE / FAILED_*); mem may be uninitialised
 if mode==33 {return ctx_make(a,b,0);}
 fail(3);return 0;
}
fn main(){print(entry(0,0,0,0,0,0));}
