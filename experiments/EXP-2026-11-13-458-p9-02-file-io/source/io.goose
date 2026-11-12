// Bounded reader decisions in Goose, from ggml/src/gguf.cpp (struct gguf_reader:
// read, read(string), read(vector), seek, read_raw, file_remain).
//
// Owned decisions: whether a read fits, the array/string limits, the seek bounds,
// the chunked transfer length and EOF handling, and the resulting
// (data_offset, nbytes_remain) state. The actual byte copy, the FILE* and the
// callback stay native.
//
// Limits: GGUF_MAX_STRING_LENGTH and GGUF_MAX_ARRAY_ELEMENTS are both 1<<30.
extern "fg458_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn put(out:u64,i:i64,v:i64){ st_i64(out,(i*(8 as i64)) as u64,v); }

const MAX_STRING:i64 = 1073741824 as i64;
const MAX_ARRAY:i64  = 1073741824 as i64;
const I64_MAX:i64    = 9223372036854775807 as i64;

// 1 when a scalar of `size` bytes fits in the remaining bytes.
export fn fits(size:i64,remain:i64)->i64{
 if size<0 as i64 { return 0 as i64; }
 if remain<0 as i64 { return 0 as i64; }
 if size<=remain { return 1 as i64; }
 return 0 as i64;
}

// 0 ok, 1 too many elements, 2 element count overflows, 3 not enough bytes.
export fn array_check(n:i64,elem:i64,remain:i64,out:u64)->i64{
 var st:i64=0 as i64;
 if n<0 as i64 { st=1 as i64; }
 else{
  if n>MAX_ARRAY { st=1 as i64; }
  else{
   if elem<=0 as i64 { st=2 as i64; }
   else{
    if n>(I64_MAX/elem) { st=2 as i64; }
    else{
     if n*elem>remain { st=3 as i64; }
    }
   }
  }
 }
 put(out,0 as i64,st);
 return st;
}

// 0 ok, 1 longer than the string limit, 2 longer than the remaining bytes.
export fn string_check(size:i64,remain:i64,out:u64)->i64{
 var st:i64=0 as i64;
 if size<0 as i64 { st=1 as i64; }
 else{
  if size>MAX_STRING { st=1 as i64; }
  else{
   if size>remain { st=2 as i64; }
  }
 }
 put(out,0 as i64,st);
 return st;
}

// seek: 1 accepted with the new remaining count, 0 rejected.
export fn seek(abs_off:i64,data_offset:i64,remain:i64,out:u64)->i64{
 var ok:i64=0 as i64;
 var new_remain:i64=remain;
 if abs_off>=0 as i64 {
  let end:i64=data_offset+remain;
  if end>=data_offset {
   if abs_off<=end { ok=1 as i64; new_remain=end-abs_off; }
  }
 }
 put(out,0 as i64,ok);
 put(out,1 as i64,new_remain);
 return ok;
}

// Chunked transfer: writes total copied, new data_offset, new remain and the
// reached-EOF flag. Returns the copied byte count.
export fn read_raw(size:i64,max_chunk:i64,data_offset:i64,remain:i64,file_end:i64,out:u64)->i64{
 var total:i64=0 as i64;
 var eof:i64=0 as i64;
 var off:i64=data_offset;
 if size>0 as i64 && max_chunk>0 as i64 {
  var done:i64=0 as i64;
  while done==0 as i64 && total<size {
   let want:i64=size-total;
   var chunk:i64=max_chunk;
   if want<chunk { chunk=want; }
   let pos:i64=data_offset+total;
   if pos<data_offset { done=1 as i64; }else{
    let avail:i64=file_end-pos;
    if avail>=chunk { total=total+chunk; }else{
     if avail>0 as i64 { total=total+avail; }
     eof=1 as i64;
     done=1 as i64;
    }
   }
  }
 }
 var new_remain:i64=remain-total;
 if eof!=0 as i64 { new_remain=0 as i64; }
 if new_remain<0 as i64 { new_remain=0 as i64; }
 off=data_offset+total;
 put(out,0 as i64,total);
 put(out,1 as i64,off);
 put(out,2 as i64,new_remain);
 put(out,3 as i64,eof);
 return total;
}

// Remaining bytes of a file: 0 when either position is unknown.
export fn file_remain(file_end:i64,cur:i64,out:u64)->i64{
 var r:i64=0 as i64;
 if cur>=0 as i64 && file_end>=cur { r=file_end-cur; }
 put(out,0 as i64,r);
 return r;
}

