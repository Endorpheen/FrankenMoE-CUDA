// canonical-context.goose - canonical CONTEXT/BATCH/UBATCH storage (APX-019/030).
// Goose owns all row/seq/ownership loops; imports are raw record/vector/pointer
// primitives declared in canonical-storage-imports.h. No C++ containers and no
// C++ class casts are used by this module.
//
// Entry modes:
//  0 ctx_new(n_embd,n_embd_state,n_pos,n_seq_max,n_vocab,allow_mixed)
//  1 ctx_free(ctx)
//  2 batch_new(ctx)             3 batch_free(batch)        4 batch_clear(batch)
//  5 add_token(batch,seq)       6 add_seq(batch,idx,seq)   7 set_id(batch,idx,id)
//  8 set_embd(batch,idx,ptr,rows,cols)  9 set_state(...)
// 10 set_pos(batch,idx,ptr)    11 set_output(batch,idx,v) 12 set_decision(batch,idx,v)
// 13 ubatch_reserve(batch,n_seq_tokens,n_seqs)  14 ubatch_from_batch(batch)
// 15 ubatch_free(ub)            16 view_build(ub,out_ptr)  17 ubatch_as_batch(ub,batch)
// 18 batch_n_tokens(batch)

extern "fgc_record_new" fn rnew(kind:i64)->i64;
extern "fgc_record_get" fn rget(r:i64,k:i64)->i64;
extern "fgc_record_set" fn rset(r:i64,k:i64,v:i64)->i64;
extern "fgc_record_kind" fn rkind(r:i64)->i64;
extern "fgc_i64_record_free" fn rfree(r:i64)->i64;
extern "fgc_vec_new" fn vnew(item:i64)->i64;
extern "fgc_vec_len" fn vlen(v:i64)->i64;
extern "fgc_i64_vec_data" fn vdata(v:i64)->i64;
extern "fgc_vec_reserve" fn vreserve(v:i64,n:i64)->i64;
extern "fgc_vec_resize" fn vresize(v:i64,n:i64)->i64;
extern "fgc_i64_vec_free" fn vfree(v:i64)->i64;
extern "fgc_raw_calloc" fn rcalloc(n:i64)->i64;
extern "fgc_raw_free" fn rfree_mem(p:i64);
extern "fgc_raw_memcpy" fn rmemcpy(dst:i64,src:i64,n:i64);
extern "fgc_raw_memset" fn rmemset(dst:i64,val:i64,n:i64);
extern "fgc_raw_ptr_add" fn rptr(base:i64,off:i64)->i64;
extern "fgc_raw_load_i32" fn load32(a:i64)->i64;
extern "fgc_raw_load_i8" fn load8(a:i64)->i64;
extern "fgc_raw_store_i32" fn store32(a:i64,v:i64);
extern "fgc_raw_store_i8" fn store8(a:i64,v:i64);
extern "fgc_raw_store_i64" fn store64(a:i64,v:i64);
extern "fgc_ctx_entries" fn ctx_entries()->i64;

fn ensure(ok:bool) {if !ok {assert(false);}}
fn add(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {ensure(a>=0&&b>=0&&(a==0||b<=9223372036854775807/a));return a*b;}
fn sub(a:i64,b:i64)->i64 {ensure(a>=b);return a-b;}
fn idiv(a:i64,b:i64)->i64 {ensure(b>0&&a>=0);return a/b;}

fn kind_ctx()->i64 {return 1129601073;}
fn kind_batch()->i64 {return 1111577649;}
fn kind_ub()->i64 {return 1430410289;}
fn token_null()->i64 {return -1;}

fn g(r:i64,k:i64)->i64 {return rget(r,k);}
fn s(r:i64,k:i64,v:i64) {let ignored=rset(r,k,v);}

fn vmake(item:i64)->i64 {
    let v=vnew(item);
    ensure(v!=0);
    return v;
}
fn vpush32(v:i64,val:i64) {
    let n=vlen(v);
    ensure(vresize(v,add(n,1))>=0);
    store32(rptr(vdata(v),mul(n,4)),val);
}
fn vpush8(v:i64,val:i64) {
    let n=vlen(v);
    ensure(vresize(v,add(n,1))>=0);
    store8(rptr(vdata(v),n),val);
}
fn vget32(v:i64,i:i64)->i64 {ensure(i>=0&&i<vlen(v));return load32(rptr(vdata(v),mul(i,4)));}
fn vget8(v:i64,i:i64)->i64 {ensure(i>=0&&i<vlen(v));return load8(rptr(vdata(v),i));}

fn batch_array(b:i64,k:i64,item:i64) {
    let v=vmake(item);
    s(b,k,v);
}
fn batch_init_arrays(b:i64) {
    batch_array(b,20,1);  // is_embd
    batch_array(b,21,4);  // token
    batch_array(b,22,4);  // embd
    batch_array(b,23,4);  // state
    batch_array(b,24,4);  // pos
    batch_array(b,25,4);  // n_seq_id
    batch_array(b,26,4);  // seq flat
    batch_array(b,27,4);  // seq offsets
    batch_array(b,28,1);  // output
    batch_array(b,29,4);  // decision
    batch_array(b,30,1);  // used
    batch_array(b,31,4);  // out ids
    batch_array(b,32,1);  // has_embd
    batch_array(b,33,1);  // has_state
    vpush32(g(b,27),0);
}
fn batch_free_arrays(b:i64) {
    var k:i64=20;
    while k<=33 {let v=g(b,k);if v!=0 {let ignored=vfree(v);}k=add(k,1);}
}

fn ctx_new(n_embd:i64,n_state:i64,n_pos:i64,n_seq_max:i64,n_vocab:i64,allow:i64)->i64 {
    let r=rnew(kind_ctx());
    ensure(r!=0);
    s(r,0,n_embd);s(r,1,n_state);s(r,2,n_pos);s(r,3,n_seq_max);s(r,4,n_vocab);s(r,5,0);s(r,6,allow);
    return r;
}
fn batch_new(ctx:i64)->i64 {
    ensure(rkind(ctx)==kind_ctx());
    let b=rnew(kind_batch());
    ensure(b!=0);
    s(b,0,ctx);
    s(b,1,0);s(b,2,0);s(b,3,0);
    s(b,4,g(ctx,2));s(b,5,g(ctx,3));s(b,6,g(ctx,4));s(b,7,g(ctx,6));
    s(b,8,0);s(b,9,0);s(b,10,0);s(b,11,0);s(b,12,0);s(b,13,0);
    batch_init_arrays(b);
    return b;
}
fn batch_clear(b:i64) {
    ensure(rkind(b)==kind_batch());
    s(b,1,0);s(b,2,0);s(b,8,0);s(b,9,0);s(b,10,0);s(b,11,0);s(b,12,0);s(b,13,0);
    var k:i64=20;
    while k<=33 {let v=g(b,k);if v!=0 {let ignored=vresize(v,0);}k=add(k,1);}
    vpush32(g(b,27),0);
}

fn add_seq_raw(b:i64,idx:i64,seq:i64) {
    let fv=g(b,26);
    vpush32(fv,seq);
    let nv=g(b,25);
    let nsq=vget32(nv,idx);
    store32(rptr(vdata(nv),mul(idx,4)),add(nsq,1));
}
fn add_token(b:i64,seq:i64)->i64 {
    ensure(rkind(b)==kind_batch());
    let ctx=g(b,0);
    if seq<0||seq>=g(ctx,3) {return -3;}
    let n=g(b,1);
    s(b,1,add(n,1));
    let tv=g(b,21);let iv=g(b,20);let ov=g(b,28);let dv=g(b,29);let hev=g(b,32);let hsv=g(b,33);let nqv=g(b,25);let ofv=g(b,27);let fv=g(b,26);
    vpush32(tv,token_null());
    vpush8(iv,0);
    vpush8(ov,0);
    vpush32(dv,0);
    vpush8(hev,0);
    vpush8(hsv,0);
    vpush32(nqv,0);
    let np=g(b,4);
    let pv=g(b,24);
    var q:i64=0;
    while q<np {vpush32(pv,0);q=add(q,1);}
    add_seq_raw(b,n,seq);
    // close the row: seqoff[i+1] = current flat length
    let fe=vlen(fv);
    vpush32(ofv,fe);
    return n;
}
fn add_seq(b:i64,idx:i64,seq:i64)->i64 {
    ensure(rkind(b)==kind_batch());
    let ctx=g(b,0);
    if idx<0||idx>=g(b,1) {return 0;}
    if seq<0||seq>=g(ctx,3) {return 0;}
    add_seq_raw(b,idx,seq);
    return 1;
}
fn set_id(b:i64,idx:i64,id:i64)->i64 {
    if idx<0||idx>=g(b,1) {return 0;}
    if id<0||id>=g(g(b,0),4) {return 0;}
    store32(rptr(vdata(g(b,21)),mul(idx,4)),id);
    return 1;
}
fn append_floats(v:i64,src:i64,total:i64) {
    let old=vlen(v);
    ensure(vresize(v,add(old,total))>=0);
    rmemcpy(rptr(vdata(v),mul(old,4)),src,mul(total,4));
}
fn set_embd(b:i64,idx:i64,src:i64,rows:i64,cols:i64)->i64 {
    if idx<0||idx>=g(b,1)||src==0 {return 0;}
    let total=mul(rows,cols);
    let cur=g(b,2);
    if cur==0 {
        let expect=g(g(b,0),0);
        if expect>0&&total!=expect {return 0;}
        s(b,2,total);
    } else {if total!=cur {return 0;}}
    if vget8(g(b,32),idx)!=0 {return 0;}
    append_floats(g(b,22),src,total);
    store8(rptr(vdata(g(b,32)),idx),1);
    s(b,11,1);
    return 1;
}
fn set_state(b:i64,idx:i64,src:i64,rows:i64,cols:i64)->i64 {
    if idx<0||idx>=g(b,1)||src==0 {return 0;}
    let w=g(g(b,0),1);
    if w==0 {return 0;}
    let total=mul(rows,cols);
    if total!=w {return 0;}
    if vget8(g(b,33),idx)!=0 {return 0;}
    append_floats(g(b,23),src,total);
    store8(rptr(vdata(g(b,33)),idx),1);
    s(b,12,1);
    return 1;
}
fn set_pos(b:i64,idx:i64,src:i64)->i64 {
    if idx<0||idx>=g(b,1)||src==0 {return 0;}
    let npos=g(b,4);
    let n=if vget32(g(b,21),idx)==token_null() {npos} else {1};
    let dst=rptr(vdata(g(b,24)),mul(mul(idx,npos),4));
    rmemcpy(dst,src,mul(n,4));
    return 1;
}
fn set_output(b:i64,idx:i64,v:i64)->i64 {
    if idx<0||idx>=g(b,1) {return 0;}
    store8(rptr(vdata(g(b,28)),idx),if v!=0 {1} else {0});
    return 1;
}
fn set_decision(b:i64,idx:i64,v:i64)->i64 {
    if idx<0||idx>=g(b,1) {return 0;}
    store32(rptr(vdata(g(b,29)),mul(idx,4)),v);
    return 1;
}

fn copy_i32(dst:i64,src:i64) {
    let n=vlen(src);
    ensure(vresize(dst,n)>=0);
    var i:i64=0;
    while i<n {store32(rptr(vdata(dst),mul(i,4)),load32(rptr(vdata(src),mul(i,4))));i=add(i,1);}
}
fn copy_i8(dst:i64,src:i64) {
    let n=vlen(src);
    ensure(vresize(dst,n)>=0);
    var i:i64=0;
    while i<n {store8(rptr(vdata(dst),i),load8(rptr(vdata(src),i)));i=add(i,1);}
}
fn copy_float(dst:i64,src:i64) {
    let n=vlen(src);
    ensure(vresize(dst,n)>=0);
    if n>0 {rmemcpy(vdata(dst),vdata(src),mul(n,4));}
}
fn ub_array(u:i64,k:i64,item:i64) {
    let v=vmake(item);
    s(u,k,v);
}
fn ub_init_arrays(u:i64) {
    ub_array(u,20,4);ub_array(u,21,4);ub_array(u,22,4);ub_array(u,23,4);
    ub_array(u,24,4);ub_array(u,25,4);ub_array(u,26,4);ub_array(u,27,4);
    ub_array(u,28,4);ub_array(u,29,1);ub_array(u,30,1);ub_array(u,31,4);
    ub_array(u,32,4);ub_array(u,33,8);
}
fn ub_new()->i64 {
    let u=rnew(kind_ub());
    ensure(u!=0);
    ub_init_arrays(u);
    return u;
}

fn ubatch_from_batch(b:i64)->i64 {
    ensure(rkind(b)==kind_batch());
    let u=ub_new();
    let nt=g(b,1);
    s(u,0,nt);
    s(u,1,if nt>0 {1} else {0});
    s(u,2,nt);
    s(u,3,0);
    s(u,4,g(b,4));
    s(u,5,0);
    s(u,6,g(b,10));
    let ut=g(u,20);let ue=g(u,21);let us=g(u,22);let up=g(u,23);let un=g(u,24);let uf=g(u,25);let uo=g(u,26);
    let uq=g(u,27);let ui=g(u,28);let uout=g(u,29);let ub=g(u,31);let ud=g(u,32);
    let bt=g(b,21);let be=g(b,22);let bs=g(b,23);let bp=g(b,24);let bn=g(b,25);let bf=g(b,26);let bo=g(b,27);
    let bi=g(b,28);let bd=g(b,29);let bidx=g(b,31);
    copy_i32(ut,bt);
    copy_float(ue,be);
    copy_float(us,bs);
    copy_i32(up,bp);
    copy_i32(un,bn);
    copy_i32(uf,bf);
    copy_i32(uo,bo);
    copy_i8(uout,bi);
    copy_i32(ub,bd);
    copy_i32(ud,bidx);
    // unique seq ids and per-seq index, O(n^2) scan
    var i:i64=0;
    while i<nt {
        let off=vget32(bo,i);
        let nsq=vget32(bn,i);
        var k:i64=0;
        while k<nsq {
            let sid=load32(rptr(vdata(bf),mul(add(off,k),4)));
            var found:i64=0;
            var j:i64=0;
            while j<vlen(uq) {if vget32(uq,j)==sid {found=1;break;}j=add(j,1);}
            if found==0 {vpush32(uq,sid);}
            k=add(k,1);
        }
        i=add(i,1);
    }
    s(u,3,vlen(uq));
    let ctx=g(b,0);
    let nsqmax=g(ctx,3);
    ensure(vresize(ui,nsqmax)>=0);
    var q:i64=0;
    while q<nsqmax {store32(rptr(vdata(ui),mul(q,4)),-1);q=add(q,1);}
    var w:i64=0;
    while w<vlen(uq) {
        let sid=vget32(uq,w);
        store32(rptr(vdata(ui),mul(sid,4)),w);
        w=add(w,1);
    }
    return u;
}

fn ubatch_reserve(b:i64,nst:i64,nseq:i64)->i64 {
    ensure(rkind(b)==kind_batch());
    if nseq<=0 {return 0;}
    batch_clear(b);
    let nt=mul(nst,nseq);
    s(b,1,nt);s(b,10,1);
    let u=ub_new();
    s(u,0,nt);s(u,1,nst);s(u,2,nseq);s(u,3,nseq);s(u,4,g(b,4));s(u,5,1);s(u,6,1);
    ensure(vresize(g(b,21),nt)>=0);
    ensure(vresize(g(b,25),nt)>=0);
    ensure(vresize(g(b,26),nt)>=0);
    ensure(vresize(g(b,28),nt)>=0);
    ensure(vresize(g(b,31),nt)>=0);
    ensure(vresize(g(b,24),mul(nt,g(b,4)))>=0);
    ensure(vresize(g(b,27),add(nt,1))>=0);
    var z:i64=0;
    while z<mul(nt,g(b,4)) {store32(rptr(vdata(g(b,24)),mul(z,4)),0);z=add(z,1);}
    var i:i64=0;
    while i<nt {
        let sid=sub(i,mul(idiv(i,nseq),nseq));
        store32(rptr(vdata(g(b,21)),mul(i,4)),0);
        store32(rptr(vdata(g(b,25)),mul(i,4)),1);
        store32(rptr(vdata(g(b,27)),mul(i,4)),i);
        store32(rptr(vdata(g(b,26)),mul(i,4)),sid);
        store8(rptr(vdata(g(b,28)),i),0);
        i=add(i,1);
    }
    store32(rptr(vdata(g(b,27)),mul(nt,4)),nt);
    copy_i32(g(u,20),g(b,21));
    copy_i32(g(u,24),g(b,25));
    copy_i32(g(u,25),g(b,26));
    copy_i32(g(u,26),g(b,27));
    copy_i8(g(u,29),g(b,28));
    ensure(vresize(g(u,27),nseq)>=0);
    ensure(vresize(g(u,28),g(g(b,0),3))>=0);
    var t:i64=0;
    while t<nseq {
        store32(rptr(vdata(g(u,27)),mul(t,4)),t);
        store32(rptr(vdata(g(u,28)),mul(t,4)),t);
        t=add(t,1);
    }
    return u;
}

fn ubatch_as_batch(u:i64,b:i64)->i64 {
    ensure(rkind(u)==kind_ub());
    ensure(rkind(b)==kind_batch());
    s(b,1,g(u,0));s(b,2,g(u,2));s(b,4,g(u,4));
    copy_i32(g(b,21),g(u,20));
    copy_float(g(b,22),g(u,21));
    copy_float(g(b,23),g(u,22));
    copy_i32(g(b,24),g(u,23));
    copy_i32(g(b,25),g(u,24));
    copy_i32(g(b,26),g(u,25));
    copy_i32(g(b,27),g(u,26));
    copy_i8(g(b,28),g(u,29));
    copy_i32(g(b,29),g(u,31));
    copy_i32(g(b,31),g(u,32));
    return 1;
}

fn view_build(u:i64,out:i64)->i64 {
    ensure(rkind(u)==kind_ub());
    let nt=g(u,0);
    ensure(vresize(g(u,33),nt)>=0);
    let sbase=vdata(g(u,25));
    var i:i64=0;
    while i<nt {
        let off=vget32(g(u,26),i);
        store64(rptr(vdata(g(u,33)),mul(i,8)),rptr(sbase,mul(off,4)));
        i=add(i,1);
    }
    store64(out,g(u,0));          // n_tokens
    store64(rptr(out,8),g(u,1));  // n_seq_tokens
    store64(rptr(out,16),g(u,2)); // n_seqs
    store64(rptr(out,24),g(u,3)); // n_seqs_unq
    store64(rptr(out,32),g(u,4)); // n_pos
    store32(rptr(out,40),g(u,5)); // equal
    store64(rptr(out,48),vdata(g(u,20)));  // token
    store64(rptr(out,56),vdata(g(u,21)));  // embd
    store64(rptr(out,64),vdata(g(u,22)));  // embd_state
    store64(rptr(out,72),vdata(g(u,23)));  // pos
    store64(rptr(out,80),vdata(g(u,24)));  // n_seq_id
    store64(rptr(out,88),vdata(g(u,33)));  // seq_id
    store64(rptr(out,96),vdata(g(u,27)));  // seq_id_unq
    store64(rptr(out,104),vdata(g(u,28))); // seq_idx
    store64(rptr(out,112),vdata(g(u,29))); // output
    store64(rptr(out,120),vdata(g(u,30))); // type
    store64(rptr(out,128),vdata(g(u,31))); // decision_order
    store64(rptr(out,136),vdata(g(u,32))); // batch_idxs
    return 1;
}

fn entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
    if mode==0 {return ctx_new(a,b,c,d,e,f);}
    if mode==1 {return rfree(a);}
    if mode==2 {return batch_new(a);}
    if mode==3 {batch_free_arrays(a);return rfree(a);}
    if mode==4 {batch_clear(a);return 0;}
    if mode==5 {return add_token(a,b);}
    if mode==6 {return add_seq(a,b,c);}
    if mode==7 {return set_id(a,b,c);}
    if mode==8 {return set_embd(a,b,c,d,e);}
    if mode==9 {return set_state(a,b,c,d,e);}
    if mode==10 {return set_pos(a,b,c);}
    if mode==11 {return set_output(a,b,c);}
    if mode==12 {return set_decision(a,b,c);}
    if mode==13 {return ubatch_reserve(a,b,c);}
    if mode==14 {return ubatch_from_batch(a);}
    if mode==15 {
        ensure(rkind(a)==kind_ub());
        var k:i64=20;
        while k<=33 {
            let v=g(a,k);
            if v!=0 {let ignored=vfree(v);}
            k=add(k,1);
        }
        return rfree(a);
    }
    if mode==16 {return view_build(a,b);}
    if mode==17 {return ubatch_as_batch(a,b);}
    if mode==18 {return g(a,1);}
    ensure(false);
    return -1;
}

fn main() {print(entry(18,0,0,0,0,0,0));}
