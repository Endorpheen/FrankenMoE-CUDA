// JSON text rendering in Goose, from nlohmann's serializer as reached through
// common_json::dump / dump_safe.
//
// Owned decisions:
//   - compact form (indent < 0) and pretty form with a step of `indent` spaces;
//   - string escaping: '"' and '\' are escaped, 0x08 0x09 0x0A 0x0C 0x0D get
//     their short forms, every other byte below 0x20 becomes \u00xx (lowercase);
//     everything else is copied verbatim;
//   - integers print in decimal, unsigned above i64 in unsigned decimal;
//   - doubles print as the shortest decimal that reads back to the same value,
//     laid out like the library's format_buffer: fixed when the decimal point
//     position n satisfies k <= n <= 15 or 0 < n <= 15, "0.000digits" when
//     -4 < n <= 0, otherwise d[.igits]e±XX with a two-digit exponent;
//   - dump_safe replaces ill-formed UTF-8 runs with U+FFFD instead of failing.
//
// The decimal digits of a double come from the C library's correctly rounded
// "%.*e", one primitive at the formatter's boundary.
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

extern "gs_json_strtod" fn libc_strtod(s:const u8[:])->f64;
extern "gs_json_fmt_e" fn libc_fmt_e(out:u8[:], prec:i64, x:f64)->i64;

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
fn app_dec(dst:u8[:], at:i64, v:i64)->i64 {
 var p:i64=at;
 var neg:bool=v<0;
 var m:u64=0;
 if v<0 { m=((0-(v+1)) as u64)+1; } else { m=v as u64; }
 if neg { p=appc(dst,p,'-'); }
 return app_u64(dst,p,m);
}
fn app_u64(dst:u8[:], at:i64, v:u64)->i64 {
 if v==0 { return appc(dst,at,'0'); }
 var tmp: u8[32] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var n:i64=0;
 var x:u64=v;
 while x>0 {
  let d=(x%10) as u8;
  tmp[n]=(48 as u8)+d;
  n=n+1;
  x=x/10;
 }
 var p:i64=at;
 var i:i64=n-1;
 while i>=0 {
  p=appc(dst,p,tmp[i]);
  i=i-1;
 }
 return p;
}

// ---- strings ---------------------------------------------------------------
fn hexlow(out:u8[:], p:i64, v:i64)->i64 {
 let d: const u8[:] = "0123456789abcdef";
 var q=appc(out,p,92);
 q=appc(out,q,'u');
 q=appc(out,q,'0');
 q=appc(out,q,'0');
 q=appc(out,q,d[(v/16)%16]);
 return appc(out,q,d[v%16]);
}

fn esc_string(out:u8[:], at:i64, sbuf:const u8[:], off:i64, len:i64, safe:i64)->i64 {
 var p=appc(out,at,'"');
 var i:i64=0;
 while i<len {
  let c=sbuf[off+i];
  if c=='"' { p=appc(out,p,92); p=appc(out,p,'"'); i=i+1; }
  else if c=='\\' { p=appc(out,p,92); p=appc(out,p,'\\'); i=i+1; }
  else if c==8 { p=appc(out,p,92); p=appc(out,p,'b'); i=i+1; }
  else if c==9 { p=appc(out,p,92); p=appc(out,p,'t'); i=i+1; }
  else if c==10 { p=appc(out,p,92); p=appc(out,p,'n'); i=i+1; }
  else if c==12 { p=appc(out,p,92); p=appc(out,p,'f'); i=i+1; }
  else if c==13 { p=appc(out,p,92); p=appc(out,p,'r'); i=i+1; }
  else if c<0x20 { p=hexlow(out,p,c as i64); i=i+1; }
  else if c<128 { p=appc(out,p,c); i=i+1; }
  else {
   let uk,ub=utf8_len(sbuf,off,i,len);
   if uk==0 {
    p=app(out,p,sbuf,off+i,ub);
    i=i+ub;
   } else if safe!=0 {
    // U+FFFD
    p=appc(out,p,239);
    p=appc(out,p,191);
    p=appc(out,p,189);
    var step:i64=ub;
    if step<1 { step=1; }
    i=i+step;
   } else {
    // strict mode would throw; the caller only reaches this with valid input
    p=appc(out,p,c);
    i=i+1;
   }
  }
 }
 return appc(out,p,'"');
}

// 0 valid, 1 incomplete, 2 invalid; count = bytes up to the deciding byte
fn utf8_len(s:const u8[:], off:i64, i:i64, len:i64)->i64,i64 {
 let b0=s[off+i];
 if b0<128 { return 0,1; }
 if b0<194 { return 2,1; }
 if b0<224 {
  if i+1>=len { return 1,1; }
  if (s[off+i+1] & 192)!=128 { return 2,2; }
  return 0,2;
 }
 if b0<240 {
  var k:i64=1;
  while k<3 {
   if i+k>=len { return 1,k; }
   if (s[off+i+k] & 192)!=128 { return 2,k+1; }
   k=k+1;
  }
  return 0,3;
 }
 if b0<245 {
  var k:i64=1;
  while k<4 {
   if i+k>=len { return 1,k; }
   if (s[off+i+k] & 192)!=128 { return 2,k+1; }
   k=k+1;
  }
  return 0,4;
 }
 return 2,1;
}

// ---- doubles ---------------------------------------------------------------
// shortest round-trip digits; writes the digits into dig and returns (k, n-1)
fn shortest(x:f64, dig:u8[:])->i64,i64,i64 {
 var buf: u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var chosen:i64=0;
 var k:i64=0;
 var e:i64=0;
 var p:i64=1;
 var done:bool=false;
 while p<=17 && !done {
  let n=libc_fmt_e(buf, p-1, x);
  let s=buf[0..n];
  let back=libc_strtod(s);
  if back==x {
   // split mantissa and exponent
   var ei:i64=0;
   var j:i64=0;
   while j<n {
    if buf[j]=='e' { ei=j; j=n; } else { j=j+1; }
   }
   // digits
   var d:i64=0;
   j=0;
   while j<ei {
    if buf[j]!='.' {
     if d<dig.len { dig[d]=buf[j]; }
     d=d+1;
    }
    j=j+1;
   }
   // exponent (with sign)
   var q:i64=ei+1;
   var neg:bool=false;
   if q<n && buf[q]=='-' { neg=true; q=q+1; }
   else if q<n && buf[q]=='+' { q=q+1; }
   var ev:i64=0;
   while q<n {
    ev=ev*10+((buf[q]-48) as i64);
    q=q+1;
   }
   if neg { ev=0-ev; }
   chosen=p;
   k=d;
   e=ev;
   done=true;
  }
  p=p+1;
 }
 return k,e,chosen;
}

fn fmt_double(out:u8[:], at:i64, x:f64)->i64 {
 if x!=x { return app(out,at,"null",0,4); }
 if is_inf(x) { return app(out,at,"null",0,4); }
 var p:i64=at;
 var v:f64=x;
 var neg:bool=false;
 // signbit through the reciprocal: -0.0 gives -inf
 if v==0.0 {
  let r:f64=1.0/v;
  if is_inf(r) && r<0.0 { neg=true; }
 } else if v<0.0 { neg=true; v=0.0-v; }
 if neg { p=appc(out,p,'-'); }
 if v==0.0 { return app(out,p,"0.0",0,3); }
 var dig: u8[32] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 let k,e,ch=shortest(v,dig);
 let n=e+1;
 if k<=n && n<=15 {
  var i:i64=0;
  while i<k { p=appc(out,p,dig[i]); i=i+1; }
  while i<n { p=appc(out,p,'0'); i=i+1; }
  return app(out,p,".0",0,2);
 }
 if 0<n && n<=15 {
  var i:i64=0;
  while i<n { p=appc(out,p,dig[i]); i=i+1; }
  p=appc(out,p,'.');
  while i<k { p=appc(out,p,dig[i]); i=i+1; }
  return p;
 }
 if -4<n && n<=0 {
  p=appc(out,p,'0');
  p=appc(out,p,'.');
  var z:i64=0;
  while z<0-n { p=appc(out,p,'0'); z=z+1; }
  var i:i64=0;
  while i<k { p=appc(out,p,dig[i]); i=i+1; }
  return p;
 }
 p=appc(out,p,dig[0]);
 if k>1 {
  p=appc(out,p,'.');
  var i:i64=1;
  while i<k { p=appc(out,p,dig[i]); i=i+1; }
 }
 p=appc(out,p,'e');
 let ex:i64=n-1;
 if ex<0 { p=appc(out,p,'-'); } else { p=appc(out,p,'+'); }
 var ax:i64=ex;
 if ax<0 { ax=0-ax; }
 if ax<10 { p=appc(out,p,'0'); }
 return app_dec(out,p,ax);
}

// ---- tree ------------------------------------------------------------------
fn indent_to(out:u8[:], at:i64, step:i64, level:i64)->i64 {
 var p:i64=at;
 var n:i64=step*level;
 var i:i64=0;
 while i<n { p=appc(out,p,' '); i=i+1; }
 return p;
}

recursive fn dump_node(id:i64, level:i64, indent:i64,
                       jk:const i64[:], jv:const i64[:], jd:const f64[:], js:const i64[:], jl:const i64[:],
                       jf:const i64[:], jn:const i64[:], jkey:const i64[:], jkl:const i64[:], jchild:const i64[:],
                       sbuf:const u8[:], out:u8[:], at:i64, safe:i64)->i64 {
 var p:i64=at;
 let k=jk[id];
 if k==K_NULL { return app(out,p,"null",0,4); }
 if k==K_BOOL {
  if jv[id]!=0 { return app(out,p,"true",0,4); }
  return app(out,p,"false",0,5);
 }
 if k==K_INT { return app_dec(out,p,jv[id]); }
 if k==K_UINT {
  var bits:u64=0;
  let raw:i64=jv[id];
  bits=raw as u64;
  return app_u64(out,p,bits);
 }
 if k==K_FLOAT { return fmt_double(out,p,jd[id]); }
 if k==K_STRING { return esc_string(out,p,sbuf,js[id],jl[id],safe); }
 let n=jn[id];
 let base=jf[id];
 if n==0 {
  if k==K_ARRAY { return app(out,p,"[]",0,2); }
  return app(out,p,"{}",0,2);
 }
 if k==K_ARRAY {
  p=appc(out,p,'[');
  var i:i64=0;
  while i<n {
   if indent>=0 {
    p=appc(out,p,10);
    p=indent_to(out,p,indent,level+1);
   }
   p=dump_node(jchild[base+i],level+1,indent,jk,jv,jd,js,jl,jf,jn,jkey,jkl,jchild,sbuf,out,p,safe);
   if i+1<n { p=appc(out,p,','); }
   i=i+1;
  }
  if indent>=0 {
   p=appc(out,p,10);
   p=indent_to(out,p,indent,level);
  }
  return appc(out,p,']');
 }
 p=appc(out,p,'{');
 var i2:i64=0;
 while i2<n {
  if indent>=0 {
   p=appc(out,p,10);
   p=indent_to(out,p,indent,level+1);
  }
  p=esc_string(out,p,sbuf,jkey[base+i2],jkl[base+i2],safe);
  p=appc(out,p,':');
  if indent>=0 { p=appc(out,p,' '); }
  p=dump_node(jchild[base+i2],level+1,indent,jk,jv,jd,js,jl,jf,jn,jkey,jkl,jchild,sbuf,out,p,safe);
  if i2+1<n { p=appc(out,p,','); }
  i2=i2+1;
 }
 if indent>=0 {
  p=appc(out,p,10);
  p=indent_to(out,p,indent,level);
 }
 return appc(out,p,'}');
}

export fn json_dump(root:i64, jk:const i64[:], jv:const i64[:], jd:const f64[:], js:const i64[:], jl:const i64[:],
                    jf:const i64[:], jn:const i64[:], jkey:const i64[:], jkl:const i64[:], jchild:const i64[:],
                    sbuf:const u8[:], out:u8[:], indent:i64, safe:i64)->i64 {
 if root<0 { return 0; }
 return dump_node(root,0,indent,jk,jv,jd,js,jl,jf,jn,jkey,jkl,jchild,sbuf,out,0,safe);
}
