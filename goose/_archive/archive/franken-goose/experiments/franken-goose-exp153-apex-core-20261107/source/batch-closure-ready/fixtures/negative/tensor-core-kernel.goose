// Negative fixture: witness is imported but omitted from the header, and get
// is declared with the wrong arity. Expect HEADER_MISSING and ARITY_MISMATCH.
extern "fg153_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg153_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_witness" fn witness(mode:i64);
fn entry(mode:i64)->i64 {witness(mode);return get(0,1,0);}
