// APX030 VOCAB canonical public facade.
//
// Reuses the qualified generated BPE/tokenizer and vocabulary-load/cache
// algorithms.  This unit owns public facade control (load/free/tokenize/
// detokenize/piece and result copying); all owned storage is behind canonical
// storage contract primitives in the services projection.

extern "apx027_closure_entry" fn closure_entry(op:i64,c:i64,a:i64,b:i64,d:i64,e:i64,f:i64)->i64;
extern "apx027_session_new" fn session_new(v:i64)->i64;
extern "apx027_session_free" fn session_free(s:i64);
extern "apx027_entry" fn tokenizer_entry(op:i64,v:i64,s:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "apx027_out_count" fn tok_out_count(s:i64)->i64;
extern "apx027_out_get" fn tok_out_get(s:i64,i:i64)->i64;
extern "vcx_ctx_get_i" fn ctx_get(c:i64,k:i64)->i64;
extern "vcx_out_copy_to" fn out_copy(c:i64,dst:i64,cap:i64)->i64;

extern "apx030_mem_store32" fn store32(addr:i64,i:i64,v:i64);

fn ck_old_vocab()->i64 {return 6;}
fn ck_api_out()->i64 {return 74;}
fn ck_api_cap()->i64 {return 75;}

fn do_load(meta:i64)->i64 {
    return closure_entry(1,meta,0,0,0,0,0);
}

fn do_free(c:i64)->i64 {
    return closure_entry(5,c,0,0,0,0,0);
}

fn do_tokenize(c:i64,text:i64,len:i64,add_special:i64,parse_special:i64)->i64 {
    let v=ctx_get(c,ck_old_vocab());
    if v<=0 {return -1;}
    let s=session_new(v);
    if s==0 {return -1;}
    let n=tokenizer_entry(1,v,s,text,len,add_special,parse_special);
    if n<0 {session_free(s);return n;}
    let dst=ctx_get(c,ck_api_out());
    let cap=ctx_get(c,ck_api_cap());
    if dst==0||cap<n {
        session_free(s);
        return -n;
    }
    var i:i64=0;
    while i<n {store32(dst,i,tok_out_get(s,i)); i+=1;}
    session_free(s);
    return n;
}

fn do_detokenize(c:i64,tokens:i64,n:i64,remove_special:i64,unparse_special:i64)->i64 {
    let r=closure_entry(3,c,tokens,n,remove_special,unparse_special,0);
    if r<0 {return r;}
    return out_copy(c,ctx_get(c,ck_api_out()),ctx_get(c,ck_api_cap()));
}

fn do_piece(c:i64,token:i64,lstrip:i64,special:i64)->i64 {
    let r=closure_entry(2,c,token,lstrip,special,0,0);
    if r<0 {return r;}
    return out_copy(c,ctx_get(c,ck_api_out()),ctx_get(c,ck_api_cap()));
}

fn entry(op:i64,c:i64,a:i64,b:i64,d:i64,e:i64,f:i64)->i64 {
    if op==6 {return do_load(a);}
    if op==5 {return do_free(c);}
    if op==2 {return do_tokenize(c,a,b,d,e);}
    if op==3 {return do_detokenize(c,a,b,d,e);}
    if op==4 {return do_piece(c,a,b,d);}
    if op==1 {return 1;}
    if op==7 {return ctx_get(c,a);}
    return -1;
}

fn main() {print(entry(1,0,0,0,0,0,0));}
