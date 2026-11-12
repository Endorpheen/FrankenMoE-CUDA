// Apex recurrent-state removal in Goose:
// llama_memory_recurrent::{set_rs_idx, seq_rm, clear, state_clear, seq_pos_min, seq_pos_max}.
//
// seq_rm is the interesting one. A recurrent state cannot be partially erased at the end of a
// sequence, so the native code has three behaviours: a full-range removal that also resets the
// sequence's snapshot index, a partial rollback that is served from a per-token snapshot (single
// use, bounded by n_rs_seq), and a plain removal over a position range. A cell shared by several
// sequences cannot roll back for only one of them.
//
// Arena layout, int64 slots:
//   pos[n] | src[n] | src0[n] | tail[n] | seqw[n*SW] | head | used | n | rs_idx[m]
// where m = n_seq_max.
//
// Packet, int64 slots:
//   k=0  0 op, 1 arena, 2 n, 3 SW, 4 n_seq_max, 5 n_rs_seq, 6 mem_empty
//   k=1  0 seq_id, 1 p0, 2 p1, 3 cell_head, 4 cell_count
//   k=3  0 done, 1 ret
extern "fg295k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg295k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg295k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg295k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg295k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn posmax()->i64{return 2147483647 as i64;}

fn pos_off(i:i64)->i64{return i*8 as i64;}
fn src_off(n:i64,i:i64)->i64{return (n+i)*8 as i64;}
fn src0_off(n:i64,i:i64)->i64{return (2*n+i)*8 as i64;}
fn tail_off(n:i64,i:i64)->i64{return (3*n+i)*8 as i64;}
fn seqw_off(n:i64,i:i64,sw:i64,w:i64)->i64{return (4*n+i*sw+w)*8 as i64;}
fn head_off(n:i64,sw:i64)->i64{return (4*n+n*sw)*8 as i64;}
fn used_off(n:i64,sw:i64)->i64{return (4*n+n*sw+1 as i64)*8 as i64;}
fn rs_off(n:i64,sw:i64,i:i64)->i64{return (4*n+n*sw+3 as i64+i)*8 as i64;}

fn bit_of(s:i64)->i64{
 var b:i64=1 as i64;
 var k:i64=0;
 while k<s%64 as i64{
  b=b*2 as i64;
  k=k+1 as i64;
 }
 b
}

fn seq_test(a:u64,n:i64,i:i64,sw:i64,s:i64)->bool{
 return (ld(a,seqw_off(n,i,sw,s/64 as i64))&bit_of(s))!=(0 as i64);
}

fn seq_del(a:u64,n:i64,i:i64,sw:i64,s:i64){
 let wi:i64=s/64 as i64;
 let cur:i64=ld(a,seqw_off(n,i,sw,wi));
 st(a,seqw_off(n,i,sw,wi),cur-(cur&bit_of(s)));
}

fn seq_clear(a:u64,n:i64,i:i64,sw:i64){
 var w:i64=0;
 while w<sw{ st(a,seqw_off(n,i,sw,w),0 as i64); w=w+1 as i64; }
}

fn seq_size(a:u64,n:i64,i:i64,sw:i64)->i64{
 var c:i64=0;
 var s:i64=0;
 while s<sw*64 as i64{
  if seq_test(a,n,i,sw,s){ c=c+1 as i64; }
  s=s+1 as i64;
 }
 c
}

fn get_rs(a:u64,n:i64,sw:i64,i:i64)->i64{return ld(a,rs_off(n,sw,i));}
fn set_rs(a:u64,n:i64,sw:i64,i:i64,v:i64){st(a,rs_off(n,sw,i),v);}

// set_rs_idx: a negative sequence resets the whole table.
fn set_rs_idx(a:u64,n:i64,sw:i64,n_seq_max:i64,n_rs_seq:i64,seq_id:i64,idx:i64){
 if seq_id<0 as i64{
  var i:i64=0;
  while i<n_seq_max{ set_rs(a,n,sw,i,0 as i64); i=i+1 as i64; }
  return;
 }
 ensure(seq_id<n_seq_max,60);
 ensure(idx<=n_rs_seq,61);
 set_rs(a,n,sw,seq_id,idx);
}

fn clear_cells(a:u64,n:i64,sw:i64,n_seq_max:i64){
 var i:i64=0;
 while i<n{
  st(a,pos_off(i),(0 as i64)-1 as i64);
  seq_clear(a,n,i,sw);
  st(a,src_off(n,i),(0 as i64)-1 as i64);
  st(a,tail_off(n,i),(0 as i64)-1 as i64);
  i=i+1 as i64;
 }
 st(a,head_off(n,sw),0 as i64);
 st(a,used_off(n,sw),0 as i64);
 i=0;
 while i<n_seq_max{ set_rs(a,n,sw,i,0 as i64); i=i+1 as i64; }
}

fn seq_rm(a:u64,n:i64,sw:i64,n_seq_max:i64,n_rs_seq:i64,mem_empty:bool,seq_id:i64,p0in:i64,p1in:i64)->i64{
 var p0:i64=p0in;
 var p1:i64=p1in;
 if p0<0 as i64{ p0=0 as i64; }
 if p1<0 as i64{ p1=posmax(); }
 // Native compares against n_seq_max as unsigned, so a negative id is rejected here too.
 if seq_id<0 as i64{ return 0 as i64; }
 if seq_id>=n_seq_max{ return 0 as i64; }
 let rm_all:bool=p0==(0 as i64)&&p1==posmax();
 if rm_all{ set_rs_idx(a,n,sw,n_seq_max,n_rs_seq,seq_id,0 as i64); }
 if seq_id>=n as i64{ return 0 as i64; }
 var new_head:i64=n;
 if seq_id>=0 as i64{
  var tail_id:i64=ld(a,tail_off(n,seq_id));
  if tail_id>=0 as i64{
   let cpos:i64=ld(a,pos_off(tail_id));
   if p0>(0 as i64)&&p0<=cpos&&p1>cpos{
    if seq_size(a,n,tail_id,sw)>(1 as i64){ return 0 as i64; }
    if mem_empty{
     st(a,pos_off(tail_id),p0-(1 as i64));
     return 1 as i64;
    }
    let rollback:i64=cpos-(p0-(1 as i64));
    var pending:bool=false;
    if get_rs(a,n,sw,seq_id)!=(0 as i64){ pending=true; }
    if !pending{
     if rollback>=(1 as i64)&&rollback<=n_rs_seq{
      set_rs_idx(a,n,sw,n_seq_max,n_rs_seq,seq_id,rollback);
      st(a,pos_off(tail_id),p0-(1 as i64));
      return 1 as i64;
     }
    }
    return 0 as i64;
   }
   if p0<=cpos&&cpos<p1{
    st(a,tail_off(n,seq_id),(0 as i64)-1 as i64);
   }
  }
 }else{
  if p0!=p1{
   if p0!=(0 as i64)||p1!=posmax(){ return 0 as i64; }
  }
 }
 var i:i64=0;
 while i<n{
  let q:i64=ld(a,pos_off(i));
  if q>=p0&&q<p1{
   var touched:bool=false;
   if seq_id<0 as i64{
    seq_clear(a,n,i,sw);
    touched=true;
   }else{
    if seq_test(a,n,i,sw,seq_id){
     seq_del(a,n,i,sw,seq_id);
     touched=true;
    }
   }
   if touched{
    if seq_size(a,n,i,sw)==(0 as i64){
     if ld(a,pos_off(i))>=0 as i64{
      st(a,used_off(n,sw),ld(a,used_off(n,sw))-(1 as i64));
     }
     st(a,pos_off(i),(0 as i64)-1 as i64);
     st(a,src_off(n,i),(0 as i64)-1 as i64);
     if new_head==n{ new_head=i; }
    }
   }
  }
  i=i+1 as i64;
 }
 if new_head!=n{
  if new_head<ld(a,head_off(n,sw)){ st(a,head_off(n,sw),new_head); }
 }
 return 1 as i64;
}

fn seq_pos_min(a:u64,n:i64,sw:i64,seq_id:i64)->i64{
 var r:i64=posmax();
 var i:i64=0;
 while i<n{
  if seq_test(a,n,i,sw,seq_id){
   let p:i64=ld(a,pos_off(i));
   if p<r{ r=p; }
  }
  i=i+1 as i64;
 }
 return r;
}

fn seq_pos_max(a:u64,n:i64,sw:i64,seq_id:i64)->i64{
 var r:i64=(0 as i64)-1 as i64;
 var i:i64=0;
 while i<n{
  if seq_test(a,n,i,sw,seq_id){
   let p:i64=ld(a,pos_off(i));
   if p>r{ r=p; }
  }
  i=i+1 as i64;
 }
 return r;
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let a:u64=get(packet,0,1);
 let n:i64=get(packet,0,2) as i64;
 let sw:i64=get(packet,0,3) as i64;
 let n_seq_max:i64=get(packet,0,4) as i64;
 let n_rs_seq:i64=get(packet,0,5) as i64;
 let mem_empty:bool=get(packet,0,6)!=(0 as u64);
 let sid:i64=get(packet,1,0) as i64;
 let p0:i64=get(packet,1,1) as i64;
 let p1:i64=get(packet,1,2) as i64;
 let cell_head:i64=get(packet,1,3) as i64;
 let cell_count:i64=get(packet,1,4) as i64;
 ensure(n>0 as i64&&sw>(0 as i64)&&n_seq_max>(0 as i64),70);
 var ret:i64=0;
 if op==(0 as i64){
  ret=seq_rm(a,n,sw,n_seq_max,n_rs_seq,mem_empty,sid,p0,p1);
 }else{
  if op==(1 as i64){
   clear_cells(a,n,sw,n_seq_max);
   ret=1 as i64;
  }else{
   if op==(2 as i64){
    set_rs_idx(a,n,sw,n_seq_max,n_rs_seq,sid,p0);
    ret=1 as i64;
   }else{
    if op==(3 as i64){
     ret=seq_pos_min(a,n,sw,sid);
    }else{
     if op==(4 as i64){
      ret=seq_pos_max(a,n,sw,sid);
     }else{
      // state_clear(seq_id, cell_head, cell_count)
      if sid==(0 as i64)-1 as i64{
       clear_cells(a,n,sw,n_seq_max);
      }else{
       ret=seq_rm(a,n,sw,n_seq_max,n_rs_seq,mem_empty,sid,(0 as i64)-1 as i64,(0 as i64)-1 as i64);
      }
      put(packet,3,2,cell_head as u64);
      put(packet,3,3,cell_count as u64);
      if cell_count==(0 as i64){ ret=1 as i64; }
     }
    }
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
