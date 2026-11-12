// llama_kv_cache::set_input_k_shift in Goose (APX: P5).
//
// Native (llama-kv-cache.cpp:1544):
//     for (s = 0; s < n_stream; ++s)
//         for (i = 0; i < cells[s].size(); ++i)
//             data[s*cells[s].size() + i] = cells[s].is_empty(i) ? 0 : cells[s].get_shift(i);
//
// This feeds the K-shift input when the context is shifted, so a wrong value silently corrupts positions
// rather than failing. The two things worth gating: an EMPTY cell writes 0 and never its stale shift
// value, and each stream's row length is that stream's own size, so a stream with fewer cells does not
// borrow the neighbour's row width.
//
// The bridge cannot hand over the cell objects, so it pre-reads shift[i] and emptiness into two flat
// arrays, one per stream, and passes per-stream sizes separately.

extern "fg230s_fail" fn fail(code:i64);
extern "fg230s_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg230s_ld_u64" fn ld_u64(base:u64,idx:i64)->i64;
extern "fg230s_st_i32" fn st_i32(base:u64,idx:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// fill_shift writes sum(size[s]) int32 values into out, laid out stream-major with per-stream widths.
fn fill_shift(n_stream:i64,sizes:u64,shifts:u64,empties:u64,out:u64)->i64{
 ensure(n_stream>=0,900);
 var s:i64=0;
 var row:i64=0;
 var n:i64=0;
 while s<n_stream{
  let sz:i64=ld_i32(sizes,s);
  ensure(sz>=0,901);
  let sh:u64=ld_u64(shifts,s) as u64;
  let em:u64=ld_u64(empties,s) as u64;
  var i:i64=0;
  while i<sz{
   var v:i64=0;
   if ld_i32(em,i)==0 as i64{
    v=ld_i32(sh,i);
   }
   st_i32(out,row+i,v);
   n=n+1 as i64;
   i=i+1 as i64;
  }
  row=row+sz;
  s=s+1 as i64;
 }
 n
}

fn main(){
 fill_shift(0 as i64,0 as u64,0 as u64,0 as u64,0 as u64);
 fill_shift(1 as i64,8 as u64,16 as u64,24 as u64,0 as u64);
 fill_shift(2 as i64,32 as u64,40 as u64,48 as u64,32 as u64);
}
