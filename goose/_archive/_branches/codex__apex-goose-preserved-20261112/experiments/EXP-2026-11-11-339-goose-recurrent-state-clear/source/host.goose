// Apex recurrent state clearing and context stepping in Goose
// (llama_memory_recurrent::state_clear, llama_memory_recurrent_context::{next, apply, get_status}).
//
// state_clear: a negative sequence id clears the whole cache and returns at once — no per-cell work, no
// tensors. Otherwise the sequence is removed first, and a zero-length section stops there: the removal
// still happens, the tensors are untouched.
//
// The tensors are then cleared per layer in the order recurrent, state, PLE. That is **not** the order
// state_read_data writes them (recurrent, PLE, state) even though the comment says the ranges mirror the
// write pattern: the ranges do mirror, the order does not. Order only shows in a journal, which is why
// this gate records it.
//
// next(): advances the cursor and answers whether there is another ubatch; the first call after
// construction on a one-ubatch context already reports the end.
//
// apply(): an empty ubatch list means "this is an update", and the recurrent cache never updates — the
// status must already be no-update, and apply returns success without touching anything. Otherwise the
// current ubatch is placed.
//
// Packet:
//   k=0  0 op, 1 seq_id, 2 cell_count, 3 n_layer, 4 r_present, 5 r_rows, 6 s_present, 7 s_rows,
//        8 p_present, 9 p_rows, 10 head, 11 n_ub, 12 i_next, 13 status
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret
//
// Records, three slots: 0 whole-cache clear, 1 sequence removed, 2 tensor cleared (a = offset,
// b = bytes; kind in the low bits of a is not used, the order is the record order), 3 cursor advanced
// (a = new index), 4 end of ubatches, 5 update applied without placement, 6 ubatch placed (a = index).
extern "fg339k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg339k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg339k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg339k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg339k_fail" fn fail(code:i64);

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
 let seq_id:i64=get(packet,0,1) as i64;
 let cell_count:i64=get(packet,0,2) as i64;
 let n_layer:i64=get(packet,0,3) as i64;
 let r_present:u64=get(packet,0,4);
 let r_rows:u64=get(packet,0,5);
 let s_present:u64=get(packet,0,6);
 let s_rows:u64=get(packet,0,7);
 let p_present:u64=get(packet,0,8);
 let p_rows:u64=get(packet,0,9);
 let head:i64=get(packet,0,10) as i64;
 let n_ub:i64=get(packet,0,11) as i64;
 let i_next:i64=get(packet,0,12) as i64;
 let status:i64=get(packet,0,13) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,1,1);
 var ret:i64=0;

 if op==(0 as i64){
  if seq_id==(0 as i64)-1 as i64{
   emit(out,cntp,0 as i64,0 as i64,0 as i64);
   ret=0 as i64;
  }else{
   emit(out,cntp,1 as i64,seq_id,0 as i64);
   if cell_count==(0 as i64){
    ret=0 as i64;
   }else{
    var il:i64=0;
    while il<n_layer{
     if at(r_present,il)!=(0 as i64){
      let rr:i64=at(r_rows,il);
      emit(out,cntp,2 as i64,head*rr,cell_count*rr);
     }
     if at(s_present,il)!=(0 as i64){
      let sr:i64=at(s_rows,il);
      emit(out,cntp,2 as i64,head*sr,cell_count*sr);
     }
     if at(p_present,il)!=(0 as i64){
      let pr:i64=at(p_rows,il);
      emit(out,cntp,2 as i64,head*pr,cell_count*pr);
     }
     il=il+1 as i64;
    }
    ret=0 as i64;
   }
  }
 }else{
  if op==(1 as i64){
   // next()
   let ni:i64=i_next+(1 as i64);
   emit(out,cntp,3 as i64,ni,0 as i64);
   if ni>=n_ub{
    emit(out,cntp,4 as i64,0 as i64,0 as i64);
    ret=0 as i64;
   }else{
    ret=1 as i64;
   }
  }else{
   if op==(2 as i64){
    // apply()
    if n_ub==(0 as i64){
     ensure(status==(1 as i64),60);   // no-update
     emit(out,cntp,5 as i64,0 as i64,0 as i64);
     ret=1 as i64;
    }else{
     emit(out,cntp,6 as i64,i_next,0 as i64);
     ret=1 as i64;
    }
   }else{
    if op==(3 as i64){
     ret=status;
    }else{
     ensure(false,61);
    }
   }
  }
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,ret as u64);
 return true;
}

fn main(){run(0);}
