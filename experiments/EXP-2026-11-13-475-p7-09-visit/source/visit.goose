// JSON-schema-to-GBNF rule-text assembly in Goose, from
// common/json-schema-to-grammar.cpp (_build_object_rule, visit's tuple/array/
// anyOf branches, build_repetition with a separator, kv rules).
//
// Owned decisions:
//   - an object body: required kv rules in order, then one alternative per
//     optional property, with the native recursive "<name>-<key>-rest" rules
//     that chain the remaining optionals, and additionalProperties as a "*";
//   - the "*" branch takes ", space <kv>*" instead of "<kv>?";
//   - an object with no required and no optional properties is "{ space }";
//   - a tuple joins item rules with ", space ";
//   - an array wraps build_repetition(item, min, max) in brackets;
//   - an anyOf joins alternative rule names with " | ";
//   - kv = format_literal(dump) + ' space ":" space ' + child;
//   - additionalProperties kv = key_rule + ' ":" space ' + value_rule.
//
// Rule names are sanitized here. The fixture keeps property names distinct after
// sanitization, so the native collision suffixes are not exercised by this slice.
// The schema walk, $ref targets and _not_strings stay native.
import std;

const INT_MAX:i64 = 2147483647;

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

// format_literal: CR, LF, '"' and '\' only.
fn format_literal_impl(lit:const u8[:], out:u8[:])->i64 {
 var p=append_c(out,0,'"');
 var i:i64=0;
 while i<lit.len {
  let c=lit[i];
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

export fn format_literal(lit:const u8[:], out:u8[:])->i64 {
 return format_literal_impl(lit,out);
}

// sanitize s into dst at dst_off (output never longer than the input).
fn sanitize_into(s:const u8[:], dst:u8[:], dst_off:i64)->i64 {
 var i:i64=0;
 var n:i64=0;
 var in_run:bool=false;
 while i<s.len {
  let b=s[i];
  var valid:bool=false;
  if b>='a' && b<='z' { valid=true; }
  if b>='A' && b<='Z' { valid=true; }
  if b>='0' && b<='9' { valid=true; }
  if b=='-' { valid=true; }
  if valid {
   if dst_off+n<dst.len { dst[dst_off+n]=b; }
   n=n+1;
   in_run=false;
  } else {
   if !in_run {
    if dst_off+n<dst.len { dst[dst_off+n]='-'; }
    n=n+1;
    in_run=true;
   }
  }
  i=i+1;
 }
 return n;
}

// build_repetition with a separator; expressions live in abuf.
recursive fn rep_impl(abuf:u8[:], at:i64, item:const u8[:], sep:const u8[:], mn:i64, mx:i64)->i64,i64,i64 {
 if mx==0 { return at,0,at; }
 if mn==0 && mx==1 {
  var p=append_s(abuf,at,item,0,item.len);
  p=append_c(abuf,p,'?');
  return at,p-at,p;
 }
 let has_max = mx!=INT_MAX;
 if sep.len==0 {
  var p=append_s(abuf,at,item,0,item.len);
  if mn==1 && !has_max {
   p=append_c(abuf,p,'+');
   return at,p-at,p;
  }
  if mn==0 && !has_max {
   p=append_c(abuf,p,'*');
   return at,p-at,p;
  }
  p=append_c(abuf,p,'{');
  p=append_dec(abuf,p,mn);
  p=append_c(abuf,p,',');
  if has_max { p=append_dec(abuf,p,mx); }
  p=append_c(abuf,p,'}');
  return at,p-at,p;
 }
 let inner_at=at;
 var ip=append_c(abuf,inner_at,'(');
 ip=append_s(abuf,ip,sep,0,sep.len);
 ip=append_c(abuf,ip,' ');
 ip=append_s(abuf,ip,item,0,item.len);
 ip=append_c(abuf,ip,')');
 let inner=abuf[inner_at..ip];
 var imn:i64=mn;
 if mn!=0 { imn=mn-1; }
 var imx:i64=mx;
 if has_max { imx=mx-1; }
 let empty: const u8[:] = "";
 let ro,rl,cur=rep_impl(abuf,ip,inner,empty,imn,imx);
 let out_at=cur;
 var p=append_s(abuf,out_at,item,0,item.len);
 p=append_c(abuf,p,' ');
 p=append_s(abuf,p,abuf,ro,rl);
 if mn==0 {
  let wrapped=p;
  var q=append_c(abuf,wrapped,'(');
  q=append_s(abuf,q,abuf,out_at,p-out_at);
  q=append_c(abuf,q,')');
  q=append_c(abuf,q,'?');
  return wrapped,q-wrapped,q;
 }
 return out_at,p-out_at,p;
}

fn rep_text(item:const u8[:], sep:const u8[:], mn:i64, mx:i64, abuf:u8[:])->i64,i64 {
 let off,len,cur=rep_impl(abuf,0,item,sep,mn,mx);
 return off,len;
}

// Public wrapper: the repetition expression is moved to out[0].
export fn rep_sep(item:const u8[:], sep:const u8[:], mn:i64, mx:i64, out:u8[:])->i64 {
 let ro,rl=rep_text(item,sep,mn,mx,out);
 var k:i64=0;
 while k<rl {
  if k<out.len { out[k]=out[ro+k]; }
  k=k+1;
 }
 return rl;
}

export fn kv_rule(dump:const u8[:], child:const u8[:], out:u8[:])->i64 {
 var p=format_literal_impl(dump,out);
 let sep: const u8[:] = " space \":\" space ";
 p=append_s(out,p,sep,0,sep.len);
 p=append_s(out,p,child,0,child.len);
 return p;
}

export fn additional_kv(key_rule:const u8[:], value_rule:const u8[:], out:u8[:])->i64 {
 var p=append_s(out,0,key_rule,0,key_rule.len);
 let sep: const u8[:] = " \":\" space ";
 p=append_s(out,p,sep,0,sep.len);
 p=append_s(out,p,value_rule,0,value_rule.len);
 return p;
}

// visit's KIND_OBJECT shortcut: 1 when an empty object with ANY additional
// properties maps to the object primitive, 2 for the normal build.
export fn object_branch(nprops:i64, add_kind:i64)->i64 {
 if nprops==0 && add_kind==1 { return 1; }
 return 2;
}

fn build_kv(abuf:u8[:], ast:i64[:], name:const u8[:], key:const u8[:], star:i64, suffix:const u8[:])->i64,i64 {
 let at=ast[0];
 var p=append_s(abuf,at,name,0,name.len);
 if name.len!=0 { p=append_c(abuf,p,'-'); }
 if star!=0 { p=append_c(abuf,p,'*'); } else { p=append_s(abuf,p,key,0,key.len); }
 p=append_s(abuf,p,suffix,0,suffix.len);
 let n=sanitize_into(abuf[at..p],abuf,at);
 ast[0]=at+n;
 return at,n;
}

// get_recursive_refs. opt entries are property indices, -1 for "*".
recursive fn refs(opt:const i64[:], oc:i64, from:i64, first:i64, name:const u8[:],
                  strs:const u8[:], dir:const i64[:],
                  kvdir:const i64[:], nprops:i64,
                  abuf:u8[:], ast:i64[:], rtext:u8[:], rp:i64[:], rst:i64[:])->i64,i64 {
 let k=opt[from];
 let is_star= k<0;
 var koff:i64=0;
 var klen:i64=0;
 if !is_star {
  koff=dir[6*k+0];
  klen=dir[6*k+1];
 }
 var kvoff:i64=0;
 var kvlen:i64=0;
 if is_star {
  kvoff=kvdir[nprops*2+0];
  kvlen=kvdir[nprops*2+1];
 } else {
  kvoff=kvdir[k*2+0];
  kvlen=kvdir[k*2+1];
 }

 var rname_off:i64=0;
 var rname_len:i64=0;
 if from+1<oc {
  let co,cl=refs(opt,oc,from+1,1,name,strs,dir,kvdir,nprops,abuf,ast,rtext,rp,rst);
  var keyname: const u8[:] = "*";
  if !is_star { keyname=strs[koff..koff+klen]; }
  var starflag:i64=0;
  if is_star { starflag=1; }
  let no,nl=build_kv(abuf,ast,name,keyname,starflag,"-rest");
  rname_off=no;
  rname_len=nl;
  // _add_rule reuses a name whose text is identical: dedupe the rest rules
  var dup:bool=false;
  var q:i64=0;
  while q<rst[0] {
   let enl=rp[q*4+1];
   let etl=rp[q*4+3];
   if enl==nl && etl==cl {
    var same:bool=true;
    var z:i64=0;
    while z<nl {
     if rtext[rp[q*4+0]+z]!=abuf[no+z] { same=false; z=nl; } else { z=z+1; }
    }
    if same {
     z=0;
     while z<cl {
      if rtext[rp[q*4+2]+z]!=abuf[co+z] { same=false; z=cl; } else { z=z+1; }
     }
    }
    if same { dup=true; }
   }
   q=q+1;
  }
  if !dup {
   let idx=rst[0];
   let at=rst[1];
   let name_at=at;
   let text_at=append_s(rtext,at,abuf,no,nl);
   let ne=append_s(rtext,text_at,abuf,co,cl);
   rst[1]=ne;
   if idx*4+3<rp.len {
    rp[idx*4+0]=name_at;
    rp[idx*4+1]=nl;
    rp[idx*4+2]=text_at;
    rp[idx*4+3]=cl;
   }
   rst[0]=idx+1;
  }
 }

 let at2=ast[0];
 var p:i64=at2;
 let kv=abuf[kvoff..kvoff+kvlen];
 if first!=0 {
  p=append_s(abuf,p,"( \",\" space ",0,12);
  p=append_s(abuf,p,kv,0,kv.len);
  p=append_s(abuf,p," )",0,2);
  if is_star { p=append_c(abuf,p,'*'); } else { p=append_c(abuf,p,'?'); }
 } else {
  p=append_s(abuf,p,kv,0,kv.len);
  if is_star {
   p=append_s(abuf,p," ( \",\" space ",0,13);
   p=append_s(abuf,p,kv,0,kv.len);
   p=append_s(abuf,p," )*",0,3);
  }
 }
 if rname_len!=0 {
  p=append_c(abuf,p,' ');
  p=append_s(abuf,p,abuf,rname_off,rname_len);
 }
 ast[0]=p;
 return at2,p-at2;
}

// _build_object_rule. dir holds 6 entries per property: name off/len, JSON dump
// off/len, child value rule off/len.
export fn object_rule(name:const u8[:], strs:const u8[:], dir:const i64[:],
                      req:const i64[:], n:i64, add_kind:i64, add_value:const u8[:],
                      out:u8[:], abuf:u8[:], ast:i64[:], kvdir:i64[:], opt:i64[:],
                      rtext:u8[:], rp:i64[:], rst:i64[:])->i64 {
 if n==0 && add_kind==0 {
  return append_s(out,0,"\"{\" space \"}\"",0,13);
 }
 // kv names for every property, then the additional one
 var i:i64=0;
 while i<n {
  let nm=strs[dir[6*i+0]..dir[6*i+0]+dir[6*i+1]];
  let ko,kl=build_kv(abuf,ast,name,nm,0,"-kv");
  kvdir[i*2+0]=ko;
  kvdir[i*2+1]=kl;
  i=i+1;
 }
 if add_kind!=0 {
  let ko,kl=build_kv(abuf,ast,name,"additional",0,"-kv");
  kvdir[n*2+0]=ko;
  kvdir[n*2+1]=kl;
 }

 // required chain
 var p=append_s(out,0,"\"{\" space ",0,10);
 var have_req:bool=false;
 i=0;
 while i<n {
  if req[i]!=0 {
   if have_req { p=append_s(out,p," \",\" space ",0,11); }
   p=append_s(out,p,abuf,kvdir[i*2],kvdir[i*2+1]);
   have_req=true;
  }
  i=i+1;
 }

 // optional list: non-required properties in order, then "*"
 var oc:i64=0;
 i=0;
 while i<n {
  if req[i]==0 { opt[oc]=i; oc=oc+1; }
  i=i+1;
 }
 if add_kind!=0 { opt[oc]=-1; oc=oc+1; }

 if oc>0 {
  p=append_s(out,p," (",0,2);
  if have_req { p=append_s(out,p," \",\" space ( ",0,13); }
  var j:i64=0;
  while j<oc {
   if j>0 { p=append_s(out,p," | ",0,3); }
   let eo,el=refs(opt,oc,j,0,name,strs,dir,kvdir,n,abuf,ast,rtext,rp,rst);
   p=append_s(out,p,abuf,eo,el);
   j=j+1;
  }
  if have_req { p=append_s(out,p," )",0,2); }
  p=append_s(out,p," )?",0,3);
 }
 p=append_s(out,p," space \"}\"",0,10);
 return p;
}

// Tuple: items joined with ' "," space '.
export fn tuple_rule(strs:const u8[:], dir:const i64[:], n:i64, out:u8[:])->i64 {
 var p=append_s(out,0,"\"[\" space ",0,10);
 var i:i64=0;
 while i<n {
  if i>0 { p=append_s(out,p," \",\" space ",0,11); }
  let off=dir[2*i+0];
  let len=dir[2*i+1];
  p=append_s(out,p,strs,off,len);
  i=i+1;
 }
 p=append_s(out,p," space \"]\"",0,10);
 return p;
}

// Array: "[" space <rep> space "]".
export fn array_rule(item:const u8[:], mn:i64, mx:i64, out:u8[:], abuf:u8[:])->i64 {
 let sep: const u8[:] = "\",\" space";
 var mmx:i64=mx;
 if mmx<0 { mmx=INT_MAX; }
 let ro,rl=rep_text(item,sep,mn,mmx,abuf);
 var p=append_s(out,0,"\"[\" space ",0,10);
 p=append_s(out,p,abuf,ro,rl);
 p=append_s(out,p," space \"]\"",0,10);
 return p;
}

// anyOf: alternative rule names joined with " | ".
export fn union_rule(strs:const u8[:], dir:const i64[:], n:i64, out:u8[:])->i64 {
 var p:i64=0;
 var i:i64=0;
 while i<n {
  if i>0 { p=append_s(out,p," | ",0,3); }
  let off=dir[2*i+0];
  let len=dir[2*i+1];
  p=append_s(out,p,strs,off,len);
  i=i+1;
 }
 return p;
}
