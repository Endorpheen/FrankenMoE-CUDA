// Owned hash-table algorithms for the graph visited set.
// Source anchors: inputs/ggml-impl.h:211-331 (bitset layout, ggml_hash,
// linear probing, GGML_HASHSET_FULL / GGML_HASHSET_ALREADY_EXISTS),
// inputs/hash-excerpt.txt:45-111.
// Raw storage comes from graph-services.c; this file owns the probing
// algorithm that graph-traversal/graph-storage previously left native.

extern "fg153_hp_size" fn hp_size(hs:i64)->i64;
extern "fg153_hp_used_get" fn hp_used_get(hs:i64,i:i64)->i64;
extern "fg153_hp_used_set" fn hp_used_set(hs:i64,i:i64,v:i64);
extern "fg153_hp_key_get" fn hp_key_get(hs:i64,i:i64)->i64;
extern "fg153_hp_key_set" fn hp_key_set(hs:i64,i:i64,v:i64);
extern "fg153_hp_abort" fn hp_abort(code:i64);
extern "fg153_hp_full" fn hp_full()->i64;
extern "fg153_hp_already" fn hp_already()->i64;

fn hp_ensure(ok:bool) {if !ok {hp_abort(2);}}

fn hp_hash(t:i64)->i64 {return t>>4;}

fn hp_find(hs:i64,t:i64)->i64 {
    let size=hp_size(hs);
    if size<=0 {hp_abort(1);}
    let h=hp_hash(t)%size;
    var i:i64=h;
    while hp_used_get(hs,i)!=0&&hp_key_get(hs,i)!=t {
        i=(i+1)%size;
        if i==h {return hp_full();}
    }
    return i;
}

fn hp_contains(hs:i64,t:i64)->i64 {
    let pos=hp_find(hs,t);
    if pos==hp_full() {return 0;}
    return hp_used_get(hs,pos);
}

fn hp_insert(hs:i64,t:i64)->i64 {
    let size=hp_size(hs);
    if size<=0 {hp_abort(1);}
    let h=hp_hash(t)%size;
    var i:i64=h;
    var active:i64=1;
    while active==1 {
        if hp_used_get(hs,i)==0 {
            hp_used_set(hs,i,1);
            hp_key_set(hs,i,t);
            return i;
        }
        if hp_key_get(hs,i)==t {return hp_already();}
        i=(i+1)%size;
        if i==h {hp_abort(3);}
    }
    hp_abort(4);
    return hp_full();
}

fn hp_find_or_insert(hs:i64,t:i64)->i64 {
    let size=hp_size(hs);
    if size<=0 {hp_abort(1);}
    let h=hp_hash(t)%size;
    var i:i64=h;
    var active:i64=1;
    while active==1 {
        if hp_used_get(hs,i)==0 {
            hp_used_set(hs,i,1);
            hp_key_set(hs,i,t);
            return i;
        }
        if hp_key_get(hs,i)==t {return i;}
        i=(i+1)%size;
        if i==h {hp_abort(5);}
    }
    hp_abort(6);
    return hp_full();
}

fn hp_selftest()->i64 {
    hp_ensure(hp_full()==-1);
    hp_ensure(hp_already()==-2);
    hp_ensure(hp_hash(160)==10);
    hp_ensure(hp_hash(0)==0);
    return 1;
}

fn hp_entry(mode:i64,hs:i64,t:i64,a:i64,b:i64,c:i64,d:i64)->i64 {
    if mode==0 {return hp_find(hs,t);}
    if mode==1 {return hp_insert(hs,t);}
    if mode==2 {return hp_find_or_insert(hs,t);}
    if mode==3 {return hp_contains(hs,t);}
    if mode==4 {return hp_hash(t);}
    if mode==5 {return hp_selftest();}
    hp_abort(7);
    return 0;
}

fn main() {print(hp_entry(5,0,0,0,0,0,0));}
