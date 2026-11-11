// Apex batch initialisation stats in Goose (llama_batch_allocr::init, output/coupling/set section).
//
// Three independent pieces, each with a rule that is easy to state and easy to break:
//
//   outputs        an entry's flag is taken verbatim; when the caller asked for all outputs and some
//                  entry is not marked, **every** flag is overridden to one — not only the missing ones.
//   decision order the array exists only if some entry carries a non-zero order; it is then fully
//                  zero-initialised and the non-zero entries are copied in.
//   coupling       a token assigned to several sequences couples them: with s0 the entry's first
//                  sequence, every later sequence s1 is marked coupled to s0 and the batch reports
//                  `has_cpl`. The reverse pair is deliberately not tracked.
//
// The sequence sets follow: one mask per entry, a map from mask to the entry indices carrying it (in
// entry order), the unique ids scanned in **increasing** order, and `seq_idx` mapping an id to its
// position in that list.
//
// Packet:
//   k=0  0 n, 1 out_flags, 2 output_all, 3 order, 4 n_seq_max, 5 nseqid, 6 seqid, 7 pos
//   k=1  0 out, 1 cnt
//   k=2  0 done, 1 ret (n_outputs)
//
// Records: 0 output flag (a = entry, b = value), 1 override (a = 0), 2 decision order (a = entry,
// b = value), 3 n_outputs (a = value), 4 coupled (a = s1, b = s0), 5 has_cpl (a = value),
// 6 seq set (a = entry, b = mask), 7 map entry (a = mask index, b = entry), 8 unique (a = seq, b = index),
// 9 seq idx (a = seq, b = index).
extern "fg353k_get" fn get(p:i64,k:i64,i:i64)->u64;
extern "fg353k_put" fn put(p:i64,k:i64,i:i64,v:u64);
extern "fg353k_ld_i64" fn ld(base:u64,byte:i64)->i64;
extern "fg353k_stw" fn stw(base:u64,byte:i64,v:i64);
extern "fg353k_fail" fn fail(code:i64);

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
 let out_flags:u64=get(packet,0,1);
 let output_all:i64=get(packet,0,2) as i64;
 let order:u64=get(packet,0,3);
 let n_seq_max:i64=get(packet,0,4) as i64;
 let nseqid:u64=get(packet,0,5);
 let seqid:u64=get(packet,0,6);
 let out:u64=get(packet,1,0);
 let cntp:u64=get(packet,1,1);
 ensure(n>(0 as i64)&&n_seq_max>=(0 as i64),60);

 // outputs
 var any_missing:i64=0 as i64;
 var i:i64=0;
 while i<n{
  if at(out_flags,i)==(0 as i64){ any_missing=1 as i64; }
  i=i+1 as i64;
 }
 var override:i64=0 as i64;
 if output_all!=(0 as i64)&&any_missing!=(0 as i64){ override=1 as i64; }

 if override!=(0 as i64){ emit(out,cntp,1 as i64,0 as i64,0 as i64); }
 var n_outputs:i64=0 as i64;
 i=0;
 while i<n{
  var v:i64=at(out_flags,i);
  if override!=(0 as i64){ v=1 as i64; }
  emit(out,cntp,0 as i64,i,v);
  if v!=(0 as i64){ n_outputs=n_outputs+(1 as i64); }
  i=i+1 as i64;
 }
 emit(out,cntp,3 as i64,n_outputs,0 as i64);

 // decision order: the array exists only when some entry carries one
 var has_order:i64=0 as i64;
 i=0;
 while i<n{
  if at(order,i)!=(0 as i64){ has_order=1 as i64; }
  i=i+1 as i64;
 }
 if has_order!=(0 as i64){
  i=0;
  while i<n{
   emit(out,cntp,2 as i64,i,at(order,i));
   i=i+1 as i64;
  }
 }

 // coupling
 var has_cpl:i64=0 as i64;
 i=0;
 while i<n{
  let s0:i64=at(seqid,i*(4 as i64));
  var j:i64=0;
  while j<at(nseqid,i){
   let s1:i64=at(seqid,i*(4 as i64)+j);
   if j>(0 as i64){
    emit(out,cntp,4 as i64,s1,s0);
    has_cpl=1 as i64;
   }
   j=j+1 as i64;
  }
  i=i+1 as i64;
 }
 emit(out,cntp,5 as i64,has_cpl,0 as i64);

 // sequence sets and their map
 var masks_chosen:i64=0 as i64;
 i=0;
 while i<n{
  var mask:i64=0 as i64;
  var j2:i64=0;
  while j2<at(nseqid,i){
   let sid:i64=at(seqid,i*(4 as i64)+j2);
   ensure(sid>=(0 as i64)&&sid<(64 as i64),61);
   mask=mask|((1 as i64)<<sid);
   j2=j2+1 as i64;
  }
  emit(out,cntp,6 as i64,i,mask);
  // the map is reported in first-appearance order of masks
  var first:i64=1 as i64;
  var q:i64=0;
  while q<i{
   var mq:i64=0 as i64;
   var q2:i64=0;
   while q2<at(nseqid,q){
    mq=mq|((1 as i64)<<at(seqid,q*(4 as i64)+q2));
    q2=q2+1 as i64;
   }
   if mq==mask{ first=0 as i64; }
   q=q+1 as i64;
  }
  if first!=(0 as i64){ emit(out,cntp,7 as i64,masks_chosen,i); masks_chosen=masks_chosen+(1 as i64); }
  i=i+1 as i64;
 }

 // unique ids in increasing order
 var s:i64=0;
 var u:i64=0;
 while s<n_seq_max{
  var present:i64=0 as i64;
  var r:i64=0;
  while r<n{
   var k:i64=0;
   while k<at(nseqid,r){
    if at(seqid,r*(4 as i64)+k)==s{ present=1 as i64; }
    k=k+1 as i64;
   }
   r=r+1 as i64;
  }
  if present!=(0 as i64){
   emit(out,cntp,8 as i64,s,u);
   emit(out,cntp,9 as i64,s,u);
   u=u+1 as i64;
  }
  s=s+1 as i64;
 }
 put(packet,2,0,1 as u64);
 put(packet,2,1,n_outputs as u64);
 return true;
}

fn main(){run(0);}
