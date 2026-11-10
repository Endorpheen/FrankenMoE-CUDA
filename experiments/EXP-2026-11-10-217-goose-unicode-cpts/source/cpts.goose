// Whole-string UTF-8 decoding with error recovery, in Goose (APX: P7-05).
//
// From unicode.cpp unicode_cpts_from_utf8. Six call sites in the pre-tokenizer use it (lines 222,
// 241, 359, 499, 647, 783), so it runs over the entire prompt and over every piece of text the
// tokenizer re-examines.
//
// The behaviour that matters is the recovery policy, and it is not obvious:
//   - on invalid UTF-8, advance by exactly ONE byte (not by the length the lead byte claims)
//     and emit U+FFFD in its place, so a malformed sequence yields one replacement character per
//     bad byte rather than swallowing the rest of the string;
//   - decoding never fails the whole string: native catches std::invalid_argument here and
//     deliberately keeps the exception from escaping llama_tokenize.
//
// Because errors are recovered rather than fatal, the decoder returns -1 instead of trapping. That
// is the only difference from the trapping decoder in EXP215: same ranges, same overlong and
// range checks, same U+10FFFF ceiling.

extern "fg217c_fail" fn fail(code:i64);
extern "fg217c_ld_u8" fn ld_u8(base:u64,byte:i64)->i64;
extern "fg217c_st_i32" fn st_i32(base:u64,idx:i64,v:i64);
extern "fg217c_ld_i32" fn ld_i32(base:u64,idx:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn is_cont(b:i64)->i64{
 if (b&0xc0 as i64)==0x80 as i64{return 1 as i64;}
 return 0 as i64;
}

// try_cpt: codepoint at `off`, or -1 when the bytes there are not valid UTF-8. `adv` receives the
// consumed length only on success; the caller advances one byte on failure.
fn try_cpt(buf:u64,size:i64,off:i64,adv:u64)->i64{
 if off<0 as i64||off>=size{return 0 as i64-1 as i64;}
 let b0:i64=ld_u8(buf,off);
 if b0&0x80 as i64==0 as i64{
  st_i32(adv,0 as i64,1 as i64);
  return b0;
 }
 if b0&0x40 as i64==0 as i64{return 0 as i64-1 as i64;}
 if b0&0x20 as i64==0 as i64{
  if off+1 as i64>=size{return 0 as i64-1 as i64;}
  let b1:i64=ld_u8(buf,off+1 as i64);
  if is_cont(b1)!=1 as i64{return 0 as i64-1 as i64;}
  let r:i64=((b0&0x1f as i64)<<(6 as i64))|(b1&0x3f as i64);
  if r<0x80 as i64{return 0 as i64-1 as i64;}
  st_i32(adv,0 as i64,2 as i64);
  return r;
 }
 if b0&0x10 as i64==0 as i64{
  if off+2 as i64>=size{return 0 as i64-1 as i64;}
  let b1:i64=ld_u8(buf,off+1 as i64);
  let b2:i64=ld_u8(buf,off+2 as i64);
  if is_cont(b1)!=1 as i64||is_cont(b2)!=1 as i64{return 0 as i64-1 as i64;}
  let r:i64=((b0&0x0f as i64)<<(12 as i64))|((b1&0x3f as i64)<<(6 as i64))|(b2&0x3f as i64);
  if r<0x800 as i64{return 0 as i64-1 as i64;}
  st_i32(adv,0 as i64,3 as i64);
  return r;
 }
 if b0&0x08 as i64==0 as i64{
  if off+3 as i64>=size{return 0 as i64-1 as i64;}
  let b1:i64=ld_u8(buf,off+1 as i64);
  let b2:i64=ld_u8(buf,off+2 as i64);
  let b3:i64=ld_u8(buf,off+3 as i64);
  if is_cont(b1)!=1 as i64||is_cont(b2)!=1 as i64||is_cont(b3)!=1 as i64{return 0 as i64-1 as i64;}
  let r:i64=((b0&0x07 as i64)<<(18 as i64))|((b1&0x3f as i64)<<(12 as i64))|((b2&0x3f as i64)<<(6 as i64))|(b3&0x3f as i64);
  if r<0x10000 as i64{return 0 as i64-1 as i64;}
  if r>0x10ffff as i64{return 0 as i64-1 as i64;}
  st_i32(adv,0 as i64,4 as i64);
  return r;
 }
 return 0 as i64-1 as i64;
}

// cpts_from_utf8: decodes the whole buffer, writing codepoints to `out`. Returns how many were
// written; never exceeds `size`, because every iteration consumes at least one byte.
fn cpts_from_utf8(buf:u64,size:i64,out:u64,scratch:u64)->i64{
 ensure(size>=0,200);
 var off:i64=0;
 var n:i64=0;
 while off<size{
  st_i32(scratch,0 as i64,0 as i64);
  let cp:i64=try_cpt(buf,size,off,scratch);
  var adv:i64=0;
  if cp>=0 as i64{adv=ld_i32(scratch,0 as i64);}
  if cp<0 as i64{
   st_i32(out,n,0xfffd as i64);
   off=off+1 as i64;
  } else {
   st_i32(out,n,cp);
   off=off+adv;
  }
  n=n+1 as i64;
 }
 n
}

fn main(){
 cpts_from_utf8(0 as u64,0 as i64,0 as u64,64 as u64);
 cpts_from_utf8(8 as u64,1 as i64,16 as u64,64 as u64);
 cpts_from_utf8(24 as u64,4 as i64,32 as u64,64 as u64);
}
