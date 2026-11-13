// KV cell state copy in Goose (APX: P5-01, the "copy" clause).
//
// From llama-kv-cells.h:166-192 (contiguous) and 197-220 (indexed). This is the ubatch rollback
// path: when an ubatch cannot be emplaced the saved cell states are copied back, so a wrong copy
// corrupts a restored context silently.
//
// The arrays are COMPACT, m entries each, not full-cache. The host gathers the m destination
// cells before the call and scatters them back after, so the cost is O(m) and not O(cache). An
// earlier version marshalled the whole cache on every call: it was functionally wrong (it used
// the contiguous form where the call site indexes arbitrarily) and ruinously slow (decode fell
// from 25.7 to 8.3 tok/s). Both are recorded because both were real.
//
// Native `ext` is not modelled: it carries no decision this profile makes.
// Native asserts shift == 0 on every copied cell; that is a trap here, not a silent acceptance.

extern "fg204x_fail" fn fail(code:i64);
extern "fg204x_ld_i64" fn ld_i64(base:u64,byte:i64)->i64;
extern "fg204x_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// copy_cells: for j in [0,m):
//   src cell j -> dst cell j
//   used[j] = 1 when the copied position is not -1, else 0
//   w sequence words are copied verbatim
//   the source shift must be zero
fn copy_cells(dpos:u64,dseq:u64,dused:u64,spos:u64,sseq:u64,sshift:u64,m:i64,w:i64)->i64{
 ensure(m>=0&&w>0,500);
 var j:i64=0;
 while j<m{
  ensure(ld_i64(sshift,j*8 as i64)==0 as i64,501);
  let sp:i64=ld_i64(spos,j*8 as i64);
  st_i64(dpos,j*8 as i64,sp);
  if sp==(0 as i64)-1 as i64{
   st_i64(dused,j*8 as i64,0 as i64);
  }else{
   st_i64(dused,j*8 as i64,1 as i64);
  }
  var k:i64=0;
  while k<w{
   st_i64(dseq,(j*w+k)*8 as i64,ld_i64(sseq,(j*w+k)*8 as i64));
   k+=1;
  }
  j+=1;
 }
 return m;
}

fn main(){
 copy_cells(0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as u64,0 as i64,0 as i64);
 copy_cells(8 as u64,16 as u64,24 as u64,32 as u64,40 as u64,48 as u64,1 as i64,4 as i64);
}
