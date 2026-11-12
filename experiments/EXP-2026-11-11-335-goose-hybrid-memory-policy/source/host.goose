// Apex hybrid memory policy in Goose (llama_memory_hybrid, P5-05/P5-09).
//
// The hybrid memory is an attention cache plus a recurrent cache, and almost everything here is about
// which of the two runs first and what happens when one of them refuses.
//
//   split      an all-output batch splits by sequence, otherwise equally — with the unified flag set
//              only when the attention cache is not unified, and with the rollback window (1 +
//              n_rs_seq) when the recurrent cache needs one.
//   prepare    the recurrent cache prepares first. Its failure is a failed context immediately, and the
//              attention cache is never asked.
//   seq_rm     recurrent first *because it can fail without mutating*, so a refusal leaves the cache
//              untouched; attention is only asked afterwards.
//   mutation   cp, keep, add, div, clear, both state halves: attention then recurrent.
//   extrema    the total minimum is the larger of the two minima and the total maximum the smaller of
//              the two maxima, so a cache that does not hold the sequence (-1) does not hide the other.
//   read       if the recurrent half throws after attention succeeded, attention is cleared for that
//              sequence — the restore is a transaction and must not leave half a sequence behind.
//
// Packet:
//   k=0  0 op, 1 out, 2 embd_all, 3 n_stream_attn, 4 n_rs_seq, 5 ub_tokens, 6 n_ub, 7 n_used,
//        8 n_tokens, 9 recr_ok, 10 attn_heads, 11 recr_ok2, 12 attn_min, 13 attn_max, 14 recr_min,
//        15 recr_max
//   k=1  0 cnt, 1 attn_keys, 2 attn_sizes, 3 recr_keys, 4 recr_sizes, 5 n_bk
//   k=2  0 done, 1 ret (0 ok, 1 failed prepare, -1 thrown)
//
// Records, three slots: 0 split mode (a = 0 seq, 1 equal, b = flag), 1 split window (a = window),
// 2 ubatch (a = tokens), 3 recurrent prepare (a = ok), 4 attention prepare (a = heads),
// 5 recurrent ran, 6 attention ran, 7 attention cleared (a = backend key), 8 extrema (a = value).
extern "fg335k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg335k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg335k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg335k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg335k_fail" fn fail(code:i64);

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
 let recr_ok2:i64=get(packet,0,11) as i64;
 let attn_min:i64=get(packet,0,12) as i64;
 let attn_max:i64=get(packet,0,13) as i64;
 let recr_min:i64=get(packet,0,14) as i64;
 let recr_max:i64=get(packet,0,15) as i64;
 let cntp:u64=get(packet,1,0);
 var ret:i64=0;

 if op==(0 as i64){
  // init_batch
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
  if n_used<n_tokens{
   put(packet,2,0,1 as u64);
   put(packet,2,1,1 as i64 as u64);
   return true;
  }
  emit(out,cntp,3 as i64,recr_ok,0 as i64);
  if recr_ok==(0 as i64){
   put(packet,2,0,1 as u64);
   put(packet,2,1,1 as i64 as u64);
   return true;
  }
  emit(out,cntp,4 as i64,attn_heads,0 as i64);
  if attn_heads==(0 as i64){
   put(packet,2,0,1 as u64);
   put(packet,2,1,1 as i64 as u64);
   return true;
  }
  ret=0 as i64;
 }else{
  if op==(1 as i64){
   // seq_rm: recurrent first and it may refuse without mutating
   emit(out,cntp,5 as i64,0 as i64,0 as i64);
   if recr_ok2==(0 as i64){
    ret=0 as i64;
   }else{
    emit(out,cntp,6 as i64,0 as i64,0 as i64);
    ret=1 as i64;
   }
  }else{
   if op==(2 as i64){
    // mutation pair / state_write / clear: attention then recurrent
    emit(out,cntp,6 as i64,0 as i64,0 as i64);
    emit(out,cntp,5 as i64,0 as i64,0 as i64);
    ret=0 as i64;
   }else{
    if op==(3 as i64){
     // state_read: attention, then recurrent; a throw clears the attention half
     emit(out,cntp,6 as i64,0 as i64,0 as i64);
     emit(out,cntp,5 as i64,0 as i64,0 as i64);
     if recr_ok2==(0 as i64){
      emit(out,cntp,7 as i64,at(get(packet,1,1),0 as i64),0 as i64);
      ret=(0 as i64)-1 as i64;
     }
    }else{
     if op==(4 as i64){
      // extrema: min = max of the two minima, max = min of the two maxima
      var lo:i64=attn_min;
      if recr_min>lo{ lo=recr_min; }
      var hi:i64=attn_max;
      if recr_max<hi{ hi=recr_max; }
      emit(out,cntp,8 as i64,lo,hi);
      ret=lo;
     }else{
      if op==(5 as i64){
       // memory_breakdown: merge by backend key, summing
       let ak:u64=get(packet,1,1);
       let as_:u64=get(packet,1,2);
       let rk:u64=get(packet,1,3);
       let rs:u64=get(packet,1,4);
       let nb:i64=get(packet,1,5) as i64;
       var i2:i64=0;
       while i2<nb{
        let key:i64=at(ak,i2);
        var total:i64=at(as_,i2);
        var j:i64=0;
        while j<nb{
         if at(rk,j)==key{ total=total+at(rs,j); }
         j=j+1 as i64;
        }
        emit(out,cntp,9 as i64,key,total);
        i2=i2+1 as i64;
       }
       // recurrent keys the attention half does not have are inserted too
       var j2:i64=0;
       while j2<nb{
        let rkey:i64=at(rk,j2);
        var seen:i64=0 as i64;
        var q:i64=0;
        while q<nb{
         if at(ak,q)==rkey{ seen=1 as i64; }
         q=q+1 as i64;
        }
        if seen==(0 as i64){
         var tot2:i64=0 as i64;
         var q2:i64=0;
         while q2<nb{
          if at(rk,q2)==rkey{ tot2=tot2+at(rs,q2); }
          q2=q2+1 as i64;
         }
         emit(out,cntp,9 as i64,rkey,tot2);
        }
        j2=j2+1 as i64;
       }
       ret=0 as i64;
      }else{
       ensure(false,60);
      }
     }
    }
   }
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
