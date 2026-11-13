// Apex KV cache copy-in plan in Goose: llama_kv_cache::{cpy_k, cpy_v} and the shape of the V index
// tensor built by build_input_v_idxs.
//
// cpy_k and the plain-V branch merge the head dimensions of the incoming tensor into one row and
// scatter it into the cache with the per-token indices. When the cache has several streams the
// buffer is reshaped so it spans all of them, because the indices are global.
//
// The transposed-V branch differs: the incoming tensor is reshaped when its rows are already
// contiguous and copied otherwise, padded when the cache's value width is wider than this token's,
// and then both sides are flattened to one element per row because the V indices index single head
// elements.
//
// Operation records, three slots each: kind, a, b.
//   kind 0: view2d(a, b)       view the incoming tensor with (a) columns and (b) rows
//   kind 1: reshape2d(a, b)    reshape, no copy
//   kind 2: cont2d(a, b)       copy into a contiguous tensor
//   kind 3: pad(a)             pad the value width by a
//   kind 4: set_rows           the scatter itself
//
// Packet, int64 slots:
//   k=0  0 op, 1 out, 2 n_embd_head, 3 n_head, 4 n_tokens, 5 nb2, 6 ts, 7 n_stream, 8 kv_size,
//        9 v_ne0, 10 v_trans, 11 v_nelements, 12 vcur_nelements, 13 n_embd_v_gqa_max
//   k=3  0 done, 1 ret
extern "fg319k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg319k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg319k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg319k_st_i64" fn st(base:u64,byte:i64,v:i64);
extern "fg319k_fail" fn fail(code:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn at(arr:u64,i:i64)->i64{return ld(arr,i*(8 as i64));}
fn setat(arr:u64,i:i64,v:i64){st(arr,i*(8 as i64),v);}

fn emit(out:u64,cntp:u64,kind:i64,a:i64,b:i64){
 let c:i64=at(cntp,0 as i64);
 setat(out,c*(3 as i64),kind);
 setat(out,c*(3 as i64)+1 as i64,a);
 setat(out,c*(3 as i64)+2 as i64,b);
 setat(cntp,0 as i64,c+(1 as i64));
}

fn run(packet:i64)->bool{
 let op:i64=get(packet,0,0) as i64;
 let out:u64=get(packet,0,1);
 let head:i64=get(packet,0,2) as i64;
 let nhead:i64=get(packet,0,3) as i64;
 let ntok:i64=get(packet,0,4) as i64;
 let nb2:i64=get(packet,0,5) as i64;
 let ts:i64=get(packet,0,6) as i64;
 let n_stream:i64=get(packet,0,7) as i64;
 let kv_size:i64=get(packet,0,8) as i64;
 let v_ne0:i64=get(packet,0,9) as i64;
 let v_trans:i64=get(packet,0,10) as i64;
 let v_nelem:i64=get(packet,0,11) as i64;
 let vcur_nelem:i64=get(packet,0,12) as i64;
 let gqa_max:i64=get(packet,0,13) as i64;
 let cntp:u64=get(packet,1,0);
 ensure(head>(0 as i64)&&nhead>(0 as i64)&&ntok>=(0 as i64),60);
 ensure(ts>(0 as i64)&&n_stream>(0 as i64),61);
 let gqa:i64=head*nhead;
 var ret:i64=0;

 if op==(0 as i64){
  // cpy_k: view, optional cross-stream reshape, scatter
  emit(out,cntp,0 as i64,gqa,ntok);
  if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }
  emit(out,cntp,4 as i64,0 as i64,0 as i64);
  ret=at(cntp,0 as i64);
 }else{
  if op==(1 as i64){
   // cpy_v
   if v_trans==(0 as i64){
    emit(out,cntp,0 as i64,gqa,ntok);
    if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }
    emit(out,cntp,4 as i64,0 as i64,0 as i64);
   }else{
    if ts*gqa==nb2{
     emit(out,cntp,1 as i64,gqa,ntok);
    }else{
     emit(out,cntp,2 as i64,gqa,ntok);
    }
    if gqa<v_ne0{ emit(out,cntp,3 as i64,v_ne0-gqa,0 as i64); }
    emit(out,cntp,1 as i64,1 as i64,v_nelem);
    emit(out,cntp,1 as i64,1 as i64,vcur_nelem);
    emit(out,cntp,4 as i64,0 as i64,0 as i64);
   }
   ret=at(cntp,0 as i64);
  }else{
   if op==(2 as i64){
    // build_input_v_idxs: one index per token, or one per token and value element
    if v_trans==(0 as i64){ ret=ntok; }else{ ret=ntok*gqa_max; }
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
