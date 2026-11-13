// Unicode lowercase mapping in Goose (APX: P7-05).
//
// From unicode.cpp unicode_tolower: a std::lower_bound over unicode_map_lowercase, then an equality
// check, falling back to the input codepoint. The pre-tokenizer calls it after an apostrophe to
// match (?i:'s|'t|'re|'ve|'m|'ll|'d), so it decides whether "don't" splits as one token.
//
// Only the ALGORITHM moves. The 11k-entry table is project data, not logic, so it stays where it
// is: the bridge hands Goose the table pointer and its length. Copying the table into a Goose array
// would duplicate data that already has an owner, and any future edit to it would then have to be
// made twice.
//
// lower_bound semantics, reproduced exactly:
//   - find the first entry whose key >= cpt (a lower_bound, not a search for equality);
//   - return its value only if that key is exactly cpt;
//   - otherwise return cpt unchanged.

extern "fg218t_fail" fn fail(code:i64);
extern "fg218t_ld_u32" fn ld_u32(base:u64,idx:i64)->i64;

fn ensure(v:bool,c:i64){if !v{fail(c);}}

// key of entry i: the table is an array of {uint32 key, uint32 value} pairs, so entry i starts at
// byte 8*i and its value sits 4 bytes further on.
fn key_at(tab:u64,i:i64)->i64{ld_u32(tab,i*(8 as i64))}
fn val_at(tab:u64,i:i64)->i64{ld_u32(tab,i*(8 as i64)+4 as i64)}

// tolower returns the lowercase mapping of `cpt`, or `cpt` when the table has no entry for it.
fn tolower(cpt:i64,tab:u64,n:i64)->i64{
 ensure(cpt>=0,300);
 ensure(n>=0,301);
 var lo:i64=0;
 var hi:i64=n;
 while lo<hi{
  let mid:i64=lo+(hi-lo)/(2 as i64);
  if key_at(tab,mid)<cpt{lo=mid+1 as i64;} else {hi=mid;}
 }
 if lo<n{
  if key_at(tab,lo)==cpt{return val_at(tab,lo);}
 }
 cpt
}

fn main(){
 tolower(0x41 as i64,0 as u64,0 as i64);
 tolower(0x61 as i64,8 as u64,1 as i64);
 tolower(0x20ac as i64,16 as u64,3 as i64);
}
