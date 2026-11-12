// Multipart / path-parameter helpers in Goose, from tools/server/server-http.cpp
// (decode_path_component, get_params) and server_http_context::post (multipart
// form translation).
//
// Owned decisions:
//   1. path-component percent-decoding: "%HH" is decoded only when both digits
//      are hex; anything else is copied literally (no '+' handling, that is
//      query-string semantics and lives in httplib);
//   2. form-field grouping: the first occurrence of a key becomes a JSON string,
//      later occurrences append to an array, comparing raw key bytes;
//   3. parameter precedence: a path parameter overrides a query parameter of the
//      same name, and only the path value is percent-decoded.
//
// The multipart boundary parsing itself stays with httplib.
extern "fg452_ld_u8" fn ld_u8(p:u64,o:u64)->u32;
extern "fg452_ld_i64" fn ld_i64(p:u64,o:u64)->i64;
extern "fg452_st_u8" fn st_u8(p:u64,o:u64,v:u32);
extern "fg452_st_i64" fn st_i64(p:u64,o:u64,v:i64);

fn hexval(c:u32)->i64{
 if c>=48 as u32 { if c<=57 as u32 { return (c-48 as u32) as i64; } }
 if c>=97 as u32 { if c<=102 as u32 { return ((c-97 as u32) as i64)+10 as i64; } }
 if c>=65 as u32 { if c<=70 as u32 { return ((c-65 as u32) as i64)+10 as i64; } }
 return -(1 as i64);
}

// Decode into out (capacity cap bytes) and return the full decoded length.
fn decode_component(p:u64,n:i64,out:u64,cap:i64)->i64{
 var o:i64=0 as i64;
 var i:i64=0 as i64;
 while i<n {
  var emit:i64=1 as i64;
  if ld_u8(p,i as u64)==37 as u32 {
   if i+2 < n {
    let hi:i64=hexval(ld_u8(p,(i+1 as i64) as u64));
    let lo:i64=hexval(ld_u8(p,(i+2 as i64) as u64));
    var ok:i64=0 as i64;
    if hi>=0 as i64 { if lo>=0 as i64 { ok=1 as i64; } }
    if ok!=0 as i64 {
     if o<cap { st_u8(out,o as u64,(((hi<<4)|lo) as u32)); }
     o=o+1 as i64;
     i=i+3 as i64;
     emit=0 as i64;
    }
   }
  }
  if emit!=0 as i64 {
   if o<cap { st_u8(out,o as u64,ld_u8(p,i as u64)); }
   o=o+1 as i64;
   i=i+1 as i64;
  }
 }
 return o;
}

fn key_eq(keys:u64,ao:i64,an:i64,bo:i64,bn:i64)->i64{
 if an!=bn {return 0 as i64;}
 var i:i64=0 as i64;
 while i<an {
  if ld_u8(keys,((ao+i) as u64))!=ld_u8(keys,((bo+i) as u64)){return 0 as i64;}
  i=i+1 as i64;
 }
 return 1 as i64;
}

// 1 when an earlier field already used this key (native appends to an array),
// 0 for the first occurrence. koffs/klens are i64 arrays of length idx.
fn form_repeat(idx:i64,keys:u64,koffs:u64,klens:u64)->i64{
 let bo:i64=ld_i64(koffs,(idx*(8 as i64)) as u64);
 let bn:i64=ld_i64(klens,(idx*(8 as i64)) as u64);
 var j:i64=0 as i64;
 while j<idx {
  let ao:i64=ld_i64(koffs,(j*(8 as i64)) as u64);
  let an:i64=ld_i64(klens,(j*(8 as i64)) as u64);
  let same:i64=key_eq(keys,ao,an,bo,bn);
  if same!=0 as i64 { return 1 as i64; }
  j=j+1 as i64;
 }
 return 0 as i64;
}

// Writes one flag per field: 0 -> new JSON string, 1 -> append to the array.
fn form_actions(n:i64,keys:u64,koffs:u64,klens:u64,out:u64)->i64{
 var i:i64=0 as i64;
 while i<n {
  let r:i64=form_repeat(i,keys,koffs,klens);
  st_i64(out,(i*(8 as i64)) as u64,r);
  i=i+1 as i64;
 }
 return n;
}

// Parameter source: 0 absent, 1 query value, 2 path value (decoded, wins).
fn param_source(has_query:i64,has_path:i64)->i64{
 if has_path!=0 as i64 { return 2 as i64; }
 if has_query!=0 as i64 { return 1 as i64; }
 return 0 as i64;
}

fn main(){
 decode_component(0 as u64,0 as i64,0 as u64,0 as i64);
 form_actions(0 as i64,0 as u64,0 as u64,0 as u64,0 as u64);
 param_source(0 as i64,0 as i64);
}
