// APX030 VOCAB service-control in Goose.
//
// Owns hash/chain lookup, merge finalization, constructors/destructors,
// session/meta/context cache control and failure propagation.  C/C++ keeps
// only exact typed storage bridges, raw loads/stores/memcpy/memcmp and named
// immutable Unicode/literal effects.

extern "sc_record_new" fn rnew(kind:i64)->i64;
extern "sc_record_get" fn rget(h:i64,k:i64)->i64;
extern "sc_record_set" fn rset_raw(h:i64,k:i64,v:i64)->i64;
extern "sc_record_kind" fn rkind(h:i64)->i64;
extern "sc_record_free" fn rfree(h:i64);
extern "sc_vec_new" fn vnew(item:i64)->i64;
extern "sc_vec_len" fn vlen_raw(h:i64)->i64;
extern "sc_vec_data" fn vdata_raw(h:i64)->i64;
extern "sc_vec_resize" fn vresize(h:i64,n:i64)->i64;
extern "sc_vec_free" fn vfree(h:i64);
extern "sc_map_new" fn mnew()->i64;
extern "sc_map_put" fn mput(h:i64,k:i64,v:i64)->i64;
extern "sc_map_found" fn mfound(h:i64,k:i64)->i64;
extern "sc_map_value" fn mvalue(h:i64,k:i64)->i64;
extern "sc_map_free" fn mfree(h:i64);

extern "apx030_mem_load8" fn load8(a:i64,i:i64)->i64;
extern "apx030_mem_load64" fn load64(a:i64,i:i64)->i64;
extern "apx030_mem_store8" fn store8(a:i64,i:i64,v:i64);
extern "apx030_mem_store64" fn store64(a:i64,i:i64,v:i64);
extern "apx030_mem_copy" fn memcopy(d:i64,s:i64,n:i64);
extern "apx030_mem_eq" fn memeq(a:i64,b:i64,n:i64)->i64;

fn kind_vocab()->i64 {return 0x564f434142000001;}
fn kind_session()->i64 {return 0x53455353494f4e02;}
fn kind_meta()->i64 {return 0x4d45544100000003;}
fn kind_ctx()->i64 {return 0x434f4e5445585404;}

// Vocab keys.
fn vk_pre()->i64 {return 1;}
fn vk_ab()->i64 {return 2;}
fn vk_bos()->i64 {return 3;}
fn vk_ae()->i64 {return 4;}
fn vk_eos()->i64 {return 5;}
fn vk_clean()->i64 {return 6;}
fn vk_ign()->i64 {return 7;}
fn vk_esc()->i64 {return 8;}
fn vk_ntok()->i64 {return 10;}
fn vk_text()->i64 {return 11;}
fn vk_toff()->i64 {return 12;}
fn vk_tlen()->i64 {return 13;}
fn vk_attr()->i64 {return 14;}
fn vk_score()->i64 {return 15;}
fn vk_tmap()->i64 {return 16;}
fn vk_tent()->i64 {return 17;}
fn vk_bmap()->i64 {return 18;}
fn vk_bent()->i64 {return 19;}
fn vk_mbytes()->i64 {return 20;}
fn vk_merges()->i64 {return 21;}
fn vk_specials()->i64 {return 22;}
fn vk_final()->i64 {return 23;}

// Session keys.
fn sk_out()->i64 {return 30;}
fn sk_cpt()->i64 {return 31;}
fn sk_word()->i64 {return 32;}
fn sk_sym()->i64 {return 33;}
fn sk_heap()->i64 {return 34;}
fn sk_frag0()->i64 {return 35;}
fn sk_frag1()->i64 {return 36;}
fn sk_cur()->i64 {return 37;}
fn sk_special()->i64 {return 38;}
fn sk_text()->i64 {return 39;}

// Meta keys.
fn mk_model_off()->i64 {return 40;}
fn mk_model_len()->i64 {return 41;}
fn mk_pre_off()->i64 {return 42;}
fn mk_pre_len()->i64 {return 43;}
fn mk_bytes()->i64 {return 44;}
fn mk_tokens()->i64 {return 45;}
fn mk_merges()->i64 {return 46;}
fn mk_mbytes()->i64 {return 47;}
fn mk_ints()->i64 {return 1000;}
fn mk_specials()->i64 {return 49;}
fn mk_suppress()->i64 {return 50;}

// Ctx keys.
fn ck_old()->i64 {return 60;}
fn ck_tokens()->i64 {return 61;}
fn ck_bytes()->i64 {return 62;}
fn ck_tmap()->i64 {return 63;}
fn ck_tent()->i64 {return 64;}
fn ck_merges()->i64 {return 65;}
fn ck_mbytes()->i64 {return 66;}
fn ck_specials()->i64 {return 67;}
fn ck_poff()->i64 {return 68;}
fn ck_plen()->i64 {return 69;}
fn ck_pbytes()->i64 {return 70;}
fn ck_eog()->i64 {return 71;}
fn ck_suppress()->i64 {return 72;}
fn ck_out()->i64 {return 73;}

fn rset(h:i64,k:i64,v:i64)->i64 {return rset_raw(h,k,v);}
fn rkind_is(h:i64,k:i64)->i64 {return if rkind(h)==k {1}else{0};}
fn vlen(h:i64)->i64 {return vlen_raw(h);}
fn vdata(h:i64)->i64 {return vdata_raw(h);}
fn rv_get(v:i64,i:i64,f:i64,item:i64)->i64 {
    if i<0||i>=vlen(v) {return 0;}
    return load64(vdata(v)+i*item,f);
}
fn rv_set(v:i64,i:i64,f:i64,x:i64,item:i64)->i64 {
    if i<0||i>=vlen(v) {return -1;}
    store64(vdata(v)+i*item,f,x);
    return 0;
}
fn push_i64(v:i64,x:i64)->i64 {
    let n=vlen(v);
    if n<0||vresize(v,n+1)!=0 {return -1;}
    store64(vdata(v),n,x);
    return 0;
}
fn get_i64(v:i64,i:i64)->i64 {
    if i<0||i>=vlen(v) {return 0;}
    return load64(vdata(v),i);
}
fn set_i64(v:i64,i:i64,x:i64)->i64 {
    if i<0||i>=vlen(v) {return -1;}
    store64(vdata(v),i,x);
    return 0;
}
fn push_byte(v:i64,b:i64)->i64 {
    let n=vlen(v);
    if n<0||vresize(v,n+1)!=0 {return -1;}
    store8(vdata(v),n,b);
    return 0;
}
fn append_mem(v:i64,a:i64,l:i64)->i64 {
    if l<=0 {return 0;}
    let n=vlen(v);
    if n<0||vresize(v,n+l)!=0 {return -1;}
    memcopy(vdata(v)+n,a,l);
    return 0;
}
fn push_rec(v:i64)->i64 {
    let n=vlen(v);
    if n<0||vresize(v,n+1)!=0 {return -1;}
    return n;
}
fn map_head(m:i64,h:i64)->i64 {
    if mfound(m,h)!=0 {return mvalue(m,h);}
    return -1;
}

fn hash_bytes(a:i64,l:i64)->i64 {
    var h:i64=1469598103934665603;
    var i:i64=0;
    while i<l {h=h^load8(a,i); h=h*1099511628211; i+=1;}
    return h;
}
fn hash_pair(la:i64,ll:i64,ra:i64,rl:i64)->i64 {
    var h=hash_bytes(la,ll);
    h=h^(ll*0x9e3779b1);
    let rh=hash_bytes(ra,rl);
    h=(h<<7)|(h>>57);
    h=h^rh^(rl*0x100000001b3);
    return h;
}
fn free_if(h:i64) {if h!=0 {vfree(h);}}
fn free_map_if(h:i64) {if h!=0 {mfree(h);}}

// ---------- vocab ----------
fn vocab_cleanup(v:i64,text:i64,toff:i64,tlen:i64,attr:i64,score:i64,tmap:i64,tent:i64,bmap:i64,bent:i64,mbytes:i64,merges:i64,specials:i64)->i64 {
    free_if(text);free_if(toff);free_if(tlen);free_if(attr);free_if(score);
    free_map_if(tmap);free_if(tent);free_map_if(bmap);free_if(bent);
    free_if(mbytes);free_if(merges);free_if(specials);
    rfree(v);
    return 0;
}
fn vocab_new()->i64 {
    let v=rnew(kind_vocab()); if v==0 {return 0;}
    let text=vnew(1); let toff=vnew(8); let tlen=vnew(8);
    let attr=vnew(8); let score=vnew(8); let tmap=mnew(); let tent=vnew(32);
    let bmap=mnew(); let bent=vnew(48); let mbytes=vnew(1); let merges=vnew(40);
    let specials=vnew(8);
    var ok:i64=1;
    if text==0||toff==0||tlen==0||attr==0||score==0||tmap==0||tent==0||bmap==0||bent==0||mbytes==0||merges==0||specials==0 {ok=0;}
    if ok==1 {
        if rset(v,vk_text(),text)!=0 {ok=0;}
        if ok==1&&rset(v,vk_toff(),toff)!=0 {ok=0;}
        if ok==1&&rset(v,vk_tlen(),tlen)!=0 {ok=0;}
        if ok==1&&rset(v,vk_attr(),attr)!=0 {ok=0;}
        if ok==1&&rset(v,vk_score(),score)!=0 {ok=0;}
        if ok==1&&rset(v,vk_tmap(),tmap)!=0 {ok=0;}
        if ok==1&&rset(v,vk_tent(),tent)!=0 {ok=0;}
        if ok==1&&rset(v,vk_bmap(),bmap)!=0 {ok=0;}
        if ok==1&&rset(v,vk_bent(),bent)!=0 {ok=0;}
        if ok==1&&rset(v,vk_mbytes(),mbytes)!=0 {ok=0;}
        if ok==1&&rset(v,vk_merges(),merges)!=0 {ok=0;}
        if ok==1&&rset(v,vk_specials(),specials)!=0 {ok=0;}
    }
    if ok==0 {return vocab_cleanup(v,text,toff,tlen,attr,score,tmap,tent,bmap,bent,mbytes,merges,specials);}
    return v;
}
fn vocab_free(v:i64) {
    if rkind_is(v,kind_vocab())==0 {return;}
    free_if(rget(v,vk_text()));free_if(rget(v,vk_toff()));free_if(rget(v,vk_tlen()));
    free_if(rget(v,vk_attr()));free_if(rget(v,vk_score()));free_if(rget(v,vk_tent()));
    free_map_if(rget(v,vk_tmap()));free_map_if(rget(v,vk_bmap()));
    free_if(rget(v,vk_bent()));free_if(rget(v,vk_mbytes()));free_if(rget(v,vk_merges()));
    free_if(rget(v,vk_specials()));
    rfree(v);
}
fn vocab_add_token(v:i64,id:i64,ta:i64,tl:i64,attr:i64)->i64 {
    if rkind_is(v,kind_vocab())==0||id!=rget(v,vk_ntok()) {return -1;}
    let text=rget(v,vk_text()); let off=vlen(text);
    let otoff=vlen(rget(v,vk_toff())); let otlen=vlen(rget(v,vk_tlen()));
    let oattr=vlen(rget(v,vk_attr())); let oscore=vlen(rget(v,vk_score()));
    if append_mem(text,ta,tl)!=0 {return -1;}
    if push_i64(rget(v,vk_toff()),off)!=0 {vresize(text,off);return -1;}
    if push_i64(rget(v,vk_tlen()),tl)!=0 {vresize(rget(v,vk_toff()),otoff);vresize(text,off);return -1;}
    if push_i64(rget(v,vk_attr()),attr)!=0 {vresize(rget(v,vk_tlen()),otlen);vresize(rget(v,vk_toff()),otoff);vresize(text,off);return -1;}
    if push_i64(rget(v,vk_score()),0)!=0 {vresize(rget(v,vk_attr()),oattr);vresize(rget(v,vk_tlen()),otlen);vresize(rget(v,vk_toff()),otoff);vresize(text,off);return -1;}
    if rset(v,vk_ntok(),id+1)!=0 {vresize(rget(v,vk_score()),oscore);return -1;}
    if rset(v,vk_final(),0)!=0 {return -1;}
    return 0;
}
fn vocab_add_merge(v:i64,la:i64,ll:i64,ra:i64,rl:i64,rank:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return -1;}
    let bytes=rget(v,vk_mbytes()); let loff=vlen(bytes);
    if append_mem(bytes,la,ll)!=0 {return -1;}
    let roff=vlen(bytes);
    if append_mem(bytes,ra,rl)!=0 {vresize(bytes,loff);return -1;}
    let merges=rget(v,vk_merges()); let i=push_rec(merges);
    if i<0 {vresize(bytes,loff);return -1;}
    if rv_set(merges,i,0,loff,40)!=0||rv_set(merges,i,1,ll,40)!=0||rv_set(merges,i,2,roff,40)!=0||rv_set(merges,i,3,rl,40)!=0||rv_set(merges,i,4,rank,40)!=0 {vresize(merges,i);vresize(bytes,loff);return -1;}
    if rset(v,vk_final(),0)!=0 {return -1;}
    return 0;
}
fn vocab_finalize(v:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return -1;}
    if rget(v,vk_final())!=0 {return rget(v,vk_ntok());}
    let tmap=rget(v,vk_tmap()); let tent=rget(v,vk_tent());
    let bmap=rget(v,vk_bmap()); let bent=rget(v,vk_bent());
    let specials=rget(v,vk_specials());
    free_map_if(tmap);free_map_if(bmap);free_if(tent);free_if(bent);free_if(specials);
    // Clear fields first so a failure cannot leave freed handles live.
    if rset(v,vk_tmap(),0)!=0||rset(v,vk_tent(),0)!=0||rset(v,vk_bmap(),0)!=0||rset(v,vk_bent(),0)!=0||rset(v,vk_specials(),0)!=0 {return -1;}
    let nt=mnew(); let ne=vnew(32); let nb=mnew(); let nbe=vnew(48); let ns=vnew(8);
    if nt==0||ne==0||nb==0||nbe==0||ns==0 {free_map_if(nt);free_if(ne);free_map_if(nb);free_if(nbe);free_if(ns);return -1;}
    var ok:i64=1;
    let text=rget(v,vk_text()); let base=vdata(text);
    let toff=rget(v,vk_toff()); let tlen=rget(v,vk_tlen()); let attrs=rget(v,vk_attr());
    let n=rget(v,vk_ntok());
    var id:i64=0;
    while ok==1&&id<n {
        let off=get_i64(toff,id); let len=get_i64(tlen,id);
        let h=hash_bytes(base+off,len);
        let head=map_head(nt,h);
        let e=push_rec(ne);
        if e<0 {ok=0;} else {
            if rv_set(ne,e,0,head,32)!=0||rv_set(ne,e,1,id,32)!=0||rv_set(ne,e,2,off,32)!=0||rv_set(ne,e,3,len,32)!=0 {ok=0;}
            else if mput(nt,h,e)!=0 {ok=0;}
            else if (get_i64(attrs,id)&(1|8|16))!=0 {if push_i64(ns,id)!=0 {ok=0;}}
        }
        id+=1;
    }
    let mbytes=rget(v,vk_mbytes()); let mbase=vdata(mbytes); let merges=rget(v,vk_merges());
    let nm=vlen(merges); var i:i64=0;
    while ok==1&&i<nm {
        let loff=rv_get(merges,i,0,40); let llen=rv_get(merges,i,1,40);
        let roff=rv_get(merges,i,2,40); let rlen=rv_get(merges,i,3,40);
        let rank=rv_get(merges,i,4,40);
        let h=hash_pair(mbase+loff,llen,mbase+roff,rlen);
        let head=map_head(nb,h);
        let e=push_rec(nbe);
        if e<0 {ok=0;} else {
            if rv_set(nbe,e,0,head,48)!=0||rv_set(nbe,e,1,rank,48)!=0||rv_set(nbe,e,2,loff,48)!=0||rv_set(nbe,e,3,llen,48)!=0||rv_set(nbe,e,4,roff,48)!=0||rv_set(nbe,e,5,rlen,48)!=0 {ok=0;}
            else if mput(nb,h,e)!=0 {ok=0;}
        }
        i+=1;
    }
    if ok==0 {free_map_if(nt);free_if(ne);free_map_if(nb);free_if(nbe);free_if(ns);return -1;}
    if rset(v,vk_tmap(),nt)!=0||rset(v,vk_tent(),ne)!=0||rset(v,vk_bmap(),nb)!=0||rset(v,vk_bent(),nbe)!=0||rset(v,vk_specials(),ns)!=0||rset(v,vk_final(),1)!=0 {free_map_if(nt);free_if(ne);free_map_if(nb);free_if(nbe);free_if(ns);return -1;}
    return n;
}
fn vocab_cfg(v:i64,k:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return -1;}
    if k==8 {return rget(v,vk_ntok());}
    if k==0 {return rget(v,vk_pre());}
    if k==1 {return rget(v,vk_ab());}
    if k==2 {return rget(v,vk_bos());}
    if k==3 {return rget(v,vk_ae());}
    if k==4 {return rget(v,vk_eos());}
    if k==5 {return rget(v,vk_clean());}
    if k==6 {return rget(v,vk_ign());}
    if k==7 {return rget(v,vk_esc());}
    return -1;
}
fn vocab_set_config1(v:i64,pre:i64,ab:i64,bos:i64)->i64 {
    if rset(v,vk_pre(),pre)!=0 {return -1;}
    if rset(v,vk_ab(),ab)!=0 {return -1;}
    if rset(v,vk_bos(),bos)!=0 {return -1;}
    return 0;
}
fn vocab_set_config2(v:i64,ae:i64,eos:i64,clean:i64,ign:i64,esc:i64)->i64 {
    if rset(v,vk_ae(),ae)!=0 {return -1;}
    if rset(v,vk_eos(),eos)!=0 {return -1;}
    if rset(v,vk_clean(),clean)!=0 {return -1;}
    if rset(v,vk_ign(),ign)!=0 {return -1;}
    if rset(v,vk_esc(),esc)!=0 {return -1;}
    return 0;
}
fn tok_text_addr(v:i64,id:i64)->i64 {
    if rkind_is(v,kind_vocab())==0||id<0||id>=rget(v,vk_ntok()) {return 0;}
    return vdata(rget(v,vk_text()))+get_i64(rget(v,vk_toff()),id);
}
fn tok_text_len(v:i64,id:i64)->i64 {
    if rkind_is(v,kind_vocab())==0||id<0||id>=rget(v,vk_ntok()) {return 0;}
    return get_i64(rget(v,vk_tlen()),id);
}
fn tok_attr(v:i64,id:i64)->i64 {
    if rkind_is(v,kind_vocab())==0||id<0||id>=rget(v,vk_ntok()) {return -1;}
    return get_i64(rget(v,vk_attr()),id);
}
fn vocab_text_to_token(v:i64,addr:i64,len:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return -1;}
    if vocab_finalize(v)<0 {return -1;}
    let h=hash_bytes(addr,len); var e=map_head(rget(v,vk_tmap()),h);
    let tent=rget(v,vk_tent()); let base=vdata(rget(v,vk_text()));
    while e>=0 {
        let off=rv_get(tent,e,2,32); let l=rv_get(tent,e,3,32);
        if l==len&&memeq(base+off,addr,len)!=0 {return rv_get(tent,e,1,32);}
        e=rv_get(tent,e,0,32);
    }
    return -1;
}
fn vocab_bpe_rank(v:i64,la:i64,ll:i64,ra:i64,rl:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return -1;}
    if vocab_finalize(v)<0 {return -1;}
    let h=hash_pair(la,ll,ra,rl); var e=map_head(rget(v,vk_bmap()),h);
    let bent=rget(v,vk_bent()); let base=vdata(rget(v,vk_mbytes()));
    while e>=0 {
        let loff=rv_get(bent,e,2,48); let llen=rv_get(bent,e,3,48);
        let roff=rv_get(bent,e,4,48); let rlen=rv_get(bent,e,5,48);
        if llen==ll&&rlen==rl&&memeq(base+loff,la,ll)!=0&&memeq(base+roff,ra,rl)!=0 {return rv_get(bent,e,1,48);}
        e=rv_get(bent,e,0,48);
    }
    return -1;
}
fn special_raw_count(v:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return 0;}
    if vocab_finalize(v)<0 {return 0;}
    return vlen(rget(v,vk_specials()));
}
fn special_raw_id(v:i64,i:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return -1;}
    if vocab_finalize(v)<0 {return -1;}
    if i<0||i>=vlen(rget(v,vk_specials())) {return -1;}
    return get_i64(rget(v,vk_specials()),i);
}

// ---------- session ----------
fn session_new(v:i64)->i64 {
    if rkind_is(v,kind_vocab())==0 {return 0;}
    let s=rnew(kind_session()); if s==0 {return 0;}
    let out=vnew(8); let cpt=vnew(8); let word=vnew(1); let sym=vnew(32);
    let heap=vnew(40); let f0=vnew(32); let f1=vnew(32); let sp=vnew(8); let text=vnew(1);
    var ok:i64=1;
    if out==0||cpt==0||word==0||sym==0||heap==0||f0==0||f1==0||sp==0||text==0 {ok=0;}
    if ok==1 {
        if rset(s,sk_out(),out)!=0||rset(s,sk_cpt(),cpt)!=0||rset(s,sk_word(),word)!=0||rset(s,sk_sym(),sym)!=0||rset(s,sk_heap(),heap)!=0||rset(s,sk_frag0(),f0)!=0||rset(s,sk_frag1(),f1)!=0||rset(s,sk_special(),sp)!=0||rset(s,sk_text(),text)!=0||rset(s,sk_cur(),0)!=0 {ok=0;}
    }
    if ok==0 {free_if(out);free_if(cpt);free_if(word);free_if(sym);free_if(heap);free_if(f0);free_if(f1);free_if(sp);free_if(text);rfree(s);return 0;}
    return s;
}
fn session_free(s:i64) {
    if rkind_is(s,kind_session())==0 {return;}
    free_if(rget(s,sk_out()));free_if(rget(s,sk_cpt()));free_if(rget(s,sk_word()));
    free_if(rget(s,sk_sym()));free_if(rget(s,sk_heap()));free_if(rget(s,sk_frag0()));
    free_if(rget(s,sk_frag1()));free_if(rget(s,sk_special()));free_if(rget(s,sk_text()));
    rfree(s);
}
fn session_reset(s:i64) {
    if rkind_is(s,kind_session())==0 {return;}
    vresize(rget(s,sk_out()),0);vresize(rget(s,sk_cpt()),0);vresize(rget(s,sk_word()),0);
    vresize(rget(s,sk_sym()),0);vresize(rget(s,sk_heap()),0);vresize(rget(s,sk_frag0()),0);
    vresize(rget(s,sk_frag1()),0);vresize(rget(s,sk_special()),0);vresize(rget(s,sk_text()),0);
    rset(s,sk_cur(),0);
}
fn frag_vec(s:i64)->i64 {return if rget(s,sk_cur())!=0 {rget(s,sk_frag1())}else{rget(s,sk_frag0())};}
fn frag_next_vec(s:i64)->i64 {return if rget(s,sk_cur())!=0 {rget(s,sk_frag0())}else{rget(s,sk_frag1())};}

// ---------- meta ----------
fn meta_new()->i64 {
    let m=rnew(kind_meta()); if m==0 {return 0;}
    let bytes=vnew(1); let tokens=vnew(32); let merges=vnew(16); let mbytes=vnew(1);
    let sp=mnew(); let sup=vnew(8);
    var ok:i64=1;
    if bytes==0||tokens==0||merges==0||mbytes==0||sp==0||sup==0 {ok=0;}
    if ok==1 {
        if rset(m,mk_bytes(),bytes)!=0||rset(m,mk_tokens(),tokens)!=0||rset(m,mk_merges(),merges)!=0||rset(m,mk_mbytes(),mbytes)!=0||rset(m,mk_specials(),sp)!=0||rset(m,mk_suppress(),sup)!=0 {ok=0;}
    }
    if ok==0 {free_if(bytes);free_if(tokens);free_if(merges);free_if(mbytes);free_map_if(sp);free_if(sup);rfree(m);return 0;}
    return m;
}
fn meta_free(m:i64) {
    if rkind_is(m,kind_meta())==0 {return;}
    free_if(rget(m,mk_bytes()));free_if(rget(m,mk_tokens()));free_if(rget(m,mk_merges()));
    free_if(rget(m,mk_mbytes()));free_map_if(rget(m,mk_specials()));free_if(rget(m,mk_suppress()));
    rfree(m);
}
fn meta_set_str(m:i64,key:i64,a:i64,l:i64)->i64 {
    if rkind_is(m,kind_meta())==0 {return -1;}
    let bytes=rget(m,mk_bytes()); let off=vlen(bytes);
    if append_mem(bytes,a,l)!=0 {return -1;}
    if key==0 {
        if rset(m,mk_model_off(),off)!=0||rset(m,mk_model_len(),l)!=0 {vresize(bytes,off);return -1;}
    } else {
        if rset(m,mk_pre_off(),off)!=0||rset(m,mk_pre_len(),l)!=0 {vresize(bytes,off);return -1;}
    }
    return 0;
}
fn meta_set_i(m:i64,key:i64,v:i64)->i64 {
    if rset(m,mk_ints()+key,v)!=0 {return -1;}
    if rset(m,mk_ints()+100+key,1)!=0 {return -1;}
    return 0;
}
fn meta_add_token(m:i64,a:i64,l:i64,score:i64,tt:i64)->i64 {
    if rkind_is(m,kind_meta())==0 {return -1;}
    let bytes=rget(m,mk_bytes()); let off=vlen(bytes);
    if append_mem(bytes,a,l)!=0 {return -1;}
    let tokens=rget(m,mk_tokens()); let i=push_rec(tokens);
    if i<0 {vresize(bytes,off);return -1;}
    if rv_set(tokens,i,0,off,32)!=0||rv_set(tokens,i,1,l,32)!=0||rv_set(tokens,i,2,score,32)!=0||rv_set(tokens,i,3,tt,32)!=0 {vresize(tokens,i);vresize(bytes,off);return -1;}
    return 0;
}
fn meta_add_merge(m:i64,a:i64,l:i64)->i64 {
    if rkind_is(m,kind_meta())==0 {return -1;}
    let bytes=rget(m,mk_mbytes()); let off=vlen(bytes);
    if append_mem(bytes,a,l)!=0 {return -1;}
    let merges=rget(m,mk_merges()); let i=push_rec(merges);
    if i<0 {vresize(bytes,off);return -1;}
    if rv_set(merges,i,0,off,16)!=0||rv_set(merges,i,1,l,16)!=0 {vresize(merges,i);vresize(bytes,off);return -1;}
    return 0;
}
fn meta_set_special(m:i64,key:i64,id:i64)->i64 {return rset(m,mk_specials()*1000+key+1,id+1);}
fn meta_add_suppress(m:i64,id:i64)->i64 {if rkind_is(m,kind_meta())==0 {return -1;} return push_i64(rget(m,mk_suppress()),id);}
fn meta_str_addr(m:i64,key:i64)->i64 {
    if rkind_is(m,kind_meta())==0 {return 0;}
    return vdata(rget(m,mk_bytes()))+rget(m,if key==0 {mk_model_off()}else{mk_pre_off()});
}
fn meta_str_len(m:i64,key:i64)->i64 {return rget(m,if key==0 {mk_model_len()}else{mk_pre_len()});}
fn meta_i(m:i64,key:i64)->i64 {
    if rkind_is(m,kind_meta())==0 {return -1;}
    if rget(m,mk_ints()+100+key)==0 {return -1;}
    return rget(m,mk_ints()+key);
}
fn meta_token_count(m:i64)->i64 {return if rkind_is(m,kind_meta())!=0 {vlen(rget(m,mk_tokens()))}else{0};}
fn meta_token_text_addr(m:i64,i:i64)->i64 {return vdata(rget(m,mk_bytes()))+rv_get(rget(m,mk_tokens()),i,0,32);}
fn meta_token_text_len(m:i64,i:i64)->i64 {return rv_get(rget(m,mk_tokens()),i,1,32);}
fn meta_token_score_bits(m:i64,i:i64)->i64 {return rv_get(rget(m,mk_tokens()),i,2,32);}
fn meta_token_type(m:i64,i:i64)->i64 {return rv_get(rget(m,mk_tokens()),i,3,32);}
fn meta_merge_count(m:i64)->i64 {return if rkind_is(m,kind_meta())!=0 {vlen(rget(m,mk_merges()))}else{0};}
fn meta_merge_addr(m:i64,i:i64)->i64 {return vdata(rget(m,mk_mbytes()))+rv_get(rget(m,mk_merges()),i,0,16);}
fn meta_merge_len(m:i64,i:i64)->i64 {return rv_get(rget(m,mk_merges()),i,1,16);}
fn meta_special(m:i64,key:i64)->i64 {let x=rget(m,mk_specials()*1000+key+1); return if x!=0 {x-1}else{-1};}
fn meta_suppress_count(m:i64)->i64 {return if rkind_is(m,kind_meta())!=0 {vlen(rget(m,mk_suppress()))}else{0};}
fn meta_suppress_get(m:i64,i:i64)->i64 {return get_i64(rget(m,mk_suppress()),i);}

// ---------- ctx ----------
fn ctx_new(old:i64)->i64 {
    let c=rnew(kind_ctx()); if c==0 {return 0;}
    let tokens=vnew(32); let bytes=vnew(1); let tmap=mnew(); let tent=vnew(32);
    let merges=vnew(40); let mbytes=vnew(1); let sp=vnew(8);
    let poff=vnew(8); let plen=vnew(8); let pbytes=vnew(1);
    let eog=vnew(8); let sup=vnew(8); let out=vnew(1);
    var ok:i64=1;
    if tokens==0||bytes==0||tmap==0||tent==0||merges==0||mbytes==0||sp==0||poff==0||plen==0||pbytes==0||eog==0||sup==0||out==0 {ok=0;}
    if ok==1 {
        if rset(c,ck_old(),old)!=0||rset(c,ck_tokens(),tokens)!=0||rset(c,ck_bytes(),bytes)!=0||rset(c,ck_tmap(),tmap)!=0||rset(c,ck_tent(),tent)!=0||rset(c,ck_merges(),merges)!=0||rset(c,ck_mbytes(),mbytes)!=0||rset(c,ck_specials(),sp)!=0||rset(c,ck_poff(),poff)!=0||rset(c,ck_plen(),plen)!=0||rset(c,ck_pbytes(),pbytes)!=0||rset(c,ck_eog(),eog)!=0||rset(c,ck_suppress(),sup)!=0||rset(c,ck_out(),out)!=0 {ok=0;}
    }
    if ok==0 {free_if(tokens);free_if(bytes);free_map_if(tmap);free_if(tent);free_if(merges);free_if(mbytes);free_if(sp);free_if(poff);free_if(plen);free_if(pbytes);free_if(eog);free_if(sup);free_if(out);rfree(c);return 0;}
    return c;
}
fn ctx_free(c:i64) {
    if rkind_is(c,kind_ctx())==0 {return;}
    let old=rget(c,ck_old());
    free_if(rget(c,ck_tokens()));free_if(rget(c,ck_bytes()));free_map_if(rget(c,ck_tmap()));
    free_if(rget(c,ck_tent()));free_if(rget(c,ck_merges()));free_if(rget(c,ck_mbytes()));
    free_if(rget(c,ck_specials()));free_if(rget(c,ck_poff()));free_if(rget(c,ck_plen()));
    free_if(rget(c,ck_pbytes()));free_if(rget(c,ck_eog()));free_if(rget(c,ck_suppress()));
    free_if(rget(c,ck_out()));
    rfree(c);
    if old!=0 {vocab_free(old);}
}
fn ctx_valid(c:i64)->i64 {return rkind_is(c,kind_ctx());}
fn ctx_get(c:i64,key:i64)->i64 {
    if rkind_is(c,kind_ctx())==0 {return -1;}
    if key==2 {return vlen(rget(c,ck_tokens()));}
    if key==6 {return rget(c,ck_old());}
    if rget(c,1000+key)==0 {return -1;}
    return rget(c,key);
}
fn ctx_set(c:i64,key:i64,v:i64)->i64 {
    if rkind_is(c,kind_ctx())==0 {return -1;}
    if rset(c,key,v)!=0 {return -1;}
    return rset(c,1000+key,1);
}
fn ctx_set_old(c:i64,v:i64)->i64 {return rset(c,ck_old(),v);}
fn ctx_token_count(c:i64)->i64 {return if rkind_is(c,kind_ctx())!=0 {vlen(rget(c,ck_tokens()))}else{-1};}
fn ctx_add_token(c:i64,id:i64,a:i64,l:i64,score:i64,attr:i64)->i64 {
    if rkind_is(c,kind_ctx())==0||id!=vlen(rget(c,ck_tokens())) {return -1;}
    let bytes=rget(c,ck_bytes()); let off=vlen(bytes);
    if append_mem(bytes,a,l)!=0 {return -1;}
    let tokens=rget(c,ck_tokens()); let i=push_rec(tokens);
    if i<0 {vresize(bytes,off);return -1;}
    if rv_set(tokens,i,0,off,32)!=0||rv_set(tokens,i,1,l,32)!=0||rv_set(tokens,i,2,score,32)!=0||rv_set(tokens,i,3,attr,32)!=0 {vresize(tokens,i);vresize(bytes,off);return -1;}
    let h=hash_bytes(a,l); let head=map_head(rget(c,ck_tmap()),h);
    let tent=rget(c,ck_tent()); let e=push_rec(tent);
    if e<0 {vresize(tokens,i);vresize(bytes,off);return -1;}
    if rv_set(tent,e,0,head,32)!=0||rv_set(tent,e,1,id,32)!=0||rv_set(tent,e,2,off,32)!=0||rv_set(tent,e,3,l,32)!=0 {vresize(tent,e);vresize(tokens,i);vresize(bytes,off);return -1;}
    if mput(rget(c,ck_tmap()),h,e)!=0 {vresize(tent,e);vresize(tokens,i);vresize(bytes,off);return -1;}
    return 0;
}
fn ctx_token_text_addr(c:i64,id:i64)->i64 {return vdata(rget(c,ck_bytes()))+rv_get(rget(c,ck_tokens()),id,0,32);}
fn ctx_token_text_len(c:i64,id:i64)->i64 {return rv_get(rget(c,ck_tokens()),id,1,32);}
fn ctx_token_score_bits(c:i64,id:i64)->i64 {return rv_get(rget(c,ck_tokens()),id,2,32);}
fn ctx_token_attr(c:i64,id:i64)->i64 {if id<0||id>=vlen(rget(c,ck_tokens())){return -1;} return rv_get(rget(c,ck_tokens()),id,3,32);}
fn ctx_token_attr_set(c:i64,id:i64,attr:i64)->i64 {return rv_set(rget(c,ck_tokens()),id,3,attr,32);}
fn ctx_text_to_token(c:i64,a:i64,l:i64)->i64 {
    let h=hash_bytes(a,l); var e=map_head(rget(c,ck_tmap()),h);
    let tent=rget(c,ck_tent()); let base=vdata(rget(c,ck_bytes()));
    while e>=0 {
        let off=rv_get(tent,e,2,32); let len=rv_get(tent,e,3,32);
        if len==l&&memeq(base+off,a,l)!=0 {return rv_get(tent,e,1,32);}
        e=rv_get(tent,e,0,32);
    }
    return -1;
}
fn ctx_add_merge(c:i64,la:i64,ll:i64,ra:i64,rl:i64,rank:i64)->i64 {
    let bytes=rget(c,ck_mbytes()); let loff=vlen(bytes);
    if append_mem(bytes,la,ll)!=0 {return -1;}
    let roff=vlen(bytes);
    if append_mem(bytes,ra,rl)!=0 {vresize(bytes,loff);return -1;}
    let merges=rget(c,ck_merges()); let i=push_rec(merges);
    if i<0 {vresize(bytes,loff);return -1;}
    if rv_set(merges,i,0,loff,40)!=0||rv_set(merges,i,1,ll,40)!=0||rv_set(merges,i,2,roff,40)!=0||rv_set(merges,i,3,rl,40)!=0||rv_set(merges,i,4,rank,40)!=0 {vresize(merges,i);vresize(bytes,loff);return -1;}
    return 0;
}
fn ctx_merge_count(c:i64)->i64 {return vlen(rget(c,ck_merges()));}
fn ctx_merge_left_addr(c:i64,i:i64)->i64 {return vdata(rget(c,ck_mbytes()))+rv_get(rget(c,ck_merges()),i,0,40);}
fn ctx_merge_left_len(c:i64,i:i64)->i64 {return rv_get(rget(c,ck_merges()),i,1,40);}
fn ctx_merge_right_addr(c:i64,i:i64)->i64 {return vdata(rget(c,ck_mbytes()))+rv_get(rget(c,ck_merges()),i,2,40);}
fn ctx_merge_right_len(c:i64,i:i64)->i64 {return rv_get(rget(c,ck_merges()),i,3,40);}
fn ctx_merge_rank(c:i64,i:i64)->i64 {return rv_get(rget(c,ck_merges()),i,4,40);}
fn ctx_special_reset(c:i64)->i64 {return vresize(rget(c,ck_specials()),0);}
fn ctx_special_count(c:i64)->i64 {return vlen(rget(c,ck_specials()));}
fn ctx_special_get(c:i64,i:i64)->i64 {return get_i64(rget(c,ck_specials()),i);}
fn ctx_special_push(c:i64,id:i64)->i64 {return push_i64(rget(c,ck_specials()),id);}
fn ctx_special_set(c:i64,i:i64,id:i64)->i64 {return set_i64(rget(c,ck_specials()),i,id);}
fn ctx_piece_count(c:i64)->i64 {return vlen(rget(c,ck_poff()));}
fn ctx_piece_set(c:i64,id:i64,a:i64,l:i64)->i64 {
    let bytes=rget(c,ck_pbytes()); let off=vlen(bytes);
    if append_mem(bytes,a,l)!=0 {return -1;}
    let poff=rget(c,ck_poff()); let plen=rget(c,ck_plen());
    while vlen(poff)<=id {if push_i64(poff,0)!=0||push_i64(plen,0)!=0 {vresize(bytes,off);return -1;}}
    if set_i64(poff,id,off)!=0||set_i64(plen,id,l)!=0 {vresize(bytes,off);return -1;}
    return 0;
}
fn ctx_piece_addr(c:i64,id:i64)->i64 {return vdata(rget(c,ck_pbytes()))+get_i64(rget(c,ck_poff()),id);}
fn ctx_piece_len(c:i64,id:i64)->i64 {return get_i64(rget(c,ck_plen()),id);}
fn ctx_eog_reset(c:i64)->i64 {return vresize(rget(c,ck_eog()),0);}
fn ctx_eog_add(c:i64,id:i64)->i64 {
    if ctx_eog_contains(c,id)!=0 {return 1;}
    return if push_i64(rget(c,ck_eog()),id)==0 {1}else{0};
}
fn ctx_eog_contains(c:i64,id:i64)->i64 {
    let v=rget(c,ck_eog()); let n=vlen(v); var i:i64=0;
    while i<n {if get_i64(v,i)==id {return 1;} i+=1;}
    return 0;
}
fn ctx_eog_count(c:i64)->i64 {return vlen(rget(c,ck_eog()));}
fn ctx_eog_get(c:i64,i:i64)->i64 {return get_i64(rget(c,ck_eog()),i);}
fn ctx_suppress_reset(c:i64)->i64 {return vresize(rget(c,ck_suppress()),0);}
fn ctx_suppress_push(c:i64,id:i64)->i64 {return push_i64(rget(c,ck_suppress()),id);}
fn ctx_suppress_count(c:i64)->i64 {return vlen(rget(c,ck_suppress()));}
fn ctx_suppress_get(c:i64,i:i64)->i64 {return get_i64(rget(c,ck_suppress()),i);}
fn ctx_out_reset(c:i64)->i64 {return vresize(rget(c,ck_out()),0);}
fn ctx_out_count(c:i64)->i64 {return vlen(rget(c,ck_out()));}
fn ctx_out_get(c:i64,i:i64)->i64 {return load8(vdata(rget(c,ck_out())),i);}
fn ctx_out_push(c:i64,b:i64)->i64 {return push_byte(rget(c,ck_out()),b);}
fn ctx_out_append(c:i64,a:i64,l:i64)->i64 {return append_mem(rget(c,ck_out()),a,l);}
fn ctx_out_set(c:i64,i:i64,b:i64)->i64 {if i<0||i>=vlen(rget(c,ck_out())){return -1;} store8(vdata(rget(c,ck_out())),i,b); return 0;}
fn ctx_out_truncate(c:i64,n:i64)->i64 {return if n>=0 {vresize(rget(c,ck_out()),n)}else{-1};}
fn ctx_out_copy(c:i64,dst:i64,cap:i64)->i64 {
    let n=ctx_out_count(c);
    if dst!=0&&cap>0 {memcopy(dst,vdata(rget(c,ck_out())),if n<cap {n}else{cap});}
    return if n<=cap {n}else{-n};
}

// op dispatch.  Argument order: op,a,b,c,d,e,f.
fn entry(op:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64)->i64 {
    if op==1 {return vocab_new();}
    if op==2 {vocab_free(a);return 0;}
    if op==3 {return vocab_add_token(a,b,c,d,e);}
    if op==4 {return vocab_add_merge(a,b,c,d,e,f);}
    if op==5 {return vocab_set_config1(a,b,c,d);}
    if op==6 {return vocab_set_config2(a,b,c,d,e,f);}
    if op==7 {return vocab_finalize(a);}
    if op==8 {return vocab_cfg(a,b);}
    if op==9 {return tok_text_addr(a,b);}
    if op==10 {return tok_text_len(a,b);}
    if op==11 {return tok_attr(a,b);}
    if op==12 {return vocab_text_to_token(a,b,c);}
    if op==13 {return vocab_bpe_rank(a,b,c,d,e);}
    if op==14 {return special_raw_count(a);}
    if op==15 {return special_raw_id(a,b);}
    if op==18 {return session_new(a);}
    if op==19 {session_free(a);return 0;}
    if op==20 {session_reset(a);return 0;}
    if op==21 {return if rkind_is(a,kind_session())!=0 {vlen(rget(a,sk_out()))}else{0};}
    if op==22 {return get_i64(rget(a,sk_out()),b);}
    if op==23 {return if rkind_is(a,kind_session())==0 {-1}else{push_i64(rget(a,sk_out()),b)};}
    if op==24 {return if rkind_is(a,kind_session())==0 {-1}else{vresize(rget(a,sk_out()),0)};}
    if op==25 {return if rkind_is(a,kind_session())==0 {-1}else{vresize(rget(a,sk_cpt()),0)};}
    if op==26 {return if rkind_is(a,kind_session())!=0 {vlen(rget(a,sk_cpt()))}else{0};}
    if op==27 {return get_i64(rget(a,sk_cpt()),b);}
    if op==28 {return if rkind_is(a,kind_session())==0 {-1}else{push_i64(rget(a,sk_cpt()),b)};}
    if op==29 {return if rkind_is(a,kind_session())==0 {-1}else{vresize(rget(a,sk_word()),0)};}
    if op==30 {return if rkind_is(a,kind_session())!=0 {vlen(rget(a,sk_word()))}else{0};}
    if op==31 {return if rkind_is(a,kind_session())!=0 {vdata(rget(a,sk_word()))}else{0};}
    if op==32 {return load8(vdata(rget(a,sk_word())),b);}
    if op==33 {return if rkind_is(a,kind_session())==0 {-1}else{push_byte(rget(a,sk_word()),b)};}
    if op==34 {return if rkind_is(a,kind_session())==0 {-1}else{vresize(rget(a,sk_sym()),0)};}
    if op==35 {return if rkind_is(a,kind_session())!=0 {vlen(rget(a,sk_sym()))}else{0};}
    if op==36 {return rv_get(rget(a,sk_sym()),b,c,32);}
    if op==37 {return rv_set(rget(a,sk_sym()),b,c,d,32);}
    if op==38 {let i=push_rec(rget(a,sk_sym())); if i<0 {return -1;} if rv_set(rget(a,sk_sym()),i,0,b,32)!=0||rv_set(rget(a,sk_sym()),i,1,c,32)!=0||rv_set(rget(a,sk_sym()),i,2,d,32)!=0||rv_set(rget(a,sk_sym()),i,3,e,32)!=0 {return -1;} return 0;}
    if op==39 {return if rkind_is(a,kind_session())==0 {-1}else{vresize(rget(a,sk_heap()),0)};}
    if op==40 {return if rkind_is(a,kind_session())!=0 {vlen(rget(a,sk_heap()))}else{0};}
    if op==41 {return rv_get(rget(a,sk_heap()),b,c,40);}
    if op==42 {return rv_set(rget(a,sk_heap()),b,c,d,40);}
    if op==43 {let i=push_rec(rget(a,sk_heap())); if i<0 {return -1;} if rv_set(rget(a,sk_heap()),i,0,b,40)!=0||rv_set(rget(a,sk_heap()),i,1,c,40)!=0||rv_set(rget(a,sk_heap()),i,2,d,40)!=0||rv_set(rget(a,sk_heap()),i,3,e,40)!=0||rv_set(rget(a,sk_heap()),i,4,f,40)!=0 {return -1;} return 0;}
    if op==44 {return if rkind_is(a,kind_session())==0 {-1}else{vresize(rget(a,sk_heap()),b)};}
    if op==45 {return vresize(frag_next_vec(a),0);}
    if op==46 {return vlen(frag_vec(a));}
    if op==47 {return rv_get(frag_vec(a),b,c,32);}
    if op==48 {let i=push_rec(frag_vec(a)); if i<0 {return -1;} if rv_set(frag_vec(a),i,0,b,32)!=0||rv_set(frag_vec(a),i,1,c,32)!=0||rv_set(frag_vec(a),i,2,d,32)!=0||rv_set(frag_vec(a),i,3,e,32)!=0 {return -1;} return 0;}
    if op==49 {let i=push_rec(frag_next_vec(a)); if i<0 {return -1;} if rv_set(frag_next_vec(a),i,0,b,32)!=0||rv_set(frag_next_vec(a),i,1,c,32)!=0||rv_set(frag_next_vec(a),i,2,d,32)!=0||rv_set(frag_next_vec(a),i,3,e,32)!=0 {return -1;} return 0;}
    if op==50 {return rset(a,sk_cur(),if rget(a,sk_cur())!=0 {0}else{1});}
    if op==51 {return vresize(rget(a,sk_special()),0);}
    if op==52 {return vlen(rget(a,sk_special()));}
    if op==53 {return get_i64(rget(a,sk_special()),b);}
    if op==54 {return push_i64(rget(a,sk_special()),b);}
    if op==55 {return set_i64(rget(a,sk_special()),b,c);}
    if op==56 {return vresize(rget(a,sk_text()),0);}
    if op==57 {return vlen(rget(a,sk_text()));}
    if op==58 {return load8(vdata(rget(a,sk_text())),b);}
    if op==59 {return vdata(rget(a,sk_text()));}
    if op==60 {return push_byte(rget(a,sk_text()),b);}
    if op==61 {if b<0||b>=vlen(rget(a,sk_text())){return -1;} store8(vdata(rget(a,sk_text())),b,c); return 0;}
    if op==62 {return if b>=0 {vresize(rget(a,sk_text()),b)}else{-1};}
    if op==63 {return append_mem(rget(a,sk_text()),b,c);}
    if op==64 {let n=vlen(rget(a,sk_text())); if b!=0&&c>0 {memcopy(b,vdata(rget(a,sk_text())),if n<c {n}else{c});} return if n<=c {n}else{-n};}
    if op==100 {return meta_new();}
    if op==101 {meta_free(a);return 0;}
    if op==102 {return meta_set_str(a,b,c,d);}
    if op==103 {return meta_set_i(a,b,c);}
    if op==104 {return meta_add_token(a,b,c,d,e);}
    if op==105 {return meta_add_merge(a,b,c);}
    if op==106 {return meta_set_special(a,b,c);}
    if op==107 {return meta_add_suppress(a,b);}
    if op==108 {return meta_str_addr(a,b);}
    if op==109 {return meta_str_len(a,b);}
    if op==110 {return meta_i(a,b);}
    if op==111 {return meta_token_count(a);}
    if op==112 {return meta_token_text_addr(a,b);}
    if op==113 {return meta_token_text_len(a,b);}
    if op==114 {return meta_token_score_bits(a,b);}
    if op==115 {return meta_token_type(a,b);}
    if op==116 {return meta_merge_count(a);}
    if op==117 {return meta_merge_addr(a,b);}
    if op==118 {return meta_merge_len(a,b);}
    if op==119 {return meta_special(a,b);}
    if op==120 {return meta_suppress_count(a);}
    if op==121 {return meta_suppress_get(a,b);}
    if op==130 {return ctx_new(a);}
    if op==131 {ctx_free(a);return 0;}
    if op==132 {return ctx_valid(a);}
    if op==133 {return ctx_get(a,b);}
    if op==134 {return ctx_set(a,b,c);}
    if op==135 {return ctx_set_old(a,b);}
    if op==136 {return ctx_token_count(a);}
    if op==137 {return ctx_add_token(a,b,c,d,e,f);}
    if op==138 {return ctx_token_text_addr(a,b);}
    if op==139 {return ctx_token_text_len(a,b);}
    if op==140 {return ctx_token_score_bits(a,b);}
    if op==141 {return ctx_token_attr(a,b);}
    if op==142 {return ctx_token_attr_set(a,b,c);}
    if op==143 {return ctx_text_to_token(a,b,c);}
    if op==144 {return ctx_add_merge(a,b,c,d,e,f);}
    if op==145 {return ctx_merge_count(a);}
    if op==146 {return ctx_merge_left_addr(a,b);}
    if op==147 {return ctx_merge_left_len(a,b);}
    if op==148 {return ctx_merge_right_addr(a,b);}
    if op==149 {return ctx_merge_right_len(a,b);}
    if op==150 {return ctx_merge_rank(a,b);}
    if op==151 {return ctx_special_reset(a);}
    if op==152 {return ctx_special_count(a);}
    if op==153 {return ctx_special_get(a,b);}
    if op==154 {return ctx_special_push(a,b);}
    if op==155 {return ctx_special_set(a,b,c);}
    if op==156 {return ctx_piece_count(a);}
    if op==157 {return ctx_piece_set(a,b,c,d);}
    if op==158 {return ctx_piece_addr(a,b);}
    if op==159 {return ctx_piece_len(a,b);}
    if op==160 {return ctx_eog_reset(a);}
    if op==161 {return ctx_eog_add(a,b);}
    if op==162 {return ctx_eog_contains(a,b);}
    if op==163 {return ctx_eog_count(a);}
    if op==164 {return ctx_eog_get(a,b);}
    if op==165 {return ctx_suppress_reset(a);}
    if op==166 {return ctx_suppress_push(a,b);}
    if op==167 {return ctx_suppress_count(a);}
    if op==168 {return ctx_suppress_get(a,b);}
    if op==169 {return ctx_out_reset(a);}
    if op==170 {return ctx_out_count(a);}
    if op==171 {return ctx_out_get(a,b);}
    if op==172 {return ctx_out_push(a,b);}
    if op==173 {return ctx_out_append(a,b,c);}
    if op==174 {return ctx_out_set(a,b,c);}
    if op==175 {return ctx_out_truncate(a,b);}
    if op==176 {return ctx_out_copy(a,b,c);}
    return -1;
}

fn main() {print(entry(1,0,0,0,0,0,0));}
