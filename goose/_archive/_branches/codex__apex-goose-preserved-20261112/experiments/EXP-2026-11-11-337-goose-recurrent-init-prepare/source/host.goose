// Apex recurrent memory initialisation and preparation in Goose
// (llama_memory_recurrent::{init_batch, prepare, init_update}).
//
// init_batch: an all-output batch splits by sequence; anything else splits **equally and sequentially
// regardless of the unified flag** — unlike the attention cache, the recurrent cache always splits
// sequentially. The rollback window is 1 + n_rs_seq and is present only when the cache has rollback
// sequences. The drain ends at the zero-token ubatch, and an allocator that did not place every token
// fails the batch.
//
// prepare is a dry run: the whole state is small, so it is copied, every ubatch is asked for a slot,
// and the copy is restored — **also on success**. Preparation does not commit anything; it only answers
// whether the sequence of ubatches could be placed. A refusal stops the walk at the first ubatch that
// could not be placed; the remaining ubatches are not consulted.
//
// init_update never updates: the recurrent cache reports "no update" rather than a context that would
// have to do something.
//
// Packet:
//   k=0  0 op, 1 out, 2 embd_all, 3 n_rs_seq, 4 ub_tokens, 5 n_ub, 6 n_used, 7 n_tokens,
//        8 find_ok, 9 n_find
//   k=1  0 cnt
//   k=2  0 done, 1 ret
//
// Records: 0 split mode (a = 0 seq / 1 equal, b = sequential flag), 1 window, 2 ubatch (a = tokens),
// 3 failed for unplaced tokens, 4 state snapshotted, 5 slot asked (a = ubatch), 6 slot refused,
// 7 state restored, 8 no-update context.
extern "fg337k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg337k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg337k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg337k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg337k_fail" fn fail(code:i64);

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
 let n_rs_seq:i64=get(packet,0,3) as i64;
 let ub_tokens:u64=get(packet,0,4);
 let n_ub:i64=get(packet,0,5) as i64;
 let n_used:i64=get(packet,0,6) as i64;
 let n_tokens:i64=get(packet,0,7) as i64;
 let find_ok:u64=get(packet,0,8);
 let n_find:i64=get(packet,0,9) as i64;
 let cntp:u64=get(packet,1,0);
 var ret:i64=0;

 if op==(0 as i64){
  var mode:i64=1 as i64;
  var window:i64=0 as i64;
  if embd_all!=(0 as i64){ mode=0 as i64; }
  if n_rs_seq>(0 as i64){ window=n_rs_seq+(1 as i64); }
  // the recurrent cache always splits sequentially, whatever the attention side does
  emit(out,cntp,0 as i64,mode,1 as i64);
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
   emit(out,cntp,3 as i64,0 as i64,0 as i64);
   put(packet,2,0,1 as u64);
   put(packet,2,1,1 as i64 as u64);
   return true;
  }
  // prepare: dry run over the ubatches
  emit(out,cntp,4 as i64,0 as i64,0 as i64);
  var ok:i64=1 as i64;
  var j:i64=0;
  while j<n_find{
   emit(out,cntp,5 as i64,j,0 as i64);
   if at(find_ok,j)==(0 as i64){
    emit(out,cntp,6 as i64,j,0 as i64);
    ok=0 as i64;
    j=n_find;
   }else{
    j=j+1 as i64;
   }
  }
  emit(out,cntp,7 as i64,0 as i64,0 as i64);
  if ok==(0 as i64){
   put(packet,2,0,1 as u64);
   put(packet,2,1,1 as i64 as u64);
   return true;
  }
  ret=0 as i64;
 }else{
  if op==(1 as i64){
   emit(out,cntp,8 as i64,0 as i64,0 as i64);
   ret=0 as i64;
  }else{
   ensure(false,60);
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
