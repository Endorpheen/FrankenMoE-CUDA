extern "fg153_sc_cfg_new" fn sc_cfg_new(cap:i64)->i64;
extern "fg153_sc_cfg_get" fn sc_cfg_get(c:i64,code:i64)->i64;
extern "fg153_sc_cfg_set" fn sc_cfg_set(c:i64,code:i64,v:i64);
extern "fg153_sc_cfg_add" fn sc_cfg_add(c:i64,t:i64,f:i64,i1:i64,i2:i64)->i64;
extern "fg153_sc_cfg_type" fn sc_cfg_type(c:i64,i:i64)->i64;
extern "fg153_sc_cfg_f1" fn sc_cfg_f1(c:i64,i:i64)->i64;
extern "fg153_sc_cfg_i1" fn sc_cfg_i1(c:i64,i:i64)->i64;
extern "fg153_sc_cfg_i2" fn sc_cfg_i2(c:i64,i:i64)->i64;
extern "fg153_sc_chain_clone" fn sc_chain_clone(c:i64)->i64;
extern "fg153_sc_hist_clone" fn sc_hist_clone(h:i64)->i64;
extern "fg153_sc_rng_clone" fn sc_rng_clone(seed:i64,draws:i64)->i64;
extern "fg153_sc_json_obj" fn sc_json_obj()->i64;
extern "fg153_sc_json_arr" fn sc_json_arr()->i64;
extern "fg153_sc_json_str" fn sc_json_str(s:i64)->i64;
extern "fg153_sc_json_str_lit" fn sc_json_str_lit(code:i64)->i64;
extern "fg153_sc_json_bool" fn sc_json_bool(v:i64)->i64;
extern "fg153_sc_json_parse" fn sc_json_parse(s:i64)->i64;
extern "fg153_sc_json_set" fn sc_json_set(o:i64,k:i64,v:i64);
extern "fg153_sc_json_set_key" fn sc_json_set_key(o:i64,code:i64,v:i64);
extern "fg153_sc_json_push" fn sc_json_push(a:i64,v:i64);
extern "fg153_sc_tmpl_render" fn sc_tmpl_render(t:i64,root:i64,buf:i64)->i64;
extern "fg153_sc_msg_count" fn sc_msg_count(m:i64)->i64;
extern "fg153_sc_msg_get" fn sc_msg_get(m:i64,i:i64,code:i64)->i64;
extern "fg153_sc_tc_get" fn sc_tc_get(m:i64,mi:i64,ti:i64,code:i64)->i64;
extern "fg153_sc_role_code" fn sc_role_code(s:i64)->i64;
extern "fg153_sc_format_name" fn sc_format_name(f:i64)->i64;
extern "fg153_sc_tool_count" fn sc_tool_count(t:i64)->i64;
extern "fg153_sc_tool_get" fn sc_tool_get(t:i64,i:i64,code:i64)->i64;
extern "fg153_sc_buf_new" fn sc_buf_new()->i64;
extern "fg153_sc_buf_free" fn sc_buf_free(b:i64);
extern "fg153_sc_buf_len" fn sc_buf_len(b:i64)->i64;
extern "fg153_sc_buf_data" fn sc_buf_data(b:i64)->i64;
extern "fg153_sc_fail" fn sc_fail(code:i64);

extern "fg153_sp_bits_float" fn sp_bits_float(b:i64)->f32;
extern "fg153_sp_chain_new" fn sp_chain_new(cap:i64)->i64;
extern "fg153_sp_chain_add" fn sp_chain_add(c:i64,t:i64,f:i64,i1:i64,i2:i64)->i64;
extern "fg153_sp_live_entry" fn sp_live_entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64)->i64;
extern "fg153_sp_chain_free" fn sp_chain_free(c:i64);
extern "fg153_sp_hist_new" fn sp_hist_new(vocab:i64,n:i64)->i64;

extern "fg153_sp_hist_free" fn sp_hist_free(h:i64);
extern "fg153_sp_rng_new" fn sp_rng_new(seed:i64)->i64;
extern "fg153_sp_rng_reset" fn sp_rng_reset(r:i64);
extern "fg153_sp_rng_free" fn sp_rng_free(r:i64);
extern "fg153_sp_str_len" fn sp_str_len(s:i64)->i64;

fn sc_ensure(ok:bool) {if !ok {sc_fail(1);}}

fn chain_build(cfg:i64)->i64 {
    let n=sc_cfg_get(cfg,0);let seed=sc_cfg_get(cfg,1);
    let vocab=sc_cfg_get(cfg,2);let last_n=sc_cfg_get(cfg,3);
    sc_ensure(vocab>0);sc_ensure(last_n>=0);
    let chain=sp_chain_new(n+1);sc_ensure(chain!=0);
    var terminal:i64=0;var i:i64=0;
    while i<n {
        let t=sc_cfg_type(cfg,i);let f=sc_cfg_f1(cfg,i);
        let a=sc_cfg_i1(cfg,i);let b=sc_cfg_i2(cfg,i);
        if t==4 {
            let rep=sp_bits_float(f);sc_ensure(rep>0.0);
        }
        let ignored=sp_chain_add(chain,t,f,a,b);
        if t==5||t==6 {terminal=1;}
        i+=1;
    }
    if terminal==0 {let ignored=sp_chain_add(chain,5,0,0,0);}
    let hist=sp_hist_new(vocab,last_n);sc_ensure(hist!=0);
    let rng=sp_rng_new(seed);sc_ensure(rng!=0);
    sc_cfg_set(cfg,5,hist);sc_cfg_set(cfg,6,rng);sc_cfg_set(cfg,7,chain);sc_cfg_set(cfg,4,0);
    return chain;
}

fn chain_accept(cfg:i64,token:i64,generated:i64) {
    if generated!=0 {let ignored=sp_live_entry(6,sc_cfg_get(cfg,5),token,0,0,0,0,0);}
}
fn chain_reset(cfg:i64) {
    let ignored=sp_live_entry(7,sc_cfg_get(cfg,5),0,0,0,0,0,0);
    sp_rng_reset(sc_cfg_get(cfg,6));
    sc_cfg_set(cfg,4,0);
}
fn chain_clone(cfg:i64)->i64 {
    let n=sc_cfg_get(cfg,0);
    let out=sc_cfg_new(n+1);sc_ensure(out!=0);
    sc_cfg_set(out,1,sc_cfg_get(cfg,1));
    sc_cfg_set(out,2,sc_cfg_get(cfg,2));
    sc_cfg_set(out,3,sc_cfg_get(cfg,3));
    sc_cfg_set(out,8,sc_cfg_get(cfg,8));
    var i:i64=0;
    while i<n {
        let ignored=sc_cfg_add(out,sc_cfg_type(cfg,i),sc_cfg_f1(cfg,i),sc_cfg_i1(cfg,i),sc_cfg_i2(cfg,i));
        i+=1;
    }
    let chain=sc_chain_clone(sc_cfg_get(cfg,7));
    let hist=sc_hist_clone(sc_cfg_get(cfg,5));
    let rng=sc_rng_clone(sc_cfg_get(cfg,1),sc_cfg_get(cfg,4));
    sc_cfg_set(out,5,hist);sc_cfg_set(out,6,rng);sc_cfg_set(out,7,chain);
    sc_cfg_set(out,4,sc_cfg_get(cfg,4));
    return out;
}
fn chain_free(cfg:i64) {
    if cfg==0 {return;}
    let ch=sc_cfg_get(cfg,7);if ch!=0 {sp_chain_free(ch);}
    let h=sc_cfg_get(cfg,5);if h!=0 {sp_hist_free(h);}
    let r=sc_cfg_get(cfg,6);if r!=0 {sp_rng_free(r);}
}
fn chain_apply(cfg:i64,a:i64) {
    let ignored=sp_live_entry(8,sc_cfg_get(cfg,7),a,sc_cfg_get(cfg,6),sc_cfg_get(cfg,5),0,0,0);
    let n=sc_cfg_get(cfg,0);var draws:i64=sc_cfg_get(cfg,4);var i:i64=0;
    while i<n {if sc_cfg_type(cfg,i)==5 {draws+=1;}i+=1;}
    sc_cfg_set(cfg,4,draws);
}

fn msg_json(m:i64,i:i64)->i64 {
    let mj=sc_json_obj();
    sc_json_set_key(mj,0,sc_json_str(sc_msg_get(m,i,0)));
    sc_json_set_key(mj,1,sc_json_str(sc_msg_get(m,i,1)));
    if sp_str_len(sc_msg_get(m,i,2))>0 {sc_json_set_key(mj,2,sc_json_str(sc_msg_get(m,i,2)));}
    if sp_str_len(sc_msg_get(m,i,3))>0 {sc_json_set_key(mj,3,sc_json_str(sc_msg_get(m,i,3)));}
    if sp_str_len(sc_msg_get(m,i,4))>0 {sc_json_set_key(mj,4,sc_json_str(sc_msg_get(m,i,4)));}
    let ntc=sc_msg_get(m,i,5);
    if ntc>0 {
        let tcs=sc_json_arr();
        var j:i64=0;
        while j<ntc {
            let tc=sc_json_obj();
            sc_json_set_key(tc,6,sc_json_str_lit(0));
            let fno=sc_json_obj();
            sc_json_set_key(fno,3,sc_json_str(sc_tc_get(m,i,j,0)));
            sc_json_set_key(fno,8,sc_json_str(sc_tc_get(m,i,j,1)));
            sc_json_set_key(tc,7,fno);
            if sp_str_len(sc_tc_get(m,i,j,2))>0 {sc_json_set_key(tc,9,sc_json_str(sc_tc_get(m,i,j,2)));}
            sc_json_push(tcs,tc);
            j+=1;
        }
        sc_json_set_key(mj,5,tcs);
    }
    return mj;
}

fn chat_build(tmpl:i64,msgs:i64,tools:i64,add_ass:i64,buf:i64)->i64 {
    let root=sc_json_obj();
    let marr=sc_json_arr();let n=sc_msg_count(msgs);var i:i64=0;
    while i<n {sc_json_push(marr,msg_json(msgs,i));i+=1;}
    sc_json_set_key(root,10,marr);
    sc_json_set_key(root,11,sc_json_str_lit(3));
    sc_json_set_key(root,12,sc_json_str_lit(3));
    sc_json_set_key(root,17,sc_json_bool(1));
    if add_ass!=0 {sc_json_set_key(root,13,sc_json_bool(1));}
    let nt=sc_tool_count(tools);
    if nt>0 {
        let tarr=sc_json_arr();var j:i64=0;
        while j<nt {
            let t=sc_json_obj();
            sc_json_set_key(t,6,sc_json_str_lit(0));
            let fno=sc_json_obj();
            sc_json_set_key(fno,3,sc_json_str(sc_tool_get(tools,j,0)));
            sc_json_set_key(fno,15,sc_json_str(sc_tool_get(tools,j,1)));
            sc_json_set_key(fno,16,sc_json_parse(sc_tool_get(tools,j,2)));
            sc_json_set_key(t,7,fno);
            sc_json_push(tarr,t);
            j+=1;
        }
        sc_json_set_key(root,14,tarr);
    }
    return sc_tmpl_render(tmpl,root,buf);
}

fn sc_selftest()->i64 {return 1;}

fn sc_entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64)->i64 {
    if mode==0 {return chain_build(a);}
    if mode==1 {chain_accept(a,b,c);return 0;}
    if mode==2 {chain_reset(a);return 0;}
    if mode==3 {return chain_clone(a);}
    if mode==4 {chain_free(a);return 0;}
    if mode==5 {chain_apply(a,b);return 0;}
    if mode==10 {return chat_build(a,b,c,d,e);}
    if mode==20 {return sc_role_code(a);}
    if mode==21 {return sc_format_name(a);}
    if mode==11 {return sc_selftest();}
    sc_fail(3);return 0;
}
fn main() {print(sc_entry(11,0,0,0,0,0,0,0));}
