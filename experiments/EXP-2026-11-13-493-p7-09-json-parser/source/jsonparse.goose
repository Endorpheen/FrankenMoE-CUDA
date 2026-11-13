// JSON text parser in Goose, from the behaviour of common_json::parse
// (nlohmann ordered_json): the RFC 8259 grammar with the library's number
// classification, string escape rules, duplicate-key policy and error
// classes. The tree is emitted flat; the JSON container type stays native.
//
// Node kinds: 0 null, 1 bool, 2 integer (i64), 3 unsigned above i64, 4 float,
// 5 string, 6 array, 7 object.
//
// Error classes: 101 parse_error, 406 out_of_range (number overflow). The line
// and column follow the library's get_position(): 1-based line, and a column
// counted from the first byte of that line (0 on a line that starts at the
// offending byte).
import std;
import math;

const K_NULL:i64=0;
const K_BOOL:i64=1;
const K_INT:i64=2;
const K_UINT:i64=3;
const K_FLOAT:i64=4;
const K_STRING:i64=5;
const K_ARRAY:i64=6;
const K_OBJECT:i64=7;

const E_PARSE:i64=101;
const E_RANGE:i64=406;

// strtod, through the C library: the one primitive the parser borrows
extern "gs_json_strtod" fn libc_strtod(s:const u8[:])->f64;

fn app(dst:u8[:], at:i64, s:const u8[:], off:i64, len:i64)->i64 {
 var k:i64=0;
 while k<len {
  if at+k<dst.len { dst[at+k]=s[off+k]; }
  k=k+1;
 }
 return at+len;
}
fn appc(dst:u8[:], at:i64, c:u8)->i64 {
 if at<dst.len { dst[at]=c; }
 return at+1;
}
fn put_utf8(dst:u8[:], at:i64, cp:i64)->i64 {
 var p:i64=at;
 if cp<128 { return appc(dst,p,cp as u8); }
 if cp<2048 {
  p=appc(dst,p,(192+(cp/64)) as u8);
  return appc(dst,p,(128+(cp%64)) as u8);
 }
 if cp<65536 {
  p=appc(dst,p,(224+(cp/4096)) as u8);
  p=appc(dst,p,(128+((cp/64)%64)) as u8);
  return appc(dst,p,(128+(cp%64)) as u8);
 }
 p=appc(dst,p,(240+(cp/262144)) as u8);
 p=appc(dst,p,(128+((cp/4096)%64)) as u8);
 p=appc(dst,p,(128+((cp/64)%64)) as u8);
 return appc(dst,p,(128+(cp%64)) as u8);
}
fn hexv(c:u8)->i64 {
 if c>='0' && c<='9' { return (c-48) as i64; }
 if c>='a' && c<='f' { return (c-87) as i64; }
 if c>='A' && c<='F' { return (c-55) as i64; }
 return -1;
}
fn is_ws(c:u8)->bool {
 if c==(32 as u8) || c==(9 as u8) || c==(10 as u8) || c==(13 as u8) { return true; }
 return false;
}
fn is_dig(c:u8)->bool { return c>='0' && c<='9'; }

// state: 0 nodes, 1 err code, 2 err line, 3 err col, 4 pos, 5 entries,
//        6 (unused), 7 depth, 8 str off, 9 str len, 10 sbuf cursor, 11 block cursor
// consumed = bytes read, the way the library counts them: one per get(), so an
// end-of-input error is one past the last byte
fn err_cons(text:const u8[:], st:i64[:], code:i64, consumed:i64)->i64 {
 var line:i64=1;
 var ls:i64=0;
 var i:i64=0;
 let lim:i64 = if consumed<text.len { consumed } else { text.len };
 while i<lim {
  if text[i]==(10 as u8) { line=line+1; ls=i+1; }
  i=i+1;
 }
 st[1]=code;
 st[2]=line;
 st[3]=consumed-ls;
 return -1;
}
// the byte at the cursor is at fault: the library has already consumed it
fn err_next(text:const u8[:], st:i64[:], code:i64)->i64 {
 return err_cons(text,st,code,st[4]+1);
}
fn err_eof(text:const u8[:], st:i64[:], code:i64)->i64 {
 return err_cons(text,st,code,text.len+1);
}
fn skip_ws(text:const u8[:], st:i64[:])->i64 {
 while st[4]<text.len && is_ws(text[st[4]]) { st[4]=st[4]+1; }
 return st[4];
}

fn new_node(jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:], jf:i64[:], jn:i64[:], jh:i64[:], st:i64[:])->i64 {
 let id=st[0];
 st[0]=id+1;
 jk[id]=-1; jv[id]=0; jd[id]=0.0; js[id]=0; jl[id]=0; jf[id]=0; jn[id]=0; jh[id]=-1;
 return id;
}

// prepend an entry to node's list
fn add_entry(node:i64, val:i64, koff:i64, klen:i64,
             e_val:i64[:], e_koff:i64[:], e_klen:i64[:], e_next:i64[:], jh:i64[:], st:i64[:])->i64 {
 let ei=st[5];
 st[5]=ei+1;
 e_val[ei]=val;
 e_koff[ei]=koff;
 e_klen[ei]=klen;
 e_next[ei]=jh[node];
 jh[node]=ei;
 return ei;
}

// replace the value of an existing key, keeping its position
fn set_entry(node:i64, val:i64, koff:i64, klen:i64, sbuf:const u8[:],
             e_val:i64[:], e_koff:i64[:], e_klen:i64[:], e_next:i64[:], jh:i64[:], st:i64[:])->i64 {
 let key=sbuf[koff..koff+klen];
 var e:i64=jh[node];
 while e>=0 {
  if e_klen[e]==key.len {
   if eqb(sbuf[e_koff[e]..e_koff[e]+e_klen[e]],key) {
    e_val[e]=val;
    return e;
   }
  }
  e=e_next[e];
 }
 return add_entry(node,val,koff,klen,e_val,e_koff,e_klen,e_next,jh,st);
}
fn eqb(a:const u8[:], b:const u8[:])->bool {
 if a.len!=b.len { return false; }
 var i:i64=0;
 while i<a.len {
  if a[i]!=b[i] { return false; }
  i=i+1;
 }
 return true;
}

recursive fn parse_value(depth:i64, text:const u8[:], jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:],
                         jf:i64[:], jn:i64[:], jh:i64[:], e_val:i64[:], e_koff:i64[:], e_klen:i64[:],
                         e_next:i64[:], sbuf:u8[:], jkey:i64[:], jkl:i64[:], jchild:i64[:], st:i64[:])->i64 {
 skip_ws(text,st);
 let p=st[4];
 if p>=text.len { return err_eof(text,st,E_PARSE); }
 let c=text[p];
 if c=='{' { return parse_object(depth+1,text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,jkey,jkl,jchild,st); }
 if c=='[' { return parse_array(depth+1,text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,jkey,jkl,jchild,st); }
 if c=='"' {
  let off,len=parse_string(text,sbuf,st);
  if st[1]!=0 { return -1; }
  let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
  jk[id]=K_STRING; js[id]=off; jl[id]=len;
  return id;
 }
 if c=='t' {
  let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
  jk[id]=K_BOOL; jv[id]=1;
  st[4]=p+1;
  if !lit_rest(text,st,"rue") { return -1; }
  return id;
 }
 if c=='f' {
  let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
  jk[id]=K_BOOL; jv[id]=0;
  st[4]=p+1;
  if !lit_rest(text,st,"alse") { return -1; }
  return id;
 }
 if c=='n' {
  let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
  jk[id]=K_NULL;
  st[4]=p+1;
  if !lit_rest(text,st,"ull") { return -1; }
  return id;
 }
 if c=='-' || is_dig(c) {
  return parse_number(text,jk,jv,jd,js,jl,jf,jn,jh,st);
 }
 return err_next(text,st,E_PARSE);
}

fn lit_rest(text:const u8[:], st:i64[:], rest:const u8[:])->bool {
 var i:i64=0;
 while i<rest.len {
  let p=st[4];
  if p>=text.len { err_eof(text,st,E_PARSE); return false; }
  if text[p]!=rest[i] { err_next(text,st,E_PARSE); return false; }
  st[4]=p+1;
  i=i+1;
 }
 // the next byte must not continue a literal
 let p=st[4];
 if p<text.len {
  let c=text[p];
  if (c>='a' && c<='z') || (c>='A' && c<='Z') || is_dig(c) {
   err_next(text,st,E_PARSE);
   return false;
  }
 }
 return true;
}

recursive fn parse_object(depth:i64, text:const u8[:], jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:],
                          jf:i64[:], jn:i64[:], jh:i64[:], e_val:i64[:], e_koff:i64[:], e_klen:i64[:],
                          e_next:i64[:], sbuf:u8[:], jkey:i64[:], jkl:i64[:], jchild:i64[:], st:i64[:])->i64 {
 let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
 jk[id]=K_OBJECT;
 st[4]=st[4]+1; // '{'
 skip_ws(text,st);
 if st[4]<text.len && text[st[4]]=='}' {
  st[4]=st[4]+1;
  return id;
 }
 var looping:bool=true;
 while looping {
  skip_ws(text,st);
  if st[4]>=text.len { return err_eof(text,st,E_PARSE); }
  if text[st[4]]!='"' { return err_next(text,st,E_PARSE); }
  let koff,klen=parse_string(text,sbuf,st);
  if st[1]!=0 { return -1; }
  skip_ws(text,st);
  if st[4]>=text.len { return err_eof(text,st,E_PARSE); }
  if text[st[4]]!=':' { return err_next(text,st,E_PARSE); }
  st[4]=st[4]+1;
  let child=parse_value(depth,text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,jkey,jkl,jchild,st);
  if st[1]!=0 { return -1; }
  set_entry(id,child,koff,klen,sbuf,e_val,e_koff,e_klen,e_next,jh,st);
  skip_ws(text,st);
  if st[4]>=text.len { return err_eof(text,st,E_PARSE); }
  let c=text[st[4]];
  if c=='}' { st[4]=st[4]+1; looping=false; }
  else if c==',' { st[4]=st[4]+1; }
  else { skip_token(text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,st); return err_cons(text,st,E_PARSE,st[4]); }
 }
 return id;
}

recursive fn parse_array(depth:i64, text:const u8[:], jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:],
                         jf:i64[:], jn:i64[:], jh:i64[:], e_val:i64[:], e_koff:i64[:], e_klen:i64[:],
                         e_next:i64[:], sbuf:u8[:], jkey:i64[:], jkl:i64[:], jchild:i64[:], st:i64[:])->i64 {
 let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
 jk[id]=K_ARRAY;
 st[4]=st[4]+1; // '['
 skip_ws(text,st);
 if st[4]<text.len && text[st[4]]==']' {
  st[4]=st[4]+1;
  return id;
 }
 var looping:bool=true;
 while looping {
  let child=parse_value(depth,text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,jkey,jkl,jchild,st);
  if st[1]!=0 { return -1; }
  add_entry(id,child,0,0,e_val,e_koff,e_klen,e_next,jh,st);
  skip_ws(text,st);
  if st[4]>=text.len { return err_eof(text,st,E_PARSE); }
  let c=text[st[4]];
  if c==']' { st[4]=st[4]+1; looping=false; }
  else if c==',' { st[4]=st[4]+1; }
  else { skip_token(text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,st); return err_cons(text,st,E_PARSE,st[4]); }
 }
 return id;
}

// string body; leaves (off,len) in st[8], st[9]
fn parse_string(text:const u8[:], sbuf:u8[:], st:i64[:])->i64,i64 {
 let at=st[10];
 var p:i64=st[4]+1; // past '"'
 var end:i64=-1;
 while p<text.len {
  let c=text[p];
  if c=='"' { end=p; p=p+1; }
  else if c=='\\' {
   p=p+1;
   if p>=text.len { return err_eof(text,st,E_PARSE),0; }
   let e=text[p];
   if e=='"' || e=='\\' || e=='/' { st[10]=appc(sbuf,st[10],e); p=p+1; }
   else if e=='b' { st[10]=appc(sbuf,st[10],8 as u8); p=p+1; }
   else if e=='f' { st[10]=appc(sbuf,st[10],12 as u8); p=p+1; }
   else if e=='n' { st[10]=appc(sbuf,st[10],10 as u8); p=p+1; }
   else if e=='r' { st[10]=appc(sbuf,st[10],13 as u8); p=p+1; }
   else if e=='t' { st[10]=appc(sbuf,st[10],9 as u8); p=p+1; }
   else if e=='u' {
    let cp,np=hex4(text,p+1,st);
    if st[1]!=0 { return -1,0; }
    p=np;
    if cp>=0xD800 && cp<=0xDBFF {
     if p<text.len && text[p]=='\\' {
      if p+1>=text.len { return err_eof(text,st,E_PARSE),0; }
      if text[p+1]!='u' { return err_cons(text,st,E_PARSE,p+2),0; }
      let lo,np2=hex4(text,p+2,st);
      if st[1]!=0 { return -1,0; }
      if !(lo>=0xDC00 && lo<=0xDFFF) { return err_cons(text,st,E_PARSE,np2),0; }
      let full=65536+(cp-0xD800)*1024+(lo-0xDC00);
      st[10]=put_utf8(sbuf,st[10],full);
      p=np2;
     } else if p>=text.len {
      return err_eof(text,st,E_PARSE),0;
     } else {
      return err_cons(text,st,E_PARSE,p+1),0;
     }
    } else if cp>=0xDC00 && cp<=0xDFFF {
     return err_cons(text,st,E_PARSE,p),0;
    } else {
     st[10]=put_utf8(sbuf,st[10],cp);
    }
   } else {
    return err_cons(text,st,E_PARSE,p+1),0;
   }
  } else if c<0x20 {
   // the control byte is consumed before it is rejected
   p=p+1;
   return err_cons(text,st,E_PARSE,p),0;
  } else {
   let uk,ub=utf8_at(text,p);
   if uk!=0 {
    return err_cons(text,st,E_PARSE,p+ub),0;
   }
   st[10]=app(sbuf,st[10],text,p,ub);
   p=p+ub;
  }
  if end>=0 { break; }
 }
 if end<0 { return err_eof(text,st,E_PARSE),0; }
 let off=at;
 let len=st[10]-at;
 st[8]=off;
 st[9]=len;
 st[4]=end+1;
 return off,len;
}

fn hex4(text:const u8[:], p0:i64, st:i64[:])->i64,i64 {
 var p:i64=p0;
 var v:i64=0;
 var i:i64=0;
 while i<4 {
  if p>=text.len { return err_eof(text,st,E_PARSE),p; }
  let d=hexv(text[p]);
  if d<0 { return err_cons(text,st,E_PARSE,p+1),p; }
  v=v*16+d;
  p=p+1;
  i=i+1;
 }
 return v,p;
}

// 0 valid, 1 incomplete, 2 invalid
// 0 valid, 1 incomplete, 2 invalid; the count is bytes up to and including the
// byte that decided it
fn utf8_at(text:const u8[:], p:i64)->i64,i64 {
 let b0=text[p];
 if b0<128 { return 0,1; }
 if b0<194 { return 2,1; }
 if b0<224 {
  if p+1>=text.len { return 1,text.len-p; }
  if (text[p+1] & 192)!=128 { return 2,2; }
  return 0,2;
 }
 if b0<240 {
  var i:i64=1;
  while i<3 {
   if p+i>=text.len { return 1,i; }
   if (text[p+i] & 192)!=128 { return 2,i+1; }
   i=i+1;
  }
  return 0,3;
 }
 if b0<245 {
  var i:i64=1;
  while i<4 {
   if p+i>=text.len { return 1,i; }
   if (text[p+i] & 192)!=128 { return 2,i+1; }
   i=i+1;
  }
  return 0,4;
 }
 return 2,1;
}

fn parse_number(text:const u8[:], jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:],
                jf:i64[:], jn:i64[:], jh:i64[:], st:i64[:])->i64 {
 let start=st[4];
 var p:i64=start;
 var neg:bool=false;
 if text[p]=='-' { neg=true; p=p+1; }
 if p>=text.len { return err_eof(text,st,E_PARSE); }
 // integer part
 var ov:bool=false;
 var mag:u64=0;
 let first=text[p];
 if first=='0' {
  p=p+1;
 } else if is_dig(first) {
  while p<text.len && is_dig(text[p]) {
   let d=(text[p]-48) as u64;
   if mag>1844674407370955161 { ov=true; }
   else if mag==1844674407370955161 && d>5 { ov=true; }
   else if !ov { mag=mag*10+d; }
   p=p+1;
  }
 } else {
  // a bare '-' is an invalid number at the byte that must follow
  return err_cons(text,st,E_PARSE,p+1);
 }
 var isflt:bool=ov;
 if p<text.len && text[p]=='.' {
  p=p+1;
  if p>=text.len || !is_dig(text[p]) { return err_cons(text,st,E_PARSE,p+1); }
  while p<text.len && is_dig(text[p]) { p=p+1; }
  isflt=true;
 }
 if p<text.len && (text[p]=='e' || text[p]=='E') {
  p=p+1;
  if p<text.len && (text[p]=='+' || text[p]=='-') { p=p+1; }
  if p>=text.len || !is_dig(text[p]) { return err_cons(text,st,E_PARSE,p+1); }
  while p<text.len && is_dig(text[p]) { p=p+1; }
  isflt=true;
 }
 st[4]=p;
 let id=new_node(jk,jv,jd,js,jl,jf,jn,jh,st);
 if !isflt {
  if neg {
   // -2^63 is the smallest signed value
   if mag<=9223372036854775808 {
    if mag==9223372036854775808 {
     jk[id]=K_INT; jv[id]=-9223372036854775807-1;
    } else {
     jk[id]=K_INT; jv[id]=0-(mag as i64);
    }
    return id;
   }
   isflt=true;
  } else {
   if mag<=9223372036854775807 {
    jk[id]=K_INT; jv[id]=mag as i64;
    return id;
   }
   jk[id]=K_UINT; jv[id]=mag as i64;
   return id;
  }
 }
 let tok=text[start..p];
 let d=libc_strtod(tok);
 if is_inf(d) || is_nan(d) {
  return err_cons(text,st,E_RANGE,st[4]);
 }
 jk[id]=K_FLOAT; jd[id]=d;
 return id;
}

// advance past one token without validating it: the library reads the next
// token before it decides that a ',' or ']' / '}' was expected
fn skip_token(text:const u8[:], jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:],
              jf:i64[:], jn:i64[:], jh:i64[:], e_val:i64[:], e_koff:i64[:], e_klen:i64[:],
              e_next:i64[:], sbuf:u8[:], st:i64[:])->i64 {
 skip_ws(text,st);
 let p=st[4];
 if p>=text.len { return st[4]; }
 let c=text[p];
 if c=='"' {
  // skip the raw string, escapes included
  var q:i64=p+1;
  var stop:bool=false;
  while q<text.len && !stop {
   if text[q]=='\\' { q=q+2; }
   else if text[q]=='"' { q=q+1; stop=true; }
   else { q=q+1; }
  }
  st[4]=q;
  return st[4];
 }
 if is_dig(c) || c=='-' {
  var q:i64=p;
  while q<text.len && (is_dig(text[q]) || text[q]=='-' || text[q]=='+' || text[q]=='.' || text[q]=='e' || text[q]=='E') { q=q+1; }
  st[4]=q;
  return st[4];
 }
 if (c>='a' && c<='z') || (c>='A' && c<='Z') {
  var q:i64=p;
  while q<text.len && (((text[q]>='a') && (text[q]<='z')) || ((text[q]>='A') && (text[q]<='Z'))) { q=q+1; }
  st[4]=q;
  return st[4];
 }
 st[4]=p+1;
 return st[4];
}

// materialise the child blocks: reverse each list, then copy it in document order
export fn json_parse(text:const u8[:], jk:i64[:], jv:i64[:], jd:f64[:], js:i64[:], jl:i64[:],
                     jf:i64[:], jn:i64[:], jh:i64[:], e_val:i64[:], e_koff:i64[:], e_klen:i64[:],
                     e_next:i64[:], sbuf:u8[:], jkey:i64[:], jkl:i64[:], jchild:i64[:], st:i64[:])->i64 {
 st[0]=0; st[1]=0; st[2]=0; st[3]=0; st[4]=0; st[5]=0; st[7]=0; st[8]=0; st[9]=0; st[10]=0; st[11]=0;
 let root=parse_value(0,text,jk,jv,jd,js,jl,jf,jn,jh,e_val,e_koff,e_klen,e_next,sbuf,jkey,jkl,jchild,st);
 if st[1]!=0 { return -1; }
 // trailing garbage is an error
 skip_ws(text,st);
 if st[4]<text.len {
  err_next(text,st,E_PARSE);
  return -1;
 }
 var i:i64=0;
 while i<st[0] {
  let k=jk[i];
  if k==K_ARRAY || k==K_OBJECT {
   // reverse
   var prev:i64=-1;
   var e:i64=jh[i];
   while e>=0 {
    let nx=e_next[e];
    e_next[e]=prev;
    prev=e;
    e=nx;
   }
   jh[i]=prev;
   // copy in order
   jf[i]=st[11];
   var cnt:i64=0;
   e=prev;
   while e>=0 {
    jchild[st[11]]=e_val[e];
    jkey[st[11]]=e_koff[e];
    jkl[st[11]]=e_klen[e];
    st[11]=st[11]+1;
    cnt=cnt+1;
    e=e_next[e];
   }
   jn[i]=cnt;
  }
  i=i+1;
 }
 return root;
}
