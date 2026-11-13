// Apex hybrid indexed memory initialisation in Goose (llama_memory_hybrid_idx).
//
// init_batch repeats the hybrid rules — all-output splits by sequence, otherwise equally with the
// inverted unified flag and the rollback window, then recurrent prepare before attention — and adds
// one thing that matters more than it looks: the indexer takes the **attention cache's slot layout
// verbatim**. It does not search for its own cells. A separate search could return a different
// placement, and the indexer keys would then point at cells the attention cache never wrote.
//
// A failed batch therefore yields no layout at all, not a partial one. Without an indexer memory
// nothing is copied.
//
// init_update simply carries the optimize flag into the context. clear clears the indexer half too,
// after the base halves, and resets its staleness for every sequence to zero.
//
// Packet:
//   k=0  0 op, 1 out, 2 embd_all, 3 n_stream_attn, 4 n_rs_seq, 5 ub_tokens, 6 n_ub, 7 n_used,
//        8 n_tokens, 9 recr_ok, 10 attn_heads, 11 has_idx, 12 optimize
//   k=1  0 cnt
//   k=2  0 done, 1 ret
//
// Records, three slots: 0 split (a = 0 seq / 1 equal, b = flag), 1 window, 2 ubatch (a = tokens),
// 3 recurrent prepare (a = ok), 4 attention prepare (a = heads), 5 layout copied (a = count),
// 6 context created (a = status), 7 base cleared, 8 indexer cleared, 9 staleness reset (a = seq).
extern "fg336k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg336k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg336k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg336k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg336k_fail" fn fail(code:i64);

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
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let embd_all:i64=get(packet,0,2) as i64;
 let n_stream_attn:i64=get(packet,0,3) as i64;
 let n_rs_seq:i64=get(packet,0,4) as i64;
 let ub_tokens:u64=get(packet,0,5);
 let n_ub:i64=get(packet,0,6) as i64;
 let n_used:i64=get(packet,0,7) as i64;
 let n_tokens:i64=get(packet,0,8) as i64;
 let recr_ok:i64=get(packet,0,9) as i64;
 let attn_heads:i64=get(packet,0,10) as i64;
 let has_idx:i64=get(packet,0,11) as i64;
 let optimize:i64=get(packet,0,12) as i64;
 let cntp:u64=get(packet,1,0);
 var ret:i64=0;

 if op==(0 as i64){
  var mode:i64=1 as i64;
  var flag:i64=0 as i64;
  var window:i64=0 as i64;
  if embd_all!=(0 as i64){
   mode=0 as i64;
  }else{
   var unified:i64=0 as i64;
   if n_stream_attn==(1 as i64){ unified=1 as i64; }
   flag=(1 as i64)-unified;
   if n_rs_seq>(0 as i64){ window=n_rs_seq+(1 as i64); }
  }
  emit(out,cntp,0 as i64,mode,flag);
  if mode==(1 as i64){ emit(out,cntp,1 as i64,window,0 as i64); }
  var i:i64=0;
  while i<n_ub{
   let t:i64=at(ub_tokens,i);
   if t==(0 as i64){ i=n_ub; }else{
    emit(out,cntp,2 as i64,t,0 as i64);
    i=i+1 as i64;
   }
  }
  if n_used>=n_tokens{
   emit(out,cntp,3 as i64,recr_ok,0 as i64);
   if recr_ok!=(0 as i64){
    emit(out,cntp,4 as i64,attn_heads,0 as i64);
    if attn_heads!=(0 as i64){
     // the indexer takes the attention layout as it is; a separate search could drift
     if has_idx!=(0 as i64){ emit(out,cntp,5 as i64,attn_heads,0 as i64); }
     emit(out,cntp,6 as i64,0 as i64,0 as i64);
     ret=0 as i64;
    }else{
     emit(out,cntp,6 as i64,1 as i64,0 as i64);
     ret=1 as i64;
    }
   }else{
    emit(out,cntp,6 as i64,1 as i64,0 as i64);
    ret=1 as i64;
   }
  }else{
   emit(out,cntp,6 as i64,1 as i64,0 as i64);
   ret=1 as i64;
  }
 }else{
  if op==(1 as i64){
   // init_update: the flag is carried, nothing is decided here
   emit(out,cntp,6 as i64,0 as i64,optimize);
   ret=0 as i64;
  }else{
   if op==(2 as i64){
    // clear: base halves first, then the indexer half and its staleness
    emit(out,cntp,7 as i64,0 as i64,0 as i64);
    if has_idx!=(0 as i64){
     emit(out,cntp,8 as i64,0 as i64,0 as i64);
     // staleness reset for every sequence, clamped at zero
     emit(out,cntp,9 as i64,(0 as i64)-1 as i64,0 as i64);
    }
    ret=0 as i64;
   }else{
    ensure(false,60);
   }
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
