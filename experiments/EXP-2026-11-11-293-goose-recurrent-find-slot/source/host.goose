// Apex recurrent-state slot placement in Goose (llama_memory_recurrent::find_slot).
//
// Recurrent caches give a whole sequence one cell, so a slot is always contiguous. The function
// clears shared-sequence tails, finds the next empty cell, gives each sequence a cell (copying the
// tail's state when the sequence was not the sole owner), re-orders the used range to start at
// `min`, records the final positions and sequence sets, and stages the source references for the
// copy pass.
//
// Containers stay native, as in the rest of P5: the cell array is an arena and every decision is
// Goose. A cell is empty exactly when its sequence set is empty (mem_cell::is_empty), not when its
// position is -1.
//
// Arena layout, int64 slots:
//   pos[n] | src[n] | src0[n] | tail[n] | seqw[n*SW] | head | used | n
// SW words of 64 bits hold the sequence ids.
//
// Packet, int64 slots:
//   k=0  0 arena, 1 n, 2 SW, 3 ub_pos, 4 ub_seq_id, 5 ub_n_seq_id, 6 ub_width,
//        7 n_seq_tokens, 8 n_seqs
//   k=3  0 done, 1 ret
extern "fg293k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg293k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg293k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg293k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg293k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn pos_off(i:i64)->i64{return i*8 as i64;}
fn src_off(n:i64,i:i64)->i64{return (n+i)*8 as i64;}
fn src0_off(n:i64,i:i64)->i64{return (2*n+i)*8 as i64;}
fn tail_off(n:i64,i:i64)->i64{return (3*n+i)*8 as i64;}
fn seqw_off(n:i64,i:i64,sw:i64,w:i64)->i64{return (4*n+i*sw+w)*8 as i64;}
fn head_off(n:i64,sw:i64)->i64{return (4*n+n*sw)*8 as i64;}
fn used_off(n:i64,sw:i64)->i64{return (4*n+n*sw+1 as i64)*8 as i64;}
fn nr_off(n:i64,sw:i64)->i64{return (4*n+n*sw+2 as i64)*8 as i64;}

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

fn seq_set(a:u64,n:i64,i:i64,sw:i64,s:i64){
 let wi:i64=s/64 as i64;
 st(a,seqw_off(n,i,sw,wi),ld(a,seqw_off(n,i,sw,wi))|bit_of(s));
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

// mem_cell::is_empty: the sequence set is empty.
fn is_empty(a:u64,n:i64,i:i64,sw:i64)->bool{
 return seq_size(a,n,i,sw)==(0 as i64);
}

fn run(packet:i64)->bool{
 let a:u64=get(packet,0,0);
 let n:i64=get(packet,0,1) as i64;
 let sw:i64=get(packet,0,2) as i64;
 let ub_pos:u64=get(packet,0,3);
 let ub_sid:u64=get(packet,0,4);
 let ub_nsid:u64=get(packet,0,5);
 let ub_w:i64=get(packet,0,6) as i64;
 let nst:i64=get(packet,0,7) as i64;
 let n_seqs:i64=get(packet,0,8) as i64;
 ensure(n>0 as i64&&sw>(0 as i64)&&ub_w>(0 as i64),60);
 ensure(nst>0 as i64&&n_seqs>0 as i64,61);


 var head:i64=ld(a,head_off(n,sw));
 var used:i64=ld(a,used_off(n,sw));

 if head>used+2 as i64*n_seqs{ head=0; }

 // shared sequences lose their second and later tail in the first phase
 var s:i64=0;
 while s<n_seqs{
  let i:i64=s*nst;
  let nsid:i64=ld(ub_nsid,i*(8 as i64));
  var j:i64=0;
  while j<nsid{
   let sid:i64=ld(ub_sid,(i*ub_w+j)*(8 as i64));
   if sid<0 as i64{ put(packet,3,0,1 as u64); put(packet,3,1,0 as u64); return true; }
   if sid>=n{ put(packet,3,0,1 as u64); put(packet,3,1,0 as u64); return true; }
   if j>0 as i64{
    let t:i64=ld(a,tail_off(n,sid));
    if t>=0 as i64{
     seq_del(a,n,t,sw,sid);
     st(a,tail_off(n,sid),(0 as i64)-1 as i64);
     if is_empty(a,n,t,sw){
      st(a,pos_off(t),(0 as i64)-1 as i64);
      st(a,src_off(n,t),(0 as i64)-1 as i64);
      used=used-(1 as i64);
     }
    }
   }
   j=j+1 as i64;
  }
  s=s+1 as i64;
 }

 var next_empty:i64=head;
 var k:i64=0;
 while k<n{
  if next_empty>=n{ next_empty=next_empty-n; }
  if is_empty(a,n,next_empty,sw){ break; }
  next_empty=next_empty+1 as i64;
  k=k+1 as i64;
 }

 var lo:i64=n-(1 as i64);
 var hi:i64=0;
 s=0;
 while s<n_seqs{
  let i:i64=s*nst;
  let sid:i64=ld(ub_sid,(i*ub_w)*(8 as i64));
  let t:i64=ld(a,tail_off(n,sid));
  var has_cell:bool=false;
  if t>=0 as i64{
   ensure(seq_test(a,n,t,sw,sid),63);
   if seq_size(a,n,t,sw)==(1 as i64){ has_cell=true; }
  }
  if !has_cell{
   ensure(is_empty(a,n,next_empty,sw),64);
   if t>=0 as i64{
    st(a,pos_off(next_empty),ld(a,pos_off(t)));
    st(a,src_off(n,next_empty),ld(a,src_off(n,t)));
    seq_del(a,n,t,sw,sid);
    seq_set(a,n,next_empty,sw,sid);
    ensure(!is_empty(a,n,t,sw),65);
   }
   st(a,tail_off(n,sid),next_empty);
   if s+(1 as i64)<n_seqs{
    var jj:i64=0;
    while jj<n{
     next_empty=next_empty+1 as i64;
     if next_empty>=n{ next_empty=next_empty-n; }
     if is_empty(a,n,next_empty,sw){ break; }
     jj=jj+1 as i64;
    }
   }
  }
  let nt:i64=ld(a,tail_off(n,sid));
  if lo>nt{ lo=nt; }
  if hi<nt{ hi=nt; }
  s=s+1 as i64;
 }

 // move the found cells to a contiguous range starting at lo
 s=0;
 while s<n_seqs{
  let i:i64=s*nst;
  let dst:i64=s+lo;
  let src:i64=ld(a,tail_off(n,ld(ub_sid,(i*ub_w)*(8 as i64))));
  if dst!=src{
   let tp:i64=ld(a,pos_off(dst));
   st(a,pos_off(dst),ld(a,pos_off(src)));
   st(a,pos_off(src),tp);
   let ts:i64=ld(a,src_off(n,dst));
   st(a,src_off(n,dst),ld(a,src_off(n,src)));
   st(a,src_off(n,src),ts);
   var w:i64=0;
   while w<sw{
    let tw:i64=ld(a,seqw_off(n,dst,sw,w));
    st(a,seqw_off(n,dst,sw,w),ld(a,seqw_off(n,src,sw,w)));
    st(a,seqw_off(n,src,sw,w),tw);
    w=w+1 as i64;
   }
   var j:i64=0;
   while j<n{
    let tl:i64=ld(a,tail_off(n,j));
    if tl==src{ st(a,tail_off(n,j),dst); }
    if tl==dst{
     if tl!=src{ st(a,tail_off(n,j),src); }
    }
    j=j+1 as i64;
   }
  }
  s=s+1 as i64;
 }

 // final positions and sequence sets
 s=0;
 while s<n_seqs{
  let i:i64=s*nst;
  let last:i64=ld(ub_pos,(i+nst-(1 as i64))*(8 as i64));
  let cid:i64=s+lo;
  st(a,pos_off(cid),last);
  seq_clear(a,n,cid,sw);
  let nsid:i64=ld(ub_nsid,i*(8 as i64));
  var j:i64=0;
  while j<nsid{
   let sid:i64=ld(ub_sid,(i*ub_w+j)*(8 as i64));
   seq_set(a,n,cid,sw,sid);
   st(a,tail_off(n,sid),cid);
   j=j+1 as i64;
  }
  s=s+1 as i64;
 }

 // stage source references: the first cell in the range that nothing points at becomes the
 // zero-state source. Native builds a refcount vector; here the count is the scan itself, and
 // src0 is never used as scratch (writing it outside [lo,hi] would clobber the untouched cells).
 var rs_z:i64=(0 as i64)-1 as i64;
 var q:i64=lo;
 while q<=hi{
  var rc:i64=0;
  var t2:i64=0;
  while t2<n{
   if ld(a,src_off(n,t2))==q{ rc=rc+1 as i64; }
   t2=t2+1 as i64;
  }
  if rc==(0 as i64){
   rs_z=q;
   q=hi+1 as i64;
  }else{
   q=q+1 as i64;
  }
 }
 q=lo;
 while q<=hi{
  if ld(a,src_off(n,q))<(0 as i64){
   ensure(rs_z>=0 as i64,66);
   st(a,src0_off(n,q),rs_z);
  }else{
   st(a,src0_off(n,q),ld(a,src_off(n,q)));
  }
  st(a,src_off(n,q),q);
  q=q+1 as i64;
 }

 st(a,head_off(n,sw),lo);
 st(a,nr_off(n,sw),hi-lo+(1 as i64));
 var cnt:i64=0;
 q=0;
 while q<n{
  if !is_empty(a,n,q,sw){ cnt=cnt+1 as i64; }
  q=q+1 as i64;
 }
 st(a,used_off(n,sw),cnt);

 let nn:i64=hi-lo+(1 as i64);
 put(packet,3,0,1 as u64);
 var ret:i64=0;
 if nn>=n_seqs{ ret=1 as i64; }
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
