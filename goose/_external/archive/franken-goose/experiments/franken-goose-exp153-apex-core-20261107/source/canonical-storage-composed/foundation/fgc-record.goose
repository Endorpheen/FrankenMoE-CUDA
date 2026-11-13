// fgc-record.goose - APX030 canonical record field store.
//
// A record is a typed (kind) set of signed i64 key -> i64 value fields stored
// in insertion order.  Field schemas belong to the owning worker; this module
// only stores/retrieves whatever signed keys the schema publishes.
//
// Entry modes:
//   0 new(kind)          -> handle or 0
//   1 get(h,key)         -> value, or 0 when absent
//   2 set(h,key,value)   -> status
//   3 kind(h)            -> kind, or -1 for a non-record handle
//   4 free(h)            -> 0
//   5 clear(h)           -> status (n=0, capacity kept)
//   6 count(h)           -> number of fields
//
// Growth allocates both arrays before replacing them, so an allocation failure
// leaves the record fully intact.  Service primitives are raw malloc/free/
// memcpy plus indexed i64 access and one CAS free guard.

extern "fgc_alloc" fn alloc(bytes:i64)->i64;
extern "fgc_free_ptr" fn release(ptr:i64,bytes:i64);
extern "fgc_memcpy" fn memcpy(dst:i64,src:i64,bytes:i64)->i64;
extern "fgc_load_i64" fn load(base:i64,i:i64)->i64;
extern "fgc_store_i64" fn store(base:i64,i:i64,v:i64);
extern "fgc_cas_i64" fn cas(base:i64,i:i64,expected:i64,desired:i64)->i64;
extern "fgc_abort" fn fail(code:i64);
extern "fgc_witness" fn witness(mode:i64);

fn ensure(ok:bool) {if !ok {fail(1);assert(false);}}
fn imax()->i64 {return 9223372036854775807;}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=imax()-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=imax()/a));return a*b;}

fn rmagic()->i64 {return 1380270897;}
fn get(h:i64,f:i64)->i64 {return load(h,f);}
fn put(h:i64,f:i64,v:i64) {store(h,f,v);}
fn is_rec(h:i64)->bool {return h!=0&&get(h,0)==rmagic();}

fn rec_new(kind:i64)->i64 {
    if kind<0 {return 0;}
    let h=alloc(48);
    if h==0 {return 0;}
    put(h,0,rmagic());put(h,1,kind);put(h,2,0);put(h,3,0);put(h,4,0);put(h,5,0);
    witness(0);
    return h;
}

fn rec_find(h:i64,key:i64)->i64 {
    let n=get(h,2);let keys=get(h,4);
    var i:i64=0;
    while i<n {
        if load(keys,i)==key {return i;}
        i+=1;
    }
    return -1;
}

fn rec_get(h:i64,key:i64)->i64 {
    if !is_rec(h) {return 0;}
    let i=rec_find(h,key);
    if i<0 {return 0;}
    return load(get(h,5),i);
}

fn rec_grow(h:i64)->i64 {
    let cap=get(h,3);
    var newcap=4;
    if cap>=4 {
        if cap>imax()/16 {return -3;}
        newcap=cap*2;
    }
    let bytes=mul(newcap,8);
    let nk=alloc(bytes);
    if nk==0 {witness(20);return -2;}
    let nv=alloc(bytes);
    if nv==0 {release(nk,bytes);witness(20);return -2;}
    let n=get(h,2);
    let keys=get(h,4);let vals=get(h,5);
    if n>0 {let ignored=memcpy(nk,keys,mul(n,8));let ignored2=memcpy(nv,vals,mul(n,8));}
    if keys!=0 {release(keys,mul(cap,8));}
    if vals!=0 {release(vals,mul(cap,8));}
    put(h,4,nk);put(h,5,nv);put(h,3,newcap);
    witness(21);
    return 0;
}

fn rec_set(h:i64,key:i64,value:i64)->i64 {
    if !is_rec(h) {return -1;}
    let i=rec_find(h,key);
    if i>=0 {store(get(h,5),i,value);return 0;}
    let n=get(h,2);
    if n>=get(h,3) {
        let r=rec_grow(h);
        if r!=0 {return r;}
    }
    store(get(h,4),n,key);
    store(get(h,5),n,value);
    put(h,2,add(n,1));
    return 0;
}

fn rec_kind(h:i64)->i64 {if !is_rec(h) {return -1;}return get(h,1);}
fn rec_count(h:i64)->i64 {if !is_rec(h) {return 0;}return get(h,2);}
fn rec_clear(h:i64)->i64 {if !is_rec(h) {return -1;}put(h,2,0);return 0;}

fn rec_free(h:i64) {
    if h==0 {return;}
    let old=cas(h,0,rmagic(),0);
    if old!=rmagic() {return;}
    let cap=get(h,3);let keys=get(h,4);let vals=get(h,5);
    if keys!=0 {release(keys,mul(cap,8));}
    if vals!=0 {release(vals,mul(cap,8));}
    release(h,48);
    witness(22);
}

fn entry(op:i64,h:i64,a:i64,b:i64)->i64 {
    witness(op);
    if op==0 {return rec_new(h);}
    if op==1 {return rec_get(h,a);}
    if op==2 {return rec_set(h,a,b);}
    if op==3 {return rec_kind(h);}
    if op==4 {rec_free(h);return 0;}
    if op==5 {return rec_clear(h);}
    if op==6 {return rec_count(h);}
    ensure(false);
    return -1;
}

fn main() {print(entry(1,0,0,0));}
