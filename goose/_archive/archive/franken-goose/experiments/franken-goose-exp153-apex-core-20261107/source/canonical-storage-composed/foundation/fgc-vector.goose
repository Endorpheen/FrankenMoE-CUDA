// fgc-vector.goose - APX030 canonical dynamic POD vector and byte/string store.
//
// Entry modes (all handles are int64 pointers owned by the caller):
//   0  new(item_bytes)          -> handle or 0
//   1  len(h)                   -> count
//   2  data(h)                  -> ptr or 0
//   3  reserve(h,count)         -> status
//   4  resize(h,count)          -> status
//   5  free(h)                  -> 0
//   6  clear(h)                 -> status
//   7  bytes_append(h,src,n)    -> status  (item_bytes must be 1)
//   8  str_set(h,src,n)         -> status  (stores n bytes + NUL)
//   9  str_data(h)              -> ptr
//   10 str_len(h)               -> bytes before NUL
//   11 item_bytes(h)            -> size of one item
//
// Growth, checked multiplication, rollback, zero-fill and ownership checks are
// Goose.  Services only allocate/free/copy/zero raw memory and expose indexed
// loads/stores plus one CAS for the free guard.

extern "fgc_alloc" fn alloc(bytes:i64)->i64;
extern "fgc_free_ptr" fn release(ptr:i64,bytes:i64);
extern "fgc_memcpy" fn memcpy(dst:i64,src:i64,bytes:i64)->i64;
extern "fgc_memzero" fn memzero(dst:i64,bytes:i64)->i64;
extern "fgc_add_ptr" fn add_ptr(ptr:i64,bytes:i64)->i64;
extern "fgc_load_i64" fn load(base:i64,i:i64)->i64;
extern "fgc_store_i64" fn store(base:i64,i:i64,v:i64);
extern "fgc_load_u8" fn load8(base:i64,i:i64)->i64;
extern "fgc_store_u8" fn store8(base:i64,i:i64,v:i64);
extern "fgc_cas_i64" fn cas(base:i64,i:i64,expected:i64,desired:i64)->i64;
extern "fgc_abort" fn fail(code:i64);
extern "fgc_witness" fn witness(mode:i64);

fn ensure(ok:bool) {if !ok {fail(1);assert(false);}}
fn imax()->i64 {return 9223372036854775807;}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=imax()-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=imax()/a));return a*b;}
fn sub(a:i64,b:i64)->i64 {ensure(a>=b);return a-b;}

fn vmagic()->i64 {return 1447379761;}
fn get(h:i64,f:i64)->i64 {return load(h,f);}
fn put(h:i64,f:i64,v:i64) {store(h,f,v);}
fn is_vec(h:i64)->bool {return h!=0&&get(h,0)==vmagic();}

fn vec_new(item_bytes:i64)->i64 {
    if item_bytes<=0 {return 0;}
    let h=alloc(40);
    if h==0 {return 0;}
    put(h,0,vmagic());put(h,1,item_bytes);put(h,2,0);put(h,3,0);put(h,4,0);
    witness(0);
    return h;
}

fn vec_reserve(h:i64,count:i64)->i64 {
    if !is_vec(h)||count<0 {return -1;}
    let cap=get(h,3);let ib=get(h,1);
    if count<=cap {return 0;}
    let maxc=imax()/ib;
    if count>maxc {witness(20);return -3;}
    var newcap=8;
    if cap>=8 {
        if cap>maxc/2 {newcap=count;} else {newcap=cap*2;}
    }
    if newcap<count {newcap=count;}
    if newcap>maxc {return -3;}
    let bytes=mul(newcap,ib);
    let p=alloc(bytes);
    if p==0 {witness(21);return -2;}
    let len=get(h,2);let old=get(h,4);
    if len>0 {let ignored=memcpy(p,old,mul(len,ib));}
    if old!=0 {release(old,mul(cap,ib));}
    put(h,4,p);put(h,3,newcap);
    witness(22);
    return 0;
}

fn vec_resize(h:i64,count:i64)->i64 {
    if !is_vec(h)||count<0 {return -1;}
    if count>get(h,3) {
        let r=vec_reserve(h,count);
        if r!=0 {return r;}
    }
    let len=get(h,2);let ib=get(h,1);
    if count<len {put(h,2,count);return 0;}
    if count>len {
        let data=get(h,4);
        let off=mul(len,ib);
        let n=mul(count-len,ib);
        let ignored=memzero(add_ptr(data,off),n);
    }
    put(h,2,count);
    return 0;
}

fn vec_len(h:i64)->i64 {if !is_vec(h) {return 0;}return get(h,2);}
fn vec_data(h:i64)->i64 {if !is_vec(h) {return 0;}return get(h,4);}
fn vec_item(h:i64)->i64 {if !is_vec(h) {return -1;}return get(h,1);}
fn vec_clear(h:i64)->i64 {if !is_vec(h) {return -1;}put(h,2,0);return 0;}

fn vec_free(h:i64) {
    if h==0 {return;}
    let old=cas(h,0,vmagic(),0);
    if old!=vmagic() {return;}
    let cap=get(h,3);let ib=get(h,1);let data=get(h,4);
    if data!=0 {release(data,mul(cap,ib));}
    release(h,40);
    witness(23);
}

fn bytes_append(h:i64,src:i64,n:i64)->i64 {
    if !is_vec(h)||get(h,1)!=1||n<0 {return -1;}
    if n>0&&src==0 {return -1;}
    let len=get(h,2);
    let total=add(len,n);
    let r=vec_reserve(h,total);
    if r!=0 {return r;}
    if n>0 {let ignored=memcpy(add_ptr(get(h,4),len),src,n);}
    put(h,2,total);
    return 0;
}

fn str_set(h:i64,src:i64,n:i64)->i64 {
    if !is_vec(h)||get(h,1)!=1||n<0 {return -1;}
    if n>0&&src==0 {return -1;}
    let total=add(n,1);
    let r=vec_reserve(h,total);
    if r!=0 {return r;}
    if n>0 {let ignored=memcpy(get(h,4),src,n);}
    store8(get(h,4),n,0);
    put(h,2,total);
    return 0;
}

fn str_len(h:i64)->i64 {
    if !is_vec(h)||get(h,1)!=1 {return 0;}
    let len=get(h,2);
    if len==0 {return 0;}
    return sub(len,1);
}
fn str_data(h:i64)->i64 {if !is_vec(h)||get(h,1)!=1 {return 0;}return get(h,4);}

fn entry(op:i64,h:i64,a:i64,b:i64)->i64 {
    witness(op);
    if op==0 {return vec_new(h);}
    if op==1 {return vec_len(h);}
    if op==2 {return vec_data(h);}
    if op==3 {return vec_reserve(h,a);}
    if op==4 {return vec_resize(h,a);}
    if op==5 {vec_free(h);return 0;}
    if op==6 {return vec_clear(h);}
    if op==7 {return bytes_append(h,a,b);}
    if op==8 {return str_set(h,a,b);}
    if op==9 {return str_data(h);}
    if op==10 {return str_len(h);}
    if op==11 {return vec_item(h);}
    ensure(false);
    return -1;
}

fn main() {print(entry(1,0,0,0));}
