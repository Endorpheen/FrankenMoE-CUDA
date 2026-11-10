// Output-id selection and seq_idx mapping in Goose (APX: P6-07).
//
// Two decisions at the tail of llama_batch_allocr::ubatch_add (llama-batch.cpp:937-957):
//
//     for i in idxs: if (udata->output[i]) out_ids.push_back(idxs[i]);
//     for s in 0..n_seq_max: if (seq_set_unq.test(s)) { seq_idx[s] = seq_id_unq.size(); seq_id_unq.push_back(s); }
//
// `out_ids` is the list of BATCH indices whose logits the caller will read, so its ORDER defines the
// correspondence between returned logits and input tokens. A wrong order still returns the right number of
// rows with plausible values.
//
// `seq_idx` is an index INTO seq_id_unq, and it is assigned from seq_id_unq.size() BEFORE the push. So the
// first unique sequence gets 0, the second 1, and the mapping is the order of increasing seq id -- because
// the loop walks s upward. Assigning from size() AFTER the push shifts every value by one, which makes
// every sequence read the wrong slice of the sequence dimension.
//
// Both are pure decisions over arrays that already exist; nothing is copied.

extern "fg242o_fail" fn fail(code:i64);
extern "fg242o_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg242o_st_i32" fn st_i32(base:u64,idx:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// pick_outids writes the batch indices whose output flag is set, and returns how many.
fn pick_outids(n_tokens:i64,idxs:u64,output:u64,out:u64)->i64{
 ensure(n_tokens>=0 as i64,2300);
 var n:i64=0;
 var i:i64=0;
 while i<n_tokens{
  let ix:i64=ld_i32(idxs,i);
  ensure(ix>=0 as i64,2301);
  if ld_i32(output,i)!=0 as i64{
   st_i32(out,n,ix);
   n=n+1 as i64;
  }
  i=i+1 as i64;
 }
 n
}

// map_seqs writes seq_idx[s] for every s present in unq (four 64-bit words) and the ordered unique ids to
// out_ids, returning the number of unique sequences.
fn map_seqs(n_max:i64,unq0:i64,unq1:i64,unq2:i64,unq3:i64,out_seq_idx:u64,out_ids:u64)->i64{
 ensure(n_max>0 as i64&&n_max<=256 as i64,2302);
 var n:i64=0;
 var s:i64=0;
 while s<n_max{
  var present:i64=0;
  if s<64 as i64{if ((unq0>>s)&1 as i64)!=0 as i64{present=1 as i64;}}
  if s>=64 as i64&&s<128 as i64{if ((unq1>>(s-64 as i64))&1 as i64)!=0 as i64{present=1 as i64;}}
  if s>=128 as i64&&s<192 as i64{if ((unq2>>(s-128 as i64))&1 as i64)!=0 as i64{present=1 as i64;}}
  if s>=192 as i64{if ((unq3>>(s-192 as i64))&1 as i64)!=0 as i64{present=1 as i64;}}
  if present!=0 as i64{
   // native assigns from seq_id_unq.size() BEFORE push_back, so the first is 0
   st_i32(out_seq_idx,s,n);
   st_i32(out_ids,n,s);
   n=n+1 as i64;
  }
  s=s+1 as i64;
 }
 n
}

fn main(){
 pick_outids(0 as i64,0 as u64,0 as u64,0 as u64);
 map_seqs(64 as i64,0 as i64,0 as i64,0 as i64,0 as i64,0 as u64,0 as u64);
 map_seqs(256 as i64,5 as i64,3 as i64,0 as i64,1 as i64,8 as u64,24 as u64);
}
