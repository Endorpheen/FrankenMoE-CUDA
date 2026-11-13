// Output reorder in Goose (APX: P6-07). llama-context.cpp:2187-2210.
//
// When the ubatch's outputs do not arrive in the user's batch order, native restores that order with a
// SELECTION SORT chosen specifically to minimise the number of swaps:
//
//     for i in 0..n_outputs-1:
//         j_min = argmin(out_ids[i..n-1])
//         if j_min == i: continue
//         swap(out_ids[i], out_ids[j_min]); output_swaps.push({i, j_min})
//     fill(output_ids, -1); for i: output_ids[out_ids[i]] = i
//
// Then, lazily on access, output_reorder() replays those identical swaps on the logits rows.
//
// Three things make this worth porting rather than treating as a sort:
//
//  1. It is not "a sort" -- it is THIS sort. Any correct sort produces the same final out_ids, but a different
//     swap list, and the swap list is replayed verbatim on the logits rows. A stable sort or an insertion
//     sort would reorder the outputs correctly and swap the WRONG rows, because the replay assumes exactly
//     these transpositions.
//  2. `j_min` starts at `i`, so when out_ids[i] is already the minimum the pair is skipped -- no swap is
//     recorded. Recording a no-op swap still works, but it doubles the replay work.
//  3. The inverse map is built from the SORTED out_ids and is indexed by the original batch index, not by
//     position. `output_ids[out_ids[i]] = i` is what lets the caller ask "where did batch token k land".
//
// Strictly-less comparison (`<`, not `<=`) keeps the FIRST minimum, which matters when duplicates exist.

extern "fg244s_fail" fn fail(code:i64);
extern "fg244s_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg244s_st_i32" fn st_i32(base:u64,idx:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// reorder sorts out_ids in place with the swap-minimising selection sort, writes the (i, j_min) pairs to
// swaps and the inverse map to inv, and returns the number of swaps.
fn reorder(n_outputs:i64,out_ids:u64,swaps:u64,inv:u64,inv_len:i64)->i64{
 ensure(n_outputs>=0 as i64,2500);
 var n_swaps:i64=0;
 if n_outputs>1 as i64{
  var i:i64=0;
  while i<n_outputs-1 as i64{
   var j_min:i64=i;
   var j:i64=i+1 as i64;
   while j<n_outputs{
    // strictly less: the FIRST minimum wins, which is what the native loop does
    if ld_i32(out_ids,j)<ld_i32(out_ids,j_min){j_min=j;}
    j=j+1 as i64;
   }
   if j_min!=i{
    let a:i64=ld_i32(out_ids,i);
    let b:i64=ld_i32(out_ids,j_min);
    st_i32(out_ids,i,b);
    st_i32(out_ids,j_min,a);
    st_i32(swaps,n_swaps*2 as i64,i);
    st_i32(swaps,n_swaps*2 as i64+1 as i64,j_min);
    n_swaps=n_swaps+1 as i64;
   }
   i=i+1 as i64;
  }
 }
 // inverse map: output_ids[out_ids[i]] = i
 var k:i64=0;
 while k<inv_len{
  st_i32(inv,k,-1 as i64);
  k=k+1 as i64;
 }
 var t:i64=0;
 while t<n_outputs{
  let idx:i64=ld_i32(out_ids,t);
  ensure(idx>=0 as i64&&idx<inv_len,2501);
  st_i32(inv,idx,t);
  t=t+1 as i64;
 }
 n_swaps
}

fn main(){
 reorder(0 as i64,0 as u64,0 as u64,0 as u64,1 as i64);
 reorder(3 as i64,8 as u64,20 as u64,32 as u64,8 as i64);
 reorder(4 as i64,64 as u64,80 as u64,96 as u64,8 as i64);
}
