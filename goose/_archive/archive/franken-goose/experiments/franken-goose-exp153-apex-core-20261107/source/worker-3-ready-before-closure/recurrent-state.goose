// recurrent-state.goose - Goose proposal for recurrent state_write/read/clear control.
// Source: inputs/llama-memory-recurrent.cpp:788-1286.
//
// Ported control logic (ordered, Goose-owned): cell range selection, rs_idx
// plane selection with shared-seq mismatch guard, ON_DEVICE single-range guard,
// count cross-checks, state_read meta/data validation order, failure rollback via
// state_clear, and the set_rs_idx(seq,0) reset.
// Raw primitives (services.cpp): io write/read, tensor write/read, clear_tensor,
// hparams, tensor handles, generic seq-set container ops. No state algorithm in services.
// IO byte transfer and tensor-row bytes remain raw primitives by design.
// NOTE (flag for Codex): the single-sequence restore builds a find_slot
// descriptor (n_seqs=1, n_seq_tokens=cell_count, last_pos=pos[last]) instead of
// balloc.ubatch_reserve(); control-equivalent but not source-identical.
//
// Cells seam fg153_sc maps to recurrent-cells entry modes:
//   1 clear(mp,data) | 2 seq_rm(mp,seq,-1,-1) | 9 set_rs_idx(mp,seq,idx) |
//   10 find_slot(mp,desc) | 18 seq_has(mp,i,seq) | 19 seq_size(mp,i)
extern "fg153_sg" fn sg(p:i64,k:i64,i:i64)->i64;
extern "fg153_sp" fn sp(p:i64,k:i64,i:i64,v:i64);
extern "fg153_ss" fn ss(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_sc" fn sc(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_sw" fn sw(mode:i64);
fn ensure(ok:bool) {if !ok {let ignored=ss(0,0,0,0);assert(false);}}
fn fail(reason:i64) {let ignored=ss(19,reason,0,0);assert(false);}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn bx()->i64 {return ss(24,64,0,0);}
fn bxfree(b:i64) {let ignored=ss(25,b,0,0);}
fn rng_count(r:i64)->i64 {return sg(r,30,0);}
fn rng_first(r:i64,i:i64)->i64 {return sg(r,31,i);}
fn rng_second(r:i64,i:i64)->i64 {return sg(r,32,i);}
fn rng_add(r:i64,a:i64,b:i64) {
 let n=rng_count(r);
 if n>0 {if rng_second(r,n-1)==a {sp(r,32,n-1,b);return;}}
 sp(r,31,n,a);sp(r,32,n,b);sp(r,30,0,add(n,1));
}
fn state_write_meta(mp:i64,io:i64,rs:i64,seq:i64) {
 var r:i64=0;
 while r<rng_count(rs) {
  var i=rng_first(rs,r);
  let end=rng_second(rs,r);
  while i<end {
   let pos=sg(mp,10,i);
   let cnt=if seq==-1 {ss(26,sg(mp,14,i),0,0)}else{0};
   ss(3,io,pos,0);ss(1,io,cnt,0);
   if cnt!=0 {
    let seth=sg(mp,14,i);var k:i64=0;
    while k<cnt {ss(3,io,ss(27,seth,k,0),0);k=add(k,1);}
   }
   i=add(i,1);
  }
  r=add(r,1);
 }
}
fn write_ranges(io:i64,t:i64,rs:i64,row:i64) {
 var r:i64=0;
 while r<rng_count(rs) {
  let a=rng_first(rs,r);let n=rng_second(rs,r)-a;
  let cfg=bx();sp(cfg,0,0,mul(a,row));sp(cfg,1,0,mul(n,row));
  ss(4,io,t,cfg);bxfree(cfg);
  r=add(r,1);
 }
}
fn state_write_data(mp:i64,io:i64,rs:i64) {
 let n_layer=ss(10,mp,0,0);
 ss(1,io,0,0);ss(1,io,n_layer,0); // s_trans=0, n_layer
 var il:i64=0;
 while il<n_layer {
  let r=ss(15,mp,0,il);
  if r!=0 {
   ss(3,io,ss(17,r,0,0),0);
   let row=ss(16,r,ss(11,mp,0,0),0);
   ss(2,io,row,0);
   write_ranges(io,r,rs,row);
   let p=ss(15,mp,2,il);
   if p!=0 {let prow=ss(16,p,ss(13,mp,0,0),0);ss(2,io,prow,0);write_ranges(io,p,rs,prow);}
  }
  il=add(il,1);
 }
 il=0;
 while il<n_layer {
  let s=ss(15,mp,1,il);
  if s!=0 {ss(3,io,ss(17,s,0,0),0);let row=ss(16,s,ss(12,mp,0,0),0);ss(2,io,row,0);write_ranges(io,s,rs,row);}
  il=add(il,1);
 }
}
fn state_write(mp:i64,io:i64,seq:i64,flags:i64) {
 let size=sg(mp,0,0);let n_rs=sg(mp,5,0);
 let ranges=bx();let data=bx();
 var cell_count:i64=0;var begin=size;var i:i64=0;
 while i<size {
  let selected=if seq==-1 {if sc(19,mp,i,0)!=0 {1}else{0}} else {sc(18,mp,i,seq)};
  if selected!=0 {
   cell_count=add(cell_count,1);var cur:i64=0;
   if n_rs!=0 {
    if seq!=-1 {cur=sg(mp,15,seq);}
    else {
     let seth=sg(mp,14,i);let cnt=ss(26,seth,0,0);var k:i64=0;var first:i64=1;
     while k<cnt {
      let s=ss(27,seth,k,0);
      if first!=0 {cur=sg(mp,15,s);first=0;}else {if sg(mp,15,s)!=cur {fail(7);}}
      k=add(k,1);
     }
    }
   }
   let src=sg(mp,11,i);let cell=add(mul(cur,size),if src>=0 {src}else{i});
   rng_add(data,cell,add(cell,1));
   if begin==size {begin=i;}
  }else {if begin!=size {rng_add(ranges,begin,i);begin=size;}}
  i=add(i,1);
 }
 if begin!=size {rng_add(ranges,begin,size);}
 if ss(22,flags,0,0)!=0 {if rng_count(ranges)>1 {fail(8);}}
 var chk:i64=0;var r:i64=0;
 while r<rng_count(ranges) {chk=add(chk,rng_second(ranges,r)-rng_first(ranges,r));r=add(r,1);}
 ensure(chk==cell_count);
 chk=0;r=0;
 while r<rng_count(data) {chk=add(chk,rng_second(data,r)-rng_first(data,r));r=add(r,1);}
 ensure(chk==cell_count);
 ss(1,io,cell_count,0);
 state_write_meta(mp,io,ranges,seq);
 state_write_data(mp,io,data);
 bxfree(ranges);bxfree(data);
}
fn state_read_meta(mp:i64,io:i64,cell_count:i64,dest:i64)->i64 {
 let size=sg(mp,0,0);
 if dest!=-1 {
  if cell_count>size {ss(20,1,cell_count,size);return 0;}
  let ignored=sc(2,mp,dest,0);
  if cell_count==0 {return 1;}
  let tmp=bx();let desc=bx();
  sp(desc,20,0,1);sp(desc,21,0,cell_count);sp(desc,22,0,dest);
  var i:i64=0;
  while i<cell_count {
   let pos=ss(7,io,0,0);let nsid=ss(5,io,0,0);
   if nsid!=0 {ss(20,2,0,0);bxfree(tmp);bxfree(desc);return 0;}
   sp(tmp,40,i,pos);i=add(i,1);
  }
  sp(desc,23,0,sg(tmp,40,cell_count-1));
  if sc(10,mp,desc,0)==0 {ss(20,3,0,0);bxfree(tmp);bxfree(desc);return 0;}
  let head=sg(mp,2,0);
  ensure(add(head,cell_count)<=size);
  ensure(sg(mp,10,head)==sg(tmp,40,0));
  ensure(sg(mp,10,add(head,cell_count-1))==sg(tmp,40,cell_count-1));
  ensure(sc(18,mp,head,dest)!=0);
  ensure(sc(18,mp,add(head,cell_count-1),dest)!=0);
  bxfree(tmp);bxfree(desc);
 }else {
  if cell_count>size {ss(20,4,cell_count,size);return 0;}
  let ignored=sc(1,mp,1,0);
  let n_seq_max=sg(mp,4,0);
  var i:i64=0;
  while i<cell_count {
   let pos=ss(7,io,0,0);let nsid=ss(5,io,0,0);
   sp(mp,10,i,pos);
   var j:i64=0;
   while j<nsid {
    let sid=ss(7,io,0,0);
    if sid<0||sid>=n_seq_max {ss(20,5,sid,n_seq_max);return 0;}
    let seth=sg(mp,14,i);let ignored2=ss(28,seth,sid,0);
    let tail=sg(mp,13,sid);
    if tail!=-1 {ss(20,6,sid,tail);return 0;}
    sp(mp,13,sid,i);
    j=add(j,1);
   }
   i=add(i,1);
  }
  sp(mp,2,0,0);sp(mp,1,0,cell_count);
 }
 var k:i64=0;let head2=sg(mp,2,0);
 while k<cell_count {sp(mp,11,add(head2,k),add(head2,k));k=add(k,1);}
 return 1;
}
fn state_read_data(mp:i64,io:i64,cell_count:i64)->i64 {
 let s_trans=ss(5,io,0,0);let n_layer=ss(5,io,0,0);
 let expect=ss(10,mp,0,0);
 if n_layer!=expect {ss(20,10,n_layer,expect);return 0;}
 let size=sg(mp,0,0);
 if cell_count>size {ss(20,11,cell_count,size);return 0;}
 if s_trans!=0 {ss(20,12,0,0);return 0;}
 let head=sg(mp,2,0);
 var il:i64=0;
 while il<n_layer {
  let r=ss(15,mp,0,il);
  if r!=0 {
   let tref=ss(7,io,0,0);let t=ss(17,r,0,0);
   if t!=tref {ss(20,13,t,tref);return 0;}
   let rref=ss(6,io,0,0);let row=ss(16,r,ss(11,mp,0,0),0);
   if row!=rref {ss(20,14,row,rref);return 0;}
   if cell_count!=0 {let cfg=bx();sp(cfg,0,0,mul(head,row));sp(cfg,1,0,mul(cell_count,row));ss(8,io,r,cfg);bxfree(cfg);}
   let p=ss(15,mp,2,il);
   if p!=0 {
    let pref=ss(6,io,0,0);let prow=ss(16,p,ss(13,mp,0,0),0);
    if prow!=pref {ss(20,15,prow,pref);return 0;}
    if cell_count!=0 {let cfg=bx();sp(cfg,0,0,mul(head,prow));sp(cfg,1,0,mul(cell_count,prow));ss(8,io,p,cfg);bxfree(cfg);}
   }
  }
  il=add(il,1);
 }
 il=0;
 while il<n_layer {
  let s=ss(15,mp,1,il);
  if s!=0 {
   let tref=ss(7,io,0,0);let t=ss(17,s,0,0);
   if t!=tref {ss(20,16,t,tref);return 0;}
   let rref=ss(6,io,0,0);let row=ss(16,s,ss(12,mp,0,0),0);
   if row!=rref {ss(20,17,row,rref);return 0;}
   if cell_count!=0 {let cfg=bx();sp(cfg,0,0,mul(head,row));sp(cfg,1,0,mul(cell_count,row));ss(8,io,s,cfg);bxfree(cfg);}
  }
  il=add(il,1);
 }
 return 1;
}
fn state_clear(mp:i64,seq:i64,cell_head:i64,cell_count:i64) {
 if seq==-1 {let ignored=sc(1,mp,1,0);return;}
 let ignored=sc(2,mp,seq,0);
 if cell_count==0 {return;}
 let n_layer=ss(10,mp,0,0);var il:i64=0;
 while il<n_layer {
  let r=ss(15,mp,0,il);
  if r!=0 {
   let row=ss(16,r,ss(11,mp,0,0),0);let cfg=bx();
   sp(cfg,0,0,mul(cell_head,row));sp(cfg,1,0,mul(cell_count,row));ss(9,r,cfg,0);bxfree(cfg);
  }
  let s=ss(15,mp,1,il);
  if s!=0 {
   let row=ss(16,s,ss(12,mp,0,0),0);let cfg=bx();
   sp(cfg,0,0,mul(cell_head,row));sp(cfg,1,0,mul(cell_count,row));ss(9,s,cfg,0);bxfree(cfg);
  }
  let p=ss(15,mp,2,il);
  if p!=0 {
   let row=ss(16,p,ss(13,mp,0,0),0);let cfg=bx();
   sp(cfg,0,0,mul(cell_head,row));sp(cfg,1,0,mul(cell_count,row));ss(9,p,cfg,0);bxfree(cfg);
  }
  il=add(il,1);
 }
}
fn state_read(mp:i64,io:i64,seq:i64,flags:i64) {
 let cell_count=ss(5,io,0,0);
 let meta=state_read_meta(mp,io,cell_count,seq);
 let cell_head=sg(mp,2,0);
 var ok=meta;
 if ok!=0 {ok=state_read_data(mp,io,cell_count);}
 if ok==0 {
  state_clear(mp,seq,cell_head,if meta!=0 {cell_count}else{0});
  ss(19,9,0,0);
  return 0;
 }
 if sg(mp,5,0)!=0 {let ignored=sc(9,mp,seq,0);}
 return 1;
}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 sw(mode);
 if mode==0 {state_write(a,b,c,d);return 0;}
 if mode==1 {return state_read(a,b,c,d);}
 if mode==2 {state_clear(a,b,c,d);return 0;}
 if mode==9 {return 2147483647;}
 fail(3);return 0;
}
fn main(){print(entry(9,0,0,0,0,0));}
