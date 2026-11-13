// PEG -> GBNF build_grammar in Goose, first slice, from
// common/peg-parser.cpp (to_gbnf, effective_parser, collect_reachable_rules
// and the root/rule emission).
//
// Owned decisions:
//   - literal formatting (gbnf_format_literal);
//   - sequence joins with spaces, skipping empty children and parenthesising
//     sequence/choice children; choice joins with " | " and parenthesises
//     nested choices; repetition suffixes with the native table and
//     parenthesises sequence/choice children;
//   - epsilon/start/end and lookaheads emit nothing; any is "."; space is the
//     "space" rule; rule/ref emit the rule name; tag/atomic unwrap;
//   - reachable rules are collected from the root (through refs), and the root
//     rule is emitted last.
//
// chars/string/until/schema/ac and lazy triggers are outside this slice.
import std;

const K_EPS:i64=0;
const K_START:i64=1;
const K_END:i64=2;
const K_LIT:i64=3;
const K_SEQ:i64=4;
const K_CHOICE:i64=5;
const K_REP:i64=6;
const K_AND:i64=7;
const K_NOT:i64=8;
const K_ANY:i64=9;
const K_SPACE:i64=10;
const K_RULE:i64=14;
const K_REF:i64=15;
const K_ATOMIC:i64=16;
const K_TAG:i64=17;
const K_CHARS:i64=11;
const K_STRING:i64=12;
const K_SCHEMA:i64=20;

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

// gbnf_format_literal into out at `at`
fn fmt_lit(s:const u8[:], out:u8[:], at:i64)->i64 {
 var p=append_c(out,at,'"');
 var i:i64=0;
 while i<s.len {
  let c=s[i];
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

fn eff(id0:i64, kind:const i64[:], children:const i64[:], cfirst:const i64[:])->i64 {
 var id:i64=id0;
 var looping:bool=true;
 while looping {
  let k=kind[id];
  if k==K_TAG || k==K_ATOMIC {
   id=children[cfirst[id]];
  } else {
   looping=false;
  }
 }
 return id;
}

fn is_seq_or_choice(id:i64, kind:const i64[:], children:const i64[:], cfirst:const i64[:])->i64 {
 let e=eff(id,kind,children,cfirst);
 if kind[e]==K_SEQ || kind[e]==K_CHOICE { return 1; }
 return 0;
}
fn is_choice(id:i64, kind:const i64[:], children:const i64[:], cfirst:const i64[:])->i64 {
 let e=eff(id,kind,children,cfirst);
 if kind[e]==K_CHOICE { return 1; }
 return 0;
}

// state: 0 out cursor, 1 rule-entry count, 2 reachability visit marks
recursive fn to_gbnf(id:i64, out:u8[:], st:i64[:], kind:const i64[:], cfirst:const i64[:], ccount:const i64[:],
                    children:const i64[:], sidx:const i64[:], mn:const i64[:], mx:const i64[:],
                    strs:const u8[:], sdir:const i64[:], schema_add:const i64[:],
                    mname:i64[:], mval:i64[:], mlen:i64[:])->i64,i64 {
 let k=kind[id];
 let at=st[0];
 if k==K_LIT {
  let si=sidx[id];
  let off=sdir[2*si+0];
  let len=sdir[2*si+1];
  let e=fmt_lit(strs[off..off+len],out,at);
  st[0]=e;
  return at,e-at;
 }
 if k==K_SEQ {
  let base=cfirst[id];
  let n=ccount[id];
  var offs: i64[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var lens: i64[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var i:i64=0;
  while i<n {
   let co,cl=to_gbnf(children[base+i],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
   offs[i]=co; lens[i]=cl;
   i=i+1;
  }
  let out_at=st[0];
  var p:i64=out_at;
  var have:bool=false;
  i=0;
  while i<n {
   if lens[i]>0 {
    if have { p=append_c(out,p,' '); }
    if is_seq_or_choice(children[base+i],kind,children,cfirst)!=0 {
     p=append_c(out,p,'(');
     p=append_s(out,p,out,offs[i],lens[i]);
     p=append_c(out,p,')');
    } else {
     p=append_s(out,p,out,offs[i],lens[i]);
    }
    have=true;
   }
   i=i+1;
  }
  st[0]=p;
  return out_at,p-out_at;
 }
 if k==K_CHOICE {
  let base=cfirst[id];
  let n=ccount[id];
  var offs: i64[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var lens: i64[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var i:i64=0;
  while i<n {
   let co,cl=to_gbnf(children[base+i],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
   offs[i]=co; lens[i]=cl;
   i=i+1;
  }
  let out_at=st[0];
  var p:i64=out_at;
  i=0;
  while i<n {
   if i>0 { p=append_s(out,p," | ",0,3); }
   if is_choice(children[base+i],kind,children,cfirst)!=0 {
    p=append_c(out,p,'(');
    p=append_s(out,p,out,offs[i],lens[i]);
    p=append_c(out,p,')');
   } else {
    p=append_s(out,p,out,offs[i],lens[i]);
   }
   i=i+1;
  }
  st[0]=p;
  return out_at,p-out_at;
 }
 if k==K_REP {
  let co,cl=to_gbnf(children[cfirst[id]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
  let out_at=st[0];
  var p:i64=out_at;
  if is_seq_or_choice(children[cfirst[id]],kind,children,cfirst)!=0 {
   p=append_c(out,p,'(');
   p=append_s(out,p,out,co,cl);
   p=append_c(out,p,')');
  } else {
   p=append_s(out,p,out,co,cl);
  }
  let lo=mn[id];
  let hi=mx[id];
  if lo==0 && hi==1 { p=append_c(out,p,'?'); }
  else if lo==0 && hi==-1 { p=append_c(out,p,'*'); }
  else if lo==1 && hi==-1 { p=append_c(out,p,'+'); }
  else if hi==-1 {
   p=append_c(out,p,'{');
   p=append_dec(out,p,lo);
   p=append_s(out,p,",}",0,2);
  } else if lo==hi {
   if lo!=1 {
    p=append_c(out,p,'{');
    p=append_dec(out,p,lo);
    p=append_c(out,p,'}');
   }
  } else {
   p=append_c(out,p,'{');
   p=append_dec(out,p,lo);
   p=append_c(out,p,',');
   p=append_dec(out,p,hi);
   p=append_c(out,p,'}');
  }
  st[0]=p;
  return out_at,p-out_at;
 }
 if k==K_ANY {
  st[0]=append_c(out,at,'.');
  return at,1;
 }
 if k==K_SPACE {
  let e=append_s(out,at,"space",0,5);
  st[0]=e;
  return at,e-at;
 }
 if k==K_RULE || k==K_REF {
  let si=sidx[id];
  let off=sdir[2*si+0];
  let len=sdir[2*si+1];
  let e=append_s(out,at,strs,off,len);
  st[0]=e;
  return at,e-at;
 }
 if k==K_TAG || k==K_ATOMIC {
  return to_gbnf(children[cfirst[id]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
 }
 if k==K_CHARS {
  let si=sidx[id];
  let off=sdir[2*si+0];
  let len=sdir[2*si+1];
  var p=append_s(out,at,strs,off,len);
  let lo=mn[id];
  let hi=mx[id];
  if lo==0 && hi==1 { p=append_c(out,p,'?'); }
  else if lo==0 && hi==-1 { p=append_c(out,p,'*'); }
  else if lo==1 && hi==-1 { p=append_c(out,p,'+'); }
  else if hi==-1 {
   p=append_c(out,p,'{');
   p=append_dec(out,p,lo);
   p=append_s(out,p,",}",0,2);
  } else if lo==hi {
   if lo!=1 {
    p=append_c(out,p,'{');
    p=append_dec(out,p,lo);
    p=append_c(out,p,'}');
   }
  } else {
   p=append_c(out,p,'{');
   p=append_dec(out,p,lo);
   p=append_c(out,p,',');
   p=append_dec(out,p,hi);
   p=append_c(out,p,'}');
  }
  st[0]=p;
  return at,p-at;
 }
 if k==K_STRING {
  let si=sidx[id];
  let off=sdir[2*si+0];
  var delim:u8=0;
  if sdir[2*si+1]>0 { delim=strs[off]; }
  var p=append_s(out,at,"( [^",0,4);
  p=append_c(out,p,delim);
  p=append_c(out,p,'\\');
  p=append_c(out,p,'\\');
  p=append_c(out,p,']');
  p=append_s(out,p," | ",0,3);
  p=append_c(out,p,'"');
  p=append_c(out,p,'\\');
  p=append_c(out,p,'\\');
  p=append_c(out,p,'"');
  p=append_s(out,p," ( [",0,4);
  p=append_c(out,p,delim);
  p=append_c(out,p,'\\');
  p=append_c(out,p,'\\');
  p=append_c(out,p,'/');
  p=append_s(out,p," bfnrt] | ",0,10);
  p=append_c(out,p,'"');
  p=append_c(out,p,'u');
  p=append_c(out,p,'"');
  p=append_s(out,p," [0-9a-fA-F]{4} ) )*",0,20);
  st[0]=p;
  return at,p-at;
 }
 if k==K_SCHEMA {
  if schema_add[id]!=0 {
   let si=sidx[id];
   let idx=st[1];
   mname[idx]=si; mval[idx]=at; mlen[idx]=8;
   st[1]=idx+1;
   let e=append_s(out,at,"<schema>",0,8);
   st[0]=e;
   let off=sdir[2*si+0];
   let len=sdir[2*si+1];
   let e2=append_s(out,e,strs,off,len);
   st[0]=e2;
   return e,e2-e;
  }
  return to_gbnf(children[cfirst[id]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
 }
 st[0]=at;
 return at,0;
}

// reachability: marks rule indices reachable from `id`
recursive fn reach(id:i64, marks:i64[:], seen:i64[:], kind:const i64[:], cfirst:const i64[:], ccount:const i64[:],
                   children:const i64[:], sidx:const i64[:], strs:const u8[:], sdir:const i64[:],
                   rname:const i64[:], rid:const i64[:], rn:i64, st:i64[:])->i64 {
 if seen[id]!=0 { return 0; }
 seen[id]=1;
 let k=kind[id];
 if k==K_RULE || k==K_REF {
  let si=sidx[id];
  let off=sdir[2*si+0];
  let len=sdir[2*si+1];
  var i:i64=0;
  while i<rn {
   let ni=rname[i];
   let noff=sdir[2*ni+0];
   let nlen=sdir[2*ni+1];
   if nlen==len {
    if eq_bytes(strs[noff..noff+nlen],strs[off..off+len]) {
     marks[i]=1;
     if k==K_RULE { reach(children[cfirst[id]],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st); }
     else { reach(rid[i],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st); }
     i=rn;
    } else { i=i+1; }
   } else { i=i+1; }
  }
  return 0;
 }
 if k==K_CHARS || k==K_STRING { return 0; }
 if k==K_SCHEMA { return reach(children[cfirst[id]],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st); }
 if k==K_SEQ || k==K_CHOICE {
  let base=cfirst[id];
  let n=ccount[id];
  var i:i64=0;
  while i<n {
   reach(children[base+i],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);
   i=i+1;
  }
  return 0;
 }
 if k==K_REP || k==K_AND || k==K_NOT || k==K_TAG || k==K_ATOMIC {
  return reach(children[cfirst[id]],marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);
 }
 return 0;
}

// Emits each reachable rule then "root". Returns the map entry count.
export fn build_grammar(root:i64, kind:const i64[:], cfirst:const i64[:], ccount:const i64[:], children:const i64[:],
                        sidx:const i64[:], mn:const i64[:], mx:const i64[:], strs:const u8[:], sdir:const i64[:],
                        rname:const i64[:], rid:const i64[:], rn:i64, schema_add:const i64[:],
                        out:u8[:], mname:i64[:], mval:i64[:], mlen:i64[:], st:i64[:], seen:i64[:], marks:i64[:])->i64 {
 st[0]=0;
 st[1]=0;
 var i:i64=0;
 while i<rn { marks[i]=0; i=i+1; }
 var z:i64=0;
 while z<rn { seen[z]=0; z=z+1; }
 reach(root,marks,seen,kind,cfirst,ccount,children,sidx,strs,sdir,rname,rid,rn,st);
 z=0;
 while z<rn {
  if marks[z]!=0 {
   let co,cl=to_gbnf(children[cfirst[rid[z]]],out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
   let idx=st[1];
   mname[idx]=rname[z];
   mval[idx]=co;
   mlen[idx]=cl;
   st[1]=idx+1;
  }
  z=z+1;
 }
 let co,cl=to_gbnf(root,out,st,kind,cfirst,ccount,children,sidx,mn,mx,strs,sdir,schema_add,mname,mval,mlen);
 let idx=st[1];
 mname[idx]=-1; // "root" is emitted by the wrapper
 mval[idx]=co;
 mlen[idx]=cl;
 st[1]=idx+1;
 return st[1];
}
