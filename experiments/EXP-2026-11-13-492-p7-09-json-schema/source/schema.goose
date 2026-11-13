// JSON-schema document builder in Goose, from common/json-schema.cpp
// (common_chat_schema_builder: build_node dispatch, build_object, build_array,
// build_string, build_integer, build_alternatives, build_ref, get_count,
// get_bound, get_format, resolve_ref, and the "JSON schema error at <path>:" text).
//
// The JSON document itself is decoded by the native parser and handed in as a
// flat tree (kind / integer value / double value / string id / children / keys);
// this module owns the schema reading, the node and field policy, and the
// document $ref map with its cycles.
import std;
import math;

const K_ANY:i64=0;
const K_REF:i64=1;
const K_ANY_OF:i64=2;
const K_ALL_OF:i64=3;
const K_CONST:i64=4;
const K_ENUM:i64=5;
const K_NULL:i64=6;
const K_BOOLEAN:i64=7;
const K_NUMBER:i64=8;
const K_INTEGER:i64=9;
const K_STRING:i64=10;
const K_ARRAY:i64=11;
const K_TUPLE:i64=12;
const K_OBJECT:i64=13;

const J_NULL:i64=0;
const J_BOOL:i64=1;
const J_INT:i64=2;
const J_DBL:i64=3;
const J_STR:i64=4;
const J_ARR:i64=5;
const J_OBJ:i64=6;

const I64_MIN:i64=-9223372036854775807-1;
const I64_MAX:i64=9223372036854775807;

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
fn appd(dst:u8[:], at:i64, v:i64)->i64 {
 var p:i64=at;
 var x:i64=v;
 if x<0 { p=appc(dst,p,'-'); x=0-x; }
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
 return p+d;
}
fn eqb(a:const u8[:], b:const u8[:])->bool {
 if a.len!=b.len { return false; }
 var i:i64=0;
 while i<a.len {
  if a[i]!=b[i] { return false; }
  i=i+1;
 }
 return true;
}
fn s_at(strs:const u8[:], sdir:const i64[:], sid:i64)->const u8[:] {
 let o=sdir[2*sid+0];
 let l=sdir[2*sid+1];
 return strs[o..o+l];
}
fn s_eq(strs:const u8[:], sdir:const i64[:], sid:i64, lit:const u8[:])->bool {
 return eqb(s_at(strs,sdir,sid),lit);
}

// ---- path building (append-only arena) ------------------------------------
fn p_lit(pb:u8[:], st:i64[:], poff:i64, plen:i64, lit:const u8[:])->i64,i64 {
 let at=st[4];
 var p=app(pb,at,pb,poff,plen);
 p=app(pb,p,lit,0,lit.len);
 st[4]=p;
 return at,p-at;
}
fn p_str(pb:u8[:], st:i64[:], poff:i64, plen:i64, lit:const u8[:], s:const u8[:])->i64,i64 {
 let at=st[4];
 var p=app(pb,at,pb,poff,plen);
 p=app(pb,p,lit,0,lit.len);
 p=app(pb,p,s,0,s.len);
 st[4]=p;
 return at,p-at;
}
fn p_dec(pb:u8[:], st:i64[:], poff:i64, plen:i64, lit:const u8[:], v:i64)->i64,i64 {
 let at=st[4];
 var p=app(pb,at,pb,poff,plen);
 p=app(pb,p,lit,0,lit.len);
 p=appd(pb,p,v);
 st[4]=p;
 return at,p-at;
}

// ---- errors ---------------------------------------------------------------
fn fail(pb:const u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64, msg:const u8[:])->i64 {
 var p=app(err,0,"JSON schema error at ",0,21);
 p=app(err,p,pb,poff,plen);
 p=app(err,p,": ",0,2);
 p=app(err,p,msg,0,msg.len);
 st[1]=1;
 st[5]=p;
 return -1;
}

// ---- JSON tree access ------------------------------------------------------
fn jkey_is(jkey:const i64[:], key:i64, strs:const u8[:], sdir:const i64[:], lit:const u8[:])->bool {
 return s_eq(strs,sdir,jkey[key],lit);
}
fn jf_find(jk:const i64[:], jf:const i64[:], jn:const i64[:], jkey:const i64[:],
           jchild:const i64[:], strs:const u8[:], sdir:const i64[:], obj:i64, lit:const u8[:])->i64 {
 if jk[obj]!=J_OBJ { return -1; }
 let base=jf[obj];
 let n=jn[obj];
 var i:i64=0;
 while i<n {
  if jkey_is(jkey,base+i,strs,sdir,lit) { return jchild[base+i]; }
  i=i+1;
 }
 return -1;
}
fn jhas(jk:const i64[:], jf:const i64[:], jn:const i64[:], jkey:const i64[:],
         jchild:const i64[:], strs:const u8[:], sdir:const i64[:], obj:i64, lit:const u8[:])->bool {
 return jf_find(jk,jf,jn,jkey,jchild,strs,sdir,obj,lit)>=0;
}

// ---- node creation ---------------------------------------------------------
fn nnew(nkind:i64[:], ncf:i64[:], ncc:i64[:], npf:i64[:], npc:i64[:], nadd:i64[:],
        nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:], nmin:i64[:], nmax:i64[:],
        nitems:i64[:], nconst:i64[:], nef:i64[:], nec:i64[:], ntarget:i64[:], st:i64[:])->i64 {
 let id=st[0];
 st[0]=id+1;
 nkind[id]=-1; ncf[id]=0; ncc[id]=0; npf[id]=0; npc[id]=0; nadd[id]=-1;
 nstr[id]=-1; nfmt[id]=0; nminl[id]=0; nmaxl[id]=-1; nmin[id]=I64_MIN; nmax[id]=I64_MAX;
 nitems[id]=-1; nconst[id]=-1; nef[id]=0; nec[id]=0; ntarget[id]=-1;
 return id;
}
fn refs_find(rk:const i64[:], rn:i64, sid:i64)->i64 {
 var i:i64=0;
 while i<rn {
  if rk[i]==sid { return i; }
  i=i+1;
 }
 return -1;
}
// index of a string in the document string table, by text
fn sid_find(strs:const u8[:], sdir:const i64[:], n:i64, tok:const u8[:])->i64 {
 var i:i64=0;
 while i<n {
  if eqb(s_at(strs,sdir,i),tok) { return i; }
  i=i+1;
 }
 return -1;
}

// ---- field readers ---------------------------------------------------------
fn get_count(jk:const i64[:], jv:const i64[:], jf:const i64[:], jn:const i64[:], jkey:const i64[:],
             jchild:const i64[:], strs:const u8[:], sdir:const i64[:], schema:i64, key:const u8[:],
             pb:const u8[:], mb:u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64, def:i64)->i64 {
 let v=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,schema,key);
 if v<0 { return def; }
 if jk[v]!=J_INT || jv[v]<0 {
  var m: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var p=app(m,0,key,0,key.len);
  p=app(m,p," must be a non-negative integer",0,31);
  return fail(pb,err,st,poff,plen,m[0..p]);
 }
 return jv[v];
}
fn get_bound(jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
             jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:],
             schema:i64, key:const u8[:], round_up:i64,
             pb:const u8[:], mb:u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64)->i64 {
 let v=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,schema,key);
 if jk[v]==J_INT { return jv[v]; }
 if jk[v]!=J_DBL {
  var m: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var p=app(m,0,key,0,key.len);
  p=app(m,p," must be a number",0,17);
  return fail(pb,err,st,poff,plen,m[0..p]);
 }
 let d=jd[v];
 if round_up!=0 { return ceil(d) as i64; }
 return floor(d) as i64;
}
fn get_format(jk:const i64[:], jv:const i64[:], jf:const i64[:], jn:const i64[:], jkey:const i64[:],
              jchild:const i64[:], strs:const u8[:], sdir:const i64[:], schema:i64,
              pb:const u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64)->i64 {
 let v=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,schema,"format");
 if v<0 { return 0; }
 if jk[v]!=J_STR {
  let m: const u8[:] = "format must be a string";
  fail(pb,err,st,poff,plen,m);
  return 0;
 }
 let sid=jv[v];
 if s_eq(strs,sdir,sid,"date") { return 2; }
 if s_eq(strs,sdir,sid,"time") { return 3; }
 if s_eq(strs,sdir,sid,"date-time") { return 4; }
 if s_eq(strs,sdir,sid,"uuid") { return 1; }
 let s=s_at(strs,sdir,sid);
 if s.len==5 {
  if s[0]=='u' && s[1]=='u' && s[2]=='i' && s[3]=='d' && s[4]>='1' && s[4]<='5' { return 1; }
 }
 return 0;
}

// ---- $ref ------------------------------------------------------------------
fn ref_fail_notfound(pb:const u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64, ref:const u8[:], sel:const u8[:])->i64 {
 var m: u8[1024] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var p=app(m,0,"cannot resolve $ref ",0,20);
 p=app(m,p,ref,0,ref.len);
 p=app(m,p,", ",0,2);
 p=app(m,p,sel,0,sel.len);
 p=app(m,p," not found",0,10);
 return fail(pb,err,st,poff,plen,m[0..p]);
}
fn ref_fail_range(pb:const u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64, ref:const u8[:], sel:const u8[:])->i64 {
 var m: u8[1024] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var p=app(m,0,"cannot resolve $ref ",0,20);
 p=app(m,p,ref,0,ref.len);
 p=app(m,p,", ",0,2);
 p=app(m,p,sel,0,sel.len);
 p=app(m,p," is out of range",0,16);
 return fail(pb,err,st,poff,plen,m[0..p]);
}

fn resolve_ref(jk:const i64[:], jf:const i64[:], jn:const i64[:], jkey:const i64[:],
               jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64, ref:const u8[:],
               pb:const u8[:], err:u8[:], st:i64[:], poff:i64, plen:i64)->i64 {
 // tokens of ref[1:] split on '/', the first (empty) token skipped
 var bounds: i64[128] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var tok_n:i64=0;
 var start:i64=1;
 var i:i64=1;
 while i<=ref.len {
  var is_sep:bool=false;
  if i==ref.len { is_sep=true; }
  else if ref[i]==(47 as u8) { is_sep=true; }
  if is_sep {
   bounds[tok_n*2+0]=start;
   bounds[tok_n*2+1]=i-start;
   tok_n=tok_n+1;
   start=i+1;
  }
  i=i+1;
 }
 var cur:i64=-2; // -2 = the document root
 var ti:i64=1;
 while ti<tok_n {
  let tob=bounds[ti*2+0];
  let tlen=bounds[ti*2+1];
  let tok=ref[tob..tob+tlen];
  if cur==-2 {
   let sid=sid_find(strs,sdir,nsid,tok);
   if sid<0 { return ref_fail_notfound(pb,err,st,poff,plen,ref,tok); }
   let v=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,st[8],tok);
   if v<0 { return ref_fail_notfound(pb,err,st,poff,plen,ref,tok); }
   cur=v;
  } else if jk[cur]==J_OBJ {
   let v=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,cur,tok);
   if v<0 { return ref_fail_notfound(pb,err,st,poff,plen,ref,tok); }
   cur=v;
  } else if jk[cur]==J_ARR {
   var idx:i64=0;
   var ok:bool=true;
   var z:i64=0;
   while z<tlen {
    let c=tok[z];
    if c<'0' || c>'9' { ok=false; z=tlen; }
    else { idx=idx*10+((c-48) as i64); z=z+1; }
   }
   if !ok || idx>=jn[cur] { return ref_fail_range(pb,err,st,poff,plen,ref,tok); }
   cur=jchild[jf[cur]+idx];
  } else {
   return ref_fail_notfound(pb,err,st,poff,plen,ref,tok);
  }
  ti=ti+1;
 }
 return cur;
}

recursive fn build_node(ji:i64, poff:i64, plen:i64, tover:i64,
                        jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                        jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64,
                        nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                        npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                        nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                        nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                        nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                        rk:i64[:], rv:i64[:], pending:i64[:],
                        pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 if jk[ji]!=J_OBJ {
  let m: const u8[:] = "schema must be an object";
  return fail(pb,err,st,poff,plen,m);
 }
 // 1. $ref
 let rvj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"$ref");
 if rvj>=0 {
  if jk[rvj]!=J_STR {
   let m: const u8[:] = "$ref must be a string";
   return fail(pb,err,st,poff,plen,m);
  }
  let refi=jv[rvj];
  let s=s_at(strs,sdir,refi);
  if !(s.len>=2 && s[0]=='#' && s[1]=='/') {
   var m: u8[1024] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
   var p=app(m,0,"unsupported $ref ",0,17);
   p=app(m,p,s,0,s.len);
   p=app(m,p,", only references into the same document are supported",0,54);
   return fail(pb,err,st,poff,plen,m[0..p]);
  }
  if refs_find(rk,st[2],refi)<0 {
   rk[st[2]]=refi;
   rv[st[2]]=-1;
   st[2]=st[2]+1;
   let tgt=resolve_ref(jk,jf,jn,jkey,jchild,strs,sdir,nsid,s,pb,err,st,poff,plen);
   if tgt<0 { return -1; }
   // ref targets build under the ref text as their path
   let rp_o,rp_l=p_lit(pb,st,0,0,s);
   let built=build_node(tgt,rp_o,rp_l,-1,
     jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,
     nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
   if built<0 { return -1; }
   let fi=refs_find(rk,st[2],refi);
   rv[fi]=built;
  }
  let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[id]=K_REF;
  nstr[id]=refi;
  pending[st[3]]=id;
  st[3]=st[3]+1;
  return id;
 }
 // 2. oneOf / anyOf
 var altj:i64=-1;
 var altk: const u8[:] = "";
 if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"oneOf") {
  altj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"oneOf");
  altk="/oneOf";
 } else if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"anyOf") {
  altj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"anyOf");
  altk="/anyOf";
 }
 if altj>=0 {
  let path_o,path_l=p_lit(pb,st,poff,plen,altk);
  return build_alts(altj,path_o,path_l,K_ANY_OF,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,
    nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
 }
 // 3. type
 var typej:i64=tover;
 if typej<0 { typej=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"type"); }
 if typej>=0 && jk[typej]==J_ARR {
  let n=jn[typej];
  if n==0 {
   let m: const u8[:] = "type must not be empty";
   return fail(pb,err,st,poff,plen,m);
  }
  let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[id]=K_ANY_OF;
  ncf[id]=st[9];
  var i:i64=0;
  while i<n {
   let t=jchild[jf[typej]+i];
   let path_o,path_l=p_dec(pb,st,poff,plen,"/type/",i);
   let alt=build_node(ji,path_o,path_l,t,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
   if alt<0 { return -1; }
   nchild[st[9]]=alt;
   st[9]=st[9]+1;
   i=i+1;
  }
  ncc[id]=st[9]-ncf[id];
  return id;
 }
 // 4. const
 let cj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"const");
 if cj>=0 {
  let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[id]=K_CONST;
  nconst[id]=cj;
  return id;
 }
 // 5. enum
 let ej=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"enum");
 if ej>=0 {
  if jk[ej]!=J_ARR || jn[ej]==0 {
   let m: const u8[:] = "enum must be a non-empty array";
   return fail(pb,err,st,poff,plen,m);
  }
  let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[id]=K_ENUM;
  nef[id]=st[10];
  var i:i64=0;
  let n=jn[ej];
  while i<n {
   eval[st[10]]=jchild[jf[ej]+i];
   st[10]=st[10]+1;
   i=i+1;
  }
  nec[id]=n;
  return id;
 }
 // 6. type must be a string when present
 if typej>=0 && jk[typej]!=J_STR {
  let m: const u8[:] = "type must be a string or an array of strings";
  return fail(pb,err,st,poff,plen,m);
 }
 var tsid:i64=-1;
 if typej>=0 { tsid=jv[typej]; }
 // has_properties
 var hasp:bool = jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"properties");
 if !hasp {
  let ap=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"additionalProperties");
  if ap>=0 {
   if !(jk[ap]==J_BOOL && jv[ap]!=0) { hasp=true; }
  }
 }
 if tsid<0 {
  if hasp {
   return build_object(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"allOf") {
   let aj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"allOf");
   let path_o,path_l=p_lit(pb,st,poff,plen,"/allOf");
   return build_alts(aj,path_o,path_l,K_ALL_OF,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,
     nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"items") || jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"prefixItems") {
   return build_array(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"pattern") || jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"minLength") || jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"maxLength") {
   return build_string(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  let fmt=get_format(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,pb,err,st,poff,plen);
  if st[1]!=0 { return -1; }
  if fmt!=0 {
   return build_string(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[id]=K_ANY;
  return id;
 }
 if s_eq(strs,sdir,tsid,"object") {
  if !hasp && jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"allOf") {
   let aj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"allOf");
   let path_o,path_l=p_lit(pb,st,poff,plen,"/allOf");
   return build_alts(aj,path_o,path_l,K_ALL_OF,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,
     nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  return build_object(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
 }
 if s_eq(strs,sdir,tsid,"string") {
  if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"allOf") {
   let aj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"allOf");
   let path_o,path_l=p_lit(pb,st,poff,plen,"/allOf");
   return build_alts(aj,path_o,path_l,K_ALL_OF,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,
     nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  }
  return build_string(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
 }
 if s_eq(strs,sdir,tsid,"array") {
  return build_array(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
 }
 if s_eq(strs,sdir,tsid,"integer") {
  return build_integer(ji,poff,plen,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
 }
 let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 if s_eq(strs,sdir,tsid,"number") { nkind[id]=K_NUMBER; return id; }
 if s_eq(strs,sdir,tsid,"boolean") { nkind[id]=K_BOOLEAN; return id; }
 if s_eq(strs,sdir,tsid,"null") { nkind[id]=K_NULL; return id; }
 var m: u8[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var q=app(m,0,"unrecognized type ",0,18);
 let tn=s_at(strs,sdir,tsid);
 q=app(m,q,tn,0,tn.len);
 return fail(pb,err,st,poff,plen,m[0..q]);
}

recursive fn build_alts(aj:i64, poff:i64, plen:i64, kind:i64,
                        jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                        jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64,
                        nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                        npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                        nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                        nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                        nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                        rk:i64[:], rv:i64[:], pending:i64[:],
                        pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 if jk[aj]!=J_ARR {
  let m: const u8[:] = "must be an array of schemas";
  return fail(pb,err,st,poff,plen,m);
 }
 let n=jn[aj];
 if n==0 {
  let m: const u8[:] = "must not be empty";
  return fail(pb,err,st,poff,plen,m);
 }
 let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 nkind[id]=kind;
 var tmp: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
 var i:i64=0;
 while i<n {
  let ch=jchild[jf[aj]+i];
  let path_o,path_l=p_dec(pb,st,poff,plen,"/",i);
  let alt=build_node(ch,path_o,path_l,-1,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  if alt<0 { return -1; }
  tmp[i]=alt;
  i=i+1;
 }
 ncf[id]=st[9];
 var z:i64=0;
 while z<n {
  nchild[st[9]]=tmp[z];
  st[9]=st[9]+1;
  z=z+1;
 }
 ncc[id]=n;
 return id;
}

recursive fn build_object(ji:i64, poff:i64, plen:i64,
                          jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                          jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64,
                          nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                          npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                          nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                          nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                          nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                          rk:i64[:], rv:i64[:], pending:i64[:],
                          pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 nkind[id]=K_OBJECT;
 let reqj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"required");
 var req_base:i64=0;
 var req_n:i64=0;
 if reqj>=0 && jk[reqj]==J_ARR { req_base=jf[reqj]; req_n=jn[reqj]; }
 let propj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"properties");
 if propj>=0 {
  if jk[propj]!=J_OBJ {
   let m: const u8[:] = "properties must be an object";
   return fail(pb,err,st,poff,plen,m);
  }
  let base=jf[propj];
  let n=jn[propj];
  var tname: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var tnode: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var treq: i64[256] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  var i:i64=0;
  while i<n {
   let nameid=jkey[base+i];
   let child=jchild[base+i];
   let nm=s_at(strs,sdir,nameid);
   let path_o,path_l=p_str(pb,st,poff,plen,"/properties/",nm);
   let pn=build_node(child,path_o,path_l,-1,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
   if pn<0 { return -1; }
   var isreq:i64=0;
   var r:i64=0;
   while r<req_n {
    let rn=jchild[req_base+r];
    if jk[rn]==J_STR {
     if s_eq(strs,sdir,jv[rn],nm) { isreq=1; r=req_n; } else { r=r+1; }
    } else { r=r+1; }
   }
   tname[i]=nameid;
   tnode[i]=pn;
   treq[i]=isreq;
   i=i+1;
  }
  npf[id]=st[11];
  var z:i64=0;
  while z<n {
   pname[st[11]]=tname[z];
   pnode[st[11]]=tnode[z];
   preq[st[11]]=treq[z];
   st[11]=st[11]+1;
   z=z+1;
  }
  npc[id]=n;
 }
 let ap=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"additionalProperties");
 if ap>=0 {
  if jk[ap]==J_BOOL {
   if jv[ap]!=0 {
    let anyid=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
    nkind[anyid]=K_ANY;
    nadd[id]=anyid;
   }
  } else if jk[ap]==J_OBJ {
   let path_o,path_l=p_lit(pb,st,poff,plen,"/additionalProperties");
   let an=build_node(ap,path_o,path_l,-1,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
   if an<0 { return -1; }
   nadd[id]=an;
  } else {
   let m: const u8[:] = "additionalProperties must be a boolean or a schema";
   return fail(pb,err,st,poff,plen,m);
  }
 } else if !jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"properties") {
  let anyid=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[anyid]=K_ANY;
  nadd[id]=anyid;
 }
 return id;
}

recursive fn build_array(ji:i64, poff:i64, plen:i64,
                         jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                         jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64,
                         nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                         npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                         nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                         nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                         nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                         rk:i64[:], rv:i64[:], pending:i64[:],
                         pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 let has_items=jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"items");
 if has_items || jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"prefixItems") {
  let key: const u8[:] = if has_items { "items" } else { "prefixItems" };
  let keypath: const u8[:] = if has_items { "/items" } else { "/prefixItems" };
  let keypath_slash: const u8[:] = if has_items { "/items/" } else { "/prefixItems/" };
  let ij=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,key);
  if jk[ij]==J_ARR {
   let tid=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
   nkind[tid]=K_TUPLE;
   ncf[tid]=st[9];
   let n=jn[ij];
   var i:i64=0;
   while i<n {
    let c=jchild[jf[ij]+i];
    let path_o,path_l=p_dec(pb,st,poff,plen,keypath_slash,i);
    let it=build_node(c,path_o,path_l,-1,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
    if it<0 { return -1; }
    nchild[st[9]]=it;
    st[9]=st[9]+1;
    i=i+1;
   }
   ncc[tid]=st[9]-ncf[tid];
   return tid;
  }
  let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
  nkind[id]=K_ARRAY;
  let path_o,path_l=p_lit(pb,st,poff,plen,keypath);
  let it=build_node(ij,path_o,path_l,-1,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,nsid,nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
  if it<0 { return -1; }
  nitems[id]=it;
  nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"minItems",pb,mb,err,st,poff,plen,0);
  if st[1]!=0 { return -1; }
  nmaxl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"maxItems",pb,mb,err,st,poff,plen,-1);
  if st[1]!=0 { return -1; }
  return id;
 }
 let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 nkind[id]=K_ARRAY;
 let anyid=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 nkind[anyid]=K_ANY;
 nitems[id]=anyid;
 nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"minItems",pb,mb,err,st,poff,plen,0);
 if st[1]!=0 { return -1; }
 nmaxl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"maxItems",pb,mb,err,st,poff,plen,-1);
 if st[1]!=0 { return -1; }
 return id;
}

recursive fn build_string(ji:i64, poff:i64, plen:i64,
                          jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                          jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64,
                          nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                          npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                          nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                          nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                          nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                          rk:i64[:], rv:i64[:], pending:i64[:],
                          pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 nkind[id]=K_STRING;
 let pj=jf_find(jk,jf,jn,jkey,jchild,strs,sdir,ji,"pattern");
 if pj>=0 {
  if jk[pj]!=J_STR {
   let m: const u8[:] = "pattern must be a string";
   return fail(pb,err,st,poff,plen,m);
  }
  if sdir[2*jv[pj]+1]==0 { nstr[id]=-1; } else { nstr[id]=jv[pj]; }
 }
 let fmt=get_format(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,pb,err,st,poff,plen);
 if st[1]!=0 { return -1; }
 nfmt[id]=fmt;
 nminl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"minLength",pb,mb,err,st,poff,plen,0);
 if st[1]!=0 { return -1; }
 nmaxl[id]=get_count(jk,jv,jf,jn,jkey,jchild,strs,sdir,ji,"maxLength",pb,mb,err,st,poff,plen,-1);
 if st[1]!=0 { return -1; }
 return id;
}

recursive fn build_integer(ji:i64, poff:i64, plen:i64,
                           jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                           jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:], nsid:i64,
                           nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                           npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                           nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                           nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                           nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                           rk:i64[:], rv:i64[:], pending:i64[:],
                           pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 let id=nnew(nkind,ncf,ncc,npf,npc,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,ntarget,st);
 nkind[id]=K_INTEGER;
 if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"minimum") {
  nmin[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"minimum",1,pb,mb,err,st,poff,plen);
  if st[1]!=0 { return -1; }
 } else if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMinimum") {
  nmin[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMinimum",0,pb,mb,err,st,poff,plen)+1;
  if st[1]!=0 { return -1; }
 }
 if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"maximum") {
  nmax[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"maximum",0,pb,mb,err,st,poff,plen);
  if st[1]!=0 { return -1; }
 } else if jhas(jk,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMaximum") {
  nmax[id]=get_bound(jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ji,"exclusiveMaximum",1,pb,mb,err,st,poff,plen)-1;
  if st[1]!=0 { return -1; }
 }
 return id;
}

// Document entry: build root, then point every pending ref at its map target.
export fn schema_build(root:i64, ndocsid:i64,
                       jk:const i64[:], jv:const i64[:], jd:const f64[:], jf:const i64[:], jn:const i64[:],
                       jkey:const i64[:], jchild:const i64[:], strs:const u8[:], sdir:const i64[:],
                       nkind:i64[:], ncf:i64[:], ncc:i64[:], nchild:i64[:],
                       npf:i64[:], npc:i64[:], pname:i64[:], pnode:i64[:], preq:i64[:],
                       nadd:i64[:], nstr:i64[:], nfmt:i64[:], nminl:i64[:], nmaxl:i64[:],
                       nmin:i64[:], nmax:i64[:], nitems:i64[:], nconst:i64[:],
                       nef:i64[:], nec:i64[:], eval:i64[:], ntarget:i64[:],
                       rk:i64[:], rv:i64[:], pending:i64[:],
                       pb:u8[:], mb:u8[:], err:u8[:], st:i64[:])->i64 {
 st[0]=0; st[1]=0; st[2]=0; st[3]=0; st[4]=0; st[5]=0; st[6]=0;
 st[8]=root; st[9]=0; st[10]=0; st[11]=0;
 let ro_o,ro_l=p_lit(pb,st,0,0,"#");
 let r=build_node(root,ro_o,ro_l,-1,jk,jv,jd,jf,jn,jkey,jchild,strs,sdir,ndocsid,
   nkind,ncf,ncc,nchild,npf,npc,pname,pnode,preq,nadd,nstr,nfmt,nminl,nmaxl,nmin,nmax,nitems,nconst,nef,nec,eval,ntarget,rk,rv,pending,pb,mb,err,st);
 if r<0 { return -1; }
 var i:i64=0;
 while i<st[3] {
  let nid=pending[i];
  let sid=nstr[nid];
  let fi=refs_find(rk,st[2],sid);
  if fi>=0 { ntarget[nid]=rv[fi]; }
  i=i+1;
 }
 return r;
}

