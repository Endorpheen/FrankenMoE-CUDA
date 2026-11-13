// worker-4: EXP107 context policy/control port.
// Status: UNVERIFIED PROPOSAL for parent Codex. Compiled by the local check.
// Covers: resolve_fused_ops (:616-691), graph_get_cb pin policy (:2933-2958),
// sched_graph_realloc counters/log (:2799-2816), and the constructor cparams
// derivation block (:252-339). Only typed storage/backend primitives remain in
// C++ (services), all policy/control lives here.
//
// fg107_pol(f,op,a,b,c,d): 0 fused_get(a) 1 fused_set(a,b) 2 auto_get(a)
// 3 auto_set(a,b) 4 graph_reserve_probe(a=nt,b=n_seqs,c=mctx)
// 5 fused_count 6 fused_op(a=i) 7 fused_il(a=i) 8 fused_tensor(a=i)
// 9 sched_backend(a=tensor) 10 backend_dev(a=be) 11 model_dev_layer(a=il)
// 12 log(a=site,b=x,c=y,d=z) 13 probe_op(a) 14 probe_tokens(a)
// 15 note_realloc(a=same?1:diff) 16 realloc_counts(a=which)
// cparams fields: params int 0 rope_type 1 attn_type 2 fa_type 3 n_batch
// 4 n_ubatch 5 n_out_max 6 n_out_max_seq; params float 100 rope_base
// 101 rope_scale 102 yarn_ext 103 yarn_attn; hparams int 200 rope_type_train
// 201 pooling 202 causal 203 arch; hparams float 300 rope_base_train
// 301 rope_scale_train 302 yarn_log_mul 303 rope_attn;
// cparams int 400 rope_type 401 pooling 402 causal 403 flash 404 auto_fa
// 405 fgdn_ar 406 fgdn_ch 407 auto_fgdn 408 flid 409 auto_flid 410 hc_pre
// 411 hc_comb 412 hc_post 413 auto_fhc 414 n_batch 415 n_ubatch 416 n_out_max
// 417 n_out_max_seq 418 n_ctx; cparams float 500 rope_scale 501 yarn_ext
// 502 yarn_attn. enum: 0 ROPE_UNSPEC 1 ROPE_NONE 2 ROPE_YARN 3 POOL_UNSPEC
// 4 POOL_NONE 5 ATTN_UNSPEC 6 ATTN_CAUSAL 7 FA_DISABLED 8 FA_AUTO 9 ARCH_DS2.

extern "fg107_pol" fn pol(f:i64,op:i64,a:i64,b:i64,c:i64,d:i64)->i64;
extern "fg107_cpi" fn cpi(f:i64,op:i64,field:i64,value:i64)->i64;
extern "fg107_cpf" fn cpf(f:i64,op:i64,field:i64,value:f32)->f32;
extern "fg107_cpc" fn cpc(f:i64,which:i64)->i64;
extern "fg107_cplog" fn cplog(f:i64,site:i64,a:i64,b:i64)->i64;
extern "fg107_cpmodel" fn cpmodel(f:i64)->i64;
extern "logf" fn log32(x:f32)->f32;

fn cpp_min(a:f32,b:f32)->f32 {return if b<a {b} else {a};}
fn cpp_max(a:f32,b:f32)->f32 {return if a<b {b} else {a};}
fn get_mscale(scale:f32,mscale:f32)->f32 {
    if scale<=(1 as f32) {return (1 as f32);}
    return (0.1 as f32)*mscale*log32(scale)+(1 as f32);
}

// which: 0 flash_attn 1 gdn_ar 2 gdn_ch 3 lid 4 hc_pre 5 hc_comb 6 hc_post
// enabled_field: cparams int field (403,405,406,408,410,411,412)
fn resolve_one(f:i64,which:i64,n_seqs:i64,mctx:i64)->i64 {
    if pol(f,0,which,0,0,0)==0 {return 0;}
    let nt=pol(f,14,which,0,0,0)*n_seqs;
    let gf=pol(f,4,nt,n_seqs,mctx,0);
    if gf==0 {pol(f,12,0,which,0,0);return -1;}
    let cnt=pol(f,5,0,0,0,0);
    var mismatch:i64=0;
    var i:i64=0;
    while i<cnt {
        if pol(f,6,i,0,0,0)==pol(f,13,which,0,0,0) {
            let il=pol(f,7,i,0,0,0);
            assert(il>=0);
            let t=pol(f,8,i,0,0,0);
            let be=pol(f,9,t,0,0,0);
            let dev=pol(f,10,be,0,0,0);
            let layer=pol(f,11,il,0,0,0);
            if dev!=layer {
                pol(f,12,1,il,dev,layer);
                mismatch=1;
            }
        }
        i+=1;
    }
    if mismatch!=0 {
        pol(f,1,which,0,0,0);
        pol(f,12,3,which,0,0);
        return 0;
    }
    pol(f,1,which,1,0,0);
    pol(f,12,2,which,0,0);
    return 0;
}

fn resolve_fused(f:i64,mctx:i64,n_seqs:i64)->i64 {
    if pol(f,2,0,0,0,0)!=0 {
        if resolve_one(f,0,n_seqs,mctx)!=0 {return -1;}
        pol(f,3,0,0,0,0);
    }
    if pol(f,2,1,0,0,0)!=0 {
        pol(f,12,4,0,0,0);
        if resolve_one(f,1,n_seqs,mctx)!=0 {return -1;}
        if resolve_one(f,2,n_seqs,mctx)!=0 {return -1;}
        pol(f,3,1,0,0,0);
    }
    if pol(f,2,2,0,0,0)!=0 {
        pol(f,12,5,0,0,0);
        if resolve_one(f,3,n_seqs,mctx)!=0 {return -1;}
        pol(f,3,2,0,0,0);
    }
    if pol(f,2,3,0,0,0)!=0 {
        pol(f,12,6,0,0,0);
        if resolve_one(f,4,n_seqs,mctx)!=0 {return -1;}
        if resolve_one(f,5,n_seqs,mctx)!=0 {return -1;}
        if resolve_one(f,6,n_seqs,mctx)!=0 {return -1;}
        pol(f,3,3,0,0,0);
    }
    return 0;
}

// returns 1 when the tensor should be pinned to its layer backend
fn graph_cb_policy(f:i64,n_tokens:i64,il:i64,is_norm:i64,is_last:i64,full_offload:i64)->i64 {
    if il==-1||(is_norm==0&&is_last==0) {return 0;}
    if n_tokens<32||full_offload!=0 {return 1;}
    return 0;
}

fn realloc_policy(f:i64,ids_changed:i64,size_changed:i64,nodes:i64,splits:i64)->i64 {
    if ids_changed!=0 {return 0;}
    if size_changed!=0 {pol(f,15,0,0,0,0);} else {pol(f,15,1,0,0,0);}
    let same=pol(f,16,0,0,0,0);
    let diff=pol(f,16,1,0,0,0);
    pol(f,12,7,nodes,splits,same*1000000+diff);
    return 0;
}

fn cparams_policy(f:i64)->i64 {
    // rope scaling type
    if cpi(f,0,400,0)==cpc(f,0) {
        cpi(f,1,400,cpi(f,0,200,0));
    }
    if cpi(f,0,400,0)==cpc(f,1) {
        cpf(f,1,500,(1 as f32));
    }
    if cpf(f,0,501,(0 as f32))<(0 as f32) {
        cpf(f,1,501,if cpi(f,0,400,0)==cpc(f,2) {(1 as f32)} else {(0 as f32)});
    }
    if cpf(f,0,501,(0 as f32))!=(0 as f32) {
        let factor=(1 as f32)/cpf(f,0,500,(1 as f32));
        var mscale=(1 as f32);
        if cpf(f,0,302,(0 as f32))!=(0 as f32) {
            let all=cpf(f,0,302,(0 as f32));
            if cpi(f,0,203,0)==cpc(f,9)&&all!=(1 as f32) {mscale=all;}
            cpf(f,1,502,get_mscale(factor,mscale)/get_mscale(factor,all));
            cplog(f,0,0,0);
        } else {
            cpf(f,1,502,get_mscale(factor,(1 as f32)));
        }
        cpf(f,1,502,cpf(f,0,502,(0 as f32))*((1 as f32)/((1 as f32)+(0.1 as f32)*log32(factor))));
    }
    cpf(f,1,502,cpf(f,0,502,(0 as f32))*cpf(f,0,303,(1 as f32)));

    // pooling
    if cpi(f,0,401,0)==cpc(f,3) {
        if cpi(f,0,201,0)==cpc(f,3) {cpi(f,1,401,cpc(f,4));}
        else {cpi(f,1,401,cpi(f,0,201,0));}
    }

    // causal attention
    if cpi(f,0,1,0)==cpc(f,5) {cpi(f,1,402,cpi(f,0,202,0));}
    else {cpi(f,1,402,if cpi(f,0,1,0)==cpc(f,6) {1} else {0});}

    // flash attention and fused defaults
    cpi(f,1,403,if cpi(f,0,2,0)!=cpc(f,7) {1} else {0});
    cpi(f,1,404,if cpi(f,0,2,0)==cpc(f,8) {1} else {0});
    cpi(f,1,405,1); cpi(f,1,406,1); cpi(f,1,407,0);
    cpi(f,1,408,1); cpi(f,1,409,0);
    cpi(f,1,410,1); cpi(f,1,411,1); cpi(f,1,412,1); cpi(f,1,413,1);

    // batch sizes
    let n_ctx=cpi(f,0,418,0);
    let n_batch=if cpi(f,0,402,0)!=0 {if n_ctx<cpi(f,0,3,0) {n_ctx} else {cpi(f,0,3,0)}} else {cpi(f,0,3,0)};
    cpi(f,1,414,n_batch);
    let req=cpi(f,0,4,0);
    cpi(f,1,415,if n_batch<req {n_batch} else {req});
    let n_out=cpi(f,0,5,0);
    cpi(f,1,416,if n_out==0||cpmodel(f)!=0 {n_batch} else {n_out});
    let per=cpi(f,0,6,0);
    cpi(f,1,417,if per==0 {cpi(f,0,416,0)} else {if per<cpi(f,0,416,0) {per} else {cpi(f,0,416,0)}});
    return 0;
}

// strong exports
fn goose_resolve_fused(f:i64,mctx:i64,n_seqs:i64)->i64 {return resolve_fused(f,mctx,n_seqs);}
fn goose_graph_cb_policy(f:i64,n_tokens:i64,il:i64,is_norm:i64,is_last:i64,full_offload:i64)->i64 {
    return graph_cb_policy(f,n_tokens,il,is_norm,is_last,full_offload);
}
fn goose_realloc_policy(f:i64,ids_changed:i64,size_changed:i64,nodes:i64,splits:i64)->i64 {
    return realloc_policy(f,ids_changed,size_changed,nodes,splits);
}
fn goose_cparams_policy(f:i64)->i64 {return cparams_policy(f);}

fn main() {
    let a=goose_resolve_fused(0,0,0);
    let b=goose_graph_cb_policy(0,0,0,0,0,0);
    let c=goose_realloc_policy(0,0,0,0,0);
    let d=goose_cparams_policy(0);
    print(a+b+c+d);
}
