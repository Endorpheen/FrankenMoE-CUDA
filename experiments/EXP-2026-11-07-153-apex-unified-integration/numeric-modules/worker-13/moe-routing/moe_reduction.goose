// moe_reduction.goose - Apex weighted expert-output reduction.
// Port of ggml-cuda/moe-weighted-reduction.cu moe_weighted_reduction_f32.
// One entry is one output column of one token.  The expert loop is strictly
// ascending, the first term is formed first, and expert_scale stays optional,
// so float ordering, NaN propagation and tie-free summation match native.
//
// Config indices are defined in fg153-moe-abi.h.
extern "fg153_mr_cfg_i64" fn ci(c:i64,k:i64)->i64;
extern "fg153_mr_load" fn ld(p:i64,i:i64)->f32;
extern "fg153_mr_store" fn st(p:i64,i:i64,v:f32);
extern "fg153_mr_tid" fn tid()->i64;
extern "fg153_mr_bdim" fn bdim()->i64;
extern "fg153_mr_bidx" fn bid(a:i64)->i64;
extern "fg153_mr_reject" fn reject(c:i64,code:i64)->i64;

fn bad(c:i64,code:i64)->i64 {
    let ignored=reject(c,code);
    return -1;
}
fn entry(c:i64)->i64 {
    let experts=ci(c,0);
    let scale=ci(c,1);
    let weights=ci(c,2);
    let dst=ci(c,3);
    let n_embd=ci(c,4);
    let n_tok=ci(c,5);
    let n_used=ci(c,6);
    if experts==0||weights==0||dst==0 {return bad(c,1);}
    if n_embd<=0||n_tok<0||n_used<1 {return bad(c,2);}
    let token=bid(0);
    let col=bid(1)*bdim()+tid();
    if token<0||token>=n_tok {return bad(c,3);}
    if col<0 {return bad(c,4);}
    if col>=n_embd {return 0;}
    let base=token*n_used;
    let first_scale=if scale!=0 {ld(scale,base)} else {1 as f32};
    var sum:f32=(ld(experts,base*n_embd+col)*first_scale)*ld(weights,base);
    var e:i64=1;
    while e<n_used {
        let row=base+e;
        let sc=if scale!=0 {ld(scale,row)} else {1 as f32};
        sum=sum+(ld(experts,row*n_embd+col)*sc)*ld(weights,row);
        e=e+1;
    }
    st(dst,token*n_embd+col,sum);
    return 0;
}
fn main(){let r=entry(0);print(r);}
