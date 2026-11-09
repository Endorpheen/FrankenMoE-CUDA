// Model-loader decisions in Goose (APX: model loader).
//
// From llama-model-loader.cpp:881-928. This is the check that decides whether a tensor found in
// the GGUF matches the shape the model asked for, and it is exactly what caught a wrong
// is_recr value in EXP200 ("blk.1.attn_qkv.weight has wrong shape").

extern "fg201l_fail" fn fail(code:i64);
extern "fg201l_ld_ne" fn ld_ne(desc:u64,i:i64)->i64;
extern "fg201l_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// 4 as i64 is written as a literal everywhere: a `const` lives in gg_globals, which the emitter
// initialises inside gg_program -- and a bridge that calls a generated function directly never
// runs that, so a const reads as zero. Same trap EXP199 hit with W.

// nelements: product of the first `n` dims of a descriptor array.
fn nelements(desc:u64,n:i64)->i64{
 ensure(n>=0&&n<=4 as i64,10);
 var p:i64=1;
 var i:i64=0;
 while i<n{
  let v:i64=ld_ne(desc,i);
  ensure(v>=0,11);
  p=p*v;
  i+=1;
 }
 return p;
}

// dims_ok: llama-model-loader.cpp:907-915, the non-reshape branch.
// A requested dim must match exactly; dims the request does not mention must be 1.
fn dims_ok(desc:u64,nreq:i64,r0:i64,r1:i64,r2:i64,r3:i64)->i64{
 ensure(nreq>=0&&nreq<=4 as i64,20);
 var i:i64=0;
 while i<4 as i64{
  let want:i64=if i==0 as i64{r0}else{if i==1 as i64{r1}else{if i==2 as i64{r2}else{r3}}};
  let have:i64=ld_ne(desc,i);
  let expect:i64=if i<nreq{want}else{1 as i64};
  if have!=expect{return 0 as i64;}
  i+=1;
 }
 return 1 as i64;
}

// shape_ok: the two branches of check_tensor_dims. allow_reshape compares element counts only.
fn shape_ok(allow_reshape:i64,desc:u64,nreq:i64,total_req:i64,
            r0:i64,r1:i64,r2:i64,r3:i64)->i64{
 ensure(total_req>=0,30);
 if allow_reshape!=0{
  if nelements(desc,4 as i64)==total_req{return 1 as i64;}
  return 0 as i64;
 }
 return dims_ok(desc,nreq,r0,r1,r2,r3);
}


// get_key_or_arr and load-mode decisions, from llama-model-loader.cpp:455-560.

// or_arr_too_many: line 470, `n > N_MAX`.
fn or_arr_too_many(n:i64,n_max:i64)->i64{
 ensure(n>=0&&n_max>=0,40);
 if n>n_max{return 1 as i64;}
 return 0 as i64;
}

// is_array_kv: line 474, `gguf_get_kv_type(...) == GGUF_TYPE_ARRAY`. The enum value is passed in.
fn is_array_kv(kv_type:i64,array_type:i64)->i64{
 if kv_type==array_type{return 1 as i64;}
 return 0 as i64;
}

// arr_length_ok: line 479, `n == arr_info.length`.
fn arr_length_ok(n:i64,length:i64)->i64{
 ensure(n>=0&&length>=0,41);
 if n==length{return 1 as i64;}
 return 0 as i64;
}

// use_mmap: line 558, `MMAP || MMAP_MLOCK || AUTO`.
//   AUTO = -1, MMAP = 1, MMAP_MLOCK = 3
fn use_mmap_mode(load_mode:i64)->i64{
 if load_mode==(0 as i64)-1 as i64{return 1 as i64;}
 if load_mode==1 as i64{return 1 as i64;}
 if load_mode==3 as i64{return 1 as i64;}
 return 0 as i64;
}

// use_direct_io: line 559, `load_mode == DIRECT_IO`, DIRECT_IO = 4.
fn use_direct_io_mode(load_mode:i64)->i64{
 if load_mode==4 as i64{return 1 as i64;}
 return 0 as i64;
}


// mmap range arithmetic, from llama-model-loader.cpp:1455-1508.
//
// merge_ranges produces a sorted, non-overlapping cover; sub_ranges subtracts one cover from
// another. Both are pure integer interval logic, and an error there means a mapped range that is
// wrong by a byte -- which shows up as a corrupted weight rather than a clear failure.

// merge_touches: line 1462, `range.first <= res.back().last` -- merge into the previous range.
// Ranges are half-open [first, last), and the comparison is `<=`, so adjacent ranges merge.
fn merge_touches(prev_last:i64,cur_first:i64)->i64{
 ensure(prev_last>=0&&cur_first>=0,50);
 if cur_first<=prev_last{return 1 as i64;}
 return 0 as i64;
}

// merged_last: line 1463, `max(prev.last, cur.last)`.
fn merged_last(prev_last:i64,cur_last:i64)->i64{
 if prev_last>cur_last{return prev_last;}
 return cur_last;
}

// sub_skip: line 1481, `y.last <= cur || y.first >= x.last` -- this subtraction does not apply.
fn sub_skip(y_first:i64,y_last:i64,cur:i64,x_last:i64)->i64{
 ensure(y_first>=0&&y_last>=0&&cur>=0&&x_last>=0,51);
 if y_last<=cur{return 1 as i64;}
 if y_first>=x_last{return 1 as i64;}
 return 0 as i64;
}

// sub_emit_gap: line 1484, `y.first > cur` -- emit [cur, y.first) before advancing.
fn sub_emit_gap(cur:i64,y_first:i64)->i64{
 if y_first>cur{return 1 as i64;}
 return 0 as i64;
}

// sub_advance: line 1486, `cur = max(cur, y.last)`.
fn sub_advance(cur:i64,y_last:i64)->i64{
 if cur>y_last{return cur;}
 return y_last;
}

// sub_emit_tail: line 1489, `cur < x.last` -- the remainder of the range survives.
fn sub_emit_tail(cur:i64,x_last:i64)->i64{
 if cur<x_last{return 1 as i64;}
 return 0 as i64;
}


// Full interval algorithms over flat arrays of (first,last) int64 pairs.
//   src: n pairs, assumed sorted by first (the caller sorts, as native does)
//   dst: capacity-cap pairs; the return value is the number of pairs written.
// The caller owns both buffers, so no allocation happens in Goose.

fn st_pair(dst:u64,k:i64,a:i64,b:i64){
 st_i64(dst,k*16 as i64,a);
 st_i64(dst,k*16 as i64+8 as i64,b);
}
fn ld_first(src:u64,k:i64)->i64{return ld_ne(src,k*2 as i64);}
fn ld_last(src:u64,k:i64)->i64{return ld_ne(src,k*2 as i64+1 as i64);}

// merge_ranges: llama-model-loader.cpp:1455-1471. Input already sorted by first.
fn merge_ranges(src:u64,n:i64,dst:u64,cap:i64)->i64{
 ensure(n>=0&&cap>=0,60);
 var k:i64=0;
 var i:i64=0;
 while i<n{
  let f:i64=ld_first(src,i);
  let l:i64=ld_last(src,i);
  ensure(f<=l,61);
  if k>0 as i64{
   let pl:i64=ld_last(dst,k-1 as i64);
   if merge_touches(pl,f)!=0{
    st_pair(dst,k-1 as i64,ld_first(dst,k-1 as i64),merged_last(pl,l));
    i+=1;
    continue;
   }
  }
  ensure(k<cap,62);
  st_pair(dst,k,f,l);
  k+=1;
  i+=1;
 }
 return k;
}

// sub_ranges: lines 1476-1495. Subtracts `sub` (sorted, non-overlapping) from `a`.
fn sub_ranges(a:u64,na:i64,sub:u64,ns:i64,dst:u64,cap:i64)->i64{
 ensure(na>=0&&ns>=0&&cap>=0,70);
 var k:i64=0;
 var x:i64=0;
 while x<na{
  let xf:i64=ld_first(a,x);
  let xl:i64=ld_last(a,x);
  var cur:i64=xf;
  var y:i64=0;
  while y<ns{
   let yf:i64=ld_first(sub,y);
   let yl:i64=ld_last(sub,y);
   if sub_skip(yf,yl,cur,xl)==0 as i64{
    if sub_emit_gap(cur,yf)!=0{
     ensure(k<cap,71);
     st_pair(dst,k,cur,yf);
     k+=1;
    }
    cur=sub_advance(cur,yl);
   }
   y+=1;
  }
  if sub_emit_tail(cur,xl)!=0{
   ensure(k<cap,72);
   st_pair(dst,k,cur,xl);
   k+=1;
  }
  x+=1;
 }
 return k;
}


// Stable insertion sort over (first,last) pairs, by first. llama-model-loader.cpp:1483 used
// std::sort for this and it had to stay in C++ while Goose had no ordering pass; with bounded
// arrays the ordering is Goose's too. Equal keys keep their input order, which matches the
// std::sort tie behaviour the loader relies on only by accident -- recorded, not assumed.
fn sort_pairs(base:u64,n:i64)->i64{
 ensure(n>=0,80);
 var i:i64=1;
 while i<n{
  let kf:i64=ld_ne(base,i*2 as i64);
  let kl:i64=ld_ne(base,i*2 as i64+1 as i64);
  var j:i64=i-1 as i64;
  while j>=0 as i64{
   let pf:i64=ld_ne(base,j*2 as i64);
   if pf<=kf{
    break;
   }
   st_i64(base,(j+1 as i64)*16 as i64,pf);
   st_i64(base,(j+1 as i64)*16 as i64+8 as i64,ld_ne(base,j*2 as i64+1 as i64));
   j-=1;
  }
  st_i64(base,(j+1 as i64)*16 as i64,kf);
  st_i64(base,(j+1 as i64)*16 as i64+8 as i64,kl);
  i+=1;
 }
 return n;
}

fn main(){
 nelements(0 as u64,1 as i64);
 dims_ok(0 as u64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64);
 shape_ok(0 as i64,0 as u64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64,1 as i64);
 or_arr_too_many(0 as i64,0 as i64);
 is_array_kv(0 as i64,0 as i64);
 arr_length_ok(0 as i64,0 as i64);
 use_mmap_mode(0 as i64);
 use_direct_io_mode(0 as i64);
 merge_touches(0 as i64,0 as i64);
 merged_last(0 as i64,0 as i64);
 sub_skip(0 as i64,0 as i64,0 as i64,0 as i64);
 sub_emit_gap(0 as i64,0 as i64);
 sub_advance(0 as i64,0 as i64);
 sub_emit_tail(0 as i64,0 as i64);
 merge_ranges(0 as u64,0 as i64,0 as u64,0 as i64);
 sub_ranges(0 as u64,0 as i64,0 as u64,0 as i64,0 as u64,0 as i64);
 sort_pairs(0 as u64,0 as i64);
 sort_pairs(16 as u64,1 as i64);
}
