// JSON-schema-to-GBNF _not_strings in Goose, from
// common/json-schema-to-grammar.cpp (_not_strings) and common/trie.h.
//
// Owned decisions:
//   - property names are inserted into a trie as Unicode codepoints, children
//     kept sorted by codepoint (the native std::map order);
//   - the exclusion text: [" ] ( per-child [c] (nested | char+) ... |
//     [^"<all child chars>] char* )? ["] — the trailing "?" appears unless an
//     empty word was inserted, because then the root node carries a pattern.
//
// The char primitive name is passed in; the schema walk and the rule map stay
// native.
import std;

const NO_EDGE:i64 = -1;

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

// Decode one UTF-8 codepoint; invalid bytes are returned as themselves.
fn utf8_next(s:const u8[:], i:i64)->i64,i64 {
 let b0=s[i];
 if b0<128 { return b0,i+1; }
 if b0>=194 && b0<=223 && i+1<s.len {
  return ((b0-192) as i64)*64+((s[i+1]-128) as i64),i+2;
 }
 if b0>=224 && b0<=239 && i+2<s.len {
  return ((b0-224) as i64)*4096+((s[i+1]-128) as i64)*64+((s[i+2]-128) as i64),i+3;
 }
 if b0>=240 && b0<=244 && i+3<s.len {
  return ((b0-240) as i64)*262144+((s[i+1]-128) as i64)*4096+((s[i+2]-128) as i64)*64+((s[i+3]-128) as i64),i+4;
 }
 return b0,i+1;
}

// state slots
const S_NODES:i64=0;
const S_EDGES:i64=1;
const S_ROOTPAT:i64=2;

// Insert one name; children stay sorted by codepoint.
fn trie_insert(head:i64[:], ecpt:i64[:], eto:i64[:], enext:i64[:], st:i64[:], s:const u8[:])->i64 {
 var node:i64=0;
 var i:i64=0;
 while i<s.len {
  let cpt,ni=utf8_next(s,i);
  i=ni;
  var prev:i64=NO_EDGE;
  var e:i64=head[node];
  var found:i64=NO_EDGE;
  var before:i64=NO_EDGE;
  var stop:bool=false;
  while e>=0 && !stop {
   if ecpt[e]==cpt {
    found=eto[e];
    stop=true;
   } else if ecpt[e]>cpt {
    before=e;
    stop=true;
   } else {
    prev=e;
    e=enext[e];
   }
  }
  if found>=0 {
   node=found;
  } else {
   let nn=st[S_NODES];
   st[S_NODES]=nn+1;
   head[nn]=NO_EDGE;
   let ei=st[S_EDGES];
   st[S_EDGES]=ei+1;
   ecpt[ei]=cpt;
   eto[ei]=nn;
   if before>=0 {
    enext[ei]=before;
   } else {
    enext[ei]=NO_EDGE;
   }
   if prev>=0 { enext[prev]=ei; } else { head[node]=ei; }
   node=nn;
  }
 }
 if s.len==0 { st[S_ROOTPAT]=1; }
 return node;
}

fn build_trie(head:i64[:], ecpt:i64[:], eto:i64[:], enext:i64[:], st:i64[:],
              strs:const u8[:], dir:const i64[:], n:i64)->i64 {
 head[0]=NO_EDGE;
 st[S_NODES]=1;
 st[S_EDGES]=0;
 st[S_ROOTPAT]=0;
 var i:i64=0;
 while i<n {
  let off=dir[2*i+0];
  let len=dir[2*i+1];
  trie_insert(head,ecpt,eto,enext,st,strs[off..off+len]);
  i=i+1;
 }
 return 0;
}

recursive fn emit_node(out:u8[:], p:i64, node:i64, head:const i64[:], ecpt:const i64[:],
                       eto:const i64[:], enext:const i64[:], char_rule:const u8[:],
                       rbuf:u8[:], rst:i64[:])->i64 {
 let base=rst[0];
 rst[0]=base+1024;
 var rlen:i64=0;
 var q:i64=p;
 var first:bool=true;
 var e:i64=head[node];
 while e>=0 {
  let cpt=ecpt[e];
  let c0:i64=rlen;
  var pos:i64=base+rlen;
  if cpt<128 {
   pos=append_c(rbuf,pos,cpt as u8);
  } else if cpt<2048 {
   pos=append_c(rbuf,pos,(192+(cpt/64)) as u8);
   pos=append_c(rbuf,pos,(128+(cpt%64)) as u8);
  } else if cpt<65536 {
   pos=append_c(rbuf,pos,(224+(cpt/4096)) as u8);
   pos=append_c(rbuf,pos,(128+((cpt/64)%64)) as u8);
   pos=append_c(rbuf,pos,(128+(cpt%64)) as u8);
  } else {
   pos=append_c(rbuf,pos,(240+(cpt/262144)) as u8);
   pos=append_c(rbuf,pos,(128+((cpt/4096)%64)) as u8);
   pos=append_c(rbuf,pos,(128+((cpt/64)%64)) as u8);
   pos=append_c(rbuf,pos,(128+(cpt%64)) as u8);
  }
  rlen=pos-base;
  if !first { q=append_s(out,q," | ",0,3); }
  first=false;
  q=append_c(out,q,'[');
  q=append_s(out,q,rbuf,base+c0,rlen-c0);
  q=append_c(out,q,']');
  let child=eto[e];
  if head[child]>=0 {
   q=append_s(out,q," (",0,2);
   q=emit_node(out,q,child,head,ecpt,eto,enext,char_rule,rbuf,rst);
   q=append_c(out,q,')');
  } else {
   q=append_c(out,q,' ');
   q=append_s(out,q,char_rule,0,char_rule.len);
   q=append_c(out,q,'+');
  }
  e=enext[e];
 }
 if head[node]>=0 {
  q=append_s(out,q," | [^\"",0,6);
  q=append_s(out,q,rbuf,base,rlen);
  q=append_s(out,q,"] ",0,2);
  q=append_s(out,q,char_rule,0,char_rule.len);
  q=append_c(out,q,'*');
 }
 rst[0]=base;
 return q;
}

// [" ] ( <visit root> ) [" ] with the trailing "?" unless the root is a pattern.
export fn not_strings(strs:const u8[:], dir:const i64[:], n:i64, char_rule:const u8[:],
                      head:i64[:], ecpt:i64[:], eto:i64[:], enext:i64[:], st:i64[:],
                      out:u8[:], rbuf:u8[:], rst:i64[:])->i64 {
 build_trie(head,ecpt,eto,enext,st,strs,dir,n);
 rst[0]=0;
 var p=append_s(out,0,"[\"] ( ",0,6);
 p=emit_node(out,p,0,head,ecpt,eto,enext,char_rule,rbuf,rst);
 p=append_s(out,p," )",0,2);
 if st[S_ROOTPAT]==0 { p=append_c(out,p,'?'); }
 p=append_s(out,p," [\"]",0,4);
 return p;
}
