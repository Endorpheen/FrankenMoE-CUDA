// Detokenization piece-copy decision in Goose (APX: P7-07).
//
// From llama-vocab.cpp:3980-3994, the `_try_copy` lambda inside token_to_piece. Every generated
// token goes through it: strip up to `lstrip` leading spaces, then either copy the remaining bytes
// or report how many would be needed. The negative return is the "buffer too small" signal the
// server uses to retry with a bigger buffer, so getting the sign or the count wrong truncates
// output text.
//
// Native:
//   for (i = 0; i < lstrip && size && *token == ' '; ++i) { token++; size--; }
//   if (length < (int32_t) size) return -(int32_t) size;
//   memcpy(buf, token, size); return (int32_t) size;
//
// The bytes stay in the host buffer; Goose decides the skip count and the result code. A copy
// primitive is imported so the module is testable against the native behaviour byte for byte.

extern "fg213p_fail" fn fail(code:i64);
extern "fg213p_ld_u8" fn ld_u8(base:u64,byte:i64)->i64;
extern "fg213p_st_u8" fn st_u8(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn is_space(b:i64)->i64{
 if b==32 as i64{return 1 as i64;}
 return 0 as i64;
}

// lstrip_count: how many leading spaces to skip, bounded by lstrip and by the piece length.
fn lstrip_count(piece:u64,size:i64,lstrip:i64)->i64{
 ensure(size>=0,10);
 ensure(lstrip>=0,11);
 var i:i64=0;
 while i<lstrip{
  if size-i<=0 as i64{return i;}
  if is_space(ld_u8(piece,i))==0 as i64{return i;}
  i+=1;
 }
 return i;
}

// try_copy: the whole decision. Returns the byte count copied (>=0) or -(needed) on overflow.
//   src: piece bytes, dst: output buffer, avail: dst capacity
//   the return value is exactly native's: -(size after stripping) when it does not fit.
fn try_copy(src:u64,size:i64,lstrip:i64,dst:u64,avail:i64)->i64{
 ensure(size>=0&&lstrip>=0,20);
 let skip:i64=lstrip_count(src,size,lstrip);
 let n:i64=size-skip;
 if avail<n{
  return 0 as i64-n;      // negative = how much room is needed
 }
 var i:i64=0;
 while i<n{
  st_u8(dst,i,ld_u8(src,skip+i));
  i+=1;
 }
 return n;
}

fn main(){
 lstrip_count(0 as u64,0 as i64,0 as i64);
 lstrip_count(8 as u64,1 as i64,2 as i64);
 try_copy(0 as u64,0 as i64,0 as i64,0 as u64,0 as i64);
 try_copy(16 as u64,3 as i64,1 as i64,32 as u64,8 as i64);
}
