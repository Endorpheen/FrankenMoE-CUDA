// PEG AST helpers in Goose, from common/peg-parser.cpp:
// common_peg_ast_arena::find_by_tag / find_by_rule / visit,
// common_peg_ast_node::sanitized_text and
// common_peg_parse_result_type_name.
//
// Owned decisions:
//   - a lookup walks the children in order and never matches the node it is
//     called on; a match returns immediately, otherwise the child is searched
//     when the remaining depth allows it (max_depth > 1, decremented);
//   - visit is a pre-order DFS from the root (the node, then its children);
//   - sanitized_text replaces each recorded invalid run with U+FFFD and keeps
//     the rest of the text byte for byte;
//   - the result-type names are fail/success/need_more_input, "unknown" else.
import std;

const R_FAIL:i64=0;
const R_SUCCESS:i64=1;
const R_NEED:i64=2;

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
fn eqb(a:const u8[:], b:const u8[:])->bool {
 if a.len!=b.len { return false; }
 var i:i64=0;
 while i<a.len {
  if a[i]!=b[i] { return false; }
  i=i+1;
 }
 return true;
}
fn str_of(strs:const u8[:], sdir:const i64[:], sid:i64)->const u8[:] {
 if sid<0 { return strs[0..0]; }
 let o=sdir[2*sid+0];
 let l=sdir[2*sid+1];
 return strs[o..o+l];
}

// ---- name tables -----------------------------------------------------------
export fn result_type_name(t:i64, out:u8[:])->i64 {
 var n: const u8[:] = "unknown";
 if t==R_FAIL { n="fail"; }
 else if t==R_SUCCESS { n="success"; }
 else if t==R_NEED { n="need_more_input"; }
 return app(out,0,n,0,n.len);
}

// ---- lookups ---------------------------------------------------------------
// kind 0 matches the tag, 1 the rule
recursive fn find_rec(parent:i64, kind:i64, want:const u8[:], depth:i64,
                      a_tag:const i64[:], a_rule:const i64[:], a_cs:const i64[:], a_cc:const i64[:],
                      achild:const i64[:], strs:const u8[:], sdir:const i64[:])->i64 {
 var i:i64=0;
 let n=a_cc[parent];
 let base=a_cs[parent];
 while i<n {
  let ch=achild[base+i];
  var sid:i64=-1;
  if kind==0 { sid=a_tag[ch]; } else { sid=a_rule[ch]; }
  if sid>=0 {
   if eqb(str_of(strs,sdir,sid),want) { return ch; }
  }
  if depth>1 {
   let r=find_rec(ch,kind,want,depth-1,a_tag,a_rule,a_cs,a_cc,achild,strs,sdir);
   if r>=0 { return r; }
  }
  i=i+1;
 }
 return -1;
}

export fn ast_find(parent:i64, kind:i64, want:const u8[:], depth:i64,
                   a_tag:const i64[:], a_rule:const i64[:], a_cs:const i64[:], a_cc:const i64[:],
                   achild:const i64[:], strs:const u8[:], sdir:const i64[:])->i64 {
 return find_rec(parent,kind,want,depth,a_tag,a_rule,a_cs,a_cc,achild,strs,sdir);
}

// ---- visit -----------------------------------------------------------------
recursive fn visit_rec(id:i64, a_cs:const i64[:], a_cc:const i64[:], achild:const i64[:],
                       out:i64[:], st:i64[:])->i64 {
 if id<0 { return 0; }
 out[st[0]]=id;
 st[0]=st[0]+1;
 var i:i64=0;
 let n=a_cc[id];
 let base=a_cs[id];
 while i<n {
  visit_rec(achild[base+i],a_cs,a_cc,achild,out,st);
  i=i+1;
 }
 return st[0];
}

export fn ast_visit(root:i64, a_cs:const i64[:], a_cc:const i64[:], achild:const i64[:],
                    out:i64[:], st:i64[:])->i64 {
 st[0]=0;
 return visit_rec(root,a_cs,a_cc,achild,out,st);
}

// ---- sanitized text --------------------------------------------------------
export fn ast_sanitized(id:i64, input:const u8[:], a_toff:const i64[:], a_tlen:const i64[:],
                        a_is:const i64[:], a_ic:const i64[:], inv:const i64[:],
                        out:u8[:])->i64 {
 let off=a_toff[id];
 let len=a_tlen[id];
 let nrun=a_ic[id];
 if nrun==0 { return app(out,0,input,off,len); }
 var p:i64=0;
 var cur:i64=0;
 var r:i64=0;
 while r<nrun {
  let ri=a_is[id]+r;
  let pos=inv[ri*2+0]-off;
  var run_len=inv[ri*2+1];
  if pos>cur {
   p=app(out,p,input,off+cur,pos-cur);
  }
  p=appc(out,p,239);
  p=appc(out,p,191);
  p=appc(out,p,189);
  cur=pos+run_len;
  r=r+1;
 }
 if cur<len { p=app(out,p,input,off+cur,len-cur); }
 return p;
}
