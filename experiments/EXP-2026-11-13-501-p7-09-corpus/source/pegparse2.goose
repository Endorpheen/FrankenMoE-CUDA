// PEG matching engine, second slice: from common/peg-parser.cpp parser_executor
// for and, not, atomic, chars, string, until, ac and gbnf.
//
// Owned decisions:
//   - lookahead: And returns the child's type at the start position (no input
//     consumed); Not fails on success, propagates NEED_MORE, succeeds otherwise;
//   - Atomic is the child's result (node clearing is AST-only);
//   - Chars walks codepoints over the encoded ranges with the native
//     INCOMPLETE/INVALID handling and the minimum/lenient rules;
//   - String scans to the delimiter with the native escape table (delimiter,
//     backslash, /, b, f, n, r, t, \uXXXX);
//   - Until returns everything before the first delimiter start (complete or a
//     prefix that reaches the end of input), consuming invalid UTF-8;
//   - Ac and Gbnf are the child's result.
//
// The ranges themselves are parsed by the builder and passed in as native data.
import std;

const R_FAIL:i64=0;
const R_OK:i64=1;
const R_NEED:i64=2;

const K_LIT:i64=3;
const K_SEQ:i64=4;
const K_CHOICE:i64=5;
const K_REP:i64=6;
const K_AND:i64=7;
const K_NOT:i64=8;
const K_ANY:i64=9;
const K_SPACE:i64=10;
const K_CHARS:i64=11;
const K_STRING:i64=12;
const K_UNTIL:i64=13;
const K_ATOMIC:i64=16;
const K_GBNF:i64=18;
const K_AC:i64=19;

fn is_space_byte(c:u8)->bool {
 if c==' ' || c=='\t' || c=='\n' || c=='\r' { return true; }
 if c==(11 as u8) || c==(12 as u8) { return true; }
 return false;
}
fn is_hex(c:u8)->bool {
 if c>='0' && c<='9' { return true; }
 if c>='a' && c<='f' { return true; }
 if c>='A' && c<='F' { return true; }
 return false;
}

fn utf8_kind(input:const u8[:], off:i64)->i64,i64 {
 if off>=input.len { return 1,0; }
 let b0=input[off];
 if b0<128 { return 0,1; }
 if b0<192 { return 2,1; }
 if b0<224 {
  if off+1>=input.len { return 1,1; }
  if (input[off+1] & 192)!=128 { return 2,1; }
  return 0,2;
 }
 if b0<240 {
  var i:i64=1;
  while i<3 {
   if off+i>=input.len { return 1,i; }
   if (input[off+i] & 192)!=128 { return 2,i; }
   i=i+1;
  }
  return 0,3;
 }
 if b0<248 {
  var i:i64=1;
  while i<4 {
   if off+i>=input.len { return 1,i; }
   if (input[off+i] & 192)!=128 { return 2,i; }
   i=i+1;
  }
  return 0,4;
 }
 return 2,1;
}

fn utf8_cp(input:const u8[:], off:i64)->i64 {
 let b0=input[off];
 if b0<128 { return b0; }
 if b0<224 { return ((b0-192) as i64)*64+((input[off+1]-128) as i64); }
 if b0<240 { return ((b0-224) as i64)*4096+((input[off+1]-128) as i64)*64+((input[off+2]-128) as i64); }
 return ((b0-240) as i64)*262144+((input[off+1]-128) as i64)*4096+((input[off+2]-128) as i64)*64+((input[off+3]-128) as i64);
}

// string escape at [pos] (backslash). start is the parser's start position.
fn escape_step(input:const u8[:], pos0:i64, start:i64, lenient:i64, delim:u8)->i64,i64 {
 var pos:i64=pos0+1;
 if pos>=input.len {
  if lenient==0 { return R_FAIL,start; }
  return R_NEED,pos0;
 }
 let c=input[pos];
 if c==delim || c=='\\' || c=='/' || c=='b' || c=='f' || c=='n' || c=='r' || c=='t' {
  return R_OK,pos+1;
 }
 if c=='u' {
  pos=pos+1;
  var i:i64=0;
  while i<4 {
   if pos>=input.len {
    if lenient==0 { return R_FAIL,start; }
    return R_NEED,pos;
   }
   if !is_hex(input[pos]) { return R_FAIL,start; }
   pos=pos+1;
   i=i+1;
  }
  return R_OK,pos;
 }
 return R_FAIL,start;
}

recursive fn parse_node(id:i64, start:i64, lenient:i64, input:const u8[:],
                        kind:const i64[:], cfirst:const i64[:], ccount:const i64[:], children:const i64[:],
                        sidx:const i64[:], mn:const i64[:], mx:const i64[:], strs:const u8[:], sdir:const i64[:],
                        rstart:const i64[:], rcount:const i64[:], rngs:const i64[:], rnge:const i64[:],
                        neg:const i64[:], dstart:const i64[:], dcount:const i64[:], delims:const i64[:])->i64,i64,i64 {
 let k=kind[id];
 if k==K_LIT {
  let si=sidx[id];
  let off=sdir[2*si+0];
  let lit_len=sdir[2*si+1];
  var pos:i64=start;
  var i:i64=0;
  while i<lit_len {
   if pos>=input.len {
    if lenient==0 { return R_FAIL,start,start; }
    return R_NEED,start,pos;
   }
   if input[pos]!=strs[off+i] { return R_FAIL,start,start; }
   pos=pos+1;
   i=i+1;
  }
  return R_OK,start,pos;
 }
 if k==K_SEQ {
  var pos:i64=start;
  let base=cfirst[id];
  let n=ccount[id];
  var i:i64=0;
  while i<n {
   let t,s,e=parse_node(children[base+i],pos,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
   if t==R_FAIL { return R_FAIL,start,e; }
   if t==R_NEED { return R_NEED,start,e; }
   pos=e;
   i=i+1;
  }
  return R_OK,start,pos;
 }
 if k==K_CHOICE {
  let base=cfirst[id];
  let n=ccount[id];
  var i:i64=0;
  while i<n {
   let t,s,e=parse_node(children[base+i],start,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
   if t!=R_FAIL { return t,s,e; }
   i=i+1;
  }
  return R_FAIL,start,start;
 }
 if k==K_REP {
  let child=children[cfirst[id]];
  let mxv=mx[id];
  let mnv=mn[id];
  var pos:i64=start;
  var count:i64=0;
  var looping:bool=true;
  while looping {
   if mxv!=-1 && count>=mxv { looping=false; } else {
    if pos>=input.len { looping=false; } else {
     let t,s,e=parse_node(child,pos,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
     if t==R_OK {
      if e==pos { looping=false; } else { pos=e; count=count+1; }
     } else if t==R_NEED {
      return R_NEED,start,e;
     } else {
      looping=false;
     }
    }
   }
  }
  if mnv>0 && count<mnv {
   if pos>=input.len && lenient!=0 { return R_NEED,start,pos; }
   return R_FAIL,start,pos;
  }
  return R_OK,start,pos;
 }
 if k==K_ANY {
  let uk,ub=utf8_kind(input,start);
  if uk==1 {
   if lenient==0 { return R_FAIL,start,start; }
   return R_NEED,start,start;
  }
  if uk==2 { return R_FAIL,start,start; }
  return R_OK,start,ub+start;
 }
 if k==K_SPACE {
  var pos:i64=start;
  while pos<input.len && is_space_byte(input[pos]) { pos=pos+1; }
  return R_OK,start,pos;
 }
 if k==K_AND {
  let t,s,e=parse_node(children[cfirst[id]],start,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
  return t,start,start;
 }
 if k==K_NOT {
  let t,s,e=parse_node(children[cfirst[id]],start,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
  if t==R_OK { return R_FAIL,start,start; }
  if t==R_NEED { return R_NEED,start,start; }
  return R_OK,start,start;
 }
 if k==K_ATOMIC || k==K_GBNF || k==K_AC {
  return parse_node(children[cfirst[id]],start,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
 }
 if k==K_CHARS {
  var pos:i64=start;
  var count:i64=0;
  var stop:bool=false;
  while !stop {
   if mx[id]!=-1 && count>=mx[id] { stop=true; } else {
    let uk,ub=utf8_kind(input,pos);
    if uk==1 {
     if count>=mn[id] { stop=true; } else if lenient!=0 { return R_NEED,start,pos; } else { return R_FAIL,start,start; }
    } else if uk==2 {
     if count>=mn[id] { stop=true; } else { return R_FAIL,start,start; }
    } else {
     let cp=utf8_cp(input,pos);
     var m:bool=false;
     let base=rstart[id];
     let nr=rcount[id];
     var r:i64=0;
     while r<nr {
      if cp>=rngs[base+r] && cp<=rnge[base+r] { m=true; }
      r=r+1;
     }
     if neg[id]!=0 { m=!m; }
     if m { pos=pos+ub; count=count+1; } else { stop=true; }
    }
   }
  }
  if count<mn[id] {
   if pos>=input.len && lenient!=0 { return R_NEED,start,pos; }
   return R_FAIL,start,pos;
  }
  return R_OK,start,pos;
 }
 if k==K_STRING {
  let si=sidx[id];
  let off=sdir[2*si+0];
  let dl=sdir[2*si+1];
  var delim:u8=0;
  if dl>0 { delim=strs[off]; }
  var pos:i64=start;
  var stop:bool=false;
  while pos<input.len && !stop {
   let c=input[pos];
   if c==delim { return R_OK,start,pos; }
   if c=='\\' {
    let t,e=escape_step(input,pos,start,lenient,delim);
    if t!=R_OK { return t,start,e; }
    pos=e;
   } else {
    let uk,ub=utf8_kind(input,pos);
    if uk==1 {
     if lenient==0 { return R_FAIL,start,start; }
     return R_NEED,start,pos;
    }
    if uk==2 { return R_FAIL,start,start; }
    pos=pos+ub;
   }
  }
  if lenient==0 { return R_FAIL,start,pos; }
  return R_NEED,start,pos;
 }
 if k==K_UNTIL {
  var pos:i64=start;
  var last:i64=start;
  var stop:bool=false;
  while pos<input.len && !stop {
   let uk,ub=utf8_kind(input,pos);
   if uk==1 && lenient!=0 { return R_NEED,start,last; }
   if uk!=0 {
    var step:i64=ub;
    if step<1 { step=1; }
    pos=pos+step;
    last=pos;
   } else {
    let base=dstart[id];
    let n=dcount[id];
    var hit:bool=false;
    var r:i64=0;
    while r<n {
     let di=delims[base+r];
     let d_off=sdir[2*di+0];
     let d_len=sdir[2*di+1];
     var z:i64=0;
     var ok:bool=true;
     while z<d_len && pos+z<input.len {
      if input[pos+z]!=strs[d_off+z] { ok=false; z=d_len; } else { z=z+1; }
     }
     if ok { hit=true; }
     r=r+1;
    }
    if hit { return R_OK,start,pos; }
    pos=pos+ub;
    last=pos;
   }
  }
  if last==input.len && lenient!=0 { return R_NEED,start,last; }
  return R_OK,start,last;
 }
 return R_FAIL,start,start;
}

export fn peg_parse2(root:i64, input:const u8[:], start:i64, lenient:i64,
                     kind:const i64[:], cfirst:const i64[:], ccount:const i64[:], children:const i64[:],
                     sidx:const i64[:], mn:const i64[:], mx:const i64[:], strs:const u8[:], sdir:const i64[:],
                     rstart:const i64[:], rcount:const i64[:], rngs:const i64[:], rnge:const i64[:], neg:const i64[:],
                     dstart:const i64[:], dcount:const i64[:], delims:const i64[:],
                     out:i64[:])->i64 {
 let t,s,e=parse_node(root,start,lenient,input,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,rstart,rcount,rngs,rnge,neg,dstart,dcount,delims);
 out[0]=t;
 out[1]=s;
 out[2]=e;
 return t;
}
