// Jinja parser in Goose, from common/jinja/parser.cpp (parse_from_tokens).
//
// Input is the token stream produced by the lexer (EXP502): token type, value
// bytes and position. Output is a flat AST: for every node its kind, position,
// scalar fields (integer, double, string, operator, flags) and up to three
// direct children plus two child lists. The shapes match the native nodes of
// common/jinja/runtime.h, which the fixture dumps for comparison.
//
// Owned decisions:
//   - the statement set (set, if/elif/else, macro, for/else, break, continue,
//     call, filter, generation/endgeneration) and the end-tag expectations;
//   - the expression precedence chain: if/ternary, or, and, not, comparison
//     (with "in"/"not in"), additive, multiplicative, is-tests, filters, unary,
//     call/member, primary;
//   - member and slice forms, argument lists with keyword and spread forms,
//     tuple/array/object literals, adjacent string concatenation;
//   - the two error families: parser_exception text with the source view, and
//     the plain runtime_error messages the native parser throws.
import std;

const T_EOF:i64=0;
const T_TEXT:i64=1;
const T_NUM:i64=2;
const T_STR:i64=3;
const T_ID:i64=4;
const T_EQ:i64=5;
const T_OP:i64=6;
const T_CP:i64=7;
const T_OS:i64=8;
const T_CS:i64=9;
const T_OE:i64=10;
const T_CE:i64=11;
const T_OSQ:i64=12;
const T_CSQ:i64=13;
const T_OCB:i64=14;
const T_CCB:i64=15;
const T_COMMA:i64=16;
const T_DOT:i64=17;
const T_COLON:i64=18;
const T_PIPE:i64=19;
const T_ADD:i64=21;
const T_MUL:i64=22;
const T_CMP:i64=23;
const T_UNARY:i64=24;
const T_COMMENT:i64=25;

// node kinds, matching the dump table in the fixture
const K_PROGRAM:i64=0;
const K_IF:i64=1;
const K_FOR:i64=2;
const K_BREAK:i64=3;
const K_CONTINUE:i64=4;
const K_NOOP:i64=5;
const K_SET:i64=6;
const K_MACRO:i64=7;
const K_COMMENT:i64=8;
const K_BLANK:i64=9;
const K_MEMBER:i64=10;
const K_CALL:i64=11;
const K_IDENT:i64=12;
const K_INT:i64=13;
const K_FLOAT:i64=14;
const K_STRING:i64=15;
const K_ARRAY:i64=16;
const K_TUPLE:i64=17;
const K_OBJECT:i64=18;
const K_BINARY:i64=19;
const K_FILTER:i64=20;
const K_FILTERSTMT:i64=21;
const K_SELECT:i64=22;
const K_TEST:i64=23;
const K_UNARY:i64=24;
const K_SLICE:i64=25;
const K_KWARG:i64=26;
const K_SPREAD:i64=27;
const K_CALLSTMT:i64=28;
const K_TERNARY:i64=29;

const MAXN:i64=65536;

extern "gs_json_strtod" fn libc_strtod(s:const u8[:])->f64;

fn app(dst:u8[:], at:i64, s:const u8[:], off:i64, len:i64)->i64 {
 var k:i64=0;
 while k<len {
  if at+k<dst.len { dst[at+k]=s[off+k]; }
  k=k+1;
 }
 return at+len;
}
fn apps(dst:u8[:], at:i64, lit:const u8[:])->i64 {
 return app(dst,at,lit,0,lit.len);
}
fn appc(dst:u8[:], at:i64, c:u8)->i64 {
 if at<dst.len { dst[at]=c; }
 return at+1;
}
fn appd(dst:u8[:], at:i64, v:i64)->i64 {
 var p:i64=at;
 var x:i64=v;
 var neg:bool=x<0;
 if neg { x=0-x; }
 var d:i64=1;
 var y:i64=x;
 while y>=10 { d=d+1; y=y/10; }
 var q:i64=p+d-1;
 y=x;
 while y>0 {
  if q<dst.len { dst[q]=(48 as u8)+((y%10) as u8); }
  q=q-1;
  y=y/10;
 }
 if x==0 { if p<dst.len { dst[p]='0'; } }
 if neg {
  // shift right by one to make room for the sign
  var z:i64=0;
  while z<d { if p+d-z<dst.len { dst[p+d-z]=dst[p+d-1-z]; } z=z+1; }
  if p<dst.len { dst[p]='-'; }
  return p+d+1;
 }
 return p+d;
}

// ---- state layout ----------------------------------------------------------
// st[0] error flag, st[1] error length, st[2] current token, st[3] node count,
// st[4] pool cursor, st[5] string-buffer cursor, st[6] token count,
// st[16..32] the array bases computed at entry.
fn set_bases(st:i64[:])->i64 {
 let nt=st[6];
 var b:i64=16;
 st[16]=b;
 st[17]=b+nt;
 st[18]=b+2*nt;
 st[19]=b+3*nt;
 var x:i64=b+4*nt;
 st[20]=x;
 st[21]=x+MAXN;
 st[22]=x+2*MAXN;
 st[23]=x+3*MAXN;
 st[24]=x+4*MAXN;
 st[25]=x+5*MAXN;
 st[26]=x+6*MAXN;
 st[27]=x+7*MAXN;
 st[28]=x+8*MAXN;
 st[29]=x+9*MAXN;
 st[30]=x+10*MAXN;
 st[31]=x+11*MAXN;
 st[32]=x+12*MAXN;
 st[33]=x+13*MAXN;
 st[34]=x+14*MAXN;
 return 0;
}

// ---- node access -----------------------------------------------------------
fn nnew(A:i64[:], F:f64[:], st:i64[:], kind:i64, pos:i64)->i64 {
 let id=st[3];
 st[3]=id+1;
 A[st[20]+id]=kind;
 A[st[21]+id]=pos;
 A[st[22]+id]=0;
 A[st[23]+id]=0;
 A[st[24]+id]=0;
 A[st[25]+id]=-1;
 A[st[26]+id]=-1;
 A[st[27]+id]=-1;
 A[st[28]+id]=-1;
 A[st[29]+id]=0;
 A[st[30]+id]=-1;
 A[st[31]+id]=0;
 F[id]=0.0;
 return id;
}
fn set_c0(A:i64[:], st:i64[:], id:i64, v:i64)->i64 { A[st[25]+id]=v; return 0; }
fn set_c1(A:i64[:], st:i64[:], id:i64, v:i64)->i64 { A[st[26]+id]=v; return 0; }
fn set_c2(A:i64[:], st:i64[:], id:i64, v:i64)->i64 { A[st[27]+id]=v; return 0; }
fn set_ival(A:i64[:], st:i64[:], id:i64, v:i64)->i64 { A[st[22]+id]=v; return 0; }
fn set_str(A:i64[:], st:i64[:], id:i64, off:i64, len:i64)->i64 {
 A[st[23]+id]=off;
 A[st[24]+id]=len;
 return 0;
}
// Lists are linked entries (value, next) in the pool, so nested nodes can
// append to their own lists without breaking the parent's contiguous region.
fn l0_add(A:i64[:], st:i64[:], id:i64, v:i64)->i64 {
 let c=st[29]+id;
 let e=st[4];
 if e<262144 {
  A[st[34]+2*e]=-1;
  A[st[34]+2*e+1]=v;
  if A[c]==0 { A[st[28]+id]=e; } else { A[st[34]+2*A[st[32]+id]]=e; }
  A[st[32]+id]=e;
  A[c]=A[c]+1;
  st[4]=e+1;
 }
 return 0;
}
fn l1_add(A:i64[:], st:i64[:], id:i64, v:i64)->i64 {
 let c=st[31]+id;
 let e=st[4];
 if e<262144 {
  A[st[34]+2*e]=-1;
  A[st[34]+2*e+1]=v;
  if A[c]==0 { A[st[30]+id]=e; } else { A[st[34]+2*A[st[33]+id]]=e; }
  A[st[33]+id]=e;
  A[c]=A[c]+1;
  st[4]=e+1;
 }
 return 0;
}

// ---- token access ----------------------------------------------------------
fn ttype(A:const i64[:], st:const i64[:], i:i64)->i64 {
 if i<0 || i>=st[6] { return T_EOF; }
 return A[st[16]+i];
}
fn tpos(A:const i64[:], st:const i64[:], i:i64)->i64 {
 if i<0 || i>=st[6] { return 0; }
 return A[st[19]+i];
}
fn tval_is(A:const i64[:], B:const u8[:], st:const i64[:], i:i64, lit:const u8[:])->bool {
 if ttype(A,st,i)!=T_ID { return false; }
 let vo=A[st[17]+i];
 let vl=A[st[18]+i];
 if vl!=lit.len { return false; }
 var k:i64=0;
 while k<vl {
  if B[vo+k]!=lit[k] { return false; }
  k=k+1;
 }
 return true;
}
fn tval_bytes_is(A:const i64[:], B:const u8[:], st:const i64[:], i:i64, lit:const u8[:])->bool {
 let vo=A[st[17]+i];
 let vl=A[st[18]+i];
 if vl!=lit.len { return false; }
 var k:i64=0;
 while k<vl {
  if B[vo+k]!=lit[k] { return false; }
  k=k+1;
 }
 return true;
}
fn tok_copy(A:i64[:], B:u8[:], st:i64[:], ti:i64)->i64,i64 {
 let off=st[5];
 let vo=A[st[17]+ti];
 let vl=A[st[18]+ti];
 var k:i64=0;
 while k<vl {
  B[st[5]+k]=B[vo+k];
  k=k+1;
 }
 st[5]=st[5]+vl;
 return off,vl;
}
fn tok_append(A:i64[:], B:u8[:], st:i64[:], ti:i64)->i64 {
 let vo=A[st[17]+ti];
 let vl=A[st[18]+ti];
 var k:i64=0;
 while k<vl {
  B[st[5]+k]=B[vo+k];
  k=k+1;
 }
 st[5]=st[5]+vl;
 return vl;
}

// ---- errors ----------------------------------------------------------------
fn err_plain(E:u8[:], st:i64[:], msg:const u8[:])->i64 {
 st[0]=1;
 st[1]=apps(E,0,msg);
 return -1;
}
fn err_parse(E:u8[:], st:i64[:], src:const u8[:], msg:const u8[:], pos:i64)->i64 {
 var p=apps(E,0,"parser: ");
 p=app(E,p,msg,0,msg.len);
 p=appc(E,p,10);
 if src.len==0 {
  p=apps(E,p,"(no source available)");
 } else {
  var start:i64=0;
  if pos>=40 { start=pos-40; }
  if start>src.len { start=src.len; }
  var end:i64=pos+40;
  if end>src.len { end=src.len; }
  if end<start { end=start; }
  p=apps(E,p,"...");
  var i:i64=start;
  while i<end {
   if src[i]==10 { p=apps(E,p,"\xe2\x86\xb5"); } else { p=appc(E,p,src[i]); }
   i=i+1;
  }
  p=apps(E,p,"...");
  p=appc(E,p,10);
  var n:i64=pos-start+3;
  if n<0 { n=0; }
  var z:i64=0;
  while z<n { p=appc(E,p,32); z=z+1; }
  p=appc(E,p,94);
 }
 st[0]=1;
 st[1]=p;
 return -1;
}
fn last_pos(A:const i64[:], st:const i64[:])->i64 {
 if st[6]==0 { return 0; }
 return A[st[19]+st[6]-1];
}
fn tk_next(A:i64[:], st:i64[:], E:u8[:], src:const u8[:])->i64 {
 if st[2]>=st[6] {
  err_parse(E,st,src,"Parser Error: Unexpected EOF",last_pos(A,st));
  return -1;
 }
 let i=st[2];
 st[2]=i+1;
 return i;
}
fn err_got(E:u8[:], st:i64[:], src:const u8[:], A:const i64[:], B:const u8[:], ti:i64, msg:const u8[:])->i64 {
 var m: u8[512] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var q=apps(m,0,"Parser Error: ");
 q=app(m,q,msg,0,msg.len);
 q=apps(m,q," (Got ");
 if ti>=0 && ti<st[6] {
  let vo=A[st[17]+ti];
  let vl=A[st[18]+ti];
  q=app(m,q,B,vo,vl);
 }
 q=apps(m,q,")");
 return err_parse(E,st,src,m[0..q],tpos(A,st,ti));
}
fn tk_expect(A:i64[:], B:const u8[:], st:i64[:], E:u8[:], src:const u8[:], typ:i64, msg:const u8[:])->i64 {
 let ti=st[2];
 if ttype(A,st,ti)!=typ {
  return err_got(E,st,src,A,B,ti,msg);
 }
 st[2]=ti+1;
 return ti;
}
fn expect_ident(A:i64[:], B:const u8[:], st:i64[:], E:u8[:], src:const u8[:], name:const u8[:])->i64 {
 let ti=st[2];
 if !tval_is(A,B,st,ti,name) {
  var m: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var q=apps(m,0,"Expected identifier: ");
  q=app(m,q,name,0,name.len);
  return err_parse(E,st,src,m[0..q],tpos(A,st,ti));
 }
 st[2]=ti+1;
 return ti;
}

// ---- predicates ------------------------------------------------------------
fn is(A:const i64[:], st:const i64[:], typ:i64)->bool { return ttype(A,st,st[2])==typ; }
fn is_id(A:const i64[:], B:const u8[:], st:const i64[:], name:const u8[:])->bool {
 return tval_is(A,B,st,st[2],name);
}
fn stmt_n(A:const i64[:], B:const u8[:], st:const i64[:], a:const u8[:], b:const u8[:], c:const u8[:])->bool {
 if ttype(A,st,st[2])!=T_OS || ttype(A,st,st[2]+1)!=T_ID { return false; }
 if tval_is(A,B,st,st[2]+1,a) { return true; }
 if b.len>0 && tval_is(A,B,st,st[2]+1,b) { return true; }
 if c.len>0 && tval_is(A,B,st,st[2]+1,c) { return true; }
 return false;
}

// ---- parser ----------------------------------------------------------------
recursive fn parse_any(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 let ti=st[2];
 let t=ttype(A,st,ti);
 if t==T_COMMENT {
  let i=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_COMMENT,tpos(A,st,ti));
  let off,len=tok_copy(A,B,st,i);
  set_str(A,st,id,off,len);
  return id;
 }
 if t==T_TEXT {
  let i=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_STRING,tpos(A,st,ti));
  let off,len=tok_copy(A,B,st,i);
  set_str(A,st,id,off,len);
  return id;
 }
 if t==T_OS { return parse_jinja_statement(A,F,B,E,st,src); }
 if t==T_OE { return parse_jinja_expression(A,F,B,E,st,src); }
 return err_plain(E,st,"Unexpected token type");
}

recursive fn parse_jinja_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 if tk_expect(A,B,st,E,src,T_OE,"Expected {{")<0 { return -1; }
 let r=parse_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CE,"Expected }}")<0 { return -1; }
 return r;
}

recursive fn parse_jinja_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
 if ttype(A,st,st[2])!=T_ID { return err_plain(E,st,"Unknown statement"); }
 let start=st[2];
 let name=tk_next(A,st,E,src);
 if st[0]!=0 { return -1; }

 if tval_is(A,B,st,name,"set") { return parse_set_statement(A,F,B,E,st,src,start); }

 if tval_is(A,B,st,name,"if") {
  let r=parse_if_statement(A,F,B,E,st,src,start);
  if st[0]!=0 { return -1; }
  if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
  if expect_ident(A,B,st,E,src,"endif")<0 { return -1; }
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  return r;
 }
 if tval_is(A,B,st,name,"macro") {
  let r=parse_macro_statement(A,F,B,E,st,src,start);
  if st[0]!=0 { return -1; }
  if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
  if expect_ident(A,B,st,E,src,"endmacro")<0 { return -1; }
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  return r;
 }
 if tval_is(A,B,st,name,"for") {
  let r=parse_for_statement(A,F,B,E,st,src,start);
  if st[0]!=0 { return -1; }
  if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
  if expect_ident(A,B,st,E,src,"endfor")<0 { return -1; }
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  return r;
 }
 if tval_is(A,B,st,name,"break") {
  let id=nnew(A,F,st,K_BREAK,tpos(A,st,start));
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  return id;
 }
 if tval_is(A,B,st,name,"continue") {
  let id=nnew(A,F,st,K_CONTINUE,tpos(A,st,start));
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  return id;
 }
 if tval_is(A,B,st,name,"call") {
  return parse_call_statement(A,F,B,E,st,src,start);
 }
 if tval_is(A,B,st,name,"filter") {
  return parse_filter_statement(A,F,B,E,st,src,start);
 }
 if tval_is(A,B,st,name,"generation") || tval_is(A,B,st,name,"endgeneration") {
  let id=nnew(A,F,st,K_NOOP,tpos(A,st,start));
  st[2]=st[2]+1;
  return id;
 }
 var m: u8[512] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var q=apps(m,0,"Unknown statement: ");
 let vo=A[st[17]+name];
 let vl=A[st[18]+name];
 q=app(m,q,B,vo,vl);
 return err_plain(E,st,m[0..q]);
}

recursive fn parse_set_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], start:i64)->i64 {
 let left=parse_expression_sequence(A,F,B,E,st,src,0);
 if st[0]!=0 { return -1; }
 var val:i64=-1;
 if is(A,st,T_EQ) {
  st[2]=st[2]+1;
  val=parse_expression_sequence(A,F,B,E,st,src,0);
  if st[0]!=0 { return -1; }
 } else {
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 }
 let id=nnew(A,F,st,K_SET,tpos(A,st,start));
 set_c0(A,st,id,left);
 set_c1(A,st,id,val);
 if val<0 {
  // multiline set: body until endset
  while !stmt_n(A,B,st,"endset","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l0_add(A,st,id,b);
  }
  if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
  if expect_ident(A,B,st,E,src,"endset")<0 { return -1; }
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 } else {
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 }
 return id;
}

recursive fn parse_if_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], start:i64)->i64 {
 let test=parse_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 let id=nnew(A,F,st,K_IF,tpos(A,st,start));
 set_c0(A,st,id,test);
 while !stmt_n(A,B,st,"elif","else","endif") {
  let b=parse_any(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  l0_add(A,st,id,b);
 }
 if stmt_n(A,B,st,"elif","","") {
  let pos0=st[2];
  st[2]=st[2]+1;
  st[2]=st[2]+1;
  let nested=parse_if_statement(A,F,B,E,st,src,pos0);
  if st[0]!=0 { return -1; }
  l1_add(A,st,id,nested);
 } else if stmt_n(A,B,st,"else","","") {
  st[2]=st[2]+1;
  st[2]=st[2]+1;
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  while !stmt_n(A,B,st,"endif","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l1_add(A,st,id,b);
  }
 }
 return id;
}

recursive fn parse_macro_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], start:i64)->i64 {
 let name=parse_primary_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 let id=nnew(A,F,st,K_MACRO,tpos(A,st,start));
 set_c0(A,st,id,name);
 if parse_args_into(A,F,B,E,st,src,id,0)<0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 while !stmt_n(A,B,st,"endmacro","","") {
  let b=parse_any(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  l1_add(A,st,id,b);
 }
 return id;
}

recursive fn parse_expression_sequence(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], primary:i64)->i64 {
 let start=st[2];
 var first:i64=-1;
 if primary!=0 { first=parse_primary_expression(A,F,B,E,st,src); }
 else { first=parse_expression(A,F,B,E,st,src); }
 if st[0]!=0 { return -1; }
 if !is(A,st,T_COMMA) { return first; }
 let tup=nnew(A,F,st,K_TUPLE,tpos(A,st,start));
 l0_add(A,st,tup,first);
 while is(A,st,T_COMMA) {
  st[2]=st[2]+1;
  var e:i64=-1;
  if primary!=0 { e=parse_primary_expression(A,F,B,E,st,src); }
  else { e=parse_expression(A,F,B,E,st,src); }
  if st[0]!=0 { return -1; }
  l0_add(A,st,tup,e);
 }
 return tup;
}

recursive fn parse_for_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], start:i64)->i64 {
 let loopvar=parse_expression_sequence(A,F,B,E,st,src,1);
 if st[0]!=0 { return -1; }
 if !is_id(A,B,st,"in") { return err_plain(E,st,"Expected 'in'"); }
 st[2]=st[2]+1;
 let iterable=parse_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 let id=nnew(A,F,st,K_FOR,tpos(A,st,start));
 set_c0(A,st,id,loopvar);
 set_c1(A,st,id,iterable);
 while !stmt_n(A,B,st,"endfor","else","") {
  let b=parse_any(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  l0_add(A,st,id,b);
 }
 if stmt_n(A,B,st,"else","","") {
  st[2]=st[2]+1;
  st[2]=st[2]+1;
  if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
  while !stmt_n(A,B,st,"endfor","","") {
   let b=parse_any(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l1_add(A,st,id,b);
  }
 }
 return id;
}

recursive fn parse_call_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], start:i64)->i64 {
 let caller=nnew(A,F,st,K_CALL,tpos(A,st,start));
 // optional caller arguments
 if is(A,st,T_OP) {
  if parse_args_into(A,F,B,E,st,src,caller,0)<0 { return -1; }
 }
 let callee=parse_primary_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 if A[st[20]+callee]!=K_IDENT { return err_plain(E,st,"Expected identifier"); }
 let call=nnew(A,F,st,K_CALL,tpos(A,st,start));
 set_c0(A,st,call,callee);
 if parse_args_into(A,F,B,E,st,src,call,0)<0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 let stmt=nnew(A,F,st,K_CALLSTMT,tpos(A,st,start));
 set_c0(A,st,stmt,call);
 // the caller arguments were parsed into K_CALL's list; move them to list1
 // of the statement by re-parsing is not possible, so copy the region
 var e:i64=A[st[28]+caller];
 while e>=0 {
  let v=A[st[34]+2*e+1];
  l0_add(A,st,stmt,v);
  e=A[st[34]+2*e];
 }
 while !stmt_n(A,B,st,"endcall","","") {
  let b=parse_any(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  l1_add(A,st,stmt,b);
 }
 if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
 if expect_ident(A,B,st,E,src,"endcall")<0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 return stmt;
}

recursive fn parse_filter_statement(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], start:i64)->i64 {
 var filter=parse_primary_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 if A[st[20]+filter]==K_IDENT && is(A,st,T_OP) {
  filter=parse_call_expression(A,F,B,E,st,src,filter);
  if st[0]!=0 { return -1; }
 }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 let id=nnew(A,F,st,K_FILTERSTMT,tpos(A,st,start));
 set_c0(A,st,id,filter);
 while !stmt_n(A,B,st,"endfilter","","") {
  let b=parse_any(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  l0_add(A,st,id,b);
 }
 if tk_expect(A,B,st,E,src,T_OS,"Expected {%")<0 { return -1; }
 if expect_ident(A,B,st,E,src,"endfilter")<0 { return -1; }
 if tk_expect(A,B,st,E,src,T_CS,"Expected %}")<0 { return -1; }
 return id;
}

recursive fn parse_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 return parse_if_expression(A,F,B,E,st,src);
}

recursive fn parse_if_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 let a=parse_logical_or(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 if is_id(A,B,st,"if") {
  let start=st[2];
  st[2]=st[2]+1;
  let test=parse_logical_or(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  if is_id(A,B,st,"else") {
   let pos0=st[2];
   st[2]=st[2]+1;
   let fe=parse_if_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   let id=nnew(A,F,st,K_TERNARY,tpos(A,st,pos0));
   set_c0(A,st,id,test);
   set_c1(A,st,id,a);
   set_c2(A,st,id,fe);
   return id;
  }
  let id=nnew(A,F,st,K_SELECT,tpos(A,st,start));
  set_c0(A,st,id,a);
  set_c1(A,st,id,test);
  return id;
 }
 return a;
}

recursive fn parse_logical_or(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var left=parse_logical_and(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 while is_id(A,B,st,"or") {
  let start=st[2];
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let right=parse_logical_and(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_BINARY,tpos(A,st,start));
  let off,len=tok_copy(A,B,st,op);
  set_str(A,st,id,off,len);
  set_c0(A,st,id,left);
  set_c1(A,st,id,right);
  left=id;
 }
 return left;
}

recursive fn parse_logical_and(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var left=parse_logical_negation(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 while is_id(A,B,st,"and") {
  let start=st[2];
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let right=parse_logical_negation(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_BINARY,tpos(A,st,start));
  let off,len=tok_copy(A,B,st,op);
  set_str(A,st,id,off,len);
  set_c0(A,st,id,left);
  set_c1(A,st,id,right);
  left=id;
 }
 return left;
}

recursive fn parse_logical_negation(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 if is_id(A,B,st,"not") {
  let start=st[2];
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let arg=parse_logical_negation(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_UNARY,tpos(A,st,start));
  let off,len=tok_copy(A,B,st,op);
  set_str(A,st,id,off,len);
  set_c0(A,st,id,arg);
  return id;
 }
 return parse_comparison(A,F,B,E,st,src);
}

recursive fn parse_comparison(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var left=parse_additive(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 var looping:bool=true;
 while looping {
  let start=st[2];
  var op:i64=-1;
  var literal_op:bool=false;
  if is_id(A,B,st,"not") && tval_is(A,B,st,st[2]+1,"in") {
   st[2]=st[2]+1;
   st[2]=st[2]+1;
  } else if is_id(A,B,st,"in") {
   op=tk_next(A,st,E,src);
   if st[0]!=0 { return -1; }
  } else if is(A,st,T_CMP) {
   op=tk_next(A,st,E,src);
   if st[0]!=0 { return -1; }
  } else {
   looping=false;
  }
  if looping {
   let right=parse_additive(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   let id=nnew(A,F,st,K_BINARY,tpos(A,st,start));
   if op>=0 {
    let off,len=tok_copy(A,B,st,op);
    set_str(A,st,id,off,len);
   } else {
    let off=st[5];
    var q=apps(B,st[5],"not in");
    st[5]=q;
    set_str(A,st,id,off,6);
   }
   set_c0(A,st,id,left);
   set_c1(A,st,id,right);
   left=id;
  }
 }
 return left;
}

recursive fn parse_additive(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var left=parse_multiplicative(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 while is(A,st,T_ADD) {
  let start=st[2];
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let right=parse_multiplicative(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_BINARY,tpos(A,st,start));
  let off,len=tok_copy(A,B,st,op);
  set_str(A,st,id,off,len);
  set_c0(A,st,id,left);
  set_c1(A,st,id,right);
  left=id;
 }
 return left;
}

recursive fn parse_multiplicative(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var left=parse_test_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 while is(A,st,T_MUL) {
  let start=st[2];
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let right=parse_test_expression(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_BINARY,tpos(A,st,start));
  let off,len=tok_copy(A,B,st,op);
  set_str(A,st,id,off,len);
  set_c0(A,st,id,left);
  set_c1(A,st,id,right);
  left=id;
 }
 return left;
}

recursive fn parse_test_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var operand=parse_filter_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 while is_id(A,B,st,"is") {
  let start=st[2];
  st[2]=st[2]+1;
  var negate:i64=0;
  if is_id(A,B,st,"not") {
   st[2]=st[2]+1;
   negate=1;
  }
  var test_id=parse_primary_expression(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  if is(A,st,T_OP) {
   test_id=parse_call_expression(A,F,B,E,st,src,test_id);
   if st[0]!=0 { return -1; }
  } else {
   var takes_arg:bool=false;
   if is(A,st,T_NUM) || is(A,st,T_STR) || is(A,st,T_OCB) || is(A,st,T_OSQ) { takes_arg=true; }
   if is(A,st,T_ID) && !is_id(A,B,st,"and") && !is_id(A,B,st,"or") && !is_id(A,B,st,"else") { takes_arg=true; }
   if takes_arg {
    let call_pos=st[2];
    let arg=parse_unary(A,F,B,E,st,src);
    if st[0]!=0 { return -1; }
    let c=nnew(A,F,st,K_CALL,tpos(A,st,call_pos));
    set_c0(A,st,c,test_id);
    l0_add(A,st,c,arg);
    test_id=c;
   }
  }
  let id=nnew(A,F,st,K_TEST,tpos(A,st,start));
  set_c0(A,st,id,operand);
  set_c1(A,st,id,test_id);
  set_ival(A,st,id,negate);
  operand=id;
 }
 return operand;
}

recursive fn parse_filter_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var operand=parse_unary(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 while is(A,st,T_PIPE) {
  let start=st[2];
  st[2]=st[2]+1;
  var filter=parse_primary_expression(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  if is(A,st,T_OP) {
   filter=parse_call_expression(A,F,B,E,st,src,filter);
   if st[0]!=0 { return -1; }
  }
  let id=nnew(A,F,st,K_FILTER,tpos(A,st,start));
  set_c0(A,st,id,operand);
  set_c1(A,st,id,filter);
  operand=id;
 }
 return operand;
}

recursive fn parse_unary(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 if is(A,st,T_UNARY) {
  let start=st[2];
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let arg=parse_unary(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  let id=nnew(A,F,st,K_UNARY,tpos(A,st,start));
  let off,len=tok_copy(A,B,st,op);
  set_str(A,st,id,off,len);
  set_c0(A,st,id,arg);
  return id;
 }
 return parse_call_member(A,F,B,E,st,src);
}

recursive fn parse_call_member(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 let prim=parse_primary_expression(A,F,B,E,st,src);
 if st[0]!=0 { return -1; }
 let member=parse_member_expression(A,F,B,E,st,src,prim);
 if st[0]!=0 { return -1; }
 if is(A,st,T_OP) { return parse_call_expression(A,F,B,E,st,src,member); }
 return member;
}

recursive fn parse_call_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], callee:i64)->i64 {
 let start=st[2];
 let id=nnew(A,F,st,K_CALL,tpos(A,st,start));
 set_c0(A,st,id,callee);
 if parse_args_into(A,F,B,E,st,src,id,0)<0 { return -1; }
 let member=parse_member_expression(A,F,B,E,st,src,id);
 if st[0]!=0 { return -1; }
 if is(A,st,T_OP) { return parse_call_expression(A,F,B,E,st,src,member); }
 return member;
}

// list 0 for a call's arguments, list 1 for a call statement's caller args
recursive fn parse_args_into(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], node:i64, which:i64)->i64 {
 if tk_expect(A,B,st,E,src,T_OP,"Expected (")<0 { return -1; }
 while !is(A,st,T_CP) {
  var arg:i64=-1;
  if is(A,st,T_MUL) && tval_bytes_is(A,B,st,st[2],"*") {
   let start=st[2];
   st[2]=st[2]+1;
   let e=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   let sp=nnew(A,F,st,K_SPREAD,tpos(A,st,start));
   set_c0(A,st,sp,e);
   arg=sp;
  } else {
   arg=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   if is(A,st,T_EQ) {
    let start=st[2];
    st[2]=st[2]+1;
    let v=parse_expression(A,F,B,E,st,src);
    if st[0]!=0 { return -1; }
    let kw=nnew(A,F,st,K_KWARG,tpos(A,st,start));
    set_c0(A,st,kw,arg);
    set_c1(A,st,kw,v);
    arg=kw;
   }
  }
  if which==0 { l0_add(A,st,node,arg); } else { l1_add(A,st,node,arg); }
  if is(A,st,T_COMMA) { st[2]=st[2]+1; }
 }
 if tk_expect(A,B,st,E,src,T_CP,"Expected )")<0 { return -1; }
 return 0;
}

recursive fn parse_member_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:], object0:i64)->i64 {
 var object=object0;
 let start=st[2];
 while is(A,st,T_DOT) || is(A,st,T_OSQ) {
  let op=tk_next(A,st,E,src);
  if st[0]!=0 { return -1; }
  let computed:i64 = if ttype(A,st,op)==T_OSQ { 1 } else { 0 };
  var prop:i64=-1;
  if computed!=0 {
   prop=parse_member_arguments(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   if tk_expect(A,B,st,E,src,T_CSQ,"Expected ]")<0 { return -1; }
  } else {
   prop=parse_primary_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
  }
  let id=nnew(A,F,st,K_MEMBER,tpos(A,st,start));
  set_c0(A,st,id,object);
  set_c1(A,st,id,prop);
  set_ival(A,st,id,computed);
  object=id;
 }
 return object;
}

recursive fn parse_member_arguments(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 var s0:i64=-1;
 var s1:i64=-1;
 var s2:i64=-1;
 var n:i64=0;
 var is_slice:i64=0;
 let start=st[2];
 while !is(A,st,T_CSQ) {
  if is(A,st,T_COLON) {
   n=n+1;
   st[2]=st[2]+1;
   is_slice=1;
  } else {
   let e=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   if n==0 { s0=e; } else if n==1 { s1=e; } else if n==2 { s2=e; }
   n=n+1;
   if is(A,st,T_COLON) {
    st[2]=st[2]+1;
    is_slice=1;
   }
  }
 }
 if is_slice!=0 {
  let id=nnew(A,F,st,K_SLICE,tpos(A,st,start));
  set_c0(A,st,id,s0);
  set_c1(A,st,id,s1);
  set_c2(A,st,id,s2);
  return id;
 }
 if n==0 {
  return nnew(A,F,st,K_BLANK,tpos(A,st,start));
 }
 return s0;
}

recursive fn parse_primary_expression(A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:], src:const u8[:])->i64 {
 let start=st[2];
 let ti=tk_next(A,st,E,src);
 if st[0]!=0 { return -1; }
 let t=ttype(A,st,ti);
 if t==T_NUM {
  let vo=A[st[17]+ti];
  let vl=A[st[18]+ti];
  var is_float:bool=false;
  var k:i64=0;
  while k<vl {
   if B[vo+k]==46 { is_float=true; }
   k=k+1;
  }
  let id=nnew(A,F,st,K_FLOAT,tpos(A,st,ti));
  if is_float {
   let d=libc_strtod(B[vo..vo+vl]);
   F[id]=d;
  } else {
   var v:i64=0;
   var neg:bool=false;
   k=0;
   if vl>0 && B[vo]==43 { k=1; } else if vl>0 && B[vo]==45 { k=1; neg=true; }
   while k<vl {
    v=v*10+((B[vo+k]-48) as i64);
    k=k+1;
   }
   if neg { v=0-v; }
   A[st[20]+id]=K_INT;
   set_ival(A,st,id,v);
  }
  return id;
 }
 if t==T_STR {
  let off=st[5];
  var len=tok_append(A,B,st,ti);
  while is(A,st,T_STR) {
   let nx=tk_next(A,st,E,src);
   if st[0]!=0 { return -1; }
   len=len+tok_append(A,B,st,nx);
  }
  let id=nnew(A,F,st,K_STRING,tpos(A,st,ti));
  set_str(A,st,id,off,len);
  return id;
 }
 if t==T_ID {
  let id=nnew(A,F,st,K_IDENT,tpos(A,st,ti));
  let off,len=tok_copy(A,B,st,ti);
  set_str(A,st,id,off,len);
  return id;
 }
 if t==T_OP {
  let e=parse_expression_sequence(A,F,B,E,st,src,0);
  if st[0]!=0 { return -1; }
  if tk_expect(A,B,st,E,src,T_CP,"Expected )")<0 { return -1; }
  return e;
 }
 if t==T_OSQ {
  let id=nnew(A,F,st,K_ARRAY,tpos(A,st,ti));
  while !is(A,st,T_CSQ) {
   let e=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l0_add(A,st,id,e);
   if is(A,st,T_COMMA) { st[2]=st[2]+1; }
  }
  st[2]=st[2]+1;
  return id;
 }
 if t==T_OCB {
  let id=nnew(A,F,st,K_OBJECT,tpos(A,st,ti));
  while !is(A,st,T_CCB) {
   let key=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   if tk_expect(A,B,st,E,src,T_COLON,"Expected :")<0 { return -1; }
   let v=parse_expression(A,F,B,E,st,src);
   if st[0]!=0 { return -1; }
   l0_add(A,st,id,key);
   l1_add(A,st,id,v);
   if is(A,st,T_COMMA) { st[2]=st[2]+1; }
  }
  st[2]=st[2]+1;
  return id;
 }
 var m: u8[512] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var q=apps(m,0,"Unexpected token: ");
 let vo=A[st[17]+ti];
 let vl=A[st[18]+ti];
 q=app(m,q,B,vo,vl);
 q=apps(m,q," of type ");
 q=appd(m,q,t);
 return err_plain(E,st,m[0..q]);
}

// ---- entry -----------------------------------------------------------------
// A layout: tokens at 16, node arrays after them, then the child pool.
// B: token value bytes first (tlen), then node strings.
export fn jinja_parse(ntok:i64, tlen:i64, src:const u8[:], A:i64[:], F:f64[:], B:u8[:], E:u8[:], st:i64[:])->i64 {
 st[0]=0; st[1]=0; st[2]=0; st[3]=0; st[4]=0; st[5]=tlen; st[6]=ntok; st[7]=tlen; st[8]=-1;
 set_bases(st);
 if ntok==0 {
  let p=nnew(A,F,st,K_PROGRAM,0);
  st[8]=p;
  return p;
 }
 let p=nnew(A,F,st,K_PROGRAM,0);
 while st[2]<ntok {
  let s=parse_any(A,F,B,E,st,src);
  if st[0]!=0 { return -1; }
  l0_add(A,st,p,s);
 }
 st[8]=p;
 return p;
}
