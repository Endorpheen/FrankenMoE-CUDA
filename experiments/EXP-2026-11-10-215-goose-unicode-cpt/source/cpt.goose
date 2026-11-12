// UTF-8 codepoint decoding in Goose (APX: P7-05).
//
// From unicode.cpp:30-74 unicode_cpt_from_utf8. This runs whenever text is tokenized or
// detokenized: every non-ASCII character in a prompt or in generated output goes through it, and
// the validation branches are what reject malformed input instead of silently emitting a wrong
// codepoint.
//
// Native raises std::invalid_argument on five distinct conditions; here each becomes a distinct
// trap code so a failure names the branch instead of a generic "invalid character".
//   lead byte says N-byte sequence, continuation byte(s) present and well-formed, and the
//   decoded value must not be an overlong encoding (below the range its length implies) nor
//   exceed U+10FFFF.
//
// The bytes stay in the host buffer; Goose reads them through a byte primitive and returns the
// codepoint, with the consumed length written back so the caller advances exactly as native does.

extern "fg215u_fail" fn fail(code:i64);
extern "fg215u_ld_u8" fn ld_u8(base:u64,byte:i64)->i64;
extern "fg215u_st_i64" fn st_i64(base:u64,byte:i64,v:i64);

fn ensure(v:bool,c:i64){if !v{fail(c);}}

fn is_cont(b:i64)->i64{
 if (b&0xc0 as i64)==0x80 as i64{return 1 as i64;}
 return 0 as i64;
}

// cpt_from_utf8: decodes one codepoint at `off`. Returns the codepoint and writes the consumed
// byte count to adv[0]. Trap codes mirror the native throw sites:
//   701 lead byte is a continuation byte
//   702 truncated 2-byte sequence or bad continuation
//   703 truncated 3-byte sequence or bad continuation
//   704 truncated 4-byte sequence or bad continuation
//   705 value out of U+10FFFF (and the lead-byte 0xF8+ case)
//   706 overlong encoding (below 0x80 / 0x800 / 0x10000 for its length)
fn cpt_from_utf8(buf:u64,size:i64,off:i64,adv:u64)->i64{
 ensure(size>=0&&off>=0&&off<size,700);
 let b0:i64=ld_u8(buf,off);
 if b0&0x80 as i64==0 as i64{
  st_i64(adv,0 as i64,1 as i64);
  return b0;
 }
 if b0&0x40 as i64==0 as i64{ensure(false,701);}
 if b0&0x20 as i64==0 as i64{
  ensure(off+1 as i64<size,702);
  let b1:i64=ld_u8(buf,off+1 as i64);
  ensure(is_cont(b1)==1 as i64,702);
  let r:i64=((b0&0x1f as i64)<<(6 as i64))|(b1&0x3f as i64);
  ensure(r>=0x80 as i64,706);
  st_i64(adv,0 as i64,2 as i64);
  return r;
 }
 if b0&0x10 as i64==0 as i64{
  ensure(off+2 as i64<size,703);
  let b1:i64=ld_u8(buf,off+1 as i64);
  let b2:i64=ld_u8(buf,off+2 as i64);
  ensure(is_cont(b1)==1 as i64,703);
  ensure(is_cont(b2)==1 as i64,703);
  let r:i64=((b0&0x0f as i64)<<(12 as i64))|((b1&0x3f as i64)<<(6 as i64))|(b2&0x3f as i64);
  ensure(r>=0x800 as i64,706);
  st_i64(adv,0 as i64,3 as i64);
  return r;
 }
 if b0&0x08 as i64==0 as i64{
  ensure(off+3 as i64<size,704);
  let b1:i64=ld_u8(buf,off+1 as i64);
  let b2:i64=ld_u8(buf,off+2 as i64);
  let b3:i64=ld_u8(buf,off+3 as i64);
  ensure(is_cont(b1)==1 as i64,704);
  ensure(is_cont(b2)==1 as i64,704);
  ensure(is_cont(b3)==1 as i64,704);
  let r:i64=((b0&0x07 as i64)<<(18 as i64))|((b1&0x3f as i64)<<(12 as i64))|((b2&0x3f as i64)<<(6 as i64))|(b3&0x3f as i64);
  ensure(r>=0x10000 as i64,706);
  ensure(r<=0x10ffff as i64,705);
  st_i64(adv,0 as i64,4 as i64);
  return r;
 }
 ensure(false,705);
 return 0 as i64;
}

fn main(){
 var a:i64=0;
 cpt_from_utf8(0 as u64,0 as i64,0 as i64,0 as u64);
 cpt_from_utf8(8 as u64,1 as i64,0 as i64,16 as u64);
}
