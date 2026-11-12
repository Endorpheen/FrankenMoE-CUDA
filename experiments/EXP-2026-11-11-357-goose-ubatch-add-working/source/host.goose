// Apex ubatch assembly on the WORKING candidate line (llama_batch_allocr::ubatch_add).
//
// The working runtime's version is the simple one: a ubatch carries the token payload when the batch
// has tokens, embeddings when it has embeddings, positions for every entry, the per-entry sequence
// count and output flag, and the unique sequence list. There is no mixed-entry mode and no state
// vector here -- those belong to another line and must not be assumed.
//
// Decisions transferred:
//   n_embd_all = batch.embd ? n_tokens*n_embd : 0
//   n_pos_all  = n_tokens*n_pos_per_embd
//   out_ids    = batch index of every row whose logits flag is set, in row order
//   seq_id_unq = unique sequences, scanned in increasing id
//   per row    = n_seq_id and output, copied from the batch entry the row points at
//
// Packet:
//   k=0  0 n, 1 idxs, 2 logits, 3 nseqid, 4 seqid, 5 n_seq_max, 6 has_embd, 7 n_embd, 8 n_pos,
//        9 has_token
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (0 ok)
//
// Records: 0 sizes (a = 0 embd_all / 1 pos_all), 1 row (a = row, b = n_seq_id), 2 out id (a = pos,
// b = batch index), 3 output flag (a = row, b = value), 4 seq unq (a = seq, b = index),
// 5 seq idx (a = seq, b = index).
extern "fg357k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg357k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg357k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg357k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg357k_fail" fn fail(code:i64);

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
 let n:i64=get(packet,0,0) as i64;
 let idxs:u64=get(packet,0,1);
 let logits:u64=get(packet,0,2);
 let nseqid:u64=get(packet,0,3);
 let seqid:u64=get(packet,0,4);
 let n_seq_max:i64=get(packet,0,5) as i64;
 let has_embd:i64=get(packet,0,6) as i64;
 let n_embd:i64=get(packet,0,7) as i64;
 let n_pos:i64=get(packet,0,8) as i64;
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,1,1);
 ensure(n>=(0 as i64)&&n_seq_max>=(0 as i64),60);

 var e_all:i64=0 as i64;
 if has_embd!=(0 as i64){ e_all=n*n_embd; }
 emit(out,cntp,0 as i64,0 as i64,e_all);
 emit(out,cntp,0 as i64,1 as i64,n*n_pos);

 var o:i64=0 as i64;
 var i:i64=0;
 while i<n{
  let idx:i64=at(idxs,i);
  emit(out,cntp,1 as i64,i,at(nseqid,idx));
  let lg:i64=at(logits,idx);
  emit(out,cntp,3 as i64,i,lg);
  if lg!=(0 as i64){ emit(out,cntp,2 as i64,o,idx); o=o+1 as i64; }
  i=i+1 as i64;
 }

 var s:i64=0;
 var u:i64=0;
 while s<n_seq_max{
  var present:i64=0 as i64;
  var r:i64=0;
  while r<n{
   let idx2:i64=at(idxs,r);
   var j:i64=0;
   while j<at(nseqid,idx2){
    if at(seqid,idx2*(4 as i64)+j)==s{ present=1 as i64; }
    j=j+1 as i64;
   }
   r=r+1 as i64;
  }
  if present!=(0 as i64){
   emit(out,cntp,4 as i64,s,u);
   emit(out,cntp,5 as i64,s,u);
   u=u+1 as i64;
  }
  s=s+1 as i64;
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,0 as i64 as u64);
 return true;
}

fn main(){run(0);}
