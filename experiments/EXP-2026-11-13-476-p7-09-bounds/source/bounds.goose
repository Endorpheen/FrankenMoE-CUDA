// JSON-schema-to-GBNF integer bounds and const/enum rules in Goose, from
// common/json-schema-to-grammar.cpp (build_min_max_int, digit_range,
// more_digits, uniform_range, _generate_constant_rule).
//
// Owned decisions:
//   - the digit-range and digit-count helpers;
//   - uniform_range's shared-prefix / split / range-merge construction;
//   - build_min_max_int's branch table for both bounds, only a minimum and only
//     a maximum, including negative ranges and the 16-digit depth budget;
//   - a const rule is format_literal(value.dump());
//   - an enum rule is "(" + values joined by " | " + ")".
//
// The JSON dumping itself and the schema walk stay native.
import std;

const I64_MAX:i64 = 9223372036854775807;
const I64_MIN:i64 = -9223372036854775807 - 1;
const I32_MAX:i64 = 2147483647;

fn append_s(dst:u8[:], at:i64, s:const u8[:], off:i64, len:i64)->i64 {
 var k:i64=0;
 while k<len {
  if at+k<dst.len { dst[at+k]=s[off+k]; }
  k=k+1;
 }
 return at+len;
}

fn append_c(dst:u8[:], at:i64, c:u8)->i64 {
 if at<dst.len { dst[at]=c; }
 return at+1;
}

fn append_dec(dst:u8[:], at:i64, v:i64)->i64 {
 var p:i64=at;
 var x:i64=v;
 if x<0 { p=append_c(dst,p,'-'); x=0-x; }
 var digits:i64=1;
 var y:i64=x;
 while y>=10 { digits=digits+1; y=y/10; }
 var pos:i64=p+digits-1;
 y=x;
 while y>0 {
  if pos<dst.len { dst[pos]=(48 as u8)+((y%10) as u8); }
  pos=pos-1;
  y=y/10;
 }
 if x==0 { if p<dst.len { dst[p]='0'; } }
 return p+digits;
}

fn eq_bytes(a:const u8[:], b:const u8[:])->bool {
 if a.len!=b.len { return false; }
 var i:i64=0;
 while i<a.len {
  if a[i]!=b[i] { return false; }
  i=i+1;
 }
 return true;
}

// decimal of v (non-negative) appended to buf, returned as a slice region
fn dec_into(buf:u8[:], ast:i64[:], v:i64)->i64,i64 {
 let at=ast[0];
 let end=append_dec(buf,at,v);
 ast[0]=end;
 return at,end-at;
}

fn rep_into(buf:u8[:], ast:i64[:], c:u8, n:i64)->i64,i64 {
 let at=ast[0];
 var p:i64=at;
 var k:i64=0;
 while k<n {
  p=append_c(buf,p,c);
  k=k+1;
 }
 ast[0]=p;
 return at,p-at;
}

fn digit_range_at(out:u8[:], p:i64, from:u8, to:u8)->i64 {
 var q=append_c(out,p,'[');
 if from==to {
  q=append_c(out,q,from);
 } else {
  q=append_c(out,q,from);
  q=append_c(out,q,'-');
  q=append_c(out,q,to);
 }
 return append_c(out,q,']');
}

fn more_digits_at(out:u8[:], p:i64, mn:i64, mx:i64)->i64 {
 var q=append_s(out,p,"[0-9]",0,5);
 if mn==mx && mn==1 { return q; }
 q=append_c(out,q,'{');
 q=append_dec(out,q,mn);
 if mx!=mn {
  q=append_c(out,q,',');
  if mx!=I32_MAX { q=append_dec(out,q,mx); }
 }
 return append_c(out,q,'}');
}

// uniform_range; from and to live in buf (or a caller buffer), out is separate.
recursive fn uniform_at(out:u8[:], p:i64, from:const u8[:], to:const u8[:], buf:u8[:], ast:i64[:])->i64 {
 var i:i64=0;
 while i<from.len && i<to.len && from[i]==to[i] { i=i+1; }
 var q:i64=p;
 if i>0 {
  q=append_c(out,q,'"');
  q=append_s(out,q,from,0,i);
  q=append_c(out,q,'"');
 }
 if i<from.len && i<to.len {
  if i>0 { q=append_c(out,q,' '); }
  let sub_len=from.len-i-1;
  if sub_len>0 {
   let from_sub=from[i+1..from.len];
   let to_sub=to[i+1..to.len];
   let zo,zl=rep_into(buf,ast,'0',sub_len);
   let no,nl=rep_into(buf,ast,'9',sub_len);
   let zeros=buf[zo..zo+zl];
   let nines=buf[no..no+nl];
   var to_reached:bool=false;
   q=append_c(out,q,'(');
   if eq_bytes(from_sub,zeros) {
    q=digit_range_at(out,q,from[i],to[i]-1);
    q=append_c(out,q,' ');
    q=more_digits_at(out,q,sub_len,sub_len);
   } else {
    q=append_c(out,q,'[');
    q=append_c(out,q,from[i]);
    q=append_c(out,q,']');
    q=append_c(out,q,' ');
    q=append_c(out,q,'(');
    q=uniform_at(out,q,from_sub,nines,buf,ast);
    q=append_c(out,q,')');
    if from[i]<to[i]-1 {
     q=append_s(out,q," | ",0,3);
     if eq_bytes(to_sub,nines) {
      q=digit_range_at(out,q,from[i]+1,to[i]);
      to_reached=true;
     } else {
      q=digit_range_at(out,q,from[i]+1,to[i]-1);
     }
     q=append_c(out,q,' ');
     q=more_digits_at(out,q,sub_len,sub_len);
    }
   }
   if !to_reached {
    q=append_s(out,q," | ",0,3);
    q=digit_range_at(out,q,to[i],to[i]);
    q=append_c(out,q,' ');
    q=uniform_at(out,q,zeros,to_sub,buf,ast);
   }
   q=append_c(out,q,')');
  } else {
   q=append_c(out,q,'[');
   q=append_c(out,q,from[i]);
   q=append_c(out,q,'-');
   q=append_c(out,q,to[i]);
   q=append_c(out,q,']');
  }
 }
 return q;
}

fn parse_digits(s:const u8[:])->i64 {
 var v:i64=0;
 var i:i64=0;
 while i<s.len {
  let c=s[i];
  if c>='0' && c<='9' {
   v=v*10+((c-48) as i64);
  }
  i=i+1;
 }
 return v;
}

recursive fn build_mm_at(out:u8[:], p:i64, min_v:i64, max_v:i64, decimals_left:i64, top_level:i64,
                         buf:u8[:], ast:i64[:])->i64 {
 let has_min = min_v!=I64_MIN;
 let has_max = max_v!=I64_MAX;
 if !has_min && !has_max { return -1; }
 var q:i64=p;

 if has_min && has_max {
  var lo:i64=min_v;
  let hi:i64=max_v;
  if lo<0 && hi<0 {
   q=append_s(out,q,"\"-\" (",0,5);
   q=build_mm_at(out,q,0-hi,0-lo,decimals_left,1,buf,ast);
   if q<0 { return -1; }
   return append_c(out,q,')');
  }
  if lo<0 {
   q=append_s(out,q,"\"-\" (",0,5);
   q=build_mm_at(out,q,0,0-lo,decimals_left,1,buf,ast);
   if q<0 { return -1; }
   q=append_s(out,q,") | ",0,4);
   lo=0;
  }
  let mo0,ml0=dec_into(buf,ast,lo);
  var min_s_off:i64=mo0;
  var min_s_len:i64=ml0;
  let xo0,xl0=dec_into(buf,ast,hi);
  let max_s_off:i64=xo0;
  let max_s_len:i64=xl0;
  var min_digits:i64=min_s_len;
  var max_digits:i64=max_s_len;
  var digits:i64=min_digits;
  while digits<max_digits {
   let nz,nl=rep_into(buf,ast,'9',digits);
   q=uniform_at(out,q,buf[min_s_off..min_s_off+min_s_len],buf[nz..nz+nl],buf,ast);
   // min_s = "1" + zeros(digits)
   let oo,ol=rep_into(buf,ast,'0',digits);
   let one_at=ast[0];
   var op=append_c(buf,one_at,'1');
   op=append_s(buf,op,buf,oo,ol);
   ast[0]=op;
   min_s_off=one_at;
   min_s_len=op-one_at;
   q=append_s(out,q," | ",0,3);
   digits=digits+1;
  }
  q=uniform_at(out,q,buf[min_s_off..min_s_off+min_s_len],buf[max_s_off..max_s_off+max_s_len],buf,ast);
  return q;
 }

 var less:i64=decimals_left-1;
 if less<1 { less=1; }

 if has_min {
  let lo:i64=min_v;
  if lo<0 {
   q=append_s(out,q,"\"-\" (",0,5);
   q=build_mm_at(out,q,I64_MIN,0-lo,decimals_left,0,buf,ast);
   if q<0 { return -1; }
   q=append_s(out,q,") | [0] | [1-9] ",0,16);
   return more_digits_at(out,q,0,decimals_left-1);
  }
  if lo==0 {
   if top_level!=0 {
    q=append_s(out,q,"[0] | [1-9] ",0,12);
    return more_digits_at(out,q,0,less);
   }
   return more_digits_at(out,q,1,decimals_left);
  }
  if lo<=9 {
   let c=(48 as u8)+(lo as u8);
   var start:u8=(48 as u8);
   if top_level!=0 { start='1'; }
   if c>start {
    q=digit_range_at(out,q,start,c-1);
    q=append_c(out,q,' ');
    q=more_digits_at(out,q,1,less);
    q=append_s(out,q," | ",0,3);
   }
   q=digit_range_at(out,q,c,'9');
   q=append_c(out,q,' ');
   return more_digits_at(out,q,0,less);
  }
  let mo,ml=dec_into(buf,ast,lo);
  let ms=buf[mo..mo+ml];
  let c=ms[0];
  if c>'1' {
   var start:u8=(48 as u8);
   if top_level!=0 { start='1'; }
   q=digit_range_at(out,q,start,c-1);
   q=append_c(out,q,' ');
   q=more_digits_at(out,q,ml,less);
   q=append_s(out,q," | ",0,3);
  }
  q=digit_range_at(out,q,c,c);
  q=append_s(out,q," (",0,2);
  let rest=parse_digits(ms[1..ml]);
  q=build_mm_at(out,q,rest,I64_MAX,less,0,buf,ast);
  if q<0 { return -1; }
  q=append_c(out,q,')');
  if c<'9' {
   q=append_s(out,q," | ",0,3);
   q=digit_range_at(out,q,c+1,'9');
   q=append_c(out,q,' ');
   q=more_digits_at(out,q,ml-1,less);
  }
  return q;
 }

 if has_max {
  if max_v>=0 {
   if top_level!=0 {
    q=append_s(out,q,"\"-\" [1-9] ",0,10);
    q=more_digits_at(out,q,0,less);
    q=append_s(out,q," | ",0,3);
   }
   q=build_mm_at(out,q,0,max_v,decimals_left,1,buf,ast);
   return q;
  }
  q=append_s(out,q,"\"-\" (",0,5);
  q=build_mm_at(out,q,0-max_v,I64_MAX,decimals_left,0,buf,ast);
  if q<0 { return -1; }
  return append_c(out,q,')');
 }

 return q;
}

// KIND_INTEGER rule body: "(" + build_min_max_int + ")".
export fn int_rule(min_v:i64, max_v:i64, out:u8[:], buf:u8[:])->i64 {
 var ast: i64[1] = [0];
 var p=append_c(out,0,'(');
 p=build_mm_at(out,p,min_v,max_v,16,1,buf,ast);
 if p<0 { return -1; }
 return append_c(out,p,')');
}

// format_literal for a JSON-encoded constant.
export fn const_rule(dump:const u8[:], out:u8[:])->i64 {
 return format_literal_at(dump,out,0);
}

fn format_literal_at(dump:const u8[:], out:u8[:], at:i64)->i64 {
 var p=append_c(out,at,'"');
 var i:i64=0;
 while i<dump.len {
  let c=dump[i];
  if c==(13 as u8) {
   p=append_c(out,p,'\\');
   p=append_c(out,p,'r');
  } else if c==(10 as u8) {
   p=append_c(out,p,'\\');
   p=append_c(out,p,'n');
  } else if c=='"' {
   p=append_c(out,p,'\\');
   p=append_c(out,p,'"');
  } else if c=='\\' {
   p=append_c(out,p,'\\');
   p=append_c(out,p,'\\');
  } else {
   p=append_c(out,p,c);
  }
  i=i+1;
 }
 return append_c(out,p,'"');
}

// Enum body: "(" + format_literal(dump_i) joined by " | " + ")".
export fn enum_rule(strs:const u8[:], dir:const i64[:], n:i64, out:u8[:])->i64 {
 var p=append_c(out,0,'(');
 var i:i64=0;
 while i<n {
  if i>0 { p=append_s(out,p," | ",0,3); }
  let off=dir[2*i+0];
  let len=dir[2*i+1];
  p=format_literal_at(strs[off..off+len],out,p);
  i=i+1;
 }
 return append_c(out,p,')');
}
