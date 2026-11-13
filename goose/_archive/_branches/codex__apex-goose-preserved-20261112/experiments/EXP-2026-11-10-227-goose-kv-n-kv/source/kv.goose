// llama_kv_cache::get_n_kv in Goose (APX: P5).
//
// Native (llama-kv-cache.cpp:1285):
//     const uint32_t n_pad_cur = std::max(n_pad, 256u);
//     for each stream s:
//         result = max(min(cells[s].size(), max(n_pad_cur, GGML_PAD(cells[s].used_max_p1(), n_pad_cur))), result)
//
// Two things about this that a rewrite must not lose:
//   * the padding floor is 256, not n_pad -- a graph must keep the same n_kv across batches so it can
//     be reused, and a small n_pad would defeat that;
//   * the result is the MAX over streams, and each stream contributes a value CLAMPED to its own cell
//     count. A stream whose cells are all free contributes 0 and cannot raise the result.
//
// GGML_PAD(x,n) = ((x + n - 1) / n) * n, with n == n_pad_cur, which is never zero here because of the
// 256 floor.

extern "fg227n_fail" fn fail(code:i64);
extern "fg227n_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn maxi(a:i64,b:i64)->i64{ if a<b{return b;} return a; }
fn mini(a:i64,b:i64)->i64{ if a<b{return a;} return b; }

// pad_up rounds x up to a multiple of n. n is at least 256 here.
fn pad_up(x:i64,n:i64)->i64{
 ensure(n>0,600);
 if x<=0 as i64{return 0 as i64;}
 return ((x+n-1 as i64)/(n))*(n);
}

// n_kv: sizes and used_max_p1 hold one entry per stream. Returns the padded maximum n_kv.
fn n_kv(n_streams:i64,sizes:u64,used_max_p1:u64,n_pad:i64)->i64{
 ensure(n_streams>=0,601);
 ensure(n_pad>=0,602);
 let n_pad_cur:i64=maxi(n_pad,256 as i64);
 var result:i64=0;
 var s:i64=0;
 while s<n_streams{
  let csize:i64=ld_i32(sizes,s);
  let used:i64=ld_i32(used_max_p1,s);
  ensure(csize>=0,603);
  ensure(used>=0,604);
  let padded:i64=pad_up(used,n_pad_cur);
  let inner:i64=maxi(n_pad_cur,padded);
  result=maxi(mini(csize,inner),result);
  s=s+1 as i64;
 }
 result
}

fn main(){
 n_kv(0 as i64,0 as u64,0 as u64,0 as i64);
 n_kv(1 as i64,8 as u64,16 as u64,32 as i64);
 n_kv(2 as i64,24 as u64,32 as u64,512 as i64);
}
