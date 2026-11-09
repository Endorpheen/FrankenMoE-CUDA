// KV slot search in Goose (APX: P5-01, the "find" clause).
//
// From llama-kv-cache.cpp:921-1117 llama_kv_cache::find_slot: the ring-buffer walk that picks the
// cells an ubatch will occupy. EXP199 ported the two decisions it calls (head_start, can_use_cell)
// but not the walk: the wraparound, the contiguous versus per-token testing and the give-up
// condition. A wrong walk picks a cell still in use, which corrupts attention rather than raising
// an error.
//
// The per-cell predicate is LAZY: Goose calls an extern primitive for exactly the cells it tests,
// so the cost is O(cells tested) like native. An earlier version took a precomputed can_use array
// for the whole cache, which would have been 2048x the work per decode step at the Apex cache size
// of 262144 cells -- the same O(cache) mistake that took EXP204 to 8.34 tok/s. Recorded so it is
// not repeated.

extern "fg205g_fail" fn fail(code:i64);
extern "fg205g_st_i64" fn st_i64(base:u64,byte:i64,v:i64);
extern "fg205g_can_use" fn can_use(idx:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// head_start: llama-kv-cache.cpp:1026-1029.
fn head_start(head_cur:i64,used:i64,n_tokens:i64)->i64{
 ensure(head_cur>=0&&used>=0&&n_tokens>=0,10);
 if head_cur>used+2 as i64*n_tokens{return 0 as i64;}
 return head_cur;
}

// find_slots: lines 1039-1107 for one stream.
//   out[n] receives the chosen cell indices in order.
//   cont 1 = the whole ubatch must land contiguously, 0 = per-token search.
// Returns the number found, or -1 when native would return an empty slot_info.
fn find_slots(size:i64,head0:i64,n_tokens:i64,cont:i64,out:u64)->i64{
 ensure(size>=0&&head0>=0&&n_tokens>=0,20);
 ensure(cont==0||cont==1,21);
 if n_tokens>size{return (0 as i64)-1 as i64;}
 var head:i64=head0;
 var n_tested:i64=0;
 var found:i64=0;
 var step:i64=1;
 if cont==1 as i64{step=n_tokens;}
 while true{
  if head+step>size{
   n_tested+=size-head;
   head=0;
  }else{
   var i:i64=0;
   var run:i64=0;
   while i<step{
    let idx:i64=head;
    head+=1;
    n_tested+=1;
    if can_use(idx)==1 as i64{
     st_i64(out,found*8 as i64,idx);
     found+=1;
     run+=1;
    }else{
     if cont==1 as i64{
      i=step;
     }
    }
    i+=1;
   }
   if found==n_tokens{return found;}
   if cont==1 as i64{
    found=0;
   }
  }
  if n_tested>=size{return (0 as i64)-1 as i64;}
 }
 return (0 as i64)-1 as i64;
}

fn main(){
 head_start(0 as i64,0 as i64,0 as i64);
 find_slots(0 as i64,0 as i64,0 as i64,0 as i64,0 as u64);
 find_slots(4 as i64,1 as i64,2 as i64,1 as i64,16 as u64);
}
