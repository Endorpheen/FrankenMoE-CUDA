// Apex k-pool layout in Goose, whole pass (llama_memory_hybrid_idx::kpool_layout_update).
//
// For every sequence the layout either **resumes** or **rebuilds**:
//
//   resume  a clean sequence whose previous layout is non-empty and whose first position is unchanged:
//           only the cells strictly after the previous last (position, cell) pair are appended, so
//           nothing already covered is re-added.
//   rebuild otherwise — and also when the resulting count no longer matches the sequence index. Count
//           alone is not enough on its own: an edit can regroup a sequence without changing its cell
//           count, so a stale sequence must rebuild regardless.
//
// A non-unified cache gives each sequence its own stream and a sequence at or beyond the stream count
// has no cells to lay out, so it is reset entirely. A unified cache reads stream 0 for every sequence.
//
// Pools then append from the stored scan point: by order, consecutive cells regardless of their
// positions; by position, only at an offset divisible by the pool size with the following cells at
// consecutive positions. Both leave the scan point past everything consumed, so the next call resumes.
//
// Packet:
//   k=0  0 kpool, 1 n_stream_kv, 2 unified, 3 stale, 4 cell_n, 5 cell_pos, 6 cell_cell, 7 by_order,
//        8 prev_n, 9 prev_pos, 10 prev_cell, 11 prev_j_next, 12 prev_pos_min, 13 pos_clean
//   k=1  0 out, 1 cnt, 2 s2s, 3 n_seq
//   k=2  0 done, 1 ret (n_pool_real)
//
// Records: 0 sequence result (a = seq, b = rebuild), 1 final cell count (a = seq, b = n), 2 pool start
// (a = seq, b = index), 3 scan point, 4 position minimum, 5 stream, 6 sequence reset.
extern "fg342k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg342k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg342k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg342k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg342k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){stw(arr,i*(8 as i64),v);}

fn emit(out:u64,cntp:u64,kind:i64,a:i64,b:i64){
 let n:i64=at(cntp,0 as i64);
 setat(out,n*(3 as i64),kind);
 setat(out,n*(3 as i64)+1 as i64,a);
 setat(out,n*(3 as i64)+2 as i64,b);
 setat(cntp,0 as i64,n+(1 as i64));
}

fn run(packet:i64)->bool{
 let kpool:i64=get(packet,0,0) as i64;
 let n_stream_kv:i64=get(packet,0,1) as i64;
 let unified:i64=get(packet,0,2) as i64;
 let stale:u64=get(packet,0,3);
 let cell_n:u64=get(packet,0,4);
 let cell_pos:u64=get(packet,0,5);
 let cell_cell:u64=get(packet,0,6);
 let by_order:i64=get(packet,0,7) as i64;
 let prev_n:u64=get(packet,0,8);
 let prev_pos:u64=get(packet,0,9);
 let prev_cell:u64=get(packet,0,10);
 let prev_j_next:u64=get(packet,0,11);
 let prev_pos_min:u64=get(packet,0,12);
 let pos_clean:i64=get(packet,0,13) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,1,1);
 let s2s:u64=get(packet,1,2);
 let n_seq:i64=get(packet,1,3) as i64;
 ensure(kpool>(0 as i64)&&n_seq>=(0 as i64),60);
 ensure(cell_n!=(0 as u64)&&prev_n!=(0 as u64),61);

 var n_pool_real:i64=0;
 var s:i64=0;
 while s<n_seq{
  if unified==(0 as i64)&&s>=n_stream_kv{
   emit(out,cntp,6 as i64,s,0 as i64);
   s=s+1 as i64;
  }else{
   // flat offsets of this sequence in both lists
   var offc:i64=0;
   var q:i64=0;
   while q<s{ offc=offc+at(cell_n,q); q=q+1 as i64; }
   var offp:i64=0;
   var q2:i64=0;
   while q2<s{ offp=offp+at(prev_n,q2); q2=q2+1 as i64; }

   let nc:i64=at(cell_n,s);
   let pn:i64=at(prev_n,s);

   var rebuild:i64=0 as i64;
   if at(stale,s)!=pos_clean{ rebuild=1 as i64; }
   if pn<=(0 as i64){ rebuild=1 as i64; }
   if nc<=(0 as i64){ rebuild=1 as i64; }
   if pn>(0 as i64){
    if at(prev_pos,offp)!=at(cell_pos,offc){ rebuild=1 as i64; }
   }

   var n_final:i64=0 as i64;
   var pos_min:i64=0 as i64;
   if rebuild==(0 as i64){
    // resume: append strictly after the previous last pair
    n_final=pn;
    pos_min=at(prev_pos_min,s);
    let last_pos:i64=at(prev_pos,offp+pn-(1 as i64));
    let last_cell:i64=at(prev_cell,offp+pn-(1 as i64));
    var a:i64=0;
    while a<nc{
     let cp:i64=at(cell_pos,offc+a);
     let cc:i64=at(cell_cell,offc+a);
     var after:bool=false;
     if cp>last_pos{ after=true; }
     if cp==last_pos&&cc>last_cell{ after=true; }
     if after{ n_final=n_final+1 as i64; }
     a=a+1 as i64;
    }
    if n_final!=nc{ rebuild=1 as i64; }
   }

   // a rebuild after a resume attempt re-copies the cell list, so the length it would have had
   // from the resume must not survive -- otherwise the pool scan walks past the real cells
   if rebuild!=(0 as i64){
    n_final=nc;
    pos_min=0 as i64;
    if nc>(0 as i64){ pos_min=at(cell_pos,offc); }
   }

   var j:i64=0 as i64;
   if rebuild==(0 as i64){ j=at(prev_j_next,s); }
   var n_pools:i64=0 as i64;
   if by_order!=(0 as i64){
    while j+kpool<=n_final{
     emit(out,cntp,2 as i64,s,j);
     n_pools=n_pools+1 as i64;
     j=j+kpool;
    }
   }else{
    while j+kpool<=n_final{
     let p0:i64=at(cell_pos,offc+j);
     if (p0-pos_min)%kpool!=(0 as i64){
      j=j+1 as i64;
     }else{
      var ok:i64=1 as i64;
      var kk:i64=1;
      while kk<kpool{
       if at(cell_pos,offc+j+kk)!=p0+kk{ ok=0 as i64; }
       kk=kk+1 as i64;
      }
      if ok!=(0 as i64){
       emit(out,cntp,2 as i64,s,j);
       n_pools=n_pools+1 as i64;
       j=j+kpool;
      }else{
       j=j+1 as i64;
      }
     }
    }
   }
   emit(out,cntp,0 as i64,s,rebuild);
   emit(out,cntp,1 as i64,s,n_final);
   emit(out,cntp,3 as i64,s,j);
   emit(out,cntp,4 as i64,s,pos_min);
   emit(out,cntp,5 as i64,s,at(s2s,s));
   n_pool_real=n_pool_real+n_pools;
   s=s+1 as i64;
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,n_pool_real as u64);
 return true;
}

fn main(){run(0);}
