// llama_batch_allocr::split_seq selection loop in Goose (APX: P6-07).
//
// Native (llama-batch.cpp:790-829):
//     cur_idx = first unused token;  if none -> done
//     cur_seq_set = seq_set[cur_idx]
//     loop:
//         idxs.push(cur_idx); used[cur_idx]=true; ++n_used
//         if (idxs.size() >= n_ubatch) break
//         do { ++cur_idx } while (cur_idx < n_tokens &&
//                                (used[cur_idx] || ((cur_seq_set & seq_set[cur_idx]) != seq_set[cur_idx])))
//         if (cur_idx == n_tokens) break
//         cur_seq_set = seq_set[cur_idx]
//
// The subset test is the whole point: a token may only be appended if its sequence set is a SUBSET of the
// one being carved out. Written the other way round (`seq_set & cur != 0`, i.e. "any overlap") it accepts a
// token that belongs to a sequence outside the ubatch, which corrupts the batch split without changing any
// token's value -- exactly the class of error a token-equality check cannot see.
//
// The C++ seq_set_t is std::bitset<LLAMA_MAX_SEQ>; here it arrives as two 64-bit words, which is enough
// for LLAMA_MAX_SEQ up to 128 and lets the subset test stay a pair of bitwise ops.

extern "fg238q_fail" fn fail(code:i64);
extern "fg238q_ld_u64" fn ld_u64(base:u64,idx:i64)->i64;
extern "fg238q_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg238q_st_i32" fn st_i32(base:u64,idx:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// is_subset: (a & b) == a, over two words
fn is_subset(a0:i64,a1:i64,b0:i64,b1:i64)->i64{
 let x0:i64=a0&b0;
 let x1:i64=a1&b1;
 if x0==a0&&x1==a1{return 1 as i64;}
 0 as i64
}

// plan_seq returns the number of tokens selected and writes their indices to out.
// used / set_lo / set_hi are per-token; out receives up to 64 indices.
fn plan_seq(n_tokens:i64,n_ubatch:i64,used:u64,set_lo:u64,set_hi:u64,out:u64)->i64{
 ensure(n_tokens>=0 as i64,1800);
 ensure(n_ubatch>=0 as i64,1801);
 var cur:i64=0;
 while cur<n_tokens{
  if ld_i32(used,cur)==0 as i64{break;}
  cur=cur+1 as i64;
 }
 if cur>=n_tokens{return 0 as i64;}
 var clo:i64=ld_u64(set_lo,cur);
 var chi:i64=ld_u64(set_hi,cur);
 var n:i64=0;
 var go:i64=1;
 while go!=0 as i64{
  st_i32(out,n,cur);
  n=n+1 as i64;
  if n>=n_ubatch{go=0 as i64;} else {
   var next:i64=cur+1 as i64;
   var found:i64=0;
   while next<n_tokens{
    if ld_i32(used,next)==0 as i64&&is_subset(clo,chi,ld_u64(set_lo,next),ld_u64(set_hi,next))==1 as i64{
     found=1 as i64;
     break;
    }
    next=next+1 as i64;
   }
   if found==0 as i64{go=0 as i64;} else {
    cur=next;
    clo=ld_u64(set_lo,cur);
    chi=ld_u64(set_hi,cur);
   }
  }
 }
 n
}

fn main(){
 plan_seq(0 as i64,4 as i64,0 as u64,8 as u64,16 as u64,24 as u64);
 plan_seq(4 as i64,2 as i64,32 as u64,40 as u64,48 as u64,56 as u64);
}
