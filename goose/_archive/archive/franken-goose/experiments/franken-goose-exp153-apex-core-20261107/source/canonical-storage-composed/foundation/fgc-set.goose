// fgc-set.goose - APX030 canonical ordered i64 set (balanced AVL).
//
// Nodes live in one growable arena owned by the set; node indices are i64 and
// 0 means null.  Rotations, height/balance maintenance, insert/erase and
// ordered queries are Goose.  Arena growth is checked and rollback-safe: the
// new block is fully allocated before the tree is touched, so a failed insert
// leaves size/root/arena unchanged.  Free is O(1): CAS the alive magic, then
// release the arena and the header.
//
// Entry modes:
//   0 new()                    -> handle or 0
//   1 insert(h,key)            -> status (duplicate is success)
//   2 erase(h,key)             -> status (absent is success)
//   3 contains(h,key)          -> 1/0
//   4 lower_bound(h,key,out)   -> 1 found (>= key), 0 none
//   5 first(h,out)             -> 1 found, 0 empty
//   6 next(h,key,out)          -> 1 found (> key), 0 none
//   7 free(h)                  -> 0
//   8 size(h)                  -> count
//   9 height(h)                -> root height (instrumentation)
//   10 clear(h)                -> status
//
// Signed keys are compared directly; 0, -1 and INT64_MIN/INT64_MAX are normal
// keys.  The only service effects are raw malloc/free/memcpy, indexed i64
// access into caller-owned memory and one CAS.

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
fn sub(a:i64,b:i64)->i64 {ensure(a>=b);return a-b;}
fn diff(a:i64,b:i64)->i64 {return a-b;}
fn maximum(a:i64,b:i64)->i64 {if a>b {return a;}return b;}

fn smagic()->i64 {return 1397052465;}
fn get(h:i64,f:i64)->i64 {return load(h,f);}
fn put(h:i64,f:i64,v:i64) {store(h,f,v);}
fn is_set(h:i64)->bool {return h!=0&&get(h,0)==smagic();}

fn nodes(h:i64)->i64 {return get(h,6);}
fn ng(h:i64,idx:i64,f:i64)->i64 {if idx==0 {return 0;}return load(nodes(h),idx*6+f);}
fn ns(h:i64,idx:i64,f:i64,v:i64) {if idx==0 {return;}store(nodes(h),idx*6+f,v);}
fn hgt(h:i64,idx:i64)->i64 {if idx==0 {return 0;}return ng(h,idx,4);}
fn upd(h:i64,idx:i64) {
    if idx==0 {return;}
    let l=hgt(h,ng(h,idx,1));let r=hgt(h,ng(h,idx,2));
    ns(h,idx,4,add(maximum(l,r),1));
}
fn bal(h:i64,idx:i64)->i64 {if idx==0 {return 0;}return diff(hgt(h,ng(h,idx,1)),hgt(h,ng(h,idx,2)));}

fn rot_left(h:i64,x:i64)->i64 {
    let y=ng(h,x,2);let b=ng(h,y,1);let p=ng(h,x,3);
    ns(h,y,3,p);
    if p==0 {put(h,2,y);} else {
        if ng(h,p,1)==x {ns(h,p,1,y);} else {ns(h,p,2,y);}
    }
    ns(h,y,1,x);ns(h,x,3,y);
    ns(h,x,2,b);if b!=0 {ns(h,b,3,x);}
    upd(h,x);upd(h,y);
    return y;
}

fn rot_right(h:i64,x:i64)->i64 {
    let y=ng(h,x,1);let b=ng(h,y,2);let p=ng(h,x,3);
    ns(h,y,3,p);
    if p==0 {put(h,2,y);} else {
        if ng(h,p,1)==x {ns(h,p,1,y);} else {ns(h,p,2,y);}
    }
    ns(h,y,2,x);ns(h,x,3,y);
    ns(h,x,1,b);if b!=0 {ns(h,b,3,x);}
    upd(h,x);upd(h,y);
    return y;
}

fn rebalance(h:i64,start:i64) {
    var n=start;
    while n!=0 {
        upd(h,n);
        let b=bal(h,n);
        var top=n;
        if b>1 {
            let l=ng(h,n,1);
            if bal(h,l)<0 {let ignored=rot_left(h,l);}
            top=rot_right(h,n);
        } else {
            if b < -1 {
                let r=ng(h,n,2);
                if bal(h,r)>0 {let ignored=rot_right(h,r);}
                top=rot_left(h,n);
            }
        }
        n=ng(h,top,3);
    }
}

fn set_grow(h:i64)->i64 {
    let cap=get(h,4);
    var newcap=8;
    if cap>=8 {
        if cap>imax()/192 {return -3;}
        newcap=cap*2;
    }
    let bytes=mul(add(newcap,1),48);
    let p=alloc(bytes);
    if p==0 {witness(20);return -2;}
    let n=get(h,3);let old=get(h,6);
    if n>0 {let ignored=memcpy(p,old,mul(add(n,1),48));}
    if old!=0 {release(old,mul(add(cap,1),48));}
    put(h,6,p);put(h,4,newcap);
    witness(21);
    return 0;
}

fn node_new(h:i64,key:i64)->i64 {
    let free=ng(h,get(h,5),5);
    let head=get(h,5);
    var idx:i64=0;
    if head!=0 {
        idx=head;
        put(h,5,free);
    } else {
        let n=get(h,3);
        if n>=get(h,4) {
            let r=set_grow(h);
            if r!=0 {return 0;}
        }
        idx=get(h,3)+1;
        put(h,3,idx);
    }
    ns(h,idx,0,key);ns(h,idx,1,0);ns(h,idx,2,0);
    ns(h,idx,3,0);ns(h,idx,4,1);ns(h,idx,5,0);
    return idx;
}

fn node_free(h:i64,idx:i64) {
    ns(h,idx,5,get(h,5));
    put(h,5,idx);
}

fn set_new()->i64 {
    let h=alloc(56);
    if h==0 {return 0;}
    put(h,0,smagic());put(h,1,0);put(h,2,0);put(h,3,0);put(h,4,0);put(h,5,0);put(h,6,0);
    witness(0);
    return h;
}

fn set_insert(h:i64,key:i64)->i64 {
    if !is_set(h) {return -1;}
    let root=get(h,2);
    if root==0 {
        let idx=node_new(h,key);
        if idx==0 {return -2;}
        put(h,2,idx);put(h,1,1);
        return 0;
    }
    var cur=root;var parent:i64=0;var go_left=false;
    while cur!=0 {
        let k=ng(h,cur,0);
        if k==key {return 0;}
        parent=cur;
        if key<k {cur=ng(h,cur,1);go_left=true;} else {cur=ng(h,cur,2);go_left=false;}
    }
    let idx=node_new(h,key);
    if idx==0 {return -2;}
    ns(h,idx,3,parent);
    if go_left {ns(h,parent,1,idx);} else {ns(h,parent,2,idx);}
    put(h,1,add(get(h,1),1));
    rebalance(h,parent);
    return 0;
}

fn set_erase(h:i64,key:i64)->i64 {
    if !is_set(h) {return -1;}
    var cur=get(h,2);
    while cur!=0 {
        let k=ng(h,cur,0);
        if k==key {break;}
        if key<k {cur=ng(h,cur,1);} else {cur=ng(h,cur,2);}
    }
    if cur==0 {return 0;}
    var target=cur;
    if ng(h,target,1)!=0&&ng(h,target,2)!=0 {
        var s=ng(h,target,2);
        while ng(h,s,1)!=0 {s=ng(h,s,1);}
        ns(h,target,0,ng(h,s,0));
        target=s;
    }
    var child=ng(h,target,1);
    if child==0 {child=ng(h,target,2);}
    let p=ng(h,target,3);
    if child!=0 {ns(h,child,3,p);}
    if p==0 {put(h,2,child);} else {
        if ng(h,p,1)==target {ns(h,p,1,child);} else {ns(h,p,2,child);}
    }
    node_free(h,target);
    put(h,1,sub(get(h,1),1));
    if p!=0 {rebalance(h,p);}
    return 0;
}

fn set_contains(h:i64,key:i64)->i64 {
    if !is_set(h) {return 0;}
    var cur=get(h,2);
    while cur!=0 {
        let k=ng(h,cur,0);
        if k==key {return 1;}
        if key<k {cur=ng(h,cur,1);} else {cur=ng(h,cur,2);}
    }
    return 0;
}

fn set_lower(h:i64,key:i64,out:i64)->i64 {
    if !is_set(h)||out==0 {return 0;}
    var cur=get(h,2);var found:i64=0;
    while cur!=0 {
        let k=ng(h,cur,0);
        if k>=key {found=cur;cur=ng(h,cur,1);} else {cur=ng(h,cur,2);}
    }
    if found==0 {return 0;}
    store(out,0,ng(h,found,0));
    return 1;
}

fn set_first(h:i64,out:i64)->i64 {
    if !is_set(h)||out==0 {return 0;}
    var cur=get(h,2);
    if cur==0 {return 0;}
    while ng(h,cur,1)!=0 {cur=ng(h,cur,1);}
    store(out,0,ng(h,cur,0));
    return 1;
}

fn set_next(h:i64,key:i64,out:i64)->i64 {
    if !is_set(h)||out==0 {return 0;}
    var cur=get(h,2);var found:i64=0;
    while cur!=0 {
        let k=ng(h,cur,0);
        if k>key {found=cur;cur=ng(h,cur,1);} else {cur=ng(h,cur,2);}
    }
    if found==0 {return 0;}
    store(out,0,ng(h,found,0));
    return 1;
}

fn set_size(h:i64)->i64 {if !is_set(h) {return 0;}return get(h,1);}
fn set_height(h:i64)->i64 {if !is_set(h) {return 0;}return hgt(h,get(h,2));}

fn set_clear(h:i64)->i64 {
    if !is_set(h) {return -1;}
    put(h,1,0);put(h,2,0);put(h,3,0);put(h,5,0);
    return 0;
}

fn set_free(h:i64) {
    if h==0 {return;}
    let old=cas(h,0,smagic(),0);
    if old!=smagic() {return;}
    let cap=get(h,4);let data=get(h,6);
    if data!=0 {release(data,mul(add(cap,1),48));}
    release(h,56);
    witness(22);
}

fn entry(op:i64,h:i64,a:i64,b:i64)->i64 {
    witness(op);
    if op==0 {return set_new();}
    if op==1 {return set_insert(h,a);}
    if op==2 {return set_erase(h,a);}
    if op==3 {return set_contains(h,a);}
    if op==4 {return set_lower(h,a,b);}
    if op==5 {return set_first(h,a);}
    if op==6 {return set_next(h,a,b);}
    if op==7 {set_free(h);return 0;}
    if op==8 {return set_size(h);}
    if op==9 {return set_height(h);}
    if op==10 {return set_clear(h);}
    ensure(false);
    return -1;
}

fn main() {print(entry(8,0,0,0));}
