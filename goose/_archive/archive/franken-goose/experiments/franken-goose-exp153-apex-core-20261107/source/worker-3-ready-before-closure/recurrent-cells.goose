// recurrent-cells.goose - Goose proposal for recurrent cell/seq bookkeeping.
// Source: inputs/llama-memory-recurrent.h, inputs/llama-memory-recurrent.cpp.
// Scope: clear / seq_rm / seq_cp / seq_keep / seq_add / seq_div / seq_pos_min /
//        seq_pos_max / set_rs_idx / find_slot decisions / prepare snapshot-restore /
//        s_copy rollback-index consumption. Tensor data kernels, graph building,
//        state_write / state_read tensor IO, hybrid forwarding and full KV are OUT.
// Storage layout below is Goose-internal only (get/put key+index), not a C layout.
// The parent owns the opaque handles and must bind the documented externs.
extern "fg153_rg" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg153_rw" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_rs" fn service(op:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_rc" fn witness(mode:i64);
fn ensure(ok:bool) {if !ok {let ignored=service(0,0,0,0);assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0);ensure(a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0);ensure(a==0||b<=9223372036854775807/a);return a*b;}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a}else{b};}
fn maximum(a:i64,b:i64)->i64 {return if a>b {a}else{b};}
fn pos_max()->i64 {return 2147483647;}
fn size_of(mp:i64)->i64 {return get(mp,0,0);}
fn used_of(mp:i64)->i64 {return get(mp,1,0);}
fn head_of(mp:i64)->i64 {return get(mp,2,0);}
fn n_of(mp:i64)->i64 {return get(mp,3,0);}
fn n_seq_max_of(mp:i64)->i64 {return get(mp,4,0);}
fn n_rs_seq_of(mp:i64)->i64 {return get(mp,5,0);}
fn rs_z_of(mp:i64)->i64 {return get(mp,6,0);}
fn empty_of(mp:i64)->i64 {return get(mp,7,0);}
fn pos_of(mp:i64,i:i64)->i64 {return get(mp,10,i);}
fn set_pos(mp:i64,i:i64,v:i64) {put(mp,10,i,v);}
fn src_of(mp:i64,i:i64)->i64 {return get(mp,11,i);}
fn set_src(mp:i64,i:i64,v:i64) {put(mp,11,i,v);}
fn src0_of(mp:i64,i:i64)->i64 {return get(mp,12,i);}
fn set_src0(mp:i64,i:i64,v:i64) {put(mp,12,i,v);}
fn tail_of(mp:i64,i:i64)->i64 {return get(mp,13,i);}
fn set_tail(mp:i64,i:i64,v:i64) {put(mp,13,i,v);}
fn seqh_of(mp:i64,i:i64)->i64 {return get(mp,14,i);}
fn seq_has_op(mp:i64,i:i64,s:i64)->i64 {return service(1,seqh_of(mp,i),s,0);}
fn seq_add_op(mp:i64,i:i64,s:i64) {let ignored=service(2,seqh_of(mp,i),s,0);}
fn seq_rm_op(mp:i64,i:i64,s:i64) {let ignored=service(3,seqh_of(mp,i),s,0);}
fn seq_clear_op(mp:i64,i:i64) {let ignored=service(4,seqh_of(mp,i),0,0);}
fn seq_size_op(mp:i64,i:i64)->i64 {return service(5,seqh_of(mp,i),0,0);}
fn seq_clone_op(dm:i64,di:i64,sm:i64,si:i64) {let ignored=service(7,seqh_of(dm,di),seqh_of(sm,si),0);}
fn seq_first_op(mp:i64,i:i64)->i64 {return service(10,seqh_of(mp,i),0,0);}
fn set_rs_idx(mp:i64,s:i64,idx:i64) {
 if s<0 {for i in n_seq_max_of(mp) {put(mp,15,i,0);}}
 if s>=0 {ensure(s<n_seq_max_of(mp));ensure(idx>=0&&idx<=n_rs_seq_of(mp));put(mp,15,s,idx);}
}
fn create(mp:i64,size:i64,n_seq_max:i64,n_rs_seq:i64,empty:i64)->i64 {
 if size<=0||n_seq_max<=0||n_rs_seq<0 {return -1;}
 put(mp,0,0,size);put(mp,4,0,n_seq_max);put(mp,5,0,n_rs_seq);put(mp,7,0,empty);
 clear(mp,0);return 0;
}
fn clear(mp:i64,data:i64) {
 for i in size_of(mp) {put(mp,10,i,-1);seq_clear_op(mp,i);put(mp,11,i,-1);put(mp,13,i,-1);}
 put(mp,2,0,0);put(mp,1,0,0);
 if data!=0 {let ignored=service(6,mp,0,0);}
 for i in n_seq_max_of(mp) {put(mp,15,i,0);}
}
fn seq_rm(mp:i64,seq:i64,p0_in:i64,p1_in:i64)->i64 {
 var p0=p0_in;var p1=p1_in;var new_head=size_of(mp);
 if p0<0 {p0=0;}
 if p1<0 {p1=pos_max();}
 if seq<0||seq>=n_seq_max_of(mp) {let ignored=service(9,seq,0,0);return 0;}
 if p0==0&&p1==pos_max() {set_rs_idx(mp,seq,0);}
 if seq>=size_of(mp) {return 0;}
 if 0<=seq {
  let tid=tail_of(mp,seq);
  if tid>=0 {
   if 0<p0&&p0<=pos_of(mp,tid)&&p1>pos_of(mp,tid) {
    if seq_size_op(mp,tid)>1 {return 0;}
    if empty_of(mp)!=0 {set_pos(mp,tid,p0-1);return 1;}
    let rollback=pos_of(mp,tid)-(p0-1);var pending=false;if get(mp,15,seq)!=0 {pending=true;}
    if !pending&&rollback>=1&&rollback<=n_rs_seq_of(mp) {set_rs_idx(mp,seq,rollback);set_pos(mp,tid,p0-1);return 1;}
    return 0;
   }
   if p0<=pos_of(mp,tid)&&pos_of(mp,tid)<p1 {set_tail(mp,seq,-1);}
  }
 }
 if seq<0 {
  if p0!=p1&&!(p0==0&&p1==pos_max()) {return 0;}
 }
 for i in size_of(mp) {
  if pos_of(mp,i)>=p0&&pos_of(mp,i)<p1 {
   if seq<0 {seq_clear_op(mp,i);}else {if seq_has_op(mp,i,seq)==0 {continue;}seq_rm_op(mp,i,seq);}
   if seq_size_op(mp,i)==0 {
    if pos_of(mp,i)>=0 {put(mp,1,0,used_of(mp)-1);}
    set_pos(mp,i,-1);set_src(mp,i,-1);
    if new_head==size_of(mp) {new_head=i;}
   }
  }
 }
 if new_head!=size_of(mp)&&new_head<head_of(mp) {put(mp,2,0,new_head);}
 return 1;
}
fn seq_cp(mp:i64,src:i64,dst:i64,p0_in:i64,p1_in:i64) {
 if src==dst {return;}
 var p0=p0_in;var p1=p1_in;
 if p0<0 {p0=0;}
 if p1<0 {p1=pos_max();}
 if dst<0||dst>=size_of(mp)||src<0||src>=size_of(mp) {return;}
 let td=tail_of(mp,dst);
 if td>=0 {
  seq_rm_op(mp,td,dst);set_tail(mp,dst,-1);
  if seq_size_op(mp,td)==0 {set_pos(mp,td,-1);set_src(mp,td,-1);put(mp,1,0,used_of(mp)-1);}
 }
 let ts=tail_of(mp,src);
 if ts>=0 {seq_add_op(mp,ts,dst);set_tail(mp,dst,ts);}
}
fn seq_keep(mp:i64,seq:i64) {
 var new_head=size_of(mp);
 for i in size_of(mp) {
  if i!=seq {set_tail(mp,i,-1);}
  if seq_has_op(mp,i,seq)==0 {
   if pos_of(mp,i)>=0 {put(mp,1,0,used_of(mp)-1);}
   set_pos(mp,i,-1);set_src(mp,i,-1);seq_clear_op(mp,i);
   if new_head==size_of(mp) {new_head=i;}
  }else {seq_clear_op(mp,i);seq_add_op(mp,i,seq);}
 }
 if new_head!=size_of(mp)&&new_head<head_of(mp) {put(mp,2,0,new_head);}
}
fn seq_add(mp:i64,seq:i64,p0_in:i64,p1_in:i64,shift:i64) {
 if shift==0 {return;}
 var p0=p0_in;var p1=p1_in;
 if p0<0 {p0=0;}
 if p1<0 {p1=pos_max();}
 if p0==p1 {return;}
 if 0<=seq&&seq<size_of(mp) {
  let tid=tail_of(mp,seq);
  if tid>=0 {if seq_has_op(mp,tid,seq)!=0&&p0<=pos_of(mp,tid)&&pos_of(mp,tid)<p1 {set_pos(mp,tid,add(pos_of(mp,tid),shift));}}
 }
}
fn seq_div(mp:i64,seq:i64,p0_in:i64,p1_in:i64,d:i64) {
 if d==1 {return;}
 ensure(d!=0);
 var p0=p0_in;var p1=p1_in;
 if p0<0 {p0=0;}
 if p1<0 {p1=pos_max();}
 if p0==p1 {return;}
 if 0<=seq&&seq<size_of(mp) {
  let tid=tail_of(mp,seq);
  if tid>=0 {if seq_has_op(mp,tid,seq)!=0&&p0<=pos_of(mp,tid)&&pos_of(mp,tid)<p1 {set_pos(mp,tid,pos_of(mp,tid)/d);}}
 }
}
fn seq_pos_min(mp:i64,seq:i64)->i64 {
 var result=pos_max();
 for i in size_of(mp) {if seq_has_op(mp,i,seq)!=0 {result=minimum(result,pos_of(mp,i));}}
 if result==pos_max() {return -1;}
 return result;
}
fn seq_pos_max(mp:i64,seq:i64)->i64 {
 var result:i64=-1;
 for i in size_of(mp) {if seq_has_op(mp,i,seq)!=0 {result=maximum(result,pos_of(mp,i));}}
 return result;
}
fn find_slot(mp:i64,desc:i64)->i64 {
 let n_seqs=get(desc,20,0);let n_tok=get(desc,21,0);let size=size_of(mp);
 ensure(n_seqs>0&&n_tok>=0);
 if head_of(mp)>used_of(mp)+add(1,1)*n_seqs {put(mp,2,0,0);}
 for s in n_seqs {
  let seq=get(desc,22,s);
  if seq<0||seq>=size {let ignored=service(9,seq,0,0);return 0;}
 }
 var min=size-1;var max:i64=0;
 var next_empty=head_of(mp);var scanned:i64=0;
 while scanned<size {
  if next_empty>=size {next_empty=next_empty-size;}
  if seq_size_op(mp,next_empty)==0 {break;}
  next_empty=add(next_empty,1);scanned=add(scanned,1);
 }
 for s in n_seqs {
  let seq=get(desc,22,s);var has_cell=false;let tail=tail_of(mp,seq);
  if tail>=0 {if seq_size_op(mp,tail)==1 {has_cell=true;}}
  if !has_cell {
   ensure(seq_size_op(mp,next_empty)==0);
   if tail>=0 {
    set_pos(mp,next_empty,pos_of(mp,tail));set_src(mp,next_empty,src_of(mp,tail));
    seq_rm_op(mp,tail,seq);seq_add_op(mp,next_empty,seq);
    ensure(seq_size_op(mp,tail)!=0);
   }
   set_tail(mp,seq,next_empty);
   if s+1<n_seqs {
    var adv:i64=0;
    var cursor=add(next_empty,1);
    while adv<size {
     if cursor>=size {cursor=cursor-size;}
     if seq_size_op(mp,cursor)==0 {break;}
     cursor=add(cursor,1);adv=add(adv,1);
    }
    next_empty=cursor;
   }
  }
  let cur=tail_of(mp,seq);min=minimum(min,cur);max=maximum(max,cur);
 }
 for s in n_seqs {
  let seq=get(desc,22,s);let dst=add(s,min);let src=tail_of(mp,seq);
  if dst!=src {
   let tp=pos_of(mp,dst);set_pos(mp,dst,pos_of(mp,src));set_pos(mp,src,tp);
   let ts=src_of(mp,dst);set_src(mp,dst,src_of(mp,src));set_src(mp,src,ts);
   let th=seqh_of(mp,dst);put(mp,14,dst,seqh_of(mp,src));put(mp,14,src,th);
   for j in size {
    let t=tail_of(mp,j);
    if t==src {set_tail(mp,j,dst);}else {if t==dst {set_tail(mp,j,src);}}
   }
  }
 }
 for s in n_seqs {
  let seq=get(desc,22,s);let last=get(desc,23,s);let cell=add(s,min);
  if pos_of(mp,cell)>=0&&last!=add(pos_of(mp,cell),n_tok) {let ignored=service(8,seq,last,pos_of(mp,cell));}
  set_pos(mp,cell,last);seq_clear_op(mp,cell);seq_add_op(mp,cell,seq);set_tail(mp,seq,cell);
 }
 var rs_z:i64=-1;var cand=min;
 while cand<=max {
  var ref:i64=0;
  for j in size {if src_of(mp,j)==cand {ref=add(ref,1);}}
  if ref==0 {rs_z=cand;break;}
  cand=add(cand,1);
 }
 put(mp,6,0,rs_z);cand=min;
 while cand<=max {
  if src_of(mp,cand)<0 {ensure(rs_z>=0);set_src0(mp,cand,rs_z);}else {set_src0(mp,cand,src_of(mp,cand));}
  set_src(mp,cand,cand);cand=add(cand,1);
 }
 put(mp,2,0,min);put(mp,3,0,max-min+1);
 var used:i64=0;
 for i in size {if seq_size_op(mp,i)!=0 {used=add(used,1);}}
 put(mp,1,0,used);
 return if max-min+1>=n_seqs {1}else{0};
}
fn prepare(mp:i64,batch:i64)->i64 {
 let cnt=get(batch,24,0);ensure(cnt>=0);
 for i in size_of(mp) {
  put(batch,30,i,pos_of(mp,i));put(batch,31,i,src_of(mp,i));put(batch,32,i,src0_of(mp,i));put(batch,33,i,tail_of(mp,i));
  seq_clone_op(batch,i,mp,i);
 }
 put(batch,34,0,used_of(mp));put(batch,35,0,head_of(mp));
 var ok=true;
 for k in cnt {if find_slot(mp,get(batch,25,k))==0 {ok=false;break;}}
 for i in size_of(mp) {
  set_pos(mp,i,get(batch,30,i));set_src(mp,i,get(batch,31,i));set_src0(mp,i,get(batch,32,i));set_tail(mp,i,get(batch,33,i));
  seq_clone_op(mp,i,batch,i);
 }
 put(mp,1,0,get(batch,34,0));put(mp,2,0,get(batch,35,0));
 return if ok {1}else{0};
}
fn s_copy(mp:i64,i:i64)->i64 {
 let cell=add(i,head_of(mp));let src0=src0_of(mp,cell);
 if n_rs_seq_of(mp)==0 {return src0;}
 var idx:i64=0;
 if seq_size_op(mp,cell)!=0 {
  let seq=seq_first_op(mp,cell);
  if seq>=0&&seq<n_seq_max_of(mp) {idx=get(mp,15,seq);put(mp,15,seq,0);}
 }
 return add(mul(idx,size_of(mp)),src0);
}
fn pos_get(mp:i64,i:i64)->i64 {return pos_of(mp,i);}
fn src_get(mp:i64,i:i64)->i64 {return src_of(mp,i);}
fn src0_get(mp:i64,i:i64)->i64 {return src0_of(mp,i);}
fn tail_get(mp:i64,i:i64)->i64 {return tail_of(mp,i);}
fn can_shift()->i64 {return 1;}
fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64)->i64 {
 witness(mode);
 if mode==0 {return create(a,b,c,d,e);}
 if mode==1 {clear(a,b);return 0;}
 if mode==2 {return seq_rm(a,b,c,d);}
 if mode==3 {seq_cp(a,b,c,d,e);return 0;}
 if mode==4 {seq_keep(a,b);return 0;}
 if mode==5 {seq_add(a,b,c,d,e);return 0;}
 if mode==6 {seq_div(a,b,c,d,e);return 0;}
 if mode==7 {return seq_pos_min(a,b);}
 if mode==8 {return seq_pos_max(a,b);}
 if mode==9 {set_rs_idx(a,b,c);return 0;}
 if mode==10 {return find_slot(a,b);}
 if mode==11 {return prepare(a,b);}
 if mode==12 {return s_copy(a,b);}
 if mode==13 {return can_shift();}
 if mode==14 {return pos_get(a,b);}
 if mode==15 {return src_get(a,b);}
 if mode==16 {return src0_get(a,b);}
 if mode==17 {return tail_get(a,b);}
 if mode==18 {return seq_has_op(a,b,c);}
 if mode==19 {return size_of(a);}
 if mode==20 {return used_of(a);}
 if mode==21 {return head_of(a);}
 if mode==22 {return n_of(a);}
 if mode==23 {return rs_z_of(a);}
 if mode==24 {return empty_of(a);}
 if mode==25 {return seq_size_op(a,b);}
 ensure(false);return 0;
}
fn main(){print(entry(13,0,0,0,0,0));}
