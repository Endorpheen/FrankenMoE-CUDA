// Positive fixture: every imported symbol is declared with matching arity.
extern "fg153_get" fn get(p:i64,k:i64,i:i64)->i64;
extern "fg153_put" fn put(p:i64,k:i64,i:i64,v:i64);
extern "fg153_service" fn service(op:i64,a:i64,b:i64)->i64;
extern "fg153_witness" fn witness(mode:i64);
fn entry(mode:i64)->i64 {witness(mode);return get(0,1,0);}
