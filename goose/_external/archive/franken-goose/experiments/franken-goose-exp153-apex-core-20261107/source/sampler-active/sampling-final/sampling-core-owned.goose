extern "fg153_sp_arr_new" fn sp_arr_new(cap:i64)->i64;
extern "fg153_sp_arr_free" fn sp_arr_free(a:i64);
extern "fg153_sp_arr_get" fn sp_arr_get(a:i64,code:i64)->i64;
extern "fg153_sp_arr_set" fn sp_arr_set(a:i64,code:i64,v:i64);
extern "fg153_sp_tok_get" fn sp_tok_get(a:i64,i:i64,code:i64)->i64;
extern "fg153_sp_tok_set" fn sp_tok_set(a:i64,i:i64,code:i64,v:i64);
extern "fg153_sp_float_bits" fn sp_float_bits(v:f32)->i64;
extern "fg153_sp_bits_float" fn sp_bits_float(b:i64)->f32;
extern "fg153_sp_expf_bits" fn sp_expf_bits(b:i64)->i64;
extern "fg153_sp_logf_bits" fn sp_logf_bits(b:i64)->i64;
extern "fg153_sp_fabsf_bits" fn sp_fabsf_bits(b:i64)->i64;
extern "fg153_sp_rng_new" fn sp_rng_new(seed:i64)->i64;
extern "fg153_sp_rng_free" fn sp_rng_free(r:i64);
extern "fg153_sp_rng_reset" fn sp_rng_reset(r:i64);
extern "fg153_sp_discrete" fn sp_discrete(r:i64,a:i64)->i64;
extern "fg153_sp_grammar_apply" fn sp_grammar_apply(g:i64,a:i64)->i64;
extern "fg153_sp_hist_new" fn sp_hist_new(vocab:i64,n:i64)->i64;
extern "fg153_sp_hist_free" fn sp_hist_free(h:i64);
extern "fg153_sp_hist_get" fn sp_hist_get(h:i64,code:i64,i:i64)->i64;
extern "fg153_sp_hist_set" fn sp_hist_set(h:i64,code:i64,i:i64,v:i64);
extern "fg153_sp_chain_new" fn sp_chain_new(cap:i64)->i64;
extern "fg153_sp_chain_free" fn sp_chain_free(c:i64);
extern "fg153_sp_chain_get" fn sp_chain_get(c:i64,code:i64,i:i64)->i64;
extern "fg153_sp_chain_set" fn sp_chain_set(c:i64,code:i64,i:i64,v:i64);
extern "fg153_sp_chain_add" fn sp_chain_add(c:i64,t:i64,f:i64,i1:i64,i2:i64)->i64;
extern "fg153_sp_msg_role" fn sp_msg_role(m:i64,i:i64)->i64;
extern "fg153_sp_msg_content" fn sp_msg_content(m:i64,i:i64)->i64;
extern "fg153_sp_str_len" fn sp_str_len(s:i64)->i64;
extern "fg153_sp_str_at" fn sp_str_at(s:i64,i:i64)->i64;
extern "fg153_sp_str_eq" fn sp_str_eq(a:i64,b:i64)->i64;
extern "fg153_sp_str_contains" fn sp_str_contains(a:i64,b:i64)->i64;
extern "fg153_sp_contains_known" fn sp_contains_known(a:i64,code:i64)->i64;
extern "fg153_sp_role_code" fn sp_role_code(s:i64)->i64;
extern "fg153_sp_name_code" fn sp_name_code(s:i64)->i64;
extern "fg153_sp_buf_new" fn sp_buf_new()->i64;
extern "fg153_sp_buf_free" fn sp_buf_free(b:i64);
extern "fg153_sp_buf_append" fn sp_buf_append(b:i64,s:i64);
extern "fg153_sp_buf_append_slice" fn sp_buf_append_slice(b:i64,s:i64,start:i64,len:i64);
extern "fg153_sp_buf_append_byte" fn sp_buf_append_byte(b:i64,ch:i64);
extern "fg153_sp_buf_append_lit" fn sp_buf_append_lit(b:i64,code:i64);
extern "fg153_sp_buf_reset" fn sp_buf_reset(b:i64);
extern "fg153_sp_buf_len" fn sp_buf_len(b:i64)->i64;
extern "fg153_sp_buf_data" fn sp_buf_data(b:i64)->i64;
extern "fg153_sp_fail" fn sp_fail(code:i64);

fn sp_ensure(ok:bool) {if !ok {sp_fail(1);}}
fn neg_inf_bits()->i64 {return -8388608;}
fn f32_of_bits(b:i64)->f32 {return sp_bits_float(b);}
fn bits_of_f32(v:f32)->i64 {return sp_float_bits(v);}
fn sp_expf(v:f32)->f32 {return f32_of_bits(sp_expf_bits(bits_of_f32(v)));}
fn sp_logf(v:f32)->f32 {return f32_of_bits(sp_logf_bits(bits_of_f32(v)));}
fn sp_fabsf(v:f32)->f32 {return f32_of_bits(sp_fabsf_bits(bits_of_f32(v)));}

fn a_size(a:i64)->i64 {return sp_arr_get(a,0);}
fn a_set_size(a:i64,v:i64) {sp_arr_set(a,0,v);}
fn a_sorted(a:i64)->bool {return sp_arr_get(a,2)!=0;}
fn a_set_sorted(a:i64,v:i64) {sp_arr_set(a,2,v);}
fn tok_id(a:i64,i:i64)->i64 {return sp_tok_get(a,i,0);}
fn tok_logit(a:i64,i:i64)->f32 {return f32_of_bits(sp_tok_get(a,i,1));}
fn tok_p(a:i64,i:i64)->f32 {return f32_of_bits(sp_tok_get(a,i,2));}
fn set_logit(a:i64,i:i64,v:f32) {sp_tok_set(a,i,1,bits_of_f32(v));}
fn set_p(a:i64,i:i64,v:f32) {sp_tok_set(a,i,2,bits_of_f32(v));}

fn tok_copy(a:i64,src:i64,dst:i64) {
    let id=tok_id(a,src);let l=sp_tok_get(a,src,1);let p=sp_tok_get(a,src,2);
    sp_tok_set(a,dst,0,id);sp_tok_set(a,dst,1,l);sp_tok_set(a,dst,2,p);
}
fn tok_swap(a:i64,i:i64,j:i64) {
    if i==j {return;}
    let idi=tok_id(a,i);let li=sp_tok_get(a,i,1);let pi=sp_tok_get(a,i,2);
    let idj=tok_id(a,j);let lj=sp_tok_get(a,j,1);let pj=sp_tok_get(a,j,2);
    sp_tok_set(a,i,0,idj);sp_tok_set(a,i,1,lj);sp_tok_set(a,i,2,pj);
    sp_tok_set(a,j,0,idi);sp_tok_set(a,j,1,li);sp_tok_set(a,j,2,pi);
}
fn sort_desc(a:i64) {
    let n=a_size(a);var i:i64=1;
    while i<n {
        var j=i;
        while j>0 {
            if tok_logit(a,j)>tok_logit(a,j-1) {tok_swap(a,j,j-1);j-=1;} else {break;}
        }
        i+=1;
    }
}

fn sp_temp(a:i64,temp:f32) {
    let n=a_size(a);if n==0 {return;}
    if temp<=0.0 {
        var max_i:i64=0;var max_l=tok_logit(a,0);var i:i64=1;
        while i<n {
            if tok_logit(a,i)>max_l {set_logit(a,max_i,f32_of_bits(neg_inf_bits()));max_i=i;max_l=tok_logit(a,i);}
            else {set_logit(a,i,f32_of_bits(neg_inf_bits()));}
            i+=1;
        }
        return;
    }
    var i:i64=0;while i<n {set_logit(a,i,tok_logit(a,i)/temp);i+=1;}
}

fn sp_softmax(a:i64,do_sort:i64) {
    let n=a_size(a);sp_ensure(n>0);
    if do_sort!=0&&!a_sorted(a) {sort_desc(a);}
    var max_l=tok_logit(a,0);
    if !a_sorted(a) {var i:i64=1;while i<n {if tok_logit(a,i)>max_l {max_l=tok_logit(a,i);}i+=1;}}
    var sum:f32=0.0;var i:i64=0;
    while i<n {let p=sp_expf(tok_logit(a,i)-max_l);set_p(a,i,p);sum+=p;i+=1;}
    i=0;while i<n {set_p(a,i,tok_p(a,i)/sum);i+=1;}
}

fn tk_gt(a:i64,i:i64,j:i64)->bool {
    return tok_logit(a,i)>tok_logit(a,j);
}
fn tk_adjust(a:i64,hole0:i64,len:i64,vid:i64,vl:i64,vp:i64) {
    let top=hole0;var hole=hole0;var second=hole0;
    while second<(len-1)/2 {
        second=2*(second+1);
        if tk_gt(a,second,second-1) {second-=1;}
        tok_copy(a,second,hole);hole=second;
    }
    if (len&1)==0&&second==(len-2)/2 {
        second=2*(second+1);
        tok_copy(a,second-1,hole);hole=second-1;
    }
    var parent=(hole-1)/2;
    while hole>top&&tok_logit(a,parent)>f32_of_bits(vl) {
        tok_copy(a,parent,hole);hole=parent;parent=(hole-1)/2;
    }
    sp_tok_set(a,hole,0,vid);sp_tok_set(a,hole,1,vl);sp_tok_set(a,hole,2,vp);
}
fn tk_make(a:i64,len:i64) {
    if len<2 {return;}
    var parent=(len-2)/2;
    while 1==1 {
        let vid=sp_tok_get(a,parent,0);let vl=sp_tok_get(a,parent,1);let vp=sp_tok_get(a,parent,2);
        tk_adjust(a,parent,len,vid,vl,vp);
        if parent==0 {return;}
        parent-=1;
    }
}
fn tk_pop(a:i64,heap_len:i64,dst:i64) {
    let vid=sp_tok_get(a,dst,0);let vl=sp_tok_get(a,dst,1);let vp=sp_tok_get(a,dst,2);
    sp_tok_set(a,dst,0,sp_tok_get(a,0,0));sp_tok_set(a,dst,1,sp_tok_get(a,0,1));sp_tok_set(a,dst,2,sp_tok_get(a,0,2));
    tk_adjust(a,0,heap_len,vid,vl,vp);
}
fn tk_select(a:i64,n:i64,k:i64) {
    tk_make(a,k);var i=k;
    while i<n {
        if tk_gt(a,i,0) {tk_pop(a,k,i);}
        i+=1;
    }
}
fn tk_sort(a:i64,k:i64) {
    var end=k;
    while end>1 {
        end-=1;tk_pop(a,end,end);
    }
}
fn sp_top_k(a:i64,kv:i64) {
    if kv<=0 {return;}
    var k=kv;let n=a_size(a);if k>n {k=n;}
    if !a_sorted(a) {
        if k<=128 {tk_select(a,n,k);tk_sort(a,k);}
        else {sort_desc(a);}
        a_set_sorted(a,1);
    }
    a_set_size(a,k);
}

fn sp_top_p(a:i64,pv:f32,min_keep:i64) {
    if pv>=1.0 {return;}
    sp_softmax(a,0);
    if !a_sorted(a) {sort_desc(a);}
    var cum:f32=0.0;var last=a_size(a);var i:i64=0;let n=a_size(a);
    while i<n {
        cum+=tok_p(a,i);
        if cum>=pv&&i+1>=min_keep {last=i+1;break;}
        i+=1;
    }
    a_set_size(a,last);
}

fn sp_min_p(a:i64,pv:f32,min_keep:i64) {
    if pv<=0.0 {return;}
    let n=a_size(a);if n==0 {return;}
    var applied:i64=0;
    if !a_sorted(a) {
        var max_l=tok_logit(a,0);var i:i64=1;
        while i<n {if tok_logit(a,i)>max_l {max_l=tok_logit(a,i);}i+=1;}
        let min_logit=max_l+sp_logf(pv);
        var out:i64=0;i=0;
        while i<n {if tok_logit(a,i)>=min_logit {tok_copy(a,i,out);out+=1;}i+=1;}
        if out>0&&out>=min_keep {a_set_size(a,out);applied=1;}
    }
    if applied==0 {
        if !a_sorted(a) {sort_desc(a);}
        let min_logit=tok_logit(a,0)+sp_logf(pv);
        var i:i64=1;
        while i<n {if tok_logit(a,i)<min_logit&&i>=min_keep {break;}i+=1;}
        a_set_size(a,i);
    }
}

fn sp_penalties(a:i64,hist:i64,rep:f32,freq:f32,pres:f32) {
    if hist==0 {return;}
    let n=a_size(a);var i:i64=0;
    while i<n {
        let id=tok_id(a,i);let count=sp_hist_get(hist,6,id);
        if count>0 {
            var l=tok_logit(a,i);
            if l<=0.0 {l=l*rep;} else {l=l/rep;}
            let cf=count as f32;
            l=l-cf*freq-pres;
            set_logit(a,i,l);
        }
        i+=1;
    }
    a_set_sorted(a,0);
}

fn sp_hist_accept(h:i64,token:i64) {
    let last_n=sp_hist_get(h,3,0);if last_n==0 {return;}
    let size=sp_hist_get(h,0,0);let first=sp_hist_get(h,1,0);let pos=sp_hist_get(h,2,0);
    sp_hist_set(h,6,token,sp_hist_get(h,6,token)+1);
    var first2=first;
    if size>=last_n {
        let old=sp_hist_get(h,5,first);
        let c=sp_hist_get(h,6,old)-1;sp_hist_set(h,6,old,c);
        first2=(first+1)%last_n;sp_hist_set(h,1,0,first2);
    } else {sp_hist_set(h,0,0,size+1);}
    sp_hist_set(h,5,pos,token);
    sp_hist_set(h,2,0,(pos+1)%last_n);
}

fn sp_hist_reset(h:i64) {
    let vocab=sp_hist_get(h,4,0);
    sp_hist_set(h,0,0,0);sp_hist_set(h,1,0,0);sp_hist_set(h,2,0,0);
    var i:i64=0;while i<vocab {sp_hist_set(h,6,i,0);i+=1;}
}

fn sp_chain_apply(chain:i64,a:i64,rng:i64,hist:i64) {
    let n=sp_chain_get(chain,0,0);var i:i64=0;
    while i<n {
        let t=sp_chain_get(chain,1,i);
        let f=sp_bits_float(sp_chain_get(chain,2,i));
        let i1=sp_chain_get(chain,3,i);let i2=sp_chain_get(chain,4,i);
        if t==0 {sp_temp(a,f);}
        else if t==1 {sp_top_k(a,i1);}
        else if t==2 {sp_top_p(a,f,i1);}
        else if t==3 {sp_min_p(a,f,i1);}
        else if t==4 {sp_penalties(a,hist,f,sp_bits_float(i1),sp_bits_float(i2));}
        else if t==5 {sp_arr_set(a,1,sp_discrete(rng,a));}
        else if t==6 {let ignored=sp_grammar_apply(i1,a);}
        i+=1;
    }
}

fn sp_append_role(b:i64,role:i64) {sp_buf_append(b,role);}
fn is_space(c:i64)->bool {return c==32||(c>=9&&c<=13);}
fn append_trimmed(b:i64,s:i64) {
    let n=sp_str_len(s);var start:i64=0;var end=n;
    while start<end&&is_space(sp_str_at(s,start)) {start+=1;}
    while end>start&&is_space(sp_str_at(s,end-1)) {end-=1;}
    sp_buf_append_slice(b,s,start,end-start);
}

fn chat_apply(code:i64,msgs:i64,n:i64,add_ass:i64,b:i64) {
    if code==0 {
        var i:i64=0;while i<n {sp_buf_append_lit(b,0);sp_append_role(b,sp_msg_role(msgs,i));sp_buf_append_lit(b,21);sp_buf_append(b,sp_msg_content(msgs,i));sp_buf_append_lit(b,1);i+=1;}
        if add_ass!=0 {sp_buf_append_lit(b,25);}
        return;
    }
    if code==8||code==9 {
        let trail=if code==8 {1} else {0};
        var i:i64=0;
        while i<n {
            let rc=sp_role_code(sp_msg_role(msgs,i));let content=sp_msg_content(msgs,i);
            if rc==0 {sp_buf_append_lit(b,17);if trail!=0 {sp_buf_append_lit(b,13);}sp_buf_append(b,content);sp_buf_append_lit(b,18);}
            else if rc==1 {sp_buf_append_lit(b,12);if trail!=0 {sp_buf_append_lit(b,13);}sp_buf_append(b,content);sp_buf_append_lit(b,29);}
            else {if trail!=0 {sp_buf_append_lit(b,13);}sp_buf_append(b,content);sp_buf_append_lit(b,10);}
            i+=1;
        }
        return;
    }
    if code==5||code==6||code==7 {
        let leading=if code==5 {13} else {14};
        let trailing=if code==7 {14} else {13};
        let trim_ass=if code==6 {1} else {0};
        var inside:i64=0;var i:i64=0;
        while i<n {
            if inside==0 {sp_buf_append_lit(b,leading);sp_buf_append_lit(b,12);sp_buf_append_lit(b,trailing);inside=1;}
            let rc=sp_role_code(sp_msg_role(msgs,i));let content=sp_msg_content(msgs,i);
            if rc==0 {sp_buf_append(b,content);sp_buf_append_lit(b,19);}
            else if rc==1 {sp_buf_append(b,content);sp_buf_append_lit(b,leading);sp_buf_append_lit(b,29);}
            else {sp_buf_append_lit(b,trailing);if trim_ass!=0 {append_trimmed(b,content);} else {sp_buf_append(b,content);}sp_buf_append_lit(b,10);inside=0;}
            i+=1;
        }
        return;
    }
    if code==1||code==2||code==3||code==4 {
        let sys=if code==1 {0} else {1};
        let bos=if code==3 {1} else {0};
        let strip=if code==4 {1} else {0};
        var inside:i64=1;sp_buf_append_lit(b,8);var i:i64=0;
        while i<n {
            let content=sp_msg_content(msgs,i);
            if inside==0 {inside=1;sp_buf_append_lit(b,if bos!=0 {11} else {8});}
            let rc=sp_role_code(sp_msg_role(msgs,i));
            if rc==0 {
                if sys!=0 {sp_buf_append_lit(b,15);if strip!=0 {append_trimmed(b,content);} else {sp_buf_append(b,content);}sp_buf_append_lit(b,16);}
                else {sp_buf_append(b,content);sp_buf_append_lit(b,21);}
            } else if rc==1 {sp_buf_append(b,content);sp_buf_append_lit(b,9);}
            else {sp_buf_append(b,content);sp_buf_append_lit(b,10);inside=0;}
            i+=1;
        }
        return;
    }
    if code==10 {
        var i:i64=0;while i<n {sp_buf_append_lit(b,5);sp_append_role(b,sp_msg_role(msgs,i));sp_buf_append_lit(b,6);sp_buf_append(b,sp_msg_content(msgs,i));sp_buf_append_lit(b,7);i+=1;}
        if add_ass!=0 {sp_buf_append_lit(b,4);}
        return;
    }
    if code==11 {
        var i:i64=0;while i<n {sp_buf_append_lit(b,0);sp_append_role(b,sp_msg_role(msgs,i));sp_buf_append_lit(b,2);sp_buf_append(b,sp_msg_content(msgs,i));sp_buf_append_lit(b,3);i+=1;}
        if add_ass!=0 {sp_buf_append_lit(b,26);}
        return;
    }
    if code==15 {
        let sysbuf=sp_buf_new();var i:i64=0;
        while i<n {
            let rc=sp_role_code(sp_msg_role(msgs,i));let content=sp_msg_content(msgs,i);
            if rc==0 {append_trimmed(sysbuf,content);}
            else {
                let is_model=rc==2||rc==6;
                if is_model {sp_buf_append_lit(b,20);sp_buf_append_lit(b,23);}
                else {sp_buf_append_lit(b,20);sp_append_role(b,sp_msg_role(msgs,i));sp_buf_append_lit(b,21);}
                if sp_buf_len(sysbuf)>0&&rc!=6&&!is_model {sp_buf_append(b,sp_buf_data(sysbuf));sp_buf_append_lit(b,19);sp_buf_reset(sysbuf);}
                append_trimmed(b,content);sp_buf_append_lit(b,22);
            }
            i+=1;
        }
        if add_ass!=0 {sp_buf_append_lit(b,20);sp_buf_append_lit(b,23);}
        sp_buf_free(sysbuf);return;
    }
    sp_fail(2);
}

fn chat_detect(s:i64)->i64 {
    let named=sp_name_code(s);if named>=0 {return named;}
    if sp_contains_known(s,0)!=0 {
        if sp_contains_known(s,1)!=0 {return 11;}
        if sp_contains_known(s,2)!=0 {return -1;}
        return 0;
    }
    if sp_contains_known(s,3)!=0 {
        if sp_contains_known(s,4)!=0 {return 8;}
        if sp_contains_known(s,5)!=0||sp_contains_known(s,6)!=0 {
            if sp_contains_known(s,7)!=0 {return 5;}
            if sp_contains_known(s,8)!=0 {return 7;}
            return 6;
        }
        if sp_contains_known(s,11)!=0 {return 4;}
        if sp_contains_known(s,10)!=0 {return 3;}
        if sp_contains_known(s,9)!=0 {return 2;}
        return 1;
    }
    if sp_contains_known(s,12)!=0&&sp_contains_known(s,13)!=0 {return 10;}
    if sp_contains_known(s,14)!=0 {return 15;}
    return -1;
}

fn sp_selftest()->i64 {return 1;}

fn sp_entry(mode:i64,a:i64,b:i64,c:i64,d:i64,e:i64,f:i64,g:i64)->i64 {
    if mode==0 {sp_temp(a,sp_bits_float(b));return 0;}
    if mode==1 {sp_softmax(a,b);return 0;}
    if mode==2 {sp_top_k(a,b);return 0;}
    if mode==3 {sp_top_p(a,sp_bits_float(b),c);return 0;}
    if mode==4 {sp_min_p(a,sp_bits_float(b),c);return 0;}
    if mode==5 {sp_penalties(a,b,sp_bits_float(c),sp_bits_float(d),sp_bits_float(e));return 0;}
    if mode==6 {sp_hist_accept(a,b);return 0;}
    if mode==7 {sp_hist_reset(a);return 0;}
    if mode==8 {sp_chain_apply(a,b,c,d);return 0;}
    if mode==9 {sp_hist_accept(a,b);return 0;}
    if mode==10 {let idx=sp_discrete(a,b);sp_arr_set(b,1,idx);return idx;}
    if mode==11 {return sp_chain_add(a,b,c,d,e);}
    if mode==20 {return chat_detect(a);}
    if mode==21 {chat_apply(a,b,c,d,e);return sp_buf_len(e);}
    if mode==22 {return sp_selftest();}
    sp_fail(3);return 0;
}
fn main() {print(sp_entry(22,0,0,0,0,0,0,0));}
