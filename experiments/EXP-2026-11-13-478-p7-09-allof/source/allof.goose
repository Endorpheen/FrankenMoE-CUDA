// JSON-schema-to-GBNF _visit_all_of in Goose, from
// common/json-schema-to-grammar.cpp (_visit_all_of and add_component).
//
// Owned decisions:
//   - components are walked in order: a top-level anyOf expands its alternatives
//     with is_required = false, every other top-level component starts with true;
//   - a ref follows its target with the same is_required; an anyOf reached
//     through a ref is ignored (native add_component has no branch for it);
//   - object properties are appended in traversal order, duplicates included;
//   - required is a set of names: a name is required when any occurrence was
//     visited with is_required, and every occurrence of that name follows it;
//   - enum values are counted per rendered constant text (format_literal(dump)),
//     and the intersection keeps the texts whose count equals the number of
//     top-level children, in std::map (byte-wise) order;
//   - a non-empty intersection wins over the object build.
//
// The schema walk, rule map and object/enum text stay native.
import std;

const I64_NONE:i64 = -1;

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

fn eq_bytes(a:const u8[:], b:const u8[:])->bool {
 if a.len!=b.len { return false; }
 var i:i64=0;
 while i<a.len {
  if a[i]!=b[i] { return false; }
  i=i+1;
 }
 return true;
}

// std::string ordering: byte-wise, a prefix sorts first.
fn less_bytes(a:const u8[:], b:const u8[:])->bool {
 var i:i64=0;
 while i<a.len && i<b.len {
  if a[i]<b[i] { return true; }
  if a[i]>b[i] { return false; }
  i=i+1;
 }
 return a.len<b.len;
}

// state: 0 props out, 1 vals out, 2 kind (0 enum, 1 object), 3 enum rows, 4 arena used
recursive fn walk(ci:i64, req:i64, ckind:const i64[:], cfirst:const i64[:], ccount:const i64[:],
                  citems:const i64[:], pnames:const u8[:], pdir:const i64[:],
                  vdumps:const u8[:], vdir:const i64[:],
                  out_props:i64[:], out_req:i64[:], tinfo:i64[:], abuf:u8[:], st:i64[:])->i64 {
 let k=ckind[ci];
 if k==0 {
  let base=cfirst[ci];
  let n=ccount[ci];
  var j:i64=0;
  while j<n {
   let pi=citems[base+j];
   let oi=st[0];
   out_props[oi]=pi;
   out_req[oi]=req;
   st[0]=oi+1;
   j=j+1;
  }
  return 0;
 }
 if k==1 {
  let base=cfirst[ci];
  let n=ccount[ci];
  var j:i64=0;
  while j<n {
   let vi=citems[base+j];
   let vo=vdir[2*vi+0];
   let vl=vdir[2*vi+1];
   let to=st[4];
   let te=format_literal_at(vdumps[vo..vo+vl],abuf,to);
   st[4]=te;
   // look up the rendered text among the table rows
   var r:i64=0;
   var found:i64=I64_NONE;
   while r<st[3] {
    let ro=tinfo[r*4+0];
    let rl=tinfo[r*4+1];
    if rl==te-to {
     if eq_bytes(abuf[ro..ro+rl],abuf[to..te]) { found=r; r=st[3]; } else { r=r+1; }
    } else {
     r=r+1;
    }
   }
   if found>=0 {
    tinfo[found*4+3]=tinfo[found*4+3]+1;
   } else {
    let ri=st[3];
    tinfo[ri*4+0]=to;
    tinfo[ri*4+1]=te-to;
    tinfo[ri*4+2]=vi;
    tinfo[ri*4+3]=1;
    st[3]=ri+1;
   }
   j=j+1;
  }
  return 0;
 }
 if k==3 {
  let t= cfirst[ci];
  let target=citems[t];
  return walk(target,req,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);
 }
 return 0;
}

// _visit_all_of merge. Returns kind: 0 enum intersection, 1 object.
export fn allof_merge(ckind:const i64[:], cfirst:const i64[:], ccount:const i64[:], citems:const i64[:],
                      top:const i64[:], nchildren:i64,
                      pnames:const u8[:], pdir:const i64[:],
                      vdumps:const u8[:], vdir:const i64[:],
                      out_props:i64[:], out_req:i64[:], out_vals:i64[:],
                      tinfo:i64[:], abuf:u8[:], st:i64[:])->i64 {
 st[0]=0;
 st[1]=0;
 st[2]=1;
 st[3]=0;
 st[4]=0;
 // top-level walk: anyOf expands its alternatives as not required
 var i:i64=0;
 while i<top.len {
  let ci=top[i];
  if ckind[ci]==2 {
   let base=cfirst[ci];
   let n=ccount[ci];
   var j:i64=0;
   while j<n {
    walk(citems[base+j],0,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);
    j=j+1;
   }
  } else {
   walk(ci,1,ckind,cfirst,ccount,citems,pnames,pdir,vdumps,vdir,out_props,out_req,tinfo,abuf,st);
  }
  i=i+1;
 }

 // required is a set of names: any required occurrence makes every one follow
 var a:i64=0;
 while a<st[0] {
  if out_req[a]==0 {
   let pai=out_props[a];
   let pa=pnames[pdir[2*pai+0]..pdir[2*pai+0]+pdir[2*pai+1]];
   var b:i64=0;
   while b<st[0] {
    if out_req[b]!=0 {
     let pbi=out_props[b];
     let pb=pnames[pdir[2*pbi+0]..pdir[2*pbi+0]+pdir[2*pbi+1]];
     if eq_bytes(pa,pb) { out_req[a]=1; b=st[0]; } else { b=b+1; }
    } else {
     b=b+1;
    }
   }
  }
  a=a+1;
 }

 // enum intersection in map order
 var n:i64=0;
 var r:i64=0;
 while r<st[3] {
  if tinfo[r*4+3]==nchildren {
   out_vals[n]=r;
   n=n+1;
  }
  r=r+1;
 }
 // insertion sort the row indices by their text
 var i2:i64=1;
 while i2<n {
  let cur=out_vals[i2];
  let cur_off=tinfo[cur*4+0];
  let cur_len=tinfo[cur*4+1];
  var j2:i64=i2;
  var stop:bool=false;
  while j2>0 && !stop {
   let prev=out_vals[j2-1];
   let po=tinfo[prev*4+0];
   let pl=tinfo[prev*4+1];
   if less_bytes(abuf[cur_off..cur_off+cur_len],abuf[po..po+pl]) {
    out_vals[j2]=prev;
    j2=j2-1;
   } else {
    stop=true;
   }
  }
  out_vals[j2]=cur;
  i2=i2+1;
 }
 var m:i64=0;
 while m<n {
  let row=out_vals[m];
  out_vals[m]=tinfo[row*4+2];
  m=m+1;
 }
 st[1]=n;
 if n>0 { st[2]=0; } else { st[2]=1; }
 return st[2];
}
