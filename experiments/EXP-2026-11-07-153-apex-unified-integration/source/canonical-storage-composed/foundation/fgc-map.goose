// fgc-map.goose - APX030 canonical i64 -> i64 open-addressing map.
//
// Every signed i64 key is valid, including 0, -1, INT64_MIN and INT64_MAX.
// Empty/used/tombstone states are explicit bytes, so a key is never inferred
// from a sentinel value.  Probing, load-factor growth, tombstone reuse,
// rehash and rollback are Goose; services only allocate/free/copy/zero and
// provide indexed i64/u8 access plus one CAS free guard.
//
// Entry modes:
//   0 new()                 -> handle or 0
//   1 put(h,key,value)      -> status
//   2 get(h,key,out_ptr)    -> 1 found (writes value), 0 absent
//   3 erase(h,key)          -> status (absent is a no-op success)
//   4 free(h)               -> 0
//   5 size(h)               -> live entries
//   6 clear(h)              -> status
//
// Hash is computed in Goose with non-negative operands only:
//   fold high bits, mask to 52 bits, xorshift, mask to 63 bits.
// No signed overflow and no service-side hash policy.

extern "fgc_alloc" fn alloc(bytes:i64)->i64;
extern "fgc_free_ptr" fn release(ptr:i64,bytes:i64);
extern "fgc_memzero" fn memzero(dst:i64,bytes:i64)->i64;
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

fn mmagic()->i64 {return 1296126001;}
fn get(h:i64,f:i64)->i64 {return load(h,f);}
fn put(h:i64,f:i64,v:i64) {store(h,f,v);}
fn is_map(h:i64)->bool {return h!=0&&get(h,0)==mmagic();}

fn mix(key:i64)->i64 {
    var h=key;
    h=h^(h>>33);
    h=h&4503599627370495;
    h=h^(h<<11);
    h=h&9223372036854775807;
    h=h^(h>>29);
    h=h&9223372036854775807;
    return h;
}

fn map_rehash(h:i64,requested:i64)->i64 {
    var newcap=requested;
    if newcap<8 {newcap=8;}
    let maxc=imax()/8;
    if newcap>maxc {return -3;}
    let bytes=mul(newcap,8);
    let nk=alloc(bytes);
    if nk==0 {witness(20);return -2;}
    let nv=alloc(bytes);
    if nv==0 {release(nk,bytes);witness(20);return -2;}
    let ns=alloc(newcap);
    if ns==0 {release(nk,bytes);release(nv,bytes);witness(20);return -2;}
    let ignored=memzero(ns,newcap);
    let oldcap=get(h,1);let ok=get(h,4);let ov=get(h,5);let os=get(h,6);
    let mask=newcap-1;
    var i:i64=0;
    while i<oldcap {
        if load8(os,i)==1 {
            let key=load(ok,i);let val=load(ov,i);
            var idx=mix(key)&mask;
            while load8(ns,idx)!=0 {idx=(idx+1)&mask;}
            store(nk,idx,key);store(nv,idx,val);store8(ns,idx,1);
        }
        i+=1;
    }
    if ok!=0 {
        release(ok,mul(oldcap,8));
        release(ov,mul(oldcap,8));
        release(os,oldcap);
    }
    put(h,4,nk);put(h,5,nv);put(h,6,ns);put(h,1,newcap);put(h,3,0);
    witness(21);
    return 0;
}

fn map_new()->i64 {
    let h=alloc(56);
    if h==0 {return 0;}
    put(h,0,mmagic());put(h,1,0);put(h,2,0);put(h,3,0);
    put(h,4,0);put(h,5,0);put(h,6,0);
    let r=map_rehash(h,8);
    if r!=0 {release(h,56);return 0;}
    witness(0);
    return h;
}

fn map_put(h:i64,key:i64,value:i64)->i64 {
    if !is_map(h) {return -1;}
    let cap=get(h,1);let used=get(h,2);let tombs=get(h,3);
    if mul(add(add(used,tombs),1),10)>=mul(cap,7) {
        let r=map_rehash(h,mul(cap,2));
        if r!=0 {return r;}
    }
    let cap2=get(h,1);let used2=get(h,2);let tombs2=get(h,3);
    let mask=cap2-1;
    var idx=mix(key)&mask;
    var tomb:i64=-1;
    var i:i64=0;
    while i<cap2 {
        let st=load8(get(h,6),idx);
        if st==0 {
            var slot=idx;
            if tomb>=0 {slot=tomb;put(h,3,sub(tombs2,1));}
            store(get(h,4),slot,key);
            store(get(h,5),slot,value);
            store8(get(h,6),slot,1);
            put(h,2,add(used2,1));
            return 0;
        }
        if st==2 {
            if tomb<0 {tomb=idx;}
        } else {
            if load(get(h,4),idx)==key {
                store(get(h,5),idx,value);
                return 0;
            }
        }
        idx=(idx+1)&mask;
        i+=1;
    }
    if tomb>=0 {
        store(get(h,4),tomb,key);
        store(get(h,5),tomb,value);
        store8(get(h,6),tomb,1);
        put(h,3,sub(tombs2,1));
        put(h,2,add(used2,1));
        return 0;
    }
    return -1;
}

fn map_get(h:i64,key:i64,out:i64)->i64 {
    if !is_map(h)||out==0 {return 0;}
    let cap=get(h,1);let mask=cap-1;
    var idx=mix(key)&mask;
    var i:i64=0;
    while i<cap {
        let st=load8(get(h,6),idx);
        if st==0 {return 0;}
        if st==1&&load(get(h,4),idx)==key {
            store(out,0,load(get(h,5),idx));
            return 1;
        }
        idx=(idx+1)&mask;
        i+=1;
    }
    return 0;
}

fn map_erase(h:i64,key:i64)->i64 {
    if !is_map(h) {return -1;}
    let cap=get(h,1);let mask=cap-1;
    var idx=mix(key)&mask;
    var i:i64=0;
    while i<cap {
        let st=load8(get(h,6),idx);
        if st==0 {return 0;}
        if st==1&&load(get(h,4),idx)==key {
            store8(get(h,6),idx,2);
            put(h,2,sub(get(h,2),1));
            put(h,3,add(get(h,3),1));
            return 0;
        }
        idx=(idx+1)&mask;
        i+=1;
    }
    return 0;
}

fn map_size(h:i64)->i64 {if !is_map(h) {return 0;}return get(h,2);}

fn map_clear(h:i64)->i64 {
    if !is_map(h) {return -1;}
    let cap=get(h,1);
    let ignored=memzero(get(h,6),cap);
    put(h,2,0);put(h,3,0);
    return 0;
}

fn map_free(h:i64) {
    if h==0 {return;}
    let old=cas(h,0,mmagic(),0);
    if old!=mmagic() {return;}
    let cap=get(h,1);let ok=get(h,4);let ov=get(h,5);let os=get(h,6);
    if ok!=0 {release(ok,mul(cap,8));}
    if ov!=0 {release(ov,mul(cap,8));}
    if os!=0 {release(os,cap);}
    release(h,56);
    witness(22);
}

fn entry(op:i64,h:i64,a:i64,b:i64)->i64 {
    witness(op);
    if op==0 {return map_new();}
    if op==1 {return map_put(h,a,b);}
    if op==2 {return map_get(h,a,b);}
    if op==3 {return map_erase(h,a);}
    if op==4 {map_free(h);return 0;}
    if op==5 {return map_size(h);}
    if op==6 {return map_clear(h);}
    ensure(false);
    return -1;
}

fn main() {print(entry(5,0,0,0));}
