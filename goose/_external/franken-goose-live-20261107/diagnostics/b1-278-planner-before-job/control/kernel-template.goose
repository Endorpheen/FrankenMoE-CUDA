extern "fg143_ctl_meta" fn meta(context:i64,node:i64,field:i64)->i64;
extern "fg143_ctl_extra_work" fn extra_work(context:i64,node:i64,threads:i64)->i64;
extern "fg143_ctl_tiled_work" fn tiled_work(context:i64,node:i64,tasks:i64)->i64;
extern "fg143_ctl_goose_work" fn goose_work(context:i64,node:i64,tasks:i64)->i64;
extern "fg143_ctl_extra_compute" fn extra_compute(context:i64)->bool;
extern "fg143_ctl_witness" fn witness(kind:i64);
fn add(a:i64,b:i64)->i64 {assert(a>=0&&b>=0&&a<=9223372036854775807-b);return a+b;}
fn mul(a:i64,b:i64)->i64 {assert(a>=0&&b>=0);if a==0||b==0 {return 0;}assert(a<=9223372036854775807/b);return a*b;}
fn maximum(a:i64,b:i64)->i64 {return if a>b {a}else {b};}
fn minimum(a:i64,b:i64)->i64 {return if a<b {a}else {b};}
fn align_size(a:i64,alignment:i64)->i64 {assert(alignment>0);return mul(add(a,alignment-1)/alignment,alignment);}
fn shape(c:i64,n:i64,s:i64,axis:i64)->i64 {assert(s>=0&&s<3&&axis>=0&&axis<4);return meta(c,n,20+4*s+axis);}
fn elements(c:i64,n:i64,s:i64)->i64 {var total:i64=1;for axis in 4 {total=mul(total,shape(c,n,s,axis));}return total;}
fn empty(c:i64,n:i64)->bool {for axis in 4 {if meta(c,n,10+axis)==0 {return true;}}return false;}
fn tasks(c:i64,n:i64,threads:i64)->i64 {
    assert(threads>0);if empty(c,n) {return 1;}
    let op=meta(c,n,0);let sub=meta(c,n,8);
    if op==OP_UNARY {
        if sub==UNARY_OP_GELU||sub==UNARY_OP_GELU_ERF||sub==UNARY_OP_GELU_QUICK||sub==UNARY_OP_SILU||sub==UNARY_OP_XIELU {return threads;}
        assert(sub>=0&&sub<meta(c,n,60));return 1;
    }
    if op==OP_GLU {assert(sub>=0&&sub<meta(c,n,59));return threads;}
    if op==OP_SOFT_MAX {return minimum(threads,mul(mul(shape(c,n,0,1),shape(c,n,0,2)),shape(c,n,0,3)));}
    if op==OP_RWKV_WKV6||op==OP_RWKV_WKV7||op==OP_GATED_LINEAR_ATTN {return minimum(threads,shape(c,n,1,1));}
    if op==OP_MAP_CUSTOM1||op==OP_MAP_CUSTOM2||op==OP_MAP_CUSTOM3||op==OP_CUSTOM {let requested=meta(c,n,7);let count=if requested== -1 {threads}else {minimum(requested,threads)};assert(count>0);return count;}
    if op==OP_SUB||op==OP_SQR||op==OP_SQRT||op==OP_LOG||op==OP_SIN||op==OP_COS||op==OP_SUM||op==OP_SUM_ROWS||op==OP_MEAN||op==OP_ARGMAX||op==OP_REPEAT||op==OP_REPEAT_BACK||op==OP_LEAKY_RELU||op==OP_GET_ROWS||op==OP_SCALE||op==OP_SET||op==OP_RESHAPE||op==OP_VIEW||op==OP_PERMUTE||op==OP_TRANSPOSE||op==OP_GET_ROWS_BACK||op==OP_DIAG||op==OP_CLAMP||op==OP_POOL_1D||op==OP_POOL_2D||op==OP_POOL_2D_BACK||op==OP_WIN_PART||op==OP_WIN_UNPART||op==OP_GET_REL_POS||op==OP_NONE {return 1;}
    if op==OP_CPY||op==OP_DUP||op==OP_CONT||op==OP_ADD||op==OP_ADD_ID||op==OP_ADD1||op==OP_ACC||op==OP_CUMSUM||op==OP_TRI||op==OP_FILL||op==OP_COUNT_EQUAL||op==OP_SOLVE_TRI||op==OP_GATED_DELTA_NET||op==OP_DSV4_HC_COMB||op==OP_DSV4_HC_PRE||op==OP_DSV4_HC_POST||op==OP_SILU_BACK||op==OP_MUL||op==OP_DIV||op==OP_NORM||op==OP_RMS_NORM||op==OP_RMS_NORM_BACK||op==OP_L2_NORM||op==OP_GROUP_NORM||op==OP_CONCAT||op==OP_MUL_MAT||op==OP_MUL_MAT_ID||op==OP_OUT_PROD||op==OP_SET_ROWS||op==OP_DIAG_MASK_ZERO||op==OP_DIAG_MASK_INF||op==OP_SOFT_MAX_BACK||op==OP_ROPE||op==OP_ROPE_BACK||op==OP_ADD_REL_POS||op==OP_IM2COL||op==OP_IM2COL_BACK||op==OP_IM2COL_3D||op==OP_CONV_2D||op==OP_CONV_3D||op==OP_CONV_2D_DW||op==OP_COL2IM_1D||op==OP_CONV_TRANSPOSE_1D||op==OP_CONV_TRANSPOSE_2D||op==OP_UPSCALE||op==OP_PAD||op==OP_PAD_REFLECT_1D||op==OP_ROLL||op==OP_ARANGE||op==OP_TIMESTEP_EMBEDDING||op==OP_ARGSORT||op==OP_TOP_K||op==OP_FLASH_ATTN_EXT||op==OP_FLASH_ATTN_BACK||op==OP_SSM_CONV||op==OP_SSM_SCAN||op==OP_LIGHTNING_INDEXER||op==OP_CROSS_ENTROPY_LOSS||op==OP_CROSS_ENTROPY_LOSS_BACK||op==OP_OPT_STEP_ADAMW||op==OP_OPT_STEP_SGD {return threads;}
    assert(false);return 0;
}
fn row_size(c:i64,n:i64,count:i64)->i64 {let block_width=meta(c,n,42);assert(block_width>0&&count>=0&&count%block_width==0);return mul(count/block_width,meta(c,n,41));}
fn workspace(c:i64,n:i64,threads:i64,count:i64)->i64 {
    let extra=extra_work(c,n,threads);var cur:i64=0;
    if extra>=0 {cur=extra;}else {
        let op=meta(c,n,0);let kind=meta(c,n,1);let x=meta(c,n,2);let y=meta(c,n,3);let dstwidth=meta(c,n,10);let width=shape(c,n,0,0);
        if op==OP_CPY||op==OP_DUP {
            if meta(c,n,5)!=0||(x==1&&y==30)||(x==30&&y==1)||(x==0&&y==26)||(x==26&&y==0) {cur=mul(mul(4,dstwidth),count);}
        }else if op==OP_ADD||op==OP_ADD_ID||op==OP_ADD1 {
            if meta(c,n,6)!=0 {cur=mul(mul(4,width),count);}
        }else if op==OP_ACC {if meta(c,n,6)!=0 {cur=mul(mul(4,shape(c,n,1,0)),count);}}
        else if op==OP_COUNT_EQUAL {cur=mul(meta(c,n,43),count);}
        else if op==OP_MUL_MAT {
            if y!=meta(c,n,40) {cur=row_size(c,n,elements(c,n,1));}
            cur=add(align_size(cur,64),tiled_work(c,n,count));
        }else if op==OP_MUL_MAT_ID {
            if y!=meta(c,n,40) {cur=add(row_size(c,n,elements(c,n,1)),8);}
            let experts=shape(c,n,0,2);cur=add(cur,add(mul(experts,8),8));
            cur=add(cur,add(mul(mul(mul(experts,shape(c,n,2,0)),shape(c,n,2,1)),meta(c,n,54)),8));
            cur=add(cur,mul(meta(c,n,53),add(experts,1)));cur=add(align_size(cur,64),tiled_work(c,n,count));
        }else if op==OP_OUT_PROD {if meta(c,n,6)!=0||x==1 {cur=mul(mul(4,width),count);}}
        else if op==OP_SET_ROWS {if x==1&&kind!=1 {cur=mul(mul(4,width),count);}}
        else if op==OP_SOFT_MAX||op==OP_ROPE||op==OP_ROPE_BACK {cur=mul(mul(4,dstwidth),count);}
        else if op==OP_CONV_TRANSPOSE_1D {
            assert(shape(c,n,0,3)==1&&shape(c,n,1,2)==1&&shape(c,n,1,3)==1);
            assert((x==1||x==30||x==0)&&y==0);let bytes=if x==0 {4}else {2};
            cur=mul(bytes,add(mul(mul(width,shape(c,n,0,1)),shape(c,n,0,2)),mul(shape(c,n,1,0),shape(c,n,1,1))));
        }else if op==OP_CONV_2D||op==OP_CONV_3D {cur=meta(c,n,58);}
        else if op==OP_CONV_TRANSPOSE_2D {assert((x==0||x==1)&&y==0);cur=mul(meta(c,n,44),add(elements(c,n,0),elements(c,n,1)));}
        else if op==OP_TOP_K {cur=mul(mul(4,width),count);}
        else if op==OP_FLASH_ATTN_EXT {
            let heads=shape(c,n,0,2);let dk=shape(c,n,1,0);let dv=shape(c,n,2,0);let tq=meta(c,n,55);let tk=meta(c,n,56);
            let prefill=mul(mul(4,count),add(add(mul(tq,dk),mul(mul(2,tq),tk)),add(mul(tq,dv),add(mul(tk,dv),mul(tk,dk)))));
            let decode=mul(4,add(mul(mul(heads,count),add(2,dv)),mul(count,add(dk,mul(2,dv)))));cur=maximum(prefill,decode);
        }else if op==OP_FLASH_ATTN_BACK {
            if y==0||y==1||y==30 {let dimension=maximum(width,align_size(shape(c,n,1,1),meta(c,n,57)));cur=mul(mul(mul(16,dimension),count),1);}
        }else if op==OP_CROSS_ENTROPY_LOSS {cur=mul(meta(c,n,43),add(count,mul(width,count)));}
        else if op==OP_GATED_DELTA_NET {let width_v=shape(c,n,2,0);let per=add(width_v,if sub_snapshot(c,n)>1 {mul(width_v,width_v)}else {0});cur=mul(mul(per,4),count);}
        else if op==OP_LIGHTNING_INDEXER {cur=mul(mul(4,shape(c,n,1,0)),count);}
        else {assert(op!=OP_COUNT);}
    }
    return maximum(cur,goose_work(c,n,count));
}
fn sub_snapshot(c:i64,n:i64)->i64 {return meta(c,n,8);}
fn plan(c:i64,count:i64,requested:i64,out:i64[:]) {
    assert(count>=0&&out.len>=2);var threads=requested;
    if threads<=0 {let pool=meta(c,-1,50);threads=if pool>0 {pool}else {meta(c,-1,51)};}
    if meta(c,-1,52)!=0 {threads=1;}assert(threads>0);
    var max_tasks:i64=1;var work:i64=0;
    for node in count {let nt=tasks(c,node,threads);assert(nt>0);max_tasks=maximum(max_tasks,nt);work=maximum(work,workspace(c,node,threads,nt));}
    if work>0 {work=add(work,mul(meta(c,-1,53),threads));}
    out[0]=work;out[1]=minimum(max_tasks,threads);witness(0);
}
extern "fg143_ctl_leaf_dup" fn leaf_dup(context:i64);
extern "fg143_ctl_leaf_add" fn leaf_add(context:i64);
extern "fg143_ctl_leaf_add_id" fn leaf_add_id(context:i64);
extern "fg143_ctl_leaf_add1" fn leaf_add1(context:i64);
extern "fg143_ctl_leaf_acc" fn leaf_acc(context:i64);
extern "fg143_ctl_leaf_sub" fn leaf_sub(context:i64);
extern "fg143_ctl_leaf_mul" fn leaf_mul(context:i64);
extern "fg143_ctl_leaf_div" fn leaf_div(context:i64);
extern "fg143_ctl_leaf_sqr" fn leaf_sqr(context:i64);
extern "fg143_ctl_leaf_sqrt" fn leaf_sqrt(context:i64);
extern "fg143_ctl_leaf_log" fn leaf_log(context:i64);
extern "fg143_ctl_leaf_sin" fn leaf_sin(context:i64);
extern "fg143_ctl_leaf_cos" fn leaf_cos(context:i64);
extern "fg143_ctl_leaf_sum" fn leaf_sum(context:i64);
extern "fg143_ctl_leaf_sum_rows" fn leaf_sum_rows(context:i64);
extern "fg143_ctl_leaf_cumsum" fn leaf_cumsum(context:i64);
extern "fg143_ctl_leaf_mean" fn leaf_mean(context:i64);
extern "fg143_ctl_leaf_argmax" fn leaf_argmax(context:i64);
extern "fg143_ctl_leaf_count_equal" fn leaf_count_equal(context:i64);
extern "fg143_ctl_leaf_repeat" fn leaf_repeat(context:i64);
extern "fg143_ctl_leaf_repeat_back" fn leaf_repeat_back(context:i64);
extern "fg143_ctl_leaf_concat" fn leaf_concat(context:i64);
extern "fg143_ctl_leaf_silu_back" fn leaf_silu_back(context:i64);
extern "fg143_ctl_leaf_norm" fn leaf_norm(context:i64);
extern "fg143_ctl_leaf_rms_norm" fn leaf_rms_norm(context:i64);
extern "fg143_ctl_leaf_rms_norm_back" fn leaf_rms_norm_back(context:i64);
extern "fg143_ctl_leaf_group_norm" fn leaf_group_norm(context:i64);
extern "fg143_ctl_leaf_l2_norm" fn leaf_l2_norm(context:i64);
extern "fg143_ctl_leaf_mul_mat" fn leaf_mul_mat(context:i64);
extern "fg143_ctl_leaf_mul_mat_id" fn leaf_mul_mat_id(context:i64);
extern "fg143_ctl_leaf_out_prod" fn leaf_out_prod(context:i64);
extern "fg143_ctl_leaf_scale" fn leaf_scale(context:i64);
extern "fg143_ctl_leaf_set" fn leaf_set(context:i64);
extern "fg143_ctl_leaf_cpy" fn leaf_cpy(context:i64);
extern "fg143_ctl_leaf_cont" fn leaf_cont(context:i64);
extern "fg143_ctl_leaf_get_rows" fn leaf_get_rows(context:i64);
extern "fg143_ctl_leaf_get_rows_back" fn leaf_get_rows_back(context:i64);
extern "fg143_ctl_leaf_set_rows" fn leaf_set_rows(context:i64);
extern "fg143_ctl_leaf_diag" fn leaf_diag(context:i64);
extern "fg143_ctl_leaf_diag_mask_inf" fn leaf_diag_mask_inf(context:i64);
extern "fg143_ctl_leaf_diag_mask_zero" fn leaf_diag_mask_zero(context:i64);
extern "fg143_ctl_leaf_soft_max" fn leaf_soft_max(context:i64);
extern "fg143_ctl_leaf_soft_max_back" fn leaf_soft_max_back(context:i64);
extern "fg143_ctl_leaf_rope" fn leaf_rope(context:i64);
extern "fg143_ctl_leaf_rope_back" fn leaf_rope_back(context:i64);
extern "fg143_ctl_leaf_clamp" fn leaf_clamp(context:i64);
extern "fg143_ctl_leaf_conv_transpose_1d" fn leaf_conv_transpose_1d(context:i64);
extern "fg143_ctl_leaf_im2col" fn leaf_im2col(context:i64);
extern "fg143_ctl_leaf_im2col_back" fn leaf_im2col_back(context:i64);
extern "fg143_ctl_leaf_im2col_3d" fn leaf_im2col_3d(context:i64);
extern "fg143_ctl_leaf_col2im_1d" fn leaf_col2im_1d(context:i64);
extern "fg143_ctl_leaf_conv_2d" fn leaf_conv_2d(context:i64);
extern "fg143_ctl_leaf_conv_3d" fn leaf_conv_3d(context:i64);
extern "fg143_ctl_leaf_conv_2d_dw" fn leaf_conv_2d_dw(context:i64);
extern "fg143_ctl_leaf_conv_transpose_2d" fn leaf_conv_transpose_2d(context:i64);
extern "fg143_ctl_leaf_pool_1d" fn leaf_pool_1d(context:i64);
extern "fg143_ctl_leaf_pool_2d" fn leaf_pool_2d(context:i64);
extern "fg143_ctl_leaf_pool_2d_back" fn leaf_pool_2d_back(context:i64);
extern "fg143_ctl_leaf_upscale" fn leaf_upscale(context:i64);
extern "fg143_ctl_leaf_pad" fn leaf_pad(context:i64);
extern "fg143_ctl_leaf_pad_reflect_1d" fn leaf_pad_reflect_1d(context:i64);
extern "fg143_ctl_leaf_roll" fn leaf_roll(context:i64);
extern "fg143_ctl_leaf_arange" fn leaf_arange(context:i64);
extern "fg143_ctl_leaf_timestep_embedding" fn leaf_timestep_embedding(context:i64);
extern "fg143_ctl_leaf_argsort" fn leaf_argsort(context:i64);
extern "fg143_ctl_leaf_top_k" fn leaf_top_k(context:i64);
extern "fg143_ctl_leaf_leaky_relu" fn leaf_leaky_relu(context:i64);
extern "fg143_ctl_leaf_tri" fn leaf_tri(context:i64);
extern "fg143_ctl_leaf_fill" fn leaf_fill(context:i64);
extern "fg143_ctl_leaf_flash_attn_ext" fn leaf_flash_attn_ext(context:i64);
extern "fg143_ctl_leaf_flash_attn_back" fn leaf_flash_attn_back(context:i64,masked:i64);
extern "fg143_ctl_leaf_ssm_conv" fn leaf_ssm_conv(context:i64);
extern "fg143_ctl_leaf_ssm_scan" fn leaf_ssm_scan(context:i64);
extern "fg143_ctl_leaf_win_part" fn leaf_win_part(context:i64);
extern "fg143_ctl_leaf_win_unpart" fn leaf_win_unpart(context:i64);
extern "fg143_ctl_leaf_unary" fn leaf_unary(context:i64);
extern "fg143_ctl_leaf_glu" fn leaf_glu(context:i64);
extern "fg143_ctl_leaf_get_rel_pos" fn leaf_get_rel_pos(context:i64);
extern "fg143_ctl_leaf_add_rel_pos" fn leaf_add_rel_pos(context:i64);
extern "fg143_ctl_leaf_rwkv_wkv6" fn leaf_rwkv_wkv6(context:i64);
extern "fg143_ctl_leaf_gated_linear_attn" fn leaf_gated_linear_attn(context:i64);
extern "fg143_ctl_leaf_rwkv_wkv7" fn leaf_rwkv_wkv7(context:i64);
extern "fg143_ctl_leaf_solve_tri" fn leaf_solve_tri(context:i64);
extern "fg143_ctl_leaf_gated_delta_net" fn leaf_gated_delta_net(context:i64);
extern "fg143_ctl_leaf_lightning_indexer" fn leaf_lightning_indexer(context:i64);
extern "fg143_ctl_leaf_dsv4_hc_comb" fn leaf_dsv4_hc_comb(context:i64);
extern "fg143_ctl_leaf_dsv4_hc_pre" fn leaf_dsv4_hc_pre(context:i64);
extern "fg143_ctl_leaf_dsv4_hc_post" fn leaf_dsv4_hc_post(context:i64);
extern "fg143_ctl_leaf_map_custom1" fn leaf_map_custom1(context:i64);
extern "fg143_ctl_leaf_map_custom2" fn leaf_map_custom2(context:i64);
extern "fg143_ctl_leaf_map_custom3" fn leaf_map_custom3(context:i64);
extern "fg143_ctl_leaf_custom" fn leaf_custom(context:i64);
extern "fg143_ctl_leaf_cross_entropy_loss" fn leaf_cross_entropy_loss(context:i64);
extern "fg143_ctl_leaf_cross_entropy_loss_back" fn leaf_cross_entropy_loss_back(context:i64);
extern "fg143_ctl_leaf_opt_step_adamw" fn leaf_opt_step_adamw(context:i64);
extern "fg143_ctl_leaf_opt_step_sgd" fn leaf_opt_step_sgd(context:i64);
fn dispatch(c:i64) {
    let op=meta(c,0,0);if op==OP_NONE||empty(c,0) {return;}
    if extra_compute(c) {return;}
    if op==OP_DUP {leaf_dup(c);}
    else if op==OP_ADD {leaf_add(c);}
    else if op==OP_ADD_ID {leaf_add_id(c);}
    else if op==OP_ADD1 {leaf_add1(c);}
    else if op==OP_ACC {leaf_acc(c);}
    else if op==OP_SUB {leaf_sub(c);}
    else if op==OP_MUL {leaf_mul(c);}
    else if op==OP_DIV {leaf_div(c);}
    else if op==OP_SQR {leaf_sqr(c);}
    else if op==OP_SQRT {leaf_sqrt(c);}
    else if op==OP_LOG {leaf_log(c);}
    else if op==OP_SIN {leaf_sin(c);}
    else if op==OP_COS {leaf_cos(c);}
    else if op==OP_SUM {leaf_sum(c);}
    else if op==OP_SUM_ROWS {leaf_sum_rows(c);}
    else if op==OP_CUMSUM {leaf_cumsum(c);}
    else if op==OP_MEAN {leaf_mean(c);}
    else if op==OP_ARGMAX {leaf_argmax(c);}
    else if op==OP_COUNT_EQUAL {leaf_count_equal(c);}
    else if op==OP_REPEAT {leaf_repeat(c);}
    else if op==OP_REPEAT_BACK {leaf_repeat_back(c);}
    else if op==OP_CONCAT {leaf_concat(c);}
    else if op==OP_SILU_BACK {leaf_silu_back(c);}
    else if op==OP_NORM {leaf_norm(c);}
    else if op==OP_RMS_NORM {leaf_rms_norm(c);}
    else if op==OP_RMS_NORM_BACK {leaf_rms_norm_back(c);}
    else if op==OP_GROUP_NORM {leaf_group_norm(c);}
    else if op==OP_L2_NORM {leaf_l2_norm(c);}
    else if op==OP_MUL_MAT {leaf_mul_mat(c);}
    else if op==OP_MUL_MAT_ID {leaf_mul_mat_id(c);}
    else if op==OP_OUT_PROD {leaf_out_prod(c);}
    else if op==OP_SCALE {leaf_scale(c);}
    else if op==OP_SET {leaf_set(c);}
    else if op==OP_CPY {leaf_cpy(c);}
    else if op==OP_CONT {leaf_cont(c);}
    else if op==OP_GET_ROWS {leaf_get_rows(c);}
    else if op==OP_GET_ROWS_BACK {leaf_get_rows_back(c);}
    else if op==OP_SET_ROWS {leaf_set_rows(c);}
    else if op==OP_DIAG {leaf_diag(c);}
    else if op==OP_DIAG_MASK_INF {leaf_diag_mask_inf(c);}
    else if op==OP_DIAG_MASK_ZERO {leaf_diag_mask_zero(c);}
    else if op==OP_SOFT_MAX {leaf_soft_max(c);}
    else if op==OP_SOFT_MAX_BACK {leaf_soft_max_back(c);}
    else if op==OP_ROPE {leaf_rope(c);}
    else if op==OP_ROPE_BACK {leaf_rope_back(c);}
    else if op==OP_CLAMP {leaf_clamp(c);}
    else if op==OP_CONV_TRANSPOSE_1D {leaf_conv_transpose_1d(c);}
    else if op==OP_IM2COL {leaf_im2col(c);}
    else if op==OP_IM2COL_BACK {leaf_im2col_back(c);}
    else if op==OP_IM2COL_3D {leaf_im2col_3d(c);}
    else if op==OP_COL2IM_1D {leaf_col2im_1d(c);}
    else if op==OP_CONV_2D {leaf_conv_2d(c);}
    else if op==OP_CONV_3D {leaf_conv_3d(c);}
    else if op==OP_CONV_2D_DW {leaf_conv_2d_dw(c);}
    else if op==OP_CONV_TRANSPOSE_2D {leaf_conv_transpose_2d(c);}
    else if op==OP_POOL_1D {leaf_pool_1d(c);}
    else if op==OP_POOL_2D {leaf_pool_2d(c);}
    else if op==OP_POOL_2D_BACK {leaf_pool_2d_back(c);}
    else if op==OP_UPSCALE {leaf_upscale(c);}
    else if op==OP_PAD {leaf_pad(c);}
    else if op==OP_PAD_REFLECT_1D {leaf_pad_reflect_1d(c);}
    else if op==OP_ROLL {leaf_roll(c);}
    else if op==OP_ARANGE {leaf_arange(c);}
    else if op==OP_TIMESTEP_EMBEDDING {leaf_timestep_embedding(c);}
    else if op==OP_ARGSORT {leaf_argsort(c);}
    else if op==OP_TOP_K {leaf_top_k(c);}
    else if op==OP_LEAKY_RELU {leaf_leaky_relu(c);}
    else if op==OP_TRI {leaf_tri(c);}
    else if op==OP_FILL {leaf_fill(c);}
    else if op==OP_FLASH_ATTN_EXT {leaf_flash_attn_ext(c);}
    else if op==OP_FLASH_ATTN_BACK {let mask=meta(c,0,8);assert(mask==0||mask==1);leaf_flash_attn_back(c,mask);}
    else if op==OP_SSM_CONV {leaf_ssm_conv(c);}
    else if op==OP_SSM_SCAN {leaf_ssm_scan(c);}
    else if op==OP_WIN_PART {leaf_win_part(c);}
    else if op==OP_WIN_UNPART {leaf_win_unpart(c);}
    else if op==OP_UNARY {leaf_unary(c);}
    else if op==OP_GLU {leaf_glu(c);}
    else if op==OP_GET_REL_POS {leaf_get_rel_pos(c);}
    else if op==OP_ADD_REL_POS {leaf_add_rel_pos(c);}
    else if op==OP_RWKV_WKV6 {leaf_rwkv_wkv6(c);}
    else if op==OP_GATED_LINEAR_ATTN {leaf_gated_linear_attn(c);}
    else if op==OP_RWKV_WKV7 {leaf_rwkv_wkv7(c);}
    else if op==OP_SOLVE_TRI {leaf_solve_tri(c);}
    else if op==OP_GATED_DELTA_NET {leaf_gated_delta_net(c);}
    else if op==OP_LIGHTNING_INDEXER {leaf_lightning_indexer(c);}
    else if op==OP_DSV4_HC_COMB {leaf_dsv4_hc_comb(c);}
    else if op==OP_DSV4_HC_PRE {leaf_dsv4_hc_pre(c);}
    else if op==OP_DSV4_HC_POST {leaf_dsv4_hc_post(c);}
    else if op==OP_MAP_CUSTOM1 {leaf_map_custom1(c);}
    else if op==OP_MAP_CUSTOM2 {leaf_map_custom2(c);}
    else if op==OP_MAP_CUSTOM3 {leaf_map_custom3(c);}
    else if op==OP_CUSTOM {leaf_custom(c);}
    else if op==OP_CROSS_ENTROPY_LOSS {leaf_cross_entropy_loss(c);}
    else if op==OP_CROSS_ENTROPY_LOSS_BACK {leaf_cross_entropy_loss_back(c);}
    else if op==OP_OPT_STEP_ADAMW {leaf_opt_step_adamw(c);}
    else if op==OP_OPT_STEP_SGD {leaf_opt_step_sgd(c);}
    else if op==OP_RESHAPE||op==OP_VIEW||op==OP_TRANSPOSE||op==OP_PERMUTE {}
    else {assert(false);}
    witness(2);
}
fn entry(mode:i64,c:i64,count:i64,requested:i64,out:i64[:]) {
    if mode==0 {plan(c,count,requested,out);}
    else if mode==1 {assert(out.len>=1);out[0]=tasks(c,0,requested);witness(1);}
    else {assert(mode==2);dispatch(c);}
}
fn main(){var out:i64[2]=default<i64[2]>();entry(0,0,0,1,out);}
