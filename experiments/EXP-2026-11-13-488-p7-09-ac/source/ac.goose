// Aho-Corasick automaton in Goose, from common/trie.h and trie.cpp.
//
// Owned decisions: trie insertion over codepoints with ids in creation order,
// pattern marking that keeps the first index for duplicates, failure links via
// BFS, longest-pattern terminal propagation over the fail chain, and the
// next(state, codepoint) walk.
import std;

fn utf8_next(s:const u8[:], i:i64)->i64,i64 {
 let b0=s[i];
 if b0<128 { return b0,i+1; }
 if b0<224 && i+1<s.len { return ((b0-192) as i64)*64+((s[i+1]-128) as i64),i+2; }
 if b0<240 && i+2<s.len { return ((b0-224) as i64)*4096+((s[i+1]-128) as i64)*64+((s[i+2]-128) as i64),i+3; }
 if i+3<s.len { return ((b0-240) as i64)*262144+((s[i+1]-128) as i64)*4096+((s[i+2]-128) as i64)*64+((s[i+3]-128) as i64),i+4; }
 return b0,i+1;
}

fn find_child(head:const i64[:], ecpt:const i64[:], enext:const i64[:], node:i64, cpt:i64)->i64 {
 var e=head[node];
 while e>=0 {
  if ecpt[e]==cpt { return e; }
  e=enext[e];
 }
 return -1;
}

fn insert_word(s:const u8[:], off:i64, len:i64, head:i64[:], ecpt:i64[:], eto:i64[:], enext:i64[:],
               pat:i64[:], st:i64[:])->i64 {
 var node:i64=0;
 var i:i64=off;
 let end:i64=off+len;
 while i<end {
  let cpt,ni=utf8_next(s,i);
  i=ni;
  let e=find_child(head,ecpt,enext,node,cpt);
  if e>=0 {
   node=eto[e];
  } else {
   let nn=st[0];
   st[0]=nn+1;
   head[nn]=-1;
   let ei=st[1];
   st[1]=ei+1;
   ecpt[ei]=cpt;
   eto[ei]=nn;
   // keep children sorted by codepoint
   var prev:i64=-1;
   var cur:i64=head[node];
   var before:i64=-1;
   var stop:bool=false;
   while cur>=0 && !stop {
    if ecpt[cur]>cpt { before=cur; stop=true; }
    else { prev=cur; cur=enext[cur]; }
   }
   if before>=0 { enext[ei]=before; } else { enext[ei]=-1; }
   if prev>=0 { enext[prev]=ei; } else { head[node]=ei; }
   node=nn;
  }
 }
 if pat[node]<0 {
  pat[node]=st[2];
  st[2]=st[2]+1;
 }
 return pat[node];
}

export fn ac_build(strs:const u8[:], dir:const i64[:], n:i64,
                   head:i64[:], ecpt:i64[:], eto:i64[:], enext:i64[:], pat:i64[:],
                   fail:i64[:], mt:i64[:], queue:i64[:], order:i64[:], st:i64[:])->i64 {
 st[0]=1; st[1]=0; st[2]=0; st[3]=0;
 head[0]=-1;
 pat[0]=-1;
 var w:i64=0;
 while w<n {
  let off=dir[2*w+0];
  let len=dir[2*w+1];
  insert_word(strs,off,len,head,ecpt,eto,enext,pat,st);
  w=w+1;
 }
 // BFS failure links and order
 var qh:i64=0;
 var qt:i64=0;
 queue[qt]=0; qt=qt+1;
 fail[0]=0;
 while qh<qt {
  let u=queue[qh];
  qh=qh+1;
  let oi=st[3];
  order[oi]=u;
  st[3]=oi+1;
  var e=head[u];
  while e>=0 {
   let v=eto[e];
   let ch=ecpt[e];
   if u!=0 {
    var f=fail[u];
    var done:bool=false;
    while f!=0 && !done {
     let fe=find_child(head,ecpt,enext,f,ch);
     if fe>=0 { done=true; } else { f=fail[f]; }
    }
    let fe=find_child(head,ecpt,enext,f,ch);
    if fe>=0 && eto[fe]!=v { fail[v]=eto[fe]; } else { fail[v]=0; }
   } else {
    fail[v]=0;
   }
   queue[qt]=v;
   qt=qt+1;
   e=enext[e];
  }
 }
 // terminal propagation in BFS order
 var k:i64=0;
 while k<st[3] {
  let u=order[k];
  if pat[u]>=0 { mt[u]=pat[u]; }
  else if u!=0 { mt[u]=mt[fail[u]]; }
  else { mt[u]=-1; }
  k=k+1;
 }
 return st[0];
}

export fn ac_next(state0:i64, cp:i64, head:const i64[:], ecpt:const i64[:], eto:const i64[:],
                  enext:const i64[:], fail:const i64[:])->i64 {
 var state:i64=state0;
 var stop:bool=false;
 while state!=0 && !stop {
  let e=find_child(head,ecpt,enext,state,cp);
  if e>=0 { return eto[e]; }
  state=fail[state];
 }
 let e=find_child(head,ecpt,enext,state,cp);
 if e>=0 { return eto[e]; }
 return 0;
}

export fn ac_terminal(mt:const i64[:], s:i64)->i64 { return mt[s]; }
