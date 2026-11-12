// llama_kv_cache::set_input_k_idxs in Goose (APX: P5).
//
// Native (llama-kv-cache.cpp:1496):
//     for (s = 0; s < n_stream; ++s) {
//         const int64_t offs = strm[s] * get_size();
//         for (i = 0; i < size; ++i)
//             data[s*size + i] = offs + idxs[s][i];
//     }
//
// The destination is a graph input tensor whose row order must match the ubatch exactly: the value is
// the CELL INDEX inside the flat KV buffer for that token, offset by the stream's base. Two things the
// gate checks: `offs` uses the stream id times the cache size (not the position), and the write order is
// stream-major, i.e. row s*size+i, not i*n_stream+s.
//
// Per-stream index arrays are not contiguous in the C++ side (each is its own vector), so the bridge
// passes an array of pointers rather than flattening them; flattening would add a copy on a per-batch
// path for no benefit.

extern "fg229i_fail" fn fail(code:i64);
extern "fg229i_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;
extern "fg229i_ld_u32" fn ld_u32(base:u64,idx:i64)->i64;
extern "fg229i_ld_u64" fn ld_u64(base:u64,idx:i64)->i64;
extern "fg229i_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// fill_idxs writes n_stream*size int64 values into out. idx_ptrs holds one pointer per stream.
fn fill_idxs(n_stream:i64,size:i64,strm:u64,idx_ptrs:u64,kv_size:i64,out:u64)->i64{
 ensure(n_stream>=0,800);
 ensure(size>=0,801);
 ensure(kv_size>0,802);
 var s:i64=0;
 var n:i64=0;
 while s<n_stream{
  let sid:i64=ld_i32(strm,s);
  ensure(sid>=0,803);
  let offs:i64=sid*kv_size;
  let src:u64=ld_u64(idx_ptrs,s) as u64;
  var i:i64=0;
  while i<size{
   let idx:i64=ld_u32(src,i);
   ensure(idx>=0,804);
   st_i64(out,(s*size+i)*(8 as i64),offs+idx);
   n=n+1 as i64;
   i=i+1 as i64;
  }
  s=s+1 as i64;
 }
 n
}

fn main(){
 fill_idxs(0 as i64,0 as i64,0 as u64,0 as u64,1 as i64,0 as u64);
 fill_idxs(1 as i64,2 as i64,8 as u64,16 as u64,64 as i64,0 as u64);
 fill_idxs(2 as i64,3 as i64,24 as u64,32 as u64,128 as i64,64 as u64);
}
