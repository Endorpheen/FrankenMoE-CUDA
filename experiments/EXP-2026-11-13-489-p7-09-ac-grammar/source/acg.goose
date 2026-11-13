// GBNF Aho-Corasick rule emitter in Goose, from common/peg-parser.cpp
// (gbnf_ac_grammar, gbnf_excluding_grammar, gbnf_including_grammar,
// gbnf_char_class, gbnf_escape_char_class).
//
// kind 0 = excluding (until), kind 1 = including (ac).
// Emits every non-terminal state rule `prefix[-NN]` and returns the start name.
import std;

fn app(dst:u8[:], at:i64, s:const u8[:], off:i64, len:i64)->i64 {
 var k:i64=0;
 while k<len { if at+k<dst.len { dst[at+k]=s[off+k]; } k=k+1; }
 return at+len;
}
fn appc(dst:u8[:], at:i64, c:u8)->i64 { if at<dst.len { dst[at]=c; } return at+1; }
fn appd(dst:u8[:], at:i64, v:i64)->i64 {
 var p:i64=at; var x:i64=v; var d:i64=1; var y:i64=x;
 while y>=10 { d=d+1; y=y/10; }
 var q:i64=p+d-1; y=x;
 while y>0 { if q<dst.len { dst[q]=(48 as u8)+((y%10) as u8); } q=q-1; y=y/10; }
 if x==0 { if p<dst.len { dst[p]='0'; } }
 return p+d;
}

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
 while e>=0 { if ecpt[e]==cpt { return e; } e=enext[e]; }
 return -1;
}

fn esc_cp(out:u8[:], at:i64, c:i64)->i64 {
 var p:i64=at;
 if c==45 || c==93 || c==91 || c==92 { p=appc(out,p,92); return appc(out,p,c as u8); }
 if c==10 { p=appc(out,p,92); return appc(out,p,'n'); }
 if c==9 { p=appc(out,p,92); return appc(out,p,'t'); }
 if c==13 { p=appc(out,p,92); return appc(out,p,'r'); }
 if c>=32 && c<=126 { return appc(out,p,c as u8); }
 let hex: const u8[:] = "0123456789ABCDEF";
 if c<=255 {
  p=appc(out,p,92); p=appc(out,p,'x');
  p=appc(out,p,hex[(c/16)%16]); return appc(out,p,hex[c%16]);
 }
 if c<=65535 {
  p=appc(out,p,92); p=appc(out,p,'u');
  p=appc(out,p,hex[(c/4096)%16]); p=appc(out,p,hex[(c/256)%16]);
  p=appc(out,p,hex[(c/16)%16]); return appc(out,p,hex[c%16]);
 }
 p=appc(out,p,92); p=appc(out,p,'U');
 var sh:i64=28;
 while sh>=0 { p=appc(out,p,hex[(c>>sh)&15]); sh=sh-4; }
 return p;
}

// chars: codepoints in alphabet order [0..n), class into out
fn emit_class(out:u8[:], at:i64, chars:const i64[:], n:i64, negate:i64)->i64 {
 var p:i64=at;
 if negate!=0 { p=appc(out,p,'['); p=appc(out,p,'^'); } else { p=appc(out,p,'['); }
 var i:i64=0;
 while i<n { p=esc_cp(out,p,chars[i]); i=i+1; }
 return appc(out,p,']');
}

fn emit_name(out:u8[:], at:i64, kind:i64, pid:i64, q:i64)->i64,i64 {
 var p:i64=at;
 if kind==0 { p=app(out,p,"until-",0,6); } else { p=app(out,p,"ac-",0,3); }
 p=appd(out,p,pid);
 if q!=0 {
  p=appc(out,p,'-');
  if q<10 { p=appc(out,p,'0'); }
  p=appd(out,p,q);
 }
 return at,p-at;
}

export fn ac_grammar(kind:i64, pid:i64, strs:const u8[:], sdir:const i64[:],
                     dstart:const i64[:], dcount:const i64[:], delims:const i64[:], nd:i64,
                     out:u8[:], roff:i64[:], rlen:i64[:], toff:i64[:], tlen:i64[:],
                     head:i64[:], ecpt:i64[:], eto:i64[:], enext:i64[:], pat:i64[:],
                     fail:i64[:], mt:i64[:], queue:i64[:], order:i64[:], alpha:i64[:], st:i64[:])->i64 {
 st[0]=1; st[1]=0; st[2]=0; st[3]=0; st[4]=0; st[5]=0;  // nodes, edges, patterns, order, rules
 head[0]=-1; pat[0]=-1;
 // trie
 var w:i64=0;
 while w<nd {
  let di=delims[dstart[w]+0];
  let off=sdir[2*di+0];
  let len=sdir[2*di+1];
  var node:i64=0;
  var i:i64=off;
  let end:i64=off+len;
  while i<end {
   let cpt,ni=utf8_next(strs,i); i=ni;
   let e=find_child(head,ecpt,enext,node,cpt);
   if e>=0 { node=eto[e]; } else {
    let nn=st[0]; st[0]=nn+1; head[nn]=-1;
    let ei=st[1]; st[1]=ei+1; ecpt[ei]=cpt; eto[ei]=nn;
    var prev:i64=-1; var cur:i64=head[node]; var before:i64=-1; var stop:bool=false;
    while cur>=0 && !stop { if ecpt[cur]>cpt { before=cur; stop=true; } else { prev=cur; cur=enext[cur]; } }
    if before>=0 { enext[ei]=before; } else { enext[ei]=-1; }
    if prev>=0 { enext[prev]=ei; } else { head[node]=ei; }
    node=nn;
   }
  }
  if pat[node]<0 { pat[node]=st[2]; st[2]=st[2]+1; }
  w=w+1;
 }
 // BFS
 var qh:i64=0; var qt:i64=0; queue[qt]=0; qt=qt+1; fail[0]=0;
 while qh<qt {
  let u=queue[qh]; qh=qh+1;
  let oi=st[3]; order[oi]=u; st[3]=oi+1;
  var e=head[u];
  while e>=0 { let ch=ecpt[e]; let v=eto[e];
   if u!=0 { var f=fail[u]; var done:bool=false;
    while f!=0 && !done { let fe=find_child(head,ecpt,enext,f,ch); if fe>=0 { done=true; } else { f=fail[f]; } }
    let fe=find_child(head,ecpt,enext,f,ch);
    if fe>=0 && eto[fe]!=v { fail[v]=eto[fe]; } else { fail[v]=0; }
   } else { fail[v]=0; }
   queue[qt]=v; qt=qt+1; e=enext[e];
  }
 }
 // terminal propagation
 var k:i64=0;
 while k<st[3] { let u=order[k];
  if pat[u]>=0 { mt[u]=pat[u]; } else if u!=0 { mt[u]=mt[fail[u]]; } else { mt[u]=-1; }
  k=k+1;
 }
 // alphabet: unique sorted codepoints
 var na:i64=0;
 var ei:i64=0;
 while ei<st[1] {
  let c=ecpt[ei];
  var found:bool=false;
  var z:i64=0;
  while z<na { if alpha[z]==c { found=true; } z=z+1; }
  if !found {
   var pos:i64=na;
   while pos>0 && alpha[pos-1]>c { alpha[pos]=alpha[pos-1]; pos=pos-1; }
   alpha[pos]=c; na=na+1;
  }
  ei=ei+1;
 }
 // rules per non-terminal state
 var q:i64=0;
 while q<st[0] {
  if mt[q]<0 {
   let no,nl=emit_name(out,st[5],kind,pid,q);
   // keep names in out
   st[5]=st[5]; // no-op keeps structure clear
   var p:i64=st[5];
   // name already written at [no..no+nl); text starts after it
   st[5]=no+nl;
   // compute sets
   var comp_c: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
   var nc:i64=0;
   var spec_c: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
   var ns:i64=0;
   var zc:i64=0;
   while zc<na {
    let c=alpha[zc];
    let d=ac_next(q,c,head,ecpt,eto,enext,fail);
    if mt[d]>=0 { comp_c[nc]=c; nc=nc+1; spec_c[ns]=c; ns=ns+1; }
    else if d!=0 { spec_c[ns]=c; ns=ns+1; }
    zc=zc+1;
   }
   var t_at:i64=st[5];
   var tp:i64=t_at;
   if kind==0 {
    tp=appc(out,tp,'|');
    var d:i64=1;
    while d<st[0] {
     if mt[d]<0 {
      // chars c with next(q,c)==d
      var bucket: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]; var nb:i64=0;
      var z2:i64=0;
      while z2<na { let c=alpha[z2]; if ac_next(q,c,head,ecpt,eto,enext,fail)==d { bucket[nb]=c; nb=nb+1; } z2=z2+1; }
      if nb>0 {
       tp=appc(out,tp,' ');
       tp=emit_class(out,tp,bucket,nb,0);
       tp=appc(out,tp,' ');
       let dno,dnl=emit_name(out,tp,kind,pid,d);
       tp=dno+dnl;
       tp=app(out,tp," |",0,2);
      }
     }
     d=d+1;
    }
    tp=appc(out,tp,' ');
    tp=emit_class(out,tp,spec_c,ns,1);
    tp=appc(out,tp,' ');
    let sno,snl=emit_name(out,tp,kind,pid,0); tp=sno+snl;
   } else {
    var first:bool=true;
    if nc>0 { tp=emit_class(out,tp,comp_c,nc,0); first=false; }
    var d:i64=1;
    while d<st[0] {
     if mt[d]<0 {
      var bucket: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]; var nb:i64=0;
      var z2:i64=0;
      while z2<na { let c=alpha[z2]; if ac_next(q,c,head,ecpt,eto,enext,fail)==d { bucket[nb]=c; nb=nb+1; } z2=z2+1; }
      if nb>0 {
       if !first { tp=app(out,tp," | ",0,3); }
       tp=emit_class(out,tp,bucket,nb,0);
       tp=appc(out,tp,' ');
       let dno,dnl=emit_name(out,tp,kind,pid,d); tp=dno+dnl;
       first=false;
      }
     }
     d=d+1;
    }
    if !first { tp=app(out,tp," | ",0,3); }
    tp=emit_class(out,tp,spec_c,ns,1);
    tp=appc(out,tp,' ');
    let sno,snl=emit_name(out,tp,kind,pid,0); tp=sno+snl;
   }
   let ri=st[4];
   roff[ri]=no; rlen[ri]=nl; toff[ri]=t_at; tlen[ri]=tp-t_at;
   st[4]=ri+1;
   st[5]=tp;
  }
  q=q+1;
 }
 if mt[0]>=0 {
  // empty delimiter: the start state is terminal
  let no,nl=emit_name(out,st[5],kind,pid,0);
  let t_at=no+nl;
  let tp=appc(out,t_at,124);
  let ri=st[4];
  roff[ri]=no; rlen[ri]=nl; toff[ri]=t_at; tlen[ri]=tp-t_at;
  st[4]=ri+1;
  st[5]=tp;
 }
 return st[4];
}

fn ac_next(state0:i64, cp:i64, head:const i64[:], ecpt:const i64[:], eto:const i64[:],
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
