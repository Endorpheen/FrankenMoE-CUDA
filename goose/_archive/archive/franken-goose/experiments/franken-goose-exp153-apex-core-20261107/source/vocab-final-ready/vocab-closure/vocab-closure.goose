// APX027 vocabulary closure: llama_vocab load/init interpretation, metadata
// maps, special/EOG caches, token->piece cache and ownership lifetime.
//
// Source anchors: inputs/native/src/llama-vocab.cpp load() token metadata,
// special-id resolution, auto-detected special/EOG sets, special-token cache,
// token->piece cache and impl lifetime.  GGUF wire reading belongs to
// loader15; this unit consumes typed metadata arrays only.

extern "vcx_meta_str_addr" fn meta_str_addr(m:i64,k:i64)->i64;
extern "vcx_meta_str_len" fn meta_str_len(m:i64,k:i64)->i64;
extern "vcx_meta_i" fn meta_i(m:i64,k:i64)->i64;
extern "vcx_meta_token_count" fn meta_token_count(m:i64)->i64;
extern "vcx_meta_token_text_addr" fn meta_token_text_addr(m:i64,i:i64)->i64;
extern "vcx_meta_token_text_len" fn meta_token_text_len(m:i64,i:i64)->i64;
extern "vcx_meta_token_score_bits" fn meta_token_score_bits(m:i64,i:i64)->i64;
extern "vcx_meta_token_type" fn meta_token_type(m:i64,i:i64)->i64;
extern "vcx_meta_merge_count" fn meta_merge_count(m:i64)->i64;
extern "vcx_meta_merge_addr" fn meta_merge_addr(m:i64,i:i64)->i64;
extern "vcx_meta_merge_len" fn meta_merge_len(m:i64,i:i64)->i64;
extern "vcx_meta_special" fn meta_special(m:i64,k:i64)->i64;
extern "vcx_meta_suppress_count" fn meta_suppress_count(m:i64)->i64;
extern "vcx_meta_suppress_get" fn meta_suppress_get(m:i64,i:i64)->i64;
extern "vcx_lit_addr" fn lit_addr(id:i64)->i64;
extern "vcx_lit_len" fn lit_len(id:i64)->i64;
extern "vcx_mem_byte" fn mem_byte(addr:i64,i:i64)->i64;

extern "vcx_ctx_new" fn ctx_new(v:i64)->i64;
extern "vcx_ctx_free" fn ctx_free(c:i64);
extern "vcx_ctx_valid" fn ctx_valid(c:i64)->i64;
extern "vcx_ctx_get_i" fn ctx_get(c:i64,k:i64)->i64;
extern "vcx_ctx_set_i" fn ctx_set(c:i64,k:i64,v:i64);
extern "vcx_ctx_set_old_vocab" fn ctx_set_old_vocab(c:i64,v:i64);
extern "vcx_ctx_token_count" fn token_count(c:i64)->i64;
extern "vcx_ctx_add_token" fn add_token(c:i64,id:i64,ta:i64,tl:i64,sb:i64,attr:i64)->i64;
extern "vcx_ctx_token_text_addr" fn token_text_addr(c:i64,id:i64)->i64;
extern "vcx_ctx_token_text_len" fn token_text_len(c:i64,id:i64)->i64;
extern "vcx_ctx_token_attr" fn token_attr(c:i64,id:i64)->i64;
extern "vcx_ctx_token_attr_set" fn token_attr_set(c:i64,id:i64,attr:i64);
extern "vcx_ctx_text_to_token" fn text_to_token(c:i64,a:i64,l:i64)->i64;
extern "vcx_ctx_add_merge" fn add_merge(c:i64,la:i64,ll:i64,ra:i64,rl:i64,rank:i64)->i64;
extern "vcx_ctx_merge_count" fn merge_count(c:i64)->i64;
extern "vcx_ctx_merge_left_addr" fn merge_left_addr(c:i64,i:i64)->i64;
extern "vcx_ctx_merge_left_len" fn merge_left_len(c:i64,i:i64)->i64;
extern "vcx_ctx_merge_right_addr" fn merge_right_addr(c:i64,i:i64)->i64;
extern "vcx_ctx_merge_right_len" fn merge_right_len(c:i64,i:i64)->i64;
extern "vcx_ctx_merge_rank" fn merge_rank(c:i64,i:i64)->i64;
extern "vcx_ctx_special_reset" fn special_reset(c:i64);
extern "vcx_ctx_special_count" fn special_count(c:i64)->i64;
extern "vcx_ctx_special_get" fn special_get(c:i64,i:i64)->i64;
extern "vcx_ctx_special_push" fn special_push(c:i64,id:i64);
extern "vcx_ctx_special_set" fn special_set(c:i64,i:i64,id:i64);
extern "vcx_ctx_piece_set" fn piece_set(c:i64,id:i64,a:i64,l:i64)->i64;
extern "vcx_ctx_piece_addr" fn piece_addr(c:i64,id:i64)->i64;
extern "vcx_ctx_piece_len" fn piece_len(c:i64,id:i64)->i64;
extern "vcx_ctx_eog_reset" fn eog_reset(c:i64);
extern "vcx_ctx_eog_add" fn eog_add(c:i64,id:i64)->i64;
extern "vcx_ctx_eog_contains" fn eog_contains(c:i64,id:i64)->i64;
extern "vcx_ctx_suppress_reset" fn suppress_reset(c:i64);
extern "vcx_ctx_suppress_push" fn suppress_push(c:i64,id:i64);
extern "vcx_out_reset" fn out_reset(c:i64);
extern "vcx_out_count" fn out_count(c:i64)->i64;
extern "vcx_out_get" fn out_get(c:i64,i:i64)->i64;
extern "vcx_out_push" fn out_push(c:i64,b:i64);
extern "vcx_out_append_mem" fn out_append(c:i64,a:i64,l:i64);
extern "vcx_out_set" fn out_set(c:i64,i:i64,b:i64);
extern "vcx_out_truncate" fn out_truncate(c:i64,n:i64);

extern "apx027_vocab_new" fn ov_new()->i64;
extern "apx027_vocab_free" fn ov_free(v:i64);
extern "apx027_vocab_add_token" fn ov_add_token(v:i64,id:i64,ta:i64,tl:i64,attr:i64)->i64;
extern "apx027_vocab_add_merge" fn ov_add_merge(v:i64,la:i64,ll:i64,ra:i64,rl:i64,rank:i64)->i64;
extern "apx027_vocab_set_config" fn ov_config(v:i64,pre:i64,ab:i64,bos:i64,ae:i64,eos:i64,clean:i64,ign:i64,esc:i64);
extern "apx027_vocab_finalize" fn ov_finalize(v:i64)->i64;
extern "apx027_session_new" fn os_new(v:i64)->i64;
extern "apx027_session_free" fn os_free(s:i64);
extern "apx027_entry" fn tokenizer_entry(op:i64,v:i64,s:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "apx027_text_count" fn tok_text_count(s:i64)->i64;
extern "apx027_text_addr" fn tok_text_addr(s:i64)->i64;
extern "apx027_out_count" fn tok_out_count(s:i64)->i64;
extern "apx027_out_get" fn tok_out_get(s:i64,i:i64)->i64;
extern "apx027_mem_i32" fn tok_i32(a:i64,i:i64)->i64;
extern "apx027_abort" fn fail(code:i64);

// llama enum values used by the Apex Qwen35 path.
fn type_bpe()->i64 {return 2;}
fn pre_qwen35()->i64 {return 46;}
fn attr_unknown()->i64 {return 1;}
fn attr_unused()->i64 {return 2;}
fn attr_normal()->i64 {return 4;}
fn attr_control()->i64 {return 8;}
fn attr_user()->i64 {return 16;}
fn attr_byte()->i64 {return 32;}
fn attr_lstrip()->i64 {return 128;}
fn attr_rstrip()->i64 {return 256;}

// Metadata keys.
fn mk_model()->i64 {return 0;}
fn mk_pre()->i64 {return 1;}
fn mk_token_types()->i64 {return 2;}
fn mk_add_prefix()->i64 {return 3;}
fn mk_remove_ws()->i64 {return 4;}
fn mk_nfc()->i64 {return 5;}
fn mk_lower()->i64 {return 6;}
fn mk_strip()->i64 {return 7;}
fn mk_add_bos()->i64 {return 8;}
fn mk_add_eos()->i64 {return 9;}
fn mk_add_sep()->i64 {return 10;}

// Context keys.
fn ck_type()->i64 {return 0;}
fn ck_pre()->i64 {return 1;}
fn ck_n_tokens()->i64 {return 2;}
fn ck_n_token_types()->i64 {return 3;}
fn ck_max_len()->i64 {return 4;}
fn ck_linefeed()->i64 {return 5;}
fn ck_old_vocab()->i64 {return 6;}
fn ck_add_prefix()->i64 {return 10;}
fn ck_add_bos()->i64 {return 11;}
fn ck_add_eos()->i64 {return 12;}
fn ck_add_sep()->i64 {return 13;}
fn ck_ignore_merges()->i64 {return 14;}
fn ck_clean_spaces()->i64 {return 15;}
fn ck_remove_ws()->i64 {return 16;}
fn ck_escape_ws()->i64 {return 17;}
fn ck_nfc()->i64 {return 20;}
fn ck_lower()->i64 {return 21;}
fn ck_strip()->i64 {return 22;}
fn ck_bos()->i64 {return 30;}
fn ck_eos()->i64 {return 31;}
fn ck_eot()->i64 {return 32;}
fn ck_eom()->i64 {return 33;}
fn ck_unk()->i64 {return 34;}
fn ck_sep()->i64 {return 35;}
fn ck_pad()->i64 {return 36;}
fn ck_mask()->i64 {return 37;}
fn ck_fim_pre()->i64 {return 38;}
fn ck_fim_suf()->i64 {return 39;}
fn ck_fim_mid()->i64 {return 40;}
fn ck_fim_pad()->i64 {return 41;}
fn ck_fim_rep()->i64 {return 42;}
fn ck_fim_sep()->i64 {return 43;}

// Literal ids.
fn lit_gpt2()->i64 {return 0;}
fn lit_qwen35()->i64 {return 1;}
fn lit_newline()->i64 {return 2;}
fn lit_unused()->i64 {return 3;}
fn lit_eot_id()->i64 {return 10;}
fn lit_im_end()->i64 {return 11;}
fn lit_end()->i64 {return 12;}
fn lit_end_turn()->i64 {return 13;}
fn lit_endoftext()->i64 {return 14;}
fn lit_end_text()->i64 {return 15;}
fn lit_cap_eot()->i64 {return 16;}
fn lit_us_eot()->i64 {return 17;}
fn lit_br_eot()->i64 {return 18;}
fn lit_eom_id()->i64 {return 19;}
fn lit_end_utt()->i64 {return 20;}
fn lit_eos()->i64 {return 21;}
fn lit_turn()->i64 {return 22;}
fn lit_tool_resp()->i64 {return 23;}
fn lit_fim_prefix()->i64 {return 30;}
fn lit_fim_suffix()->i64 {return 31;}
fn lit_fim_middle()->i64 {return 32;}
fn lit_fim_pad()->i64 {return 33;}
fn lit_fim_repo()->i64 {return 34;}
fn lit_repo_name()->i64 {return 35;}
fn lit_file_sep()->i64 {return 36;}
fn lit_fim_prefix_angle()->i64 {return 37;}
fn lit_fim_suffix_angle()->i64 {return 38;}
fn lit_fim_middle_angle()->i64 {return 39;}
fn lit_fim_pad_angle()->i64 {return 40;}
fn lit_pre()->i64 {return 41;}
fn lit_suf()->i64 {return 42;}
fn lit_mid()->i64 {return 43;}
fn lit_rep()->i64 {return 44;}
fn lit_channel()->i64 {return 50;}
fn lit_message()->i64 {return 51;}
fn lit_start()->i64 {return 52;}
fn lit_constrain()->i64 {return 53;}

fn min_i(a:i64,b:i64)->i64 {return if a<b {a}else{b};}

fn eq_lit(addr:i64,len:i64,id:i64)->bool {
    let la=lit_addr(id); let ll=lit_len(id);
    if len!=ll {return false;}
    var i:i64=0;
    while i<ll {if mem_byte(addr,i)!=mem_byte(la,i) {return false;} i+=1;}
    return true;
}

fn contains_lit(addr:i64,len:i64,id:i64)->bool {
    let la=lit_addr(id); let ll=lit_len(id);
    if ll<=0||len<ll {return false;}
    var i:i64=0;
    while i+ll<=len {
        var j:i64=0; var ok:i64=1;
        while j<ll {if mem_byte(addr,i+j)!=mem_byte(la,j) {ok=0;break;} j+=1;}
        if ok!=0 {return true;}
        i+=1;
    }
    return false;
}

fn find_space(addr:i64,len:i64,start:i64)->i64 {
    var i=start;
    while i<len {if mem_byte(addr,i)==32 {return i;} i+=1;}
    return -1;
}

fn attr_for(toktype:i64)->i64 {
    if toktype==1 {return attr_normal();}
    if toktype==2 {return attr_unknown();}
    if toktype==3 {return attr_control();}
    if toktype==4 {return attr_user();}
    if toktype==5 {return attr_unused();}
    if toktype==6 {return attr_byte();}
    if toktype==0 {return 0;}
    if toktype<0 {return attr_normal();}
    return 0;
}

fn default_specials(c:i64) {
    ctx_set(c,ck_bos(),11);
    ctx_set(c,ck_eos(),11);
    ctx_set(c,ck_eot(),-1);
    ctx_set(c,ck_eom(),-1);
    ctx_set(c,ck_unk(),-1);
    ctx_set(c,ck_sep(),-1);
    ctx_set(c,ck_pad(),-1);
    ctx_set(c,ck_mask(),-1);
    ctx_set(c,ck_fim_pre(),-1);
    ctx_set(c,ck_fim_suf(),-1);
    ctx_set(c,ck_fim_mid(),-1);
    ctx_set(c,ck_fim_pad(),-1);
    ctx_set(c,ck_fim_rep(),-1);
    ctx_set(c,ck_fim_sep(),-1);
}

fn override_special(m:i64,c:i64,key:i64,n:i64) {
    let id=meta_special(m,key);
    if id>=0&&id<n {ctx_set(c,key,id);}
}

fn match_eot(a:i64,l:i64)->bool {
    if eq_lit(a,l,lit_eot_id()) {return true;}
    if eq_lit(a,l,lit_im_end()) {return true;}
    if eq_lit(a,l,lit_end()) {return true;}
    if eq_lit(a,l,lit_end_turn()) {return true;}
    if eq_lit(a,l,lit_endoftext()) {return true;}
    if eq_lit(a,l,lit_end_text()) {return true;}
    if eq_lit(a,l,lit_cap_eot()) {return true;}
    if eq_lit(a,l,lit_us_eot()) {return true;}
    if eq_lit(a,l,lit_br_eot()) {return true;}
    if eq_lit(a,l,lit_end_utt()) {return true;}
    if eq_lit(a,l,lit_eos()) {return true;}
    if eq_lit(a,l,lit_turn()) {return true;}
    if eq_lit(a,l,lit_tool_resp()) {return true;}
    return false;
}
fn match_eom(a:i64,l:i64)->bool {return eq_lit(a,l,lit_eom_id());}
fn match_fim_pre(a:i64,l:i64)->bool {
    return eq_lit(a,l,lit_fim_prefix())||eq_lit(a,l,lit_fim_prefix_angle())||eq_lit(a,l,lit_pre());
}
fn match_fim_suf(a:i64,l:i64)->bool {
    return eq_lit(a,l,lit_fim_suffix())||eq_lit(a,l,lit_fim_suffix_angle())||eq_lit(a,l,lit_suf());
}
fn match_fim_mid(a:i64,l:i64)->bool {
    return eq_lit(a,l,lit_fim_middle())||eq_lit(a,l,lit_fim_middle_angle())||eq_lit(a,l,lit_mid());
}
fn match_fim_pad(a:i64,l:i64)->bool {
    return eq_lit(a,l,lit_fim_pad())||eq_lit(a,l,lit_fim_pad_angle());
}
fn match_fim_rep(a:i64,l:i64)->bool {
    return eq_lit(a,l,lit_fim_repo())||eq_lit(a,l,lit_repo_name())||eq_lit(a,l,lit_rep());
}
fn match_fim_sep(a:i64,l:i64)->bool {return eq_lit(a,l,lit_file_sep());}
fn match_eog_text(a:i64,l:i64)->bool {
    return match_eot(a,l)||match_eom(a,l)||eq_lit(a,l,lit_eot_id());
}
fn match_user_channel(a:i64,l:i64)->bool {
    return eq_lit(a,l,lit_channel())||eq_lit(a,l,lit_message())||eq_lit(a,l,lit_start())||eq_lit(a,l,lit_constrain());
}

fn detect_specials(c:i64) {
    let n=token_count(c);
    var i:i64=0;
    while i<n {
        let a=token_text_addr(c,i); let l=token_text_len(c,i);
        let attr=token_attr(c,i);
        if ctx_get(c,ck_eot())==-1&&match_eot(a,l) {
            ctx_set(c,ck_eot(),i);
            if (attr&attr_control())==0 {token_attr_set(c,i,attr|attr_control());}
        }
        if ctx_get(c,ck_eom())==-1&&match_eom(a,l) {
            ctx_set(c,ck_eom(),i);
            if (token_attr(c,i)&attr_control())==0 {token_attr_set(c,i,token_attr(c,i)|attr_control());}
        }
        if ctx_get(c,ck_fim_pre())==-1&&match_fim_pre(a,l) {ctx_set(c,ck_fim_pre(),i);}
        if ctx_get(c,ck_fim_suf())==-1&&match_fim_suf(a,l) {ctx_set(c,ck_fim_suf(),i);}
        if ctx_get(c,ck_fim_mid())==-1&&match_fim_mid(a,l) {ctx_set(c,ck_fim_mid(),i);}
        if ctx_get(c,ck_fim_pad())==-1&&match_fim_pad(a,l) {ctx_set(c,ck_fim_pad(),i);}
        if ctx_get(c,ck_fim_rep())==-1&&match_fim_rep(a,l) {ctx_set(c,ck_fim_rep(),i);}
        if ctx_get(c,ck_fim_sep())==-1&&match_fim_sep(a,l) {ctx_set(c,ck_fim_sep(),i);}
        i+=1;
    }
    // unused control text and user-defined render markers
    i=0;
    while i<n {
        let a=token_text_addr(c,i); let l=token_text_len(c,i);
        let attr=token_attr(c,i);
        if (attr&attr_control())!=0&&(attr&attr_unused())==0&&contains_lit(a,l,lit_unused()) {
            token_attr_set(c,i,attr|attr_unused());
        }
        if match_user_channel(a,l) {token_attr_set(c,i,attr_user());}
        i+=1;
    }
}

fn add_eog_if(c:i64,id:i64) {if id>=0 {let ignored=eog_add(c,id);}}

fn build_eog(c:i64) {
    eog_reset(c);
    add_eog_if(c,ctx_get(c,ck_fim_pad()));
    add_eog_if(c,ctx_get(c,ck_fim_rep()));
    add_eog_if(c,ctx_get(c,ck_fim_sep()));
    let n=token_count(c);
    var i:i64=0;
    while i<n {
        let a=token_text_addr(c,i); let l=token_text_len(c,i);
        if match_eog_text(a,l) {
            let ignored=eog_add(c,i);
            let attr=token_attr(c,i);
            if (attr&attr_control())==0 {token_attr_set(c,i,attr|attr_control());}
        }
        i+=1;
    }
    add_eog_if(c,ctx_get(c,ck_eos()));
    add_eog_if(c,ctx_get(c,ck_eot()));
    add_eog_if(c,ctx_get(c,ck_eom()));
}

fn build_special_cache(c:i64) {
    special_reset(c);
    let n=token_count(c);
    var i:i64=0;
    while i<n {
        let attr=token_attr(c,i);
        if (attr&(attr_control()|attr_user()|attr_unknown()))!=0 {special_push(c,i);}
        i+=1;
    }
    i=1;
    while i<special_count(c) {
        let id=special_get(c,i);
        let len=token_text_len(c,id);
        var j:i64=i;
        while j>0 {
            let p=special_get(c,j-1);
            let plen=token_text_len(c,p);
            if plen<len||(plen==len&&p>id) {special_set(c,j,p);j-=1;} else {break;}
        }
        special_set(c,j,id);
        i+=1;
    }
}

fn vcx_load(m:i64)->i64 {
    if m<=0 {return 0;}
    if !eq_lit(meta_str_addr(m,mk_model()),meta_str_len(m,mk_model()),lit_gpt2()) {return 0;}
    if !eq_lit(meta_str_addr(m,mk_pre()),meta_str_len(m,mk_pre()),lit_qwen35()) {return 0;}
    let n=meta_token_count(m);
    if n<=0 {return 0;}

    let c=ctx_new(0);
    if c==0 {return 0;}

    ctx_set(c,ck_type(),type_bpe());
    ctx_set(c,ck_pre(),pre_qwen35());
    ctx_set(c,ck_n_tokens(),n);
    let ntt=meta_i(m,mk_token_types());
    ctx_set(c,ck_n_token_types(),if ntt>=0 {ntt}else{0});
    ctx_set(c,ck_add_prefix(),0);
    let ab=meta_i(m,mk_add_bos()); ctx_set(c,ck_add_bos(),if ab>=0 {ab}else{0});
    let ae=meta_i(m,mk_add_eos()); ctx_set(c,ck_add_eos(),if ae>=0 {ae}else{0});
    let asf=meta_i(m,mk_add_sep()); ctx_set(c,ck_add_sep(),if asf>=0 {asf}else{0});
    ctx_set(c,ck_ignore_merges(),0);
    ctx_set(c,ck_clean_spaces(),0);
    ctx_set(c,ck_remove_ws(),if meta_i(m,mk_remove_ws())>0 {1}else{0});
    ctx_set(c,ck_escape_ws(),0);
    ctx_set(c,ck_nfc(),if meta_i(m,mk_nfc())>0 {1}else{0});
    let low=meta_i(m,mk_lower()); ctx_set(c,ck_lower(),if low>=0 {low}else{1});
    let strip=meta_i(m,mk_strip()); ctx_set(c,ck_strip(),if strip>=0 {strip}else{1});
    default_specials(c);
    override_special(m,c,ck_bos(),n);
    override_special(m,c,ck_eos(),n);
    override_special(m,c,ck_eot(),n);
    override_special(m,c,ck_eom(),n);
    override_special(m,c,ck_unk(),n);
    override_special(m,c,ck_sep(),n);
    override_special(m,c,ck_pad(),n);
    override_special(m,c,ck_mask(),n);
    override_special(m,c,ck_fim_pre(),n);
    override_special(m,c,ck_fim_suf(),n);
    override_special(m,c,ck_fim_mid(),n);
    override_special(m,c,ck_fim_pad(),n);
    override_special(m,c,ck_fim_rep(),n);
    override_special(m,c,ck_fim_sep(),n);

    var maxlen:i64=0;
    var i:i64=0;
    while i<n {
        let ta=meta_token_text_addr(m,i); let tl=meta_token_text_len(m,i);
        if tl<=0 {ctx_free(c);return 0;}
        let attr=attr_for(meta_token_type(m,i));
        let sb=meta_token_score_bits(m,i);
        if add_token(c,i,ta,tl,sb,attr)!=0 {ctx_free(c);return 0;}
        if tl>maxlen {maxlen=tl;}
        i+=1;
    }
    ctx_set(c,ck_max_len(),maxlen);

    let mn=meta_merge_count(m);
    i=0;
    while i<mn {
        let ma=meta_merge_addr(m,i); let ml=meta_merge_len(m,i);
        let pos=find_space(ma,ml,1);
        var la=ma; var ll:i64=0; var ra=ma; var rl:i64=0;
        if pos>=0 {ll=pos;ra=ma+pos+1;rl=ml-pos-1;}
        if add_merge(c,la,ll,ra,rl,i)!=0 {ctx_free(c);return 0;}
        i+=1;
    }

    suppress_reset(c);
    let sn=meta_suppress_count(m);
    i=0;
    while i<sn {
        let id=meta_suppress_get(m,i);
        if id>=0&&id<n {suppress_push(c,id);}
        i+=1;
    }

    // Resolve attributes/specials before binding so the bound tokenizer and
    // the piece cache observe the same final token metadata.
    detect_specials(c);
    build_eog(c);
    build_special_cache(c);

    // Bind the qualified generated tokenizer to this exact loaded vocabulary.
    let v=ov_new();
    if v==0 {ctx_free(c);return 0;}
    ctx_set_old_vocab(c,v);
    i=0;
    while i<n {
        let ta=token_text_addr(c,i); let tl=token_text_len(c,i);
        let attr=token_attr(c,i);
        if ov_add_token(v,i,ta,tl,attr)!=0 {ctx_free(c);return 0;}
        i+=1;
    }
    let omc=merge_count(c);
    i=0;
    while i<omc {
        let la=merge_left_addr(c,i); let ll=merge_left_len(c,i);
        let ra=merge_right_addr(c,i); let rl=merge_right_len(c,i);
        let rank=merge_rank(c,i);
        if ov_add_merge(v,la,ll,ra,rl,rank)!=0 {ctx_free(c);return 0;}
        i+=1;
    }
    ov_config(v,pre_qwen35(),ctx_get(c,ck_add_bos()),ctx_get(c,ck_bos()),
               ctx_get(c,ck_add_eos()),ctx_get(c,ck_eos()),
               ctx_get(c,ck_clean_spaces()),ctx_get(c,ck_ignore_merges()),
               ctx_get(c,ck_escape_ws()));
    if ov_finalize(v)<0 {ctx_free(c);return 0;}
    let sess=os_new(v);
    if sess==0 {ctx_free(c);return 0;}

    // linefeed token: normal native lifecycle tokenizes exactly one newline.
    let nl_a=lit_addr(lit_newline()); let nl_l=lit_len(lit_newline());
    let ignored=tokenizer_entry(1,v,sess,nl_a,nl_l,0,0);
    var lf:i64=-1;
    if tok_out_count(sess)>0 {lf=tok_out_get(sess,0);} else {lf=ctx_get(c,ck_pad());}
    ctx_set(c,ck_linefeed(),lf);

    // token -> piece cache, once per load, through the bound tokenizer.
    i=0;
    while i<n {
        let pc=tokenizer_entry(3,v,sess,i,0,1,0);
        if pc<0 {os_free(sess);ctx_free(c);return 0;}
        let ta=tok_text_addr(sess); let tl=tok_text_count(sess);
        if piece_set(c,i,ta,tl)!=0 {os_free(sess);ctx_free(c);return 0;}
        i+=1;
    }
    os_free(sess);
    return c;
}

// Append one cached piece to ctx out, applying token_to_piece special filter
// and lstrip.  Cache stores the special=true piece for every token.
fn append_piece(c:i64,token:i64,lstrip:i64,special:i64)->i64 {
    let attr=token_attr(c,token);
    if attr<0 {return 0;}
    if special==0&&(attr&(attr_unknown()|attr_control()))!=0 {return 0;}
    let pa=piece_addr(c,token); let pl=piece_len(c,token);
    if pl<=0 {return 0;}
    var i:i64=0; var sk:i64=0;
    while i<pl {
        if sk<lstrip&&mem_byte(pa,i)==32 {i+=1;sk+=1;} else {break;}
    }
    let outn=pl-i;
    if outn>0 {out_append(c,pa+i,outn);}
    return outn;
}

fn clean_spaces(c:i64) {
    let n=out_count(c);
    if n<=0 {return;}
    var total:i64=1; var i:i64=1;
    while i<n {
        let x=out_get(c,i);
        if out_get(c,i-1)==32 {if x==63||x==33||x==46||x==44 {total-=1;}}
        out_set(c,total,x); total+=1; i+=1;
    }
    let total2=total; total=1; i=1;
    while i<total2 {
        let x=out_get(c,i);
        if x==39&&i+1<total2&&out_get(c,i-1)==32&&out_get(c,i+1)==32 {
            total-=1; i+=1; out_set(c,i,0);
        }
        out_set(c,total,x); total+=1; i+=1;
    }
    let total3=total; total=1; i=1;
    while i<total3 {
        let x=out_get(c,i);
        if out_get(c,i-1)==32 {
            if x==39&&i+1<total3 {
                let x1=out_get(c,i+1);
                if x1==116||x1==100 { } else if x1==115||x1==109 {total-=1;}
                else if i+2<total3 {
                    let x2=out_get(c,i+2);
                    if x1==108&&x2==108 { } else if (x1==114&&x2==101)||(x1==118&&x2==101) {total-=1;}
                }
            }
        }
        out_set(c,total,x); total+=1; i+=1;
    }
    out_truncate(c,total);
}

fn vcx_detokenize(c:i64,tokens:i64,n:i64,remove_special:i64,unparse_special:i64)->i64 {
    if ctx_valid(c)==0 {return -1;}
    out_reset(c);
    var start:i64=0; var count:i64=n; var remove_space:i64=0;
    if remove_special!=0&&ctx_get(c,ck_add_bos())>0&&count>0&&tok_i32(tokens,0)==ctx_get(c,ck_bos()) {
        start=1; count-=1;
    }
    if remove_special!=0&&ctx_get(c,ck_add_eos())>0&&count>0&&tok_i32(tokens,start+count-1)==ctx_get(c,ck_eos()) {
        count-=1;
    }
    var i:i64=0;
    while i<count {
        let ignored=append_piece(c,tok_i32(tokens,start+i),remove_space,unparse_special);
        remove_space=0; i+=1;
    }
    if ctx_get(c,ck_clean_spaces())>0 {clean_spaces(c);}
    return out_count(c);
}

fn vcx_selftest()->i64 {
    let m=lit_len(lit_qwen35());
    if m!=6 {return 0;}
    return 1;
}

// op 1 load(meta), 2 piece(ctx,token,lstrip,special), 3 detokenize(ctx,tokens,n,remove,unparse),
// 4 selftest, 5 free(ctx)
fn entry(op:i64,c:i64,a:i64,b:i64,d:i64,e:i64,f:i64)->i64 {
    if op==1 {return vcx_load(c);}
    if op==2 {
        if ctx_valid(c)==0 {return -1;}
        out_reset(c);
        return append_piece(c,a,b,d);
    }
    if op==3 {return vcx_detokenize(c,a,b,d,e);}
    if op==4 {return vcx_selftest();}
    if op==5 {ctx_free(c);return 0;}
    fail(91);
    return -1;
}

fn main() {print(entry(4,0,0,0,0,0,0));}
