// Apex hybrid indexed memory: pool layout in Goose (kpool_pad, kpool_rank,
// kpool_layout_update for one sequence).
//
// Pools are fixed by positions relative to the sequence's first one, so a plain append survives:
// a clean sequence resumes from the stored scan point and only appends. A stale sequence, or one
// whose cell count no longer matches, rebuilds from scratch. Two grouping modes exist: by sequence
// order (consecutive cells regardless of position) and by position (a pool starts only where the
// offset from pos_min divides the pool size, and the following cells must be consecutive positions).
//
// The last padded pool is always unused, which is why the pad rounds n_pool+1 up to 64 with a floor
// of 64.
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 kpool, 3 by_order, 4 stale, 5 pos, 6 cell, 7 n, 8 prev_n, 9 prev_pos_min,
//        10 prev_pools, 11 prev_pool_n, 12 prev_j_next, 13 rank_pos, 14 rank_cell
//   k=3  0 done, 1 ret
extern "fg310k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg310k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg310k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg310k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg310k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

fn pad64(x:i64)->i64{
 let q:i64=(x+(63 as i64))/(64 as i64);
 return q*(64 as i64);
}

fn kpool_pad(n_pool:i64)->i64{
 let p:i64=pad64(n_pool+(1 as i64));
 if p<(64 as i64){ return 64 as i64; }
 return p;
}

// Rank of (pos, cell) among cells sorted by (pos, cell): lower_bound, then an identity check.
fn kpool_rank(n:i64,pos:i64,cell:i64,posarr:u64,cellarr:u64)->i64{
 var lo:i64=0;
 var hi:i64=n;
 while lo<hi{
  let mid:i64=(lo+hi)/(2 as i64);
  let mp:i64=at(posarr,mid);
  let mc:i64=at(cellarr,mid);
  var less:bool=mp<pos;
  if mp==pos{ if mc<cell{ less=true; } }
  if less{ lo=mid+(1 as i64); }else{ hi=mid; }
 }
 if lo<n{
  if at(posarr,lo)==pos{
   if at(cellarr,lo)==cell{ return lo; }
  }
 }
 return (0 as i64)-1 as i64;
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let kpool:i64=get(packet,0,2) as i64;
 let by_order:bool=get(packet,0,3)!=(0 as u64);
 let stale:i64=get(packet,0,4) as i64;
 let posarr:u64=get(packet,0,5);
 let cellarr:u64=get(packet,0,6);
 let n:i64=get(packet,0,7) as i64;
 let prev_n:i64=get(packet,0,8) as i64;
 let prev_pos_min:i64=get(packet,0,9) as i64;
 let prev_pools:u64=get(packet,0,10);
 let prev_pool_n:i64=get(packet,0,11) as i64;
 let prev_j_next:i64=get(packet,0,12) as i64;
 let rank_pos:i64=get(packet,0,13) as i64;
 let rank_cell:i64=get(packet,0,14) as i64;

 var ret:i64=0;
 if op==(0 as i64){
  ret=kpool_pad(kpool);
 }else{
  if op==(1 as i64){
   ret=kpool_rank(n,rank_pos,rank_cell,posarr,cellarr);
  }else{
   if op==(2 as i64){
    ensure(kpool>=(1 as i64),60);
    ensure(n>=(0 as i64)&&prev_n>=(0 as i64),61);
    // cells of the sequence: start from the previous state when the sequence is clean and the
    // first position is unchanged, otherwise rebuild.
    var cells_n:i64=0;
    var j:i64=0;
    // Native resumes when the sequence is clean, the cell count did not shrink, and the first
    // position still matches; an empty sequence on both sides also resumes.
    var rebuild:bool=true;
    if stale==(0 as i64){
     if prev_n==(0 as i64){
      if n==(0 as i64){ rebuild=false; }
     }else{
      if n>=prev_n{
       if prev_pos_min==at(posarr,0){ rebuild=false; }
      }
     }
    }
    if rebuild{
     cells_n=n;
     var w:i64=0;
     while w<n{ setat(out,100 as i64+w,at(posarr,w)); w=w+1 as i64; }
     var w2:i64=0;
     while w2<n{ setat(out,600 as i64+w2,at(cellarr,w2)); w2=w2+1 as i64; }
     j=0 as i64;
    }else{
     cells_n=prev_n+(n-prev_n);
     var w:i64=0;
     while w<n{ setat(out,100 as i64+w,at(posarr,w)); w=w+1 as i64; }
     var w2:i64=0;
     while w2<n{ setat(out,600 as i64+w2,at(cellarr,w2)); w2=w2+1 as i64; }
     j=prev_j_next;
    }
    // a resumed sequence keeps the pools it already had and appends from the scan point; a
    // rebuilt one starts empty
    var pool_n:i64=0;
    if !rebuild{
     pool_n=prev_pool_n;
     var pc:i64=0;
     while pc<prev_pool_n{
      setat(out,1200 as i64+pc,at(prev_pools,pc));
      pc=pc+1 as i64;
     }
    }
    if by_order{
     while j+kpool<=cells_n{
      setat(out,1200 as i64+pool_n,j);
      pool_n=pool_n+1 as i64;
      j=j+kpool;
     }
    }else{
     let pos_min:i64=at(out,100 as i64);
     while j+kpool<=cells_n{
      let p0:i64=at(out,100 as i64+j);
      if (p0-pos_min)%kpool!=(0 as i64){
       j=j+(1 as i64);
      }else{
       var ok:bool=true;
       var kk:i64=1;
       while kk<kpool{
        if at(out,100 as i64+j+kk)!=p0+kk{ ok=false; kk=kpool; }else{ kk=kk+1 as i64; }
       }
       if ok{
        setat(out,1200 as i64+pool_n,j);
        pool_n=pool_n+1 as i64;
        j=j+kpool;
       }else{
        j=j+(1 as i64);
       }
      }
     }
    }
    setat(out,0 as i64,pool_n);
    setat(out,1 as i64,j);
    setat(out,2 as i64,cells_n);
    ret=pool_n;
   }else{
    ensure(false,62);
   }
  }
 }
 put(packet,3,0,1 as u64);
 put(packet,3,1,ret as u64);
 return true;
}

fn main(){run(0);}
