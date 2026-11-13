extern "sf_fgc_record_new" fn rec_new(kind:i64)->i64;
extern "sf_fgc_record_get" fn rec_get(h:i64,key:i64)->i64;
extern "sf_fgc_record_set" fn rec_set(h:i64,key:i64,v:i64)->i64;
extern "sf_fgc_record_kind" fn rec_kind(h:i64)->i64;
extern "sf_fgc_record_free" fn rec_free(h:i64)->i64;
extern "sf_fgc_vec_new" fn vec_new(item:i64)->i64;
extern "sf_fgc_vec_len" fn vec_len(h:i64)->i64;
extern "sf_fgc_vec_data" fn vec_data(h:i64)->i64;
extern "sf_fgc_vec_resize" fn vec_resize(h:i64,n:i64)->i64;
extern "sf_fgc_vec_free" fn vec_free(h:i64)->i64;
extern "sf_fgc_map_new" fn map_new()->i64;
extern "sf_fgc_map_put" fn map_put(h:i64,k:i64,v:i64)->i64;
extern "sf_fgc_map_get" fn map_get(h:i64,k:i64,out:i64)->i64;
extern "sf_fgc_map_erase" fn map_erase(h:i64,k:i64)->i64;
extern "sf_fgc_map_free" fn map_free(h:i64)->i64;
extern "sf_fgc_set_new" fn set_new()->i64;
extern "sf_fgc_set_insert" fn set_insert(h:i64,k:i64)->i64;
extern "sf_fgc_set_erase" fn set_erase(h:i64,k:i64)->i64;
extern "sf_fgc_set_lower_bound" fn set_lower_bound(h:i64,k:i64,out:i64)->i64;
extern "sf_fgc_set_first" fn set_first(h:i64,out:i64)->i64;
extern "sf_fgc_set_next" fn set_next(h:i64,k:i64,out:i64)->i64;
extern "sf_fgc_set_free" fn set_free(h:i64)->i64;
extern "sf_raw_size" fn raw_size(cur:i64)->i64;
extern "sf_raw_set_size" fn raw_set_size(cur:i64,v:i64);
extern "sf_raw_selected" fn raw_selected(cur:i64)->i64;
extern "sf_raw_set_selected" fn raw_set_selected(cur:i64,v:i64);
extern "sf_raw_sorted" fn raw_sorted(cur:i64)->i64;
extern "sf_raw_set_sorted" fn raw_set_sorted(cur:i64,v:i64);
extern "sf_raw_tok_get" fn raw_tok_get(cur:i64,i:i64,code:i64)->i64;
extern "sf_raw_tok_set" fn raw_tok_set(cur:i64,i:i64,code:i64,v:i64);
extern "sf_alloc" fn sf_alloc(n:i64)->i64;
extern "sf_free_mem" fn sf_free(p:i64);
extern "sf_addr_add" fn sf_addr_add(base:i64,off:i64)->i64;
extern "sf_i64_get" fn sf_i64_get(a:i64)->i64;
extern "sf_i64_set" fn sf_i64_set(a:i64,v:i64);
extern "sf_bias_get" fn sf_bias_get(ptr:i64,index:i64,code:i64)->i64;
extern "sf_f64_bits" fn sf_f64_bits(v:f64)->i64;
extern "sf_sqrtf_bits" fn sf_sqrtf_bits(b:i64)->i64;
extern "sf_fail" fn sf_fail(code:i64);

extern "fg153_sp_arr_new" fn sp_arr_new(cap:i64)->i64;
extern "fg153_sp_arr_borrow" fn sp_arr_borrow(raw:i64)->i64;
extern "fg153_sp_arr_commit" fn sp_arr_commit(a:i64,raw:i64);
extern "fg153_sp_arr_free" fn sp_arr_free(a:i64);
extern "fg153_sp_arr_get" fn sp_arr_get(a:i64,code:i64)->i64;
extern "fg153_sp_arr_set" fn sp_arr_set(a:i64,code:i64,v:i64);
extern "fg153_sp_tok_get" fn sp_tok_get(a:i64,i:i64,code:i64)->i64;
extern "fg153_sp_tok_set" fn sp_tok_set(a:i64,i:i64,code:i64,v:i64);
extern "fg153_sp_live_entry" fn sp_live_entry(m:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64)->i64;

fn sf_ensure(ok:bool) {if !ok {sf_fail(1);}}
fn mask32()->i64 {return 4294967295;}
fn u32(v:i64)->i64 {return v&mask32();}
fn shr(v:i64,n:i64)->i64 {return (v&mask32())>>n;}
fn shl(v:i64,n:i64)->i64 {return (v<<n)&mask32();}
fn mul32(a:i64,b:i64)->i64 {return (a*b)&mask32();}

fn vget(v:i64,i:i64)->i64 {return sf_i64_get(sf_addr_add(vec_data(v),i*8));}
fn vset(v:i64,i:i64,x:i64) {sf_i64_set(sf_addr_add(vec_data(v),i*8),x);}

fn rng_new(seed:i64)->i64 {
    let r=rec_new(1);sf_ensure(r!=0);
    let v=vec_new(8);sf_ensure(v!=0);
    let ignored=vec_resize(v,625);
    let ignored2=rec_set(r,0,v);let ignored3=rec_set(r,1,seed&mask32());
    rng_seed(r);
    return r;
}
fn rng_seed(r:i64) {
    let v=rec_get(r,0);let seed=rec_get(r,1);
    vset(v,0,u32(seed));
    var i:i64=1;
    while i<624 {
        var x=vget(v,i-1);
        x=u32(x^(shr(x,30)));
        x=u32(mul32(x,1812433253)+i);
        vset(v,i,x);
        i+=1;
    }
    vset(v,624,624);
}
fn gen_rand(r:i64) {
    let v=rec_get(r,0);
    var k:i64=0;
    while k<227 {
        let y=u32((vget(v,k)&2147483648)|(vget(v,k+1)&2147483647));
        let a=if (y&1)!=0 {2567483615} else {0};
        vset(v,k,u32(vget(v,k+397)^shr(y,1)^a));
        k+=1;
    }
    while k<623 {
        let y=u32((vget(v,k)&2147483648)|(vget(v,k+1)&2147483647));
        let a=if (y&1)!=0 {2567483615} else {0};
        vset(v,k,u32(vget(v,k-227)^shr(y,1)^a));
        k+=1;
    }
    let y=u32((vget(v,623)&2147483648)|(vget(v,0)&2147483647));
    let a=if (y&1)!=0 {2567483615} else {0};
    vset(v,623,u32(vget(v,396)^shr(y,1)^a));
    vset(v,624,0);
}
fn rng_next(r:i64)->i64 {
    let v=rec_get(r,0);var p=vget(v,624);
    if p>=624 {gen_rand(r);p=0;}
    var z=vget(v,p);vset(v,624,p+1);
    z=u32(z^(shr(z,11)&mask32()));
    z=u32(z^(shl(z,7)&2636928640));
    z=u32(z^(shl(z,15)&4022730752));
    z=u32(z^shr(z,18));
    return z;
}
fn rng_uniform(r:i64)->f64 {
    let u1=rng_next(r);let u2=rng_next(r);
    return ((u1 as f64)+(u2 as f64)*4294967296.0)/18446744073709551616.0;
}
fn rng_clone(src:i64)->i64 {
    let out=rng_new(rec_get(src,1));
    let sv=rec_get(src,0);let ov=rec_get(out,0);
    var i:i64=0;while i<625 {vset(ov,i,vget(sv,i));i+=1;}
    return out;
}
fn rng_free(r:i64) {if r==0 {return;}let v=rec_get(r,0);if v!=0 {let ignored=vec_free(v);}let ignored2=rec_free(r);}

fn sampler_new(kind:i64)->i64 {let h=rec_new(2);sf_ensure(h!=0);let ignored=rec_set(h,0,kind);return h;}
fn kind(h:i64)->i64 {return rec_get(h,0);}
fn chain_init()->i64 {let h=sampler_new(7);let v=vec_new(8);let w=vec_new(8);let ignored=rec_set(h,8,v);let ignored2=rec_set(h,16,w);let ignored3=rec_set(h,9,0);return h;}
fn chain_add(c:i64,child:i64) {
    let v=rec_get(c,8);let n=rec_get(c,9);
    let ignored=vec_resize(v,n+1);vset(v,n,child);let ignored2=rec_set(c,9,n+1);
}
fn chain_add_wrap(c:i64,child:i64,wrap:i64) {
    let v=rec_get(c,8);let w=rec_get(c,16);let n=rec_get(c,9);
    let ignored=vec_resize(v,n+1);let ignored2=vec_resize(w,n+1);
    vset(v,n,child);vset(w,n,wrap);let ignored3=rec_set(c,9,n+1);
}
fn chain_n(c:i64)->i64 {return rec_get(c,9);}
fn chain_get_wrap(c:i64,i:i64)->i64 {
    let n=rec_get(c,9);if i<0||i>=n {return 0;}
    return vget(rec_get(c,16),i);
}
fn chain_remove(c:i64,i:i64)->i64 {
    let n=rec_get(c,9);if i<0||i>=n {return 0;}
    let v=rec_get(c,8);let w=rec_get(c,16);let out=vget(w,i);var j=i;
    while j+1<n {vset(v,j,vget(v,j+1));vset(w,j,vget(w,j+1));j+=1;}
    let ignored=vec_resize(v,n-1);let ignored2=vec_resize(w,n-1);
    let ignored3=rec_set(c,9,n-1);return out;
}
fn chain_free_shallow(c:i64) {
    if c==0 {return;}
    let w=rec_get(c,16);if w!=0 {vec_free(w);}
    let v=rec_get(c,8);if v!=0 {vec_free(v);}
    let ignored=rec_free(c);
}
fn make_simple(kind:i64,i1:i64,i2:i64,f1:i64,f2:i64,f3:i64)->i64 {
    let h=sampler_new(kind);let ignored=rec_set(h,1,i1);let ignored2=rec_set(h,2,i2);
    let ignored3=rec_set(h,3,f1);let ignored4=rec_set(h,4,f2);let ignored5=rec_set(h,5,f3);
    return h;
}
fn dry_init(last_n:i64,allowed:i64,mult:i64,base:i64)->i64 {
    return make_simple(8,last_n,allowed,mult,base,0);
}
fn logit_bias_init(vocab:i64,n:i64,ptr:i64)->i64 {
    let h=make_simple(12,vocab,n,0,0,0);
    let v=vec_new(8);sf_ensure(v!=0);let ignored=vec_resize(v,n*2);sf_ensure(ignored==0);
    var i:i64=0;
    while i<n {
        vset(v,i*2,sf_bias_get(ptr,i,0));
        vset(v,i*2+1,sf_bias_get(ptr,i,1));
        i+=1;
    }
    let ignored2=rec_set(h,17,v);
    return h;
}
fn logit_bias_apply(h:i64,cur:i64) {
    let n=rec_get(h,2);if n<=0 {return;}
    let v=rec_get(h,17);let size=raw_size(cur);var i:i64=0;var pending:i64=0;
    while i<n {
        let tok=vget(v,i*2);let bias=bit_to_f32(vget(v,i*2+1));
        if tok>=0&&size>tok&&raw_tok_get(cur,tok,0)==tok {
            let l=bit_to_f32(raw_tok_get(cur,tok,1));
            raw_tok_set(cur,tok,1,f32_to_bit(l+bias));
        } else {pending+=1;}
        i+=1;
    }
    if pending==0 {return;}
    i=0;
    while i<size {
        let id=raw_tok_get(cur,i,0);var j:i64=0;
        while j<n {
            let tok=vget(v,j*2);
            let fast=if tok>=0&&size>tok&&raw_tok_get(cur,tok,0)==tok {1} else {0};
            if fast==0&&id==tok {
                let l=bit_to_f32(raw_tok_get(cur,i,1));
                let bias=bit_to_f32(vget(v,j*2+1));
                raw_tok_set(cur,i,1,f32_to_bit(l+bias));j=n;
            } else {j+=1;}
        }
        i+=1;
    }
}
fn penalties_init(vocab:i64,last_n:i64,rep:i64,freq:i64,pres:i64)->i64 {
    let h=make_simple(5,vocab,last_n,rep,freq,pres);
    let ignored=rec_set(h,6,map_new());
    let ignored2=rec_set(h,10,set_new());
    let v=vec_new(8);let cap=if last_n>0 {last_n} else {1};
    let ignored3=vec_resize(v,cap);let ignored4=rec_set(h,10+1,v);
    let ignored5=rec_set(h,14,cap);let ignored6=rec_set(h,12,0);let ignored7=rec_set(h,13,0);
    return h;
}

fn hist_count(h:i64,token:i64)->i64 {
    let cell=sf_alloc(8);let found=map_get(rec_get(h,6),token,cell);let c=sf_i64_get(cell);sf_free(cell);
    return if found!=0 {c} else {0};
}
fn hist_set(h:i64,token:i64,c:i64) {
    if c<=0 {let ignored=map_erase(rec_get(h,6),token);let ignored2=set_erase(rec_get(h,10),token);}
    else {let ignored=map_put(rec_get(h,6),token,c);let ignored2=set_insert(rec_get(h,10),token);}
}
fn penalties_accept(h:i64,token:i64) {
    let last_n=rec_get(h,2);if last_n==0 {return;}
    let v=rec_get(h,11);let size=rec_get(h,12);let first=rec_get(h,13);let pos=rec_get(h,15);
    hist_set(h,token,hist_count(h,token)+1);
    if size>=last_n {
        let old=vget(v,first);hist_set(h,old,hist_count(h,old)-1);
        let ignored=rec_set(h,13,(first+1)%last_n);
    } else {let ignored=rec_set(h,12,size+1);}
    vset(v,pos,token);
    let ignored2=rec_set(h,15,(pos+1)%last_n);
}
fn penalties_reset(h:i64) {
    let v=rec_get(h,11);let cap=rec_get(h,14);
    let ignored=rec_set(h,12,0);let ignored2=rec_set(h,13,0);let ignored3=rec_set(h,15,0);
    let cell=sf_alloc(8);
    var more=set_first(rec_get(h,10),cell);
    while more!=0 {
        let key=sf_i64_get(cell);
        let ignored4=map_erase(rec_get(h,6),key);
        more=set_next(rec_get(h,10),key,cell);
    }
    sf_free(cell);
    let s=rec_get(h,10);let ignored5=set_free(s);let ignored6=rec_set(h,10,set_new());
    let ignored7=map_free(rec_get(h,6));let ignored8=rec_set(h,6,map_new());
    let ignored9=vec_free(v);let nv=vec_new(8);let cap2=if cap>0 {cap} else {1};let ignored10=vec_resize(nv,cap2);let ignored11=rec_set(h,11,nv);
}
fn penalties_apply(h:i64,cur:i64) {
    let size=raw_size(cur);let rep=bit_to_f32(rec_get(h,3));let freq=bit_to_f32(rec_get(h,4));let pres=bit_to_f32(rec_get(h,5));
    if rec_get(h,2)==0||(rep==1.0&&freq==0.0&&pres==0.0) {return;}
    let cell=sf_alloc(8);var i:i64=0;
    while i<size {
        let id=raw_tok_get(cur,i,0);
        if map_get(rec_get(h,6),id,cell)!=0 {
            let c=sf_i64_get(cell);var l=bit_to_f32(raw_tok_get(cur,i,1));
            if l<=0.0 {l=l*rep;} else {l=l/rep;}
            l=l-(c as f32)*freq-pres;
            raw_tok_set(cur,i,1,f32_to_bit(l));
        }
        i+=1;
    }
    sf_free(cell);raw_set_sorted(cur,0);
}

fn f32_to_bit(v:f32)->i64 {return sp_float_bits_extern(v);}
fn bit_to_f32(b:i64)->f32 {return sp_bits_float_extern(b);}
extern "fg153_sp_float_bits" fn sp_float_bits_extern(v:f32)->i64;
extern "fg153_sp_bits_float" fn sp_bits_float_extern(b:i64)->f32;

fn filters_via_sp(h:i64,cur:i64) {
    let a=sp_arr_borrow(cur);sf_ensure(a!=0);let k=kind(h);
    if k==1 {let ignored=sp_live_entry(2,a,rec_get(h,1),0,0,0,0,0);}
    else if k==2 {let ignored=sp_live_entry(3,a,rec_get(h,3),rec_get(h,2),0,0,0,0);}
    else if k==3 {let ignored=sp_live_entry(4,a,rec_get(h,3),rec_get(h,2),0,0,0,0);}
    else if k==4 {let ignored=sp_live_entry(0,a,rec_get(h,3),0,0,0,0,0);}
    sp_arr_commit(a,cur);
}

recursive fn sampler_apply(h:i64,cur:i64) {
    let k=kind(h);
    if k==0 {
        let size=raw_size(cur);if size==0 {raw_set_selected(cur,-1);return;}
        var best:i64=0;var i:i64=1;
        while i<size {if bit_to_f32(raw_tok_get(cur,i,1))>bit_to_f32(raw_tok_get(cur,best,1)) {best=i;}i+=1;}
        raw_set_selected(cur,best);return;
    }
    if k==6 {
        let size=raw_size(cur);if size==0 {raw_set_selected(cur,-1);return;}
        let r=rec_get(h,7);
        if size==1 {let ignored=rng_uniform(r);raw_tok_set(cur,0,2,f32_to_bit(1.0));raw_set_selected(cur,0);return;}
        var maxl=bit_to_f32(raw_tok_get(cur,0,1));var i:i64=1;
        if raw_sorted(cur)==0 {while i<size {let l=bit_to_f32(raw_tok_get(cur,i,1));if l>maxl {maxl=l;}i+=1;}}
        var sum:f64=0.0;i=0;
        while i<size {let p=sp_expf_live(bit_to_f32(raw_tok_get(cur,i,1))-maxl);raw_tok_set(cur,i,2,f32_to_bit(p));sum+=p as f64;i+=1;}
        let rnd=rng_uniform(r);let target=sum*rnd;var run:f64=0.0;var sel=size-1;var found:i64=0;i=0;
        while i<size {if found==0 {run+=bit_to_f32(raw_tok_get(cur,i,2)) as f64;if run>=target {sel=i;found=1;}}raw_tok_set(cur,i,2,f32_to_bit((bit_to_f32(raw_tok_get(cur,i,2)) as f64/sum) as f32));i+=1;}
        raw_set_selected(cur,sel);return;
    }
    if k==5 {penalties_apply(h,cur);return;}
    if k==7 {let v=rec_get(h,8);let n=rec_get(h,9);var i:i64=0;while i<n {sampler_apply(vget(v,i),cur);i+=1;}return;}
    if k==8 {dry_apply(h,cur);return;}
    if k==9 {typical_apply(h,cur);return;}
    if k==10 {top_n_sigma_apply(h,cur);return;}
    if k==11 {xtc_apply(h,cur);return;}
    if k==12 {logit_bias_apply(h,cur);return;}
    filters_via_sp(h,cur);
}
extern "fg153_sp_expf_bits" fn sp_expf_bits(b:i64)->i64;
extern "fg153_sp_logf_bits" fn sp_logf_bits(b:i64)->i64;
extern "fg153_sp_fabsf_bits" fn sp_fabsf_bits(b:i64)->i64;
fn sp_expf_live(v:f32)->f32 {return bit_to_f32(sp_expf_bits(f32_to_bit(v)));}
fn sp_logf_live(v:f32)->f32 {return bit_to_f32(sp_logf_bits(f32_to_bit(v)));}
fn sp_fabsf_live(v:f32)->f32 {return bit_to_f32(sp_fabsf_bits(f32_to_bit(v)));}

fn softmax_raw(cur:i64) {
    let a=sp_arr_borrow(cur);sf_ensure(a!=0);
    let ignored=sp_live_entry(1,a,1,0,0,0,0,0);sp_arr_commit(a,cur);
}

fn typical_apply(h:i64,cur:i64) {
    let p=bit_to_f32(rec_get(h,3));let min_keep=rec_get(h,2);
    if p>=1.0 {return;}
    softmax_raw(cur);
    let n=raw_size(cur);if n==0 {return;}
    var entropy:f32=0.0;var i:i64=0;
    while i<n {let pp=bit_to_f32(raw_tok_get(cur,i,2));entropy=entropy-pp*sp_logf_live(pp);i+=1;}
    let idx=vec_new(8);let scores=vec_new(8);let ignored=vec_resize(idx,n);let ignored2=vec_resize(scores,n);
    i=0;while i<n {let pp=bit_to_f32(raw_tok_get(cur,i,2));let sh=sp_fabsf_live((0.0-sp_logf_live(pp))-entropy);vset(idx,i,i);vset(scores,i,f32_to_bit(sh));i+=1;}
    i=0;while i<n {var best=i;var j:i64=i+1;while j<n {if bit_to_f32(vget(scores,vget(idx,j)))<bit_to_f32(vget(scores,vget(idx,best))) {best=j;}j+=1;}let ti=vget(idx,i);vset(idx,i,vget(idx,best));vset(idx,best,ti);i+=1;}
    let temp=sp_arr_new(if n>0 {n} else {1});sp_arr_set(temp,0,n);
    i=0;while i<n {sp_tok_set(temp,i,0,raw_tok_get(cur,i,0));sp_tok_set(temp,i,1,raw_tok_get(cur,i,1));sp_tok_set(temp,i,2,raw_tok_get(cur,i,2));i+=1;}
    var cum:f32=0.0;var last=n;i=0;
    while i<n {let orig=vget(idx,i);cum+=bit_to_f32(sp_tok_get(temp,orig,2));if cum>p&&(min_keep==0||i>=min_keep-1) {last=i+1;break;}i+=1;}
    i=0;while i<last {let orig=vget(idx,i);raw_tok_set(cur,i,0,sp_tok_get(temp,orig,0));raw_tok_set(cur,i,1,sp_tok_get(temp,orig,1));raw_tok_set(cur,i,2,sp_tok_get(temp,orig,2));i+=1;}
    raw_set_size(cur,last);raw_set_sorted(cur,0);
    sp_arr_free(temp);vec_free(idx);vec_free(scores);
}

fn top_n_sigma_apply(h:i64,cur:i64) {
    let nsig=bit_to_f32(rec_get(h,3));let n=raw_size(cur);
    if nsig<=0.0||n<=1 {return;}
    let ninf=-8388608;
    var maxl=bit_to_f32(raw_tok_get(cur,0,1));var sum:f32=0.0;var valid:i64=0;var i:i64=0;
    while i<n {let lb=raw_tok_get(cur,i,1);if lb!=ninf {let l=bit_to_f32(lb);if l>maxl {maxl=l;}sum+=l;valid+=1;}i+=1;}
    let mean=if valid>0 {sum/(valid as f32)} else {0.0};
    var acc:f32=0.0;i=0;
    while i<n {let lb=raw_tok_get(cur,i,1);if lb!=ninf {let d=bit_to_f32(lb)-mean;acc+=d*d;}i+=1;}
    let std=if valid>0 {bit_to_f32(sf_sqrtf_bits(f32_to_bit(acc/(valid as f32))))} else {0.0};
    let cut=maxl-nsig*std;i=0;
    while i<n {if bit_to_f32(raw_tok_get(cur,i,1))<cut {raw_tok_set(cur,i,1,ninf);}i+=1;}
}

fn xtc_apply(h:i64,cur:i64) {
    let prob=bit_to_f32(rec_get(h,3));let thr=bit_to_f32(rec_get(h,4));let min_keep=rec_get(h,2);let n=raw_size(cur);
    if prob<=0.0||thr>0.5||n<2 {return;}
    let r=rec_get(h,7);let u=rng_next(r);
    let chance=(u as f64)/4294967296.0;if (chance as f32)>prob {return;}
    softmax_raw(cur);
    var pos_last:i64=0;var i:i64=0;
    while i<n {if bit_to_f32(raw_tok_get(cur,i,2))>=thr {pos_last=i;} else {break;}i+=1;}
    if n-pos_last>=min_keep&&pos_last>0 {
        i=0;while i<n-pos_last {raw_tok_set(cur,i,0,raw_tok_get(cur,i+pos_last,0));raw_tok_set(cur,i,1,raw_tok_get(cur,i+pos_last,1));raw_tok_set(cur,i,2,raw_tok_get(cur,i+pos_last,2));i+=1;}
        raw_set_size(cur,n-pos_last);
    }
}

fn dry_apply(h:i64,cur:i64) {
    let mult=bit_to_f32(rec_get(h,3));
    if mult<=0.0 {return;}
    sf_fail(4); // enabled DRY algorithm is OPEN, not silently native-delegated
}

recursive fn sampler_accept(h:i64,token:i64,generated:i64) {
    let k=kind(h);
    if k==5 {if generated!=0 {penalties_accept(h,token);}return;}
    if k==7 {let v=rec_get(h,8);let n=rec_get(h,9);var i:i64=0;while i<n {sampler_accept(vget(v,i),token,generated);i+=1;}return;}
}
recursive fn sampler_reset(h:i64) {
    let k=kind(h);
    if k==5 {penalties_reset(h);return;}
    if k==6||k==11 {rng_seed(rec_get(h,7));return;}
    if k==7 {let v=rec_get(h,8);let n=rec_get(h,9);var i:i64=0;while i<n {sampler_reset(vget(v,i));i+=1;}}
}
recursive fn sampler_clone(h:i64)->i64 {
    let k=kind(h);var out:i64=0;
    if k==5 {
        out=penalties_init(rec_get(h,1),rec_get(h,2),rec_get(h,3),rec_get(h,4),rec_get(h,5));
        let srcv=rec_get(h,11);let dstv=rec_get(out,11);let cap=rec_get(h,14);
        var i:i64=0;while i<cap {vset(dstv,i,vget(srcv,i));i+=1;}
        let ignored=rec_set(out,12,rec_get(h,12));let ignored2=rec_set(out,13,rec_get(h,13));let ignored3=rec_set(out,15,rec_get(h,15));
        let cell=sf_alloc(8);var more=set_first(rec_get(h,10),cell);
        while more!=0 {let key=sf_i64_get(cell);let c=hist_count(h,key);hist_set(out,key,c);more=set_next(rec_get(h,10),key,cell);}
        sf_free(cell);
    } else if k==6 {
        out=make_simple(6,0,0,0,0,0);let ignored=rec_set(out,7,rng_clone(rec_get(h,7)));
    } else if k==11 {
        out=make_simple(11,0,rec_get(h,2),rec_get(h,3),rec_get(h,4),0);
        let ignored=rec_set(out,7,rng_clone(rec_get(h,7)));
    } else if k==7 {
        out=chain_init();let v=rec_get(h,8);let n=rec_get(h,9);var i:i64=0;
        while i<n {chain_add(out,sampler_clone(vget(v,i)));i+=1;}
    } else if k==12 {
        out=make_simple(12,rec_get(h,1),rec_get(h,2),0,0,0);
        let src=rec_get(h,17);let dst=vec_new(8);sf_ensure(dst!=0);let ignored=vec_resize(dst,rec_get(h,2)*2);sf_ensure(ignored==0);
        var bi:i64=0;while bi<rec_get(h,2)*2 {vset(dst,bi,vget(src,bi));bi+=1;}
        let ignored2=rec_set(out,17,dst);
    } else {
        out=make_simple(k,rec_get(h,1),rec_get(h,2),rec_get(h,3),rec_get(h,4),rec_get(h,5));
    }
    return out;
}
recursive fn sampler_free(h:i64) {
    if h==0 {return;}
    let k=kind(h);
    if k==5 {map_free(rec_get(h,6));set_free(rec_get(h,10));vec_free(rec_get(h,11));}
    else if k==6||k==11 {rng_free(rec_get(h,7));}
    else if k==12 {vec_free(rec_get(h,17));}
    else if k==7 {let v=rec_get(h,8);let n=rec_get(h,9);var i:i64=0;while i<n {sampler_free(vget(v,i));i+=1;}vec_free(v);let w=rec_get(h,16);if w!=0 {vec_free(w);}}
    let ignored=rec_free(h);
}

fn cfg_new(seed:i64,vocab:i64)->i64 {
    let c=rec_new(3);sf_ensure(c!=0);
    let t=vec_new(8);let ignored=rec_set(c,2,t);let ignored2=rec_set(c,1,0);
    let ignored3=rec_set(c,0,seed&mask32());let ignored4=rec_set(c,21,vocab);
    let ignored5=rec_set(c,3,40);let ignored6=rec_set(c,4,f32_to_bit(0.95));
    let ignored7=rec_set(c,5,f32_to_bit(0.05));let ignored8=rec_set(c,6,f32_to_bit(0.8));
    let ignored9=rec_set(c,7,64);let ignored10=rec_set(c,8,f32_to_bit(1.0));
    let ignored11=rec_set(c,9,f32_to_bit(0.0));let ignored12=rec_set(c,10,f32_to_bit(0.0));
    let ignored13=rec_set(c,11,f32_to_bit(1.0));let ignored14=rec_set(c,12,f32_to_bit(0.0));
    let ignored15=rec_set(c,13,f32_to_bit(0.1));let ignored16=rec_set(c,14,f32_to_bit(-1.0));
    let ignored17=rec_set(c,15,f32_to_bit(0.0));let ignored18=rec_set(c,16,f32_to_bit(1.75));
    let ignored19=rec_set(c,17,2);let ignored20=rec_set(c,18,64);let ignored21=rec_set(c,19,0);
    let ignored22=rec_set(c,20,0);let ignored23=rec_set(c,22,f32_to_bit(0.0));let ignored24=rec_set(c,23,0);
    return c;
}
fn cfg_add(c:i64,t:i64) {
    let v=rec_get(c,2);let n=rec_get(c,1);let ignored=vec_resize(v,n+1);vset(v,n,t);let ignored2=rec_set(c,1,n+1);
}
fn configured_init(c:i64)->i64 {
    sf_ensure(rec_get(c,23)==0);
    let chain=chain_init();let v=rec_get(c,2);let n=rec_get(c,1);
    let vocab=rec_get(c,21);let seed=rec_get(c,0);var last:i64=-1;var i:i64=0;
    while i<n {
        let t=vget(v,i);var s:i64=0;
        if t==0 {s=0;}
        else if t==1 {
            s=make_simple(8,rec_get(c,18),rec_get(c,17),rec_get(c,15),rec_get(c,16),0);
        } else if t==2 {s=make_simple(1,rec_get(c,3),0,0,0,0);}
        else if t==3 {s=make_simple(2,0,rec_get(c,19),rec_get(c,4),0,0);}
        else if t==4 {s=make_simple(3,0,rec_get(c,19),rec_get(c,5),0,0);}
        else if t==6 {s=make_simple(9,0,rec_get(c,19),rec_get(c,11),0,0);}
        else if t==7 {s=make_simple(4,0,0,rec_get(c,6),0,0);}
        else if t==8 {
            s=make_simple(11,0,rec_get(c,19),rec_get(c,12),rec_get(c,13),0);
            let ignored=rec_set(s,7,rng_new(seed));
        }
        else if t==10 {s=penalties_init(vocab,rec_get(c,7),rec_get(c,8),rec_get(c,9),rec_get(c,10));}
        else if t==11 {s=make_simple(10,0,0,rec_get(c,14),0,0);}
        else {sf_fail(5);}
        if s!=0 {chain_add(chain,s);last=t;}
        i+=1;
    }
    let nprobs=rec_get(c,20);
    let greedy=if nprobs==0&&((last==7&&bit_to_f32(rec_get(c,6))==0.0&&bit_to_f32(rec_get(c,22))==0.0)||(last==2&&rec_get(c,3)==1)) {1} else {0};
    if greedy!=0 {chain_add(chain,sampler_new(0));}
    else {let d=make_simple(6,0,0,0,0,0);let ignored=rec_set(d,7,rng_new(seed));chain_add(chain,d);}
    return chain;
}

fn selftest()->i64 {
    let r=rng_new(5489);let a=rng_next(r);let b=rng_next(r);
    let ok1=if a==3499211612 {1}else{0};let ok2=if b==581869302 {1}else{0};
    rng_free(r);return ok1&ok2;
}

fn sf_entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64)->i64 {
    if mode==0 {return sampler_new(0);}
    if mode==1 {return make_simple(1,a,0,0,0,0);}
    if mode==2 {return make_simple(2,0,b,a,0,0);}
    if mode==3 {return make_simple(3,0,b,a,0,0);}
    if mode==4 {return make_simple(4,0,0,a,0,0);}
    if mode==5 {return penalties_init(a,b,c,d,e);}
    if mode==6 {let h=make_simple(6,0,0,0,0,0);let ignored=rec_set(h,7,rng_new(a));return h;}
    if mode==7 {return chain_init();}
    if mode==8 {chain_add(a,b);return 0;}
    if mode==9 {sampler_apply(a,b);return 0;}
    if mode==10 {sampler_accept(a,b,c);return 0;}
    if mode==11 {sampler_reset(a);return 0;}
    if mode==12 {return sampler_clone(a);}
    if mode==13 {sampler_free(a);return 0;}
    if mode==14 {return rng_next(a);}
    if mode==15 {return rng_new(a);}
    if mode==16 {return sf_f64_bits(rng_uniform(a));}
    if mode==17 {return make_simple(9,0,b,a,0,0);}
    if mode==18 {return make_simple(10,0,0,a,0,0);}
    if mode==19 {let h=make_simple(11,0,c,a,b,0);let ignored=rec_set(h,7,rng_new(d));return h;}
    if mode==30 {return cfg_new(a,b);}
    if mode==31 {cfg_add(a,b);return 0;}
    if mode==32 {return rec_set(a,b,c);}
    if mode==33 {return rec_get(a,b);}
    if mode==34 {return configured_init(a);}
    if mode==35 {return chain_n(a);}
    if mode==36 {return chain_get_wrap(a,b);}
    if mode==37 {return chain_remove(a,b);}
    if mode==38 {chain_add_wrap(a,b,c);return 0;}
    if mode==39 {chain_free_shallow(a);return 0;}
    if mode==40 {return dry_init(a,b,c,d);}
    if mode==41 {return logit_bias_init(a,b,c);}
    if mode==20 {return selftest();}
    sf_fail(2);return 0;
}
fn main() {print(sf_entry(20,0,0,0,0,0,0,0));}
