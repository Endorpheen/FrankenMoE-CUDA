// Jinja lexer in Goose, from common/jinja/lexer.cpp (jinja::lexer::tokenize).
//
// Owned decisions:
//   - source normalization: "\r\n" and "\r" become "\n", then a single trailing
//     newline of the ORIGINAL source is dropped (the check is on the original,
//     so a source ending in '\r' keeps the '\n' the replacement produced);
//   - the ordered mapping table, trimmed forms first, and the rule that "}}"
//     is only a close_expression while curly_bracket_depth is zero;
//   - text blocks with lstrip_blocks/trim_blocks, both on by default, and the
//     rstrip/lstrip of the neighbouring block;
//   - comments `{# ... #}`;
//   - unary versus binary '-' and '+' decided by the previous token;
//   - numeric literals with an optional fractional part, string literals with
//     the escape table (\n \t \r \b \f \v \\ \' \" \xNN), identifiers;
//   - errors: `lexer: <msg>\n` followed by peak_source, which shows 40 bytes
//     either side of the position in the ORIGINAL source.
//
// Positions are byte offsets into the normalized source, as in the native lexer;
// the error view is built from the original source, also as in the native lexer.
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
const T_CALL:i64=20;
const T_ADD:i64=21;
const T_MUL:i64=22;
const T_CMP:i64=23;
const T_UNARY:i64=24;
const T_COMMENT:i64=25;

fn apps(dst:u8[:], at:i64, lit:const u8[:])->i64 {
 return app(dst,at,lit,0,lit.len);
}
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
fn at(s:const u8[:], i:i64)->u8 {
 if i<0 || i>=s.len { return 0; }
 return s[i];
}
fn put_utf8(dst:u8[:], at0:i64, cp:i64)->i64 {
 var p:i64=at0;
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
fn hexval(c:u8)->i64 {
 if c>='0' && c<='9' { return (c-48) as i64; }
 if c>='a' && c<='f' { return (c-87) as i64; }
 if c>='A' && c<='F' { return (c-55) as i64; }
 return -1;
}
fn is_digit(c:u8)->bool { return c>='0' && c<='9'; }
fn is_word_c(c:u8)->bool {
 if c>='a' && c<='z' { return true; }
 if c>='A' && c<='Z' { return true; }
 if c>='0' && c<='9' { return true; }
 return c=='_';
}
fn is_space5(c:u8)->bool {
 return c==32 || c==9 || c==10 || c==11 || c==12 || c==13;
}
fn is_space4(c:u8)->bool {
 return c==32 || c==9 || c==10 || c==13;
}

// ---- error text ------------------------------------------------------------
// `lexer: <msg>\n` + peak_source(original, pos): 40 bytes either side, newlines
// shown as U+21B5, then a caret under the position.
fn err_set(orig:const u8[:], msg:const u8[:], pos:i64, errb:u8[:], st:i64[:])->i64 {
 var p=apps(errb,0,"lexer: ");
 p=app(errb,p,msg,0,msg.len);
 p=appc(errb,p,10);
 if orig.len==0 {
  p=apps(errb,p,"(no source available)");
 } else {
  var start:i64=0;
  if pos>=40 { start=pos-40; }
  if start>orig.len { start=orig.len; }
  var end:i64=pos+40;
  if end>orig.len { end=orig.len; }
  if end<start { end=start; }
  p=apps(errb,p,"...");
  var i:i64=start;
  while i<end {
   if orig[i]==10 { p=apps(errb,p,"\xe2\x86\xb5"); } else { p=appc(errb,p,orig[i]); }
   i=i+1;
  }
  p=apps(errb,p,"...");
  p=appc(errb,p,10);
  var n:i64=pos-start+3;
  if n<0 { n=0; }
  var z:i64=0;
  while z<n { p=appc(errb,p,32); z=z+1; }
  p=appc(errb,p,94);
 }
 st[1]=1;
 st[2]=p;
 return -1;
}

// ---- consume_while ---------------------------------------------------------
// kind 0: digits, 1: word characters, 2: any byte except ch (string body).
// Escape processing happens before the byte is appended, as in the native loop.
fn pred(kind:i64, c:u8, ch:u8)->bool {
 if kind==0 { return is_digit(c); }
 if kind==1 { return is_word_c(c); }
 return c!=ch;
}

fn consume(kind:i64, ch:u8, s:const u8[:], orig:const u8[:], out:u8[:], errb:u8[:], st:i64[:])->i64,i64 {
 let start=st[7];
 var p=st[3];
 while pred(kind,at(s,p),ch) {
  if at(s,p)==92 {
   p=p+1;
   if p>=s.len {
    err_set(orig,"unexpected end of input after escape character",p,errb,st);
    return -1,-1;
   }
   let e=at(s,p);
   p=p+1;
   if e=='x' {
    if p+1>=s.len {
     err_set(orig,"incomplete hexadecimal escape",p,errb,st);
     return -1,-1;
    }
    let h1=hexval(at(s,p));
    let h2=hexval(at(s,p+1));
    if h1<0 || h2<0 {
     err_set(orig,"invalid hexadecimal escape",p,errb,st);
     return -1,-1;
    }
    st[7]=put_utf8(out,st[7],h1*16+h2);
    p=p+2;
   } else {
    var v:i64=-1;
    if e=='n' { v=10; }
    else if e=='t' { v=9; }
    else if e=='r' { v=13; }
    else if e=='b' { v=8; }
    else if e=='f' { v=12; }
    else if e=='v' { v=11; }
    else if e==92 { v=92; }
    else if e==39 { v=39; }
    else if e==34 { v=34; }
    if v<0 {
     var mb: u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
     var q=apps(mb,0,"unknown escape character ");
     q=appc(mb,q,92);
     q=appc(mb,q,e);
     err_set(orig,mb[0..q],p,errb,st);
     return -1,-1;
    }
    st[7]=appc(out,st[7],v as u8);
   }
  } else {
   st[7]=appc(out,st[7],at(s,p));
   p=p+1;
   if p>s.len {
    err_set(orig,"unexpected end of input during consume_while",p,errb,st);
    return -1,-1;
   }
  }
 }
 st[3]=p;
 return start,st[7]-start;
}

// ---- mapping table ---------------------------------------------------------
fn starts(s:const u8[:], p:i64, lit:const u8[:])->bool {
 if p+lit.len>s.len { return false; }
 var i:i64=0;
 while i<lit.len {
  if s[p+i]!=lit[i] { return false; }
  i=i+1;
 }
 return true;
}

fn match_tok(s:const u8[:], p:i64, depth:i64)->i64,i64 {
 if starts(s,p,"{%-") { return T_OS,3; }
 if starts(s,p,"-%}") { return T_CS,3; }
 if starts(s,p,"{{-") { return T_OE,3; }
 if starts(s,p,"-}}") { return T_CE,3; }
 if starts(s,p,"{%") { return T_OS,2; }
 if starts(s,p,"%}") { return T_CS,2; }
 if starts(s,p,"{{") { return T_OE,2; }
 if starts(s,p,"}}") && depth<=0 { return T_CE,2; }
 if starts(s,p,"(") { return T_OP,1; }
 if starts(s,p,")") { return T_CP,1; }
 if starts(s,p,"{") { return T_OCB,1; }
 if starts(s,p,"}") { return T_CCB,1; }
 if starts(s,p,"[") { return T_OSQ,1; }
 if starts(s,p,"]") { return T_CSQ,1; }
 if starts(s,p,",") { return T_COMMA,1; }
 if starts(s,p,".") { return T_DOT,1; }
 if starts(s,p,":") { return T_COLON,1; }
 if starts(s,p,"|") { return T_PIPE,1; }
 if starts(s,p,"<=") { return T_CMP,2; }
 if starts(s,p,">=") { return T_CMP,2; }
 if starts(s,p,"==") { return T_CMP,2; }
 if starts(s,p,"!=") { return T_CMP,2; }
 if starts(s,p,"<") { return T_CMP,1; }
 if starts(s,p,">") { return T_CMP,1; }
 if starts(s,p,"+") { return T_ADD,1; }
 if starts(s,p,"-") { return T_ADD,1; }
 if starts(s,p,"~") { return T_ADD,1; }
 if starts(s,p,"*") { return T_MUL,1; }
 if starts(s,p,"/") { return T_MUL,1; }
 if starts(s,p,"%") { return T_MUL,1; }
 if starts(s,p,"=") { return T_EQ,1; }
 return -1,0;
}

fn push_tok(tt:i64[:], voff:i64[:], vlen:i64[:], tpos:i64[:], st:i64[:], typ:i64, off:i64, len:i64, pos:i64)->i64 {
 let i=st[0];
 st[0]=i+1;
 tt[i]=typ;
 voff[i]=off;
 vlen[i]=len;
 tpos[i]=pos;
 return 0;
}

fn push_lit(tt:i64[:], voff:i64[:], vlen:i64[:], tpos:i64[:], out:u8[:], st:i64[:], typ:i64, s:const u8[:], p:i64, len:i64, pos:i64)->i64 {
 let off=st[7];
 st[7]=app(out,st[7],s,p,len);
 return push_tok(tt,voff,vlen,tpos,st,typ,off,len,pos);
}

fn unexpected(orig:const u8[:], ch:u8, pos:i64, errb:u8[:], st:i64[:])->i64 {
 var mb: u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var q=apps(mb,0,"unexpected character: ");
 q=appc(mb,q,ch);
 return err_set(orig,mb[0..q],pos,errb,st);
}

// ---- entry -----------------------------------------------------------------
export fn jinja_lex(source:const u8[:], tt:i64[:], voff:i64[:], vlen:i64[:], tpos:i64[:], out:u8[:], norm:u8[:], errb:u8[:], st:i64[:])->i64 {
 st[0]=0; st[1]=0; st[2]=0; st[3]=0; st[4]=0; st[5]=0; st[6]=0; st[7]=0; st[8]=0;

 // normalization
 var n:i64=0;
 var i:i64=0;
 while i<source.len {
  if source[i]==13 {
   if i+1<source.len && source[i+1]==10 { i=i+2; } else { i=i+1; }
   norm[n]=10; n=n+1;
  } else {
   norm[n]=source[i]; n=n+1; i=i+1;
  }
 }
 if source.len>0 && source[source.len-1]==10 { n=n-1; }
 st[8]=n;
 let s=norm[0..n];
 if n==0 { return 0; }

 var p:i64=0;
 var depth:i64=0;

 while p<n {
  var start_pos:i64=p;
  var lt:i64=T_CS;
  if st[0]>0 { lt=tt[st[0]-1]; }

  // ---- text outside statements --------------------------------------------
  if lt==T_CS || lt==T_CE || lt==T_COMMENT {
   var last_block_can_rm_newline:bool=false;
   var is_rstrip:bool=false;
   if p>3 {
    let c0=at(s,p-3);
    let c1=at(s,p-2);
    let c2=at(s,p-1);
    is_rstrip = c0==45 && (c1==37 || c1==125 || c1==35) && c2==125;
    last_block_can_rm_newline = (c1==35 || c1==37 || c1==45) && c2==125;
   }

   var start:i64=p;
   var end:i64=p;
   while p<n && !(at(s,p)==123 && (at(s,p+1)==37 || at(s,p+1)==123 || at(s,p+1)==35)) {
    p=p+1;
    end=p;
   }

   // lstrip_blocks: trim spaces/tabs before the block
   if at(s,p)==123 && (at(s,p+1)==37 || at(s,p+1)==35 || at(s,p+1)==45) {
    var cur:i64=end;
    var stop:bool=false;
    while cur>start && !stop {
     let c=at(s,cur-1);
     if cur==1 { end=0; stop=true; }
     else if c==10 { end=cur; stop=true; }
     else if !is_space5(c) { stop=true; }
     else { cur=cur-1; }
    }
   }

   var ts:i64=start;
   var te:i64=end;
   // trim_blocks: drop one newline right after the previous block
   if last_block_can_rm_newline && te>ts && at(s,ts)==10 { ts=ts+1; }
   // rstrip of the previous block: lstrip the text
   if is_rstrip {
    while ts<te && is_space4(at(s,ts)) { ts=ts+1; }
   }
   // lstrip of the current block: rstrip the text
   let is_lstrip_block = at(s,p)==123 && (at(s,p+1)==123 || at(s,p+1)==37 || at(s,p+1)==35) && at(s,p+2)==45;
   if is_lstrip_block {
    while te>ts && is_space4(at(s,te-1)) { te=te-1; }
   }

   if ts<te {
    let off=st[7];
    st[7]=app(out,st[7],s,ts,te-ts);
    push_tok(tt,voff,vlen,tpos,st,T_TEXT,off,te-ts,start_pos);
    continue;
   }
  }

  // ---- comment -------------------------------------------------------------
  if at(s,p)==123 && at(s,p+1)==35 {
   start_pos=p;
   p=p+2;
   let off=st[7];
   var stop:bool=false;
   while !(at(s,p)==35 && at(s,p+1)==125) && !stop {
    if p+2>=n {
     err_set(source,"missing end of comment tag",p,errb,st);
     return -1;
    }
    st[7]=appc(out,st[7],at(s,p));
    p=p+1;
   }
   push_tok(tt,voff,vlen,tpos,st,T_COMMENT,off,st[7]-off,start_pos);
   p=p+2;
   continue;
  }

  // ---- an explicit '-' right after an opening marker ----------------------
  if at(s,p)==45 && (lt==T_OE || lt==T_OS) {
   p=p+1;
   if p>=n { break; }
  }

  while p<n && is_space5(at(s,p)) { p=p+1; }
  if p>=n { break; }

  let ch=at(s,p);
  let is_closing = ch==45 && (at(s,p+1)==37 || at(s,p+1)==125);

  // ---- unary ---------------------------------------------------------------
  if !is_closing && (ch==45 || ch==43) {
   start_pos=p;
   var lt2:i64=T_EOF;
   if st[0]>0 { lt2=tt[st[0]-1]; }
   if lt2==T_TEXT || lt2==T_EOF {
    unexpected(source,ch,p,errb,st);
    return -1;
   }
   var binary:bool=false;
   if lt2==T_ID || lt2==T_NUM || lt2==T_STR || lt2==T_CP || lt2==T_CSQ { binary=true; }
   if !binary {
    p=p+1;
    st[3]=p;
    let no,nl=consume(0,0,s,source,out,errb,st);
    if st[1]!=0 { return -1; }
    p=st[3];
    let vo=st[7];
    st[7]=appc(out,st[7],ch);
    st[7]=app(out,st[7],out,no,nl);
    if nl==0 {
     push_tok(tt,voff,vlen,tpos,st,T_UNARY,vo,1,start_pos);
    } else {
     push_tok(tt,voff,vlen,tpos,st,T_NUM,vo,1+nl,start_pos);
    }
    continue;
   }
  }

  // ---- mapping table -------------------------------------------------------
  let mt,ml=match_tok(s,p,depth);
  if mt>=0 {
   push_lit(tt,voff,vlen,tpos,out,st,mt,s,p,ml,p);
   if mt==T_OE { depth=0; }
   else if mt==T_OCB { depth=depth+1; }
   else if mt==T_CCB { depth=depth-1; }
   p=p+ml;
   continue;
  }

  // ---- string --------------------------------------------------------------
  if ch==39 || ch==34 {
   start_pos=p;
   p=p+1;
   st[3]=p;
   let so,sl=consume(2,ch,s,source,out,errb,st);
   if st[1]!=0 { return -1; }
   p=st[3];
   push_tok(tt,voff,vlen,tpos,st,T_STR,so,sl,start_pos);
   p=p+1;
   continue;
  }

  // ---- number --------------------------------------------------------------
  if is_digit(ch) {
   start_pos=p;
   st[3]=p;
   let no,nl=consume(0,0,s,source,out,errb,st);
   if st[1]!=0 { return -1; }
   p=st[3];
   let vo=st[7];
   st[7]=app(out,st[7],out,no,nl);
   var total:i64=nl;
   // an optional fractional part
   if at(s,p)==46 && p+1<n && is_digit(at(s,p+1)) {
    st[7]=appc(out,st[7],46);
    p=p+1;
    st[3]=p;
    let fo,fl=consume(0,0,s,source,out,errb,st);
    if st[1]!=0 { return -1; }
    p=st[3];
    st[7]=app(out,st[7],out,fo,fl);
    total=total+1+fl;
   }
   push_tok(tt,voff,vlen,tpos,st,T_NUM,vo,total,start_pos);
   continue;
  }

  // ---- identifier ----------------------------------------------------------
  if is_word_c(ch) {
   start_pos=p;
   st[3]=p;
   let wo,wl=consume(1,0,s,source,out,errb,st);
   if st[1]!=0 { return -1; }
   p=st[3];
   push_tok(tt,voff,vlen,tpos,st,T_ID,wo,wl,start_pos);
   continue;
  }

  unexpected(source,ch,p,errb,st);
  return -1;
 }

 return st[0];
}
