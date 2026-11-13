extern "fg153_am_get" fn get(e:i64,k:i64,i:i64)->i64;
extern "fg153_am_fget" fn fget(e:i64,k:i64,i:i64)->f32;
extern "fg153_am_wt" fn wt(e:i64,il:i64,k:i64)->i64;
extern "fg153_am_put" fn put(e:i64,k:i64,i:i64,v:i64);
extern "fg153_am_service" fn service(e:i64,k:i64,a:i64,b:i64,c:i64)->i64;
extern "fg153_am_observe" fn observe(e:i64,t:i64,k:i64,il:i64);
extern "fg153_am_fused" fn fused(e:i64,k:i64,t:i64,il:i64);
extern "fg153_am_ne" fn ne(t:i64,i:i64)->i64;
extern "fg153_am_nb" fn nb(t:i64,i:i64)->i64;
extern "fg153_am_src" fn src(t:i64,i:i64)->i64;
extern "fg153_am_typ" fn typ(t:i64)->i64;
extern "fg153_am_elem" fn elem(t:i64)->i64;
extern "fg153_am_row" fn row(t:i64,n:i64)->i64;
extern "fg153_am_adapter_scale" fn adapter_scale(e:i64,w:i64,i:i64,id:i64)->f32;
extern "fg153_am_layer_float" fn layer_float(e:i64,k:i64,il:i64)->f32;
extern "fg153_am_layer_int" fn layer_int(e:i64,k:i64,il:i64)->i64;
extern "fg153_am_putin" fn putin(e:i64,inp:i64,k:i64,t:i64);
extern "fg153_am_select_input" fn select_input(e:i64,a:i64,b:i64,c:i64,n:i64,idx:i64)->i64;
extern "fg153_am_input_scale" fn input_scale(e:i64,inp:i64,s:f32);
extern "fg153_am_fsqrt" fn fsqrt(n:f32)->f32;
extern "fg153_am_infinity" fn infinity()->f32;
extern "fg153_am_p_add" fn p_add(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_argsort_top_k" fn p_argsort_top_k(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_build_forward_expand" fn p_build_forward_expand(a0:i64,a1:i64);
extern "fg153_am_p_build_forward_order" fn p_build_forward_order(a0:i64,a1:i64);
extern "fg153_am_p_cast" fn p_cast(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_clamp" fn p_clamp(a0:i64,a1:i64,a2:f32,a3:f32)->i64;
extern "fg153_am_p_concat" fn p_concat(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_cont" fn p_cont(a0:i64,a1:i64)->i64;
extern "fg153_am_p_cont_2d" fn p_cont_2d(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_cont_4d" fn p_cont_4d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64)->i64;
extern "fg153_am_p_cpy" fn p_cpy(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_cumsum" fn p_cumsum(a0:i64,a1:i64)->i64;
extern "fg153_am_p_diag" fn p_diag(a0:i64,a1:i64)->i64;
extern "fg153_am_p_div" fn p_div(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_dup" fn p_dup(a0:i64,a1:i64)->i64;
extern "fg153_am_p_exp" fn p_exp(a0:i64,a1:i64)->i64;
extern "fg153_am_p_fill" fn p_fill(a0:i64,a1:i64,a2:f32)->i64;
extern "fg153_am_p_flash_attn_ext" fn p_flash_attn_ext(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:f32,a6:f32,a7:f32)->i64;
extern "fg153_am_p_flash_attn_ext_add_sinks" fn p_flash_attn_ext_add_sinks(a0:i64,a1:i64);
extern "fg153_am_p_flash_attn_ext_set_n_kv_max" fn p_flash_attn_ext_set_n_kv_max(a0:i64,a1:i64);
extern "fg153_am_p_gated_delta_net" fn p_gated_delta_net(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64,a6:i64,a7:i64)->i64;
extern "fg153_am_p_get_rows" fn p_get_rows(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_is_contiguous" fn p_is_contiguous(a0:i64)->bool;
extern "fg153_am_p_mul" fn p_mul(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_mul_mat" fn p_mul_mat(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_mul_mat_id" fn p_mul_mat_id(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_neg" fn p_neg(a0:i64,a1:i64)->i64;
extern "fg153_am_p_nelements" fn p_nelements(a0:i64)->i64;
extern "fg153_am_p_new_tensor_1d" fn p_new_tensor_1d(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_new_tensor_2d" fn p_new_tensor_2d(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_pad" fn p_pad(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64)->i64;
extern "fg153_am_p_permute" fn p_permute(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64)->i64;
extern "fg153_am_p_prec_set_acc" fn p_prec_set_acc(a0:i64,a1:i64)->bool;
extern "fg153_am_p_repeat" fn p_repeat(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_repeat_4d" fn p_repeat_4d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64)->i64;
extern "fg153_am_p_reshape_1d" fn p_reshape_1d(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_reshape_2d" fn p_reshape_2d(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_reshape_3d" fn p_reshape_3d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64)->i64;
extern "fg153_am_p_reshape_4d" fn p_reshape_4d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64)->i64;
extern "fg153_am_p_rms_norm" fn p_rms_norm(a0:i64,a1:i64,a2:f32)->i64;
extern "fg153_am_p_rope_multi" fn p_rope_multi(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64,a6:i64,a7:i64,a8:f32,a9:f32,a10:f32,a11:f32,a12:f32,a13:f32)->i64;
extern "fg153_am_p_scale" fn p_scale(a0:i64,a1:i64,a2:f32)->i64;
extern "fg153_am_p_scale_inplace" fn p_scale_inplace(a0:i64,a1:i64,a2:f32)->i64;
extern "fg153_am_p_set_inplace" fn p_set_inplace(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64,a6:i64)->i64;
extern "fg153_am_p_set_input" fn p_set_input(a0:i64);
extern "fg153_am_p_set_rows" fn p_set_rows(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_sigmoid" fn p_sigmoid(a0:i64,a1:i64)->i64;
extern "fg153_am_p_silu" fn p_silu(a0:i64,a1:i64)->i64;
extern "fg153_am_p_soft_max" fn p_soft_max(a0:i64,a1:i64)->i64;
extern "fg153_am_p_soft_max_add_sinks" fn p_soft_max_add_sinks(a0:i64,a1:i64);
extern "fg153_am_p_soft_max_ext" fn p_soft_max_ext(a0:i64,a1:i64,a2:i64,a3:f32,a4:f32)->i64;
extern "fg153_am_p_softplus" fn p_softplus(a0:i64,a1:i64)->i64;
extern "fg153_am_p_solve_tri" fn p_solve_tri(a0:i64,a1:i64,a2:i64,a3:bool,a4:bool,a5:bool)->i64;
extern "fg153_am_p_ssm_conv" fn p_ssm_conv(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_sub" fn p_sub(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_sum_rows" fn p_sum_rows(a0:i64,a1:i64)->i64;
extern "fg153_am_p_swiglu_split" fn p_swiglu_split(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_tanh" fn p_tanh(a0:i64,a1:i64)->i64;
extern "fg153_am_p_transpose" fn p_transpose(a0:i64,a1:i64)->i64;
extern "fg153_am_p_tri" fn p_tri(a0:i64,a1:i64,a2:i64)->i64;
extern "fg153_am_p_view_1d" fn p_view_1d(a0:i64,a1:i64,a2:i64,a3:i64)->i64;
extern "fg153_am_p_view_2d" fn p_view_2d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64)->i64;
extern "fg153_am_p_view_3d" fn p_view_3d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64,a6:i64,a7:i64)->i64;
extern "fg153_am_p_view_4d" fn p_view_4d(a0:i64,a1:i64,a2:i64,a3:i64,a4:i64,a5:i64,a6:i64,a7:i64,a8:i64,a9:i64)->i64;
fn build_main(e_arg:i64)->i64 {var e:i64=e_arg;

    var n_embd_head:i64= get(e,1,0);

    assert(n_embd_head == get(e,2,0));

    let sections:i64=0;
    

    var cur:i64=0;
    var inpL:i64=0;

    inpL = input_embed(e,wt(e,-1,0));

    observe(e,inpL,0, -1);

    var inp:i64=service(e,0,0,0,0);

    var inp_pos:i64= position_input(e);
    var inp_out_ids:i64= outid_input(e);

    
    for il in get(e,6,0) {
        put(e,0,il,inpL);

        var inpSA:i64= inpL;

        cur = build_norm(e,inpL, wt(e,il,1), 0, 1, il);
        observe(e,cur,1, il);

        p_build_forward_expand(e, cur);

        
        if (get(e,5,il)!=0) {
            
            cur = build_layer_attn_linear(e,service(e,3,inp,0,0), cur, il);
        } else {
            
            cur = build_layer_attn(e,service(e,4,inp,0,0), cur, inp_pos, sections, il);
        }

        if (il == get(e,6,0) - 1 && crop_before(e,inp_out_ids)) {
            cur   = p_get_rows(e, cur, inp_out_ids);
            inpSA = p_get_rows(e, inpSA, inp_out_ids);
        }

        
        cur = p_add(e, cur, inpSA);
        observe(e,cur,2, il);

        
        var ffn_residual:i64= cur;

        
        var attn_post_norm:i64= build_norm(e,cur, wt(e,il,2), 0, 1, il);
        observe(e,attn_post_norm,3, il);

        
        cur = build_layer_ffn(e,attn_post_norm, il);
        observe(e,cur,4, il);

        
        cur = p_add(e, cur, ffn_residual);
        observe(e,cur,5, il);

        cur = service(e,10,cur,il,0);
        observe(e,cur,6, il);

        
        inpL = cur;
    }
    cur = inpL;

    
    cur = build_norm(e,cur, wt(e,-1,3), 0, 1, -1);

    observe(e,cur,7, -1);
    put(e,1,0,cur);

    if (crop_after(e,inp_out_ids)) {
        cur = p_get_rows(e, cur, inp_out_ids);
    }

    observe(e,cur,8, -1);
    put(e,2,0,cur);

    
    cur = build_lora_mm(e,wt(e,-1,5), cur, wt(e,-1,4));

    observe(e,cur,9, -1);
    put(e,3,0,cur);

return cur;
}

fn build_qkvz(e_arg:i64,input_arg:i64,il_arg:i64)->Pair {var e:i64=e_arg;var input:i64=input_arg;var il:i64=il_arg;

    var n_seqs:i64= get(e,7,0);
    var n_seq_tokens:i64= get(e,8,0);

    var qkv_mixed:i64= build_lora_mm(e,wt(e,il,7), input, wt(e,il,8));
    qkv_mixed = p_reshape_3d(e, qkv_mixed, ne(qkv_mixed,0), n_seq_tokens, n_seqs);
    observe(e,qkv_mixed,10, il);

    var z:i64= build_lora_mm(e,wt(e,il,9), input, wt(e,il,10));
    observe(e,z,11, il);

    return Pair { a:qkv_mixed,b:z };

}

fn build_norm_gated(e_arg:i64,input_arg:i64,weights_arg:i64,gate_arg:i64,layer_arg:i64)->i64 {var e:i64=e_arg;var input:i64=input_arg;var weights:i64=weights_arg;var gate:i64=gate_arg;var layer:i64=layer_arg;

    var normalized:i64= build_norm(e,input, weights, 0, 1, layer);
    var gated_silu:i64= p_silu(e, gate);

    return p_mul(e, normalized, gated_silu);

}

fn build_layer_attn(e_arg:i64,inp_arg:i64,cur_arg:i64,inp_pos_arg:i64,sections_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var inp:i64=inp_arg;var cur:i64=cur_arg;var inp_pos:i64=inp_pos_arg;var sections:i64=sections_arg;var il:i64=il_arg;

    var n_embd_head:i64= get(e,1,0);
    assert(n_embd_head == get(e,2,0));

    

    
    let qkv_tuple = build_qkv(e,il, cur,
            n_embd_head * 2, get(e,9,0),
            n_embd_head,     get(e,10,0),
            n_embd_head,     get(e,10,0),
            il, false);
var Qcur_full:i64=qkv_tuple.a;var Kcur:i64=qkv_tuple.b;var Vcur:i64=qkv_tuple.c;
    observe(e,Qcur_full,12, il);
    observe(e,Kcur,13, il);
    observe(e,Vcur,14, il);

    var Qcur:i64= p_view_3d(e, Qcur_full, n_embd_head, get(e,9,0), get(e,11,0),
        elem(Qcur_full) * n_embd_head * 2,
        elem(Qcur_full) * n_embd_head * 2 * get(e,9,0), 0);
    observe(e,Qcur,15, il);

    
    Qcur = build_norm(e,Qcur, wt(e,il,11), 0, 1, il);
    observe(e,Qcur,16, il);

    
    Kcur = p_reshape_3d(e, Kcur, n_embd_head, get(e,10,0), get(e,11,0));
    Kcur = build_norm(e,Kcur, wt(e,il,12), 0, 1, il);
    observe(e,Kcur,17, il);

    var gate:i64= p_view_3d(e, Qcur_full, n_embd_head, get(e,9,0), get(e,11,0),
        elem(Qcur_full) * n_embd_head * 2,
        elem(Qcur_full) * n_embd_head * 2 * get(e,9,0),
        elem(Qcur_full) * n_embd_head);
    gate = p_cont_2d(e, gate, n_embd_head * get(e,9,0), get(e,11,0));
    observe(e,gate,18, il);

    Vcur = p_reshape_3d(e, Vcur, n_embd_head, get(e,10,0), get(e,11,0));

    
    Qcur = p_rope_multi(
            e, Qcur, inp_pos, 0,
            get(e,12,0), sections, get(e,13,0), get(e,14,0), fget(e,0,0), fget(e,1,0),
            fget(e,2,0), fget(e,3,0), fget(e,4,0), fget(e,5,0)
            );

    Kcur = p_rope_multi(
            e, Kcur, inp_pos, 0,
            get(e,12,0), sections, get(e,13,0), get(e,14,0), fget(e,0,0), fget(e,1,0),
            fget(e,2,0), fget(e,3,0), fget(e,4,0), fget(e,5,0)
            );

    observe(e,Qcur,19, il);
    observe(e,Kcur,13, il);
    observe(e,Vcur,14, il);

    
    var kq_scale:f32= if fget(e,6,0)==0.0 {1.0/fsqrt(n_embd_head as f32)} else {fget(e,6,0)};

    cur = build_attn(e,inp,
                0, 0, 0,
                Qcur, Kcur, Vcur, 0, 0, 0, kq_scale, il);
    observe(e,cur,20, il);

    var gate_sigmoid:i64= p_sigmoid(e, gate);
    observe(e,gate_sigmoid,21, il);

    cur = p_mul(e, cur, gate_sigmoid);
    observe(e,cur,22, il);

    cur = build_lora_mm(e,wt(e,il,13), cur, wt(e,il,14));
    observe(e,cur,23, il);

    return cur;

}

fn build_layer_attn_linear(e_arg:i64,inp_arg:i64,cur_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var inp:i64=inp_arg;var cur:i64=cur_arg;var il:i64=il_arg;

    var mctx_cur= service(e,5,inp,0,0);

    var d_inner:i64= get(e,15,0);
    var n_seqs:i64= get(e,7,0);
    var head_k_dim:i64= get(e,16,0);
    var num_k_heads:i64= get(e,17,0);
    var num_v_heads:i64= get(e,18,0);
    var head_v_dim:i64= d_inner / num_v_heads;
    var n_seq_tokens:i64= get(e,8,0);

    assert(n_seqs != 0);
    assert(get(e,0,0)!=0);
    assert(get(e,19,0) == n_seq_tokens * n_seqs);

    
    var qkvz= build_qkvz(e,cur, il);
    var qkv_mixed:i64= qkvz.a;
    var z:i64= qkvz.b;

    var beta:i64= build_lora_mm(e,wt(e,il,15), cur, wt(e,il,16));
    beta = p_reshape_4d(e, beta, 1, num_v_heads, n_seq_tokens, n_seqs);
    observe(e,beta,24, il);

    beta = p_sigmoid(e, beta);
    observe(e,beta,25, il);

    var alpha:i64= build_lora_mm(e,wt(e,il,17), cur, wt(e,il,18));
    alpha = p_reshape_3d(e, alpha, num_v_heads, n_seq_tokens, n_seqs);
    observe(e,alpha,26, il);

    var alpha_biased:i64= p_add(e, alpha, wt(e,il,19));
    var alpha_softplus:i64= p_softplus(e, alpha_biased);
    observe(e,alpha_softplus,27, il);

    var gate:i64= p_mul(e, alpha_softplus, wt(e,il,20));  
    observe(e,gate,28, il);

    gate = p_reshape_4d(e, gate, 1, num_v_heads, n_seq_tokens, n_seqs);

    var conv_states_all:i64= service(e,8,mctx_cur,il,0);
    var ssm_states_all:i64= service(e,9,mctx_cur,il,0);

    var conv_kernel:i64= wt(e,il,21);
    var conv_kernel_size:i64= ne(conv_kernel,0);
    var conv_channels:i64= d_inner + 2 * get(e,17,0) * get(e,16,0);

    var conv_input:i64= build_conv_state(e,inp, conv_states_all, qkv_mixed, conv_kernel_size, conv_channels, il);

    var state:i64= build_rs(e,inp, ssm_states_all, get(e,3,0), n_seqs);
    state = p_reshape_4d(e, state, head_v_dim, head_v_dim, num_v_heads, n_seqs);
    observe(e,state,29, il);

    var conv_output_proper:i64= p_ssm_conv(e, conv_input, conv_kernel);
    observe(e,conv_output_proper,30, il);

    var conv_output_silu:i64= p_silu(e, conv_output_proper);
    observe(e,conv_output_silu,31, il);

    var conv_qkv_mix:i64= conv_output_silu;

    
    var qkv_dim:i64= head_k_dim * num_k_heads * 2 + head_v_dim * num_v_heads;
    var nb1_qkv:i64= row(conv_qkv_mix, qkv_dim);

    
    var q_conv:i64= p_view_4d(e, conv_qkv_mix, head_k_dim, num_k_heads, n_seq_tokens, n_seqs,
            row(conv_qkv_mix, head_k_dim),
            nb1_qkv,
            nb1_qkv * n_seq_tokens,
            0);

    var k_conv:i64= p_view_4d(e, conv_qkv_mix, head_k_dim, num_k_heads, n_seq_tokens, n_seqs,
            row(conv_qkv_mix, head_k_dim),
            nb1_qkv,
            nb1_qkv * n_seq_tokens,
            head_k_dim * num_k_heads * elem(conv_qkv_mix));

    var v_conv:i64= p_view_4d(e, conv_qkv_mix, head_v_dim, num_v_heads, n_seq_tokens, n_seqs,
            row(conv_qkv_mix, head_v_dim),
            nb1_qkv,
            nb1_qkv * n_seq_tokens,
            row(conv_qkv_mix, 2 * head_k_dim * num_k_heads));

    observe(e,q_conv,32, il);
    observe(e,k_conv,33, il);
    observe(e,v_conv,34, il);


    var eps_norm:f32= fget(e,7,0);

    q_conv = build_gdn_l2_norm(e, q_conv, eps_norm);
    k_conv = build_gdn_l2_norm(e, k_conv, eps_norm);

    
    
    

    
    
    if (num_k_heads != num_v_heads && (get(e,20,0)==0 || get(e,21,0)==0)) {
        assert(num_v_heads % num_k_heads == 0);
        q_conv = p_repeat_4d(e, q_conv, head_k_dim, num_v_heads, n_seq_tokens, n_seqs);
        k_conv = p_repeat_4d(e, k_conv, head_k_dim, num_v_heads, n_seq_tokens, n_seqs);
    }

    observe(e,q_conv,35, il);
    observe(e,k_conv,36, il);
    observe(e,v_conv,37, il);

    var output:i64= build_recurrent_attn(e,inp, ssm_states_all, q_conv, k_conv, v_conv, gate, beta, state, il);

    
    var z_2d:i64= p_reshape_4d(e, z, head_v_dim, num_v_heads, n_seq_tokens, n_seqs);

    
    var attn_out_norm:i64= build_norm_gated(e,output, wt(e,il,22), z_2d, il);

    
    var final_output:i64= p_reshape_2d(e, attn_out_norm, head_v_dim * num_v_heads, n_seq_tokens * n_seqs);

    
    cur = build_lora_mm(e,wt(e,il,23), final_output, wt(e,il,24));
    observe(e,cur,38, il);

    return cur;

}

fn build_layer_ffn(e_arg:i64,cur_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var cur:i64=cur_arg;var il:i64=il_arg;

    
    assert(wt(e,il,25) != 0);

    var moe_out:i64=
        build_moe_ffn(e,cur,
            wt(e,il,25),
            wt(e,il,26),
            wt(e,il,27),
            wt(e,il,28),
            0,
            get(e,22,0), get(e,23,0),
            1, true,
            fget(e,8,0),
            1, il,
            0, wt(e,il,29),
            wt(e,il,30),
            wt(e,il,31),
            wt(e,il,32));
    observe(e,moe_out,39, il);

    
    if (wt(e,il,33) != 0) {
        var ffn_shexp:i64=
            build_ffn(e,cur,
                wt(e,il,33), 0, wt(e,il,34),
                wt(e,il,35), 0, wt(e,il,36),
                wt(e,il,37), 0, wt(e,il,38),
                0,
                1, 1, il);
        observe(e,ffn_shexp,40, il);

        
        
        
        var shared_gate:i64= build_lora_mm(e,wt(e,il,6), cur,0);
        observe(e,shared_gate,41, il);

        
        shared_gate = p_sigmoid(e, shared_gate);
        observe(e,shared_gate,42, il);


        
        ffn_shexp = p_mul(e, ffn_shexp, shared_gate);
        observe(e,ffn_shexp,43, il);

        cur = p_add(e, moe_out, ffn_shexp);
        observe(e,cur,4, il);
    } else {
        cur = moe_out;
    }

    return cur;

}

fn build_delta_net_chunking(e_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,g_arg:i64,b_arg:i64,s_arg:i64,il_arg:i64)->Pair {var e:i64=e_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var g:i64=g_arg;var b:i64=b_arg;var s:i64=s_arg;var il:i64=il_arg;

    var S_k:i64= ne(q,0);
    var H_k:i64= ne(q,1);
    var n_tokens:i64= ne(q,2);
    var n_seqs:i64= ne(q,3);

    var S_v:i64= ne(v,0);
    var H_v:i64= ne(v,1);
    var kda:bool= (ne(g,0) == S_k && ne(g,1) == H_k);

    assert(S_k == S_v);
    assert(H_v % H_k == 0);

    assert(ne(q,0) == S_k && ne(q,1) == H_k && ne(q,2) == n_tokens && ne(q,3) == n_seqs);
    assert(ne(k,0) == S_k && ne(k,1) == H_k && ne(k,2) == n_tokens && ne(k,3) == n_seqs);
    assert(ne(v,0) == S_v && ne(v,1) == H_v && ne(v,2) == n_tokens && ne(v,3) == n_seqs);

    assert(ne(g,0) == 1   || ne(g,0) == S_v);
    assert(                   ne(g,1) == H_v && ne(g,2) == n_tokens && ne(g,3) == n_seqs);
    assert(ne(b,0) == 1   && ne(b,1) == H_v && ne(b,2) == n_tokens && ne(b,3) == n_seqs);
    assert(ne(s,0) == S_v && ne(s,1) == S_v && ne(s,2) == H_v      && ne(s,3) == n_seqs);

    var scale:f32= 1.0 / fsqrt(S_k as f32);

    q = p_scale(e, q, scale);

    observe(e,q,44, il);
    observe(e,k,45, il);
    observe(e,v,46, il);
    observe(e,b,47, il);
    observe(e,g,48, il);

    q = p_permute(e, q, 0, 2, 1, 3); 
    k = p_permute(e, k, 0, 2, 1, 3); 
    v = p_permute(e, v, 0, 2, 1, 3); 
    g = p_permute(e, g, 0, 2, 1, 3); 
    b = p_permute(e, b, 0, 2, 1, 3); 

    var CS:i64= if kda {16} else {64}; 

    var padding:i64= (CS - n_tokens % CS) % CS;
    var n_chunks:i64= (n_tokens + padding) / CS;

    q = p_pad(e, q, 0, padding, 0, 0);
    k = p_pad(e, k, 0, padding, 0, 0);
    v = p_pad(e, v, 0, padding, 0, 0);
    g = p_pad(e, g, 0, padding, 0, 0);
    b = p_pad(e, b, 0, padding, 0, 0);

    var v_b:i64= p_mul(e, v, b);
    var k_b:i64= p_mul(e, k, b);

    observe(e,v_b,49, il);
    observe(e,k_b,50, il);

    q   = p_reshape_4d(e, q,   S_k, CS, n_chunks, H_k * n_seqs);
    k   = p_reshape_4d(e, k,   S_k, CS, n_chunks, H_k * n_seqs);
    k_b = p_reshape_4d(e, k_b, S_k, CS, n_chunks, H_v * n_seqs);
    v   = p_reshape_4d(e, v,   S_v, CS, n_chunks, H_v * n_seqs);
    v_b = p_reshape_4d(e, v_b, S_v, CS, n_chunks, H_v * n_seqs);

    g = p_reshape_4d(e, g, ne(g,0), CS, n_chunks, H_v * n_seqs);
    b = p_reshape_4d(e, b, 1,        CS, n_chunks, H_v * n_seqs);

    
    
    var g_cs:i64= p_cumsum(e, p_cont(e, p_transpose(e, g)));
    observe(e,g_cs,51, il);

    var kb:i64= 0;
    var kq:i64= 0;
    if (kda) {
        var CHB:i64= n_chunks * H_k * n_seqs;

        var g_cs_i:i64= p_reshape_4d(e, g_cs, CS, 1, S_k, CHB);  
        var g_cs_j:i64= p_reshape_4d(e, g_cs, 1, CS, S_k, CHB);  

        g_cs_j = p_repeat_4d(e, g_cs_j, CS, CS, S_k, CHB);  

        
        var decay_mask:i64=0;
        decay_mask = p_sub(e, g_cs_j, g_cs_i);
        decay_mask = p_tri(e, decay_mask, 2);
        decay_mask = p_exp(e, decay_mask);
        observe(e,decay_mask,52, il);

        
        decay_mask = p_cont_4d(e, p_permute(e, decay_mask, 2, 1, 0, 3), S_k, CS, CS, CHB);

        var k_b_i:i64= p_reshape_4d(e, k_b, S_k, CS,  1, CHB);
        var k_j:i64= p_reshape_4d(e, k,   S_k,  1, CS, CHB);
        var q_i:i64= p_reshape_4d(e, q,   S_k, CS,  1, CHB);

        var decay_k_b_i:i64= p_mul(e, decay_mask, k_b_i);
        var decay_q_i:i64= p_mul(e, decay_mask, q_i);

        
        kb = p_mul_mat(e, decay_k_b_i, k_j);
        kq = p_mul_mat(e, decay_q_i,   k_j);

        kb = p_cont(e, p_transpose(e, p_reshape_4d(e, kb, CS, CS, n_chunks, H_v * n_seqs)));
        kq = p_cont(e, p_transpose(e, p_reshape_4d(e, kq, CS, CS, n_chunks, H_v * n_seqs)));
    } else {
        var g_cs_i:i64= g_cs;
        var g_cs_j:i64= p_reshape_4d(e, g_cs, 1, CS, n_chunks, H_v * n_seqs);

        g_cs_j = p_repeat_4d(e, g_cs_j, CS, CS, n_chunks, H_v * n_seqs);

        
        var decay_mask:i64=0;
        decay_mask = p_sub(e, g_cs_j, g_cs_i);
        decay_mask = p_tri(e, decay_mask, 2);
        decay_mask = p_exp(e, decay_mask);
        observe(e,decay_mask,52, il);

        
        kb = p_mul_mat(e, k,  k_b);
        kb = p_mul(e, kb, decay_mask);

        
        kq = p_mul_mat(e, k, q);
        kq = p_mul(e, kq, decay_mask);
    }

    kq = p_tri(e, kq, 2);
    observe(e,kq,53, il);

    
    var attn:i64=0;
    attn = p_tri(e, kb, 3);
    observe(e,attn,54, il);

    var identity:i64=0;
    identity = p_view_1d(e, attn, CS, 0);
    identity = p_fill(e, identity, 1.0);
    identity = p_diag(e, identity);

    var lhs:i64= p_add(e, attn, identity);
    observe(e,lhs,55, il);

    attn = p_neg(e, attn);
    observe(e,attn,56, il);

    var lin_solve:i64= p_solve_tri(e, lhs, attn, true, true, false);
    attn = p_add(e, lin_solve, identity);
    observe(e,attn,57, il); 

    
    v = p_mul_mat(e, p_cont(e, p_transpose(e, v_b)), attn);

    
    var g_exp:i64= p_exp(e, g_cs);

    k_b = p_cont(e, p_transpose(e, k_b));

    
    var kbg:i64= p_mul(e, k_b, g_exp);
    observe(e,kbg,58, il);

    
    var k_cd:i64= p_mul_mat(e, kbg, attn);
    observe(e,k_cd,59, il);

    
    var g_exp_t:i64= p_cont(e, p_transpose(e, g_exp));
    var q_g_exp:i64= p_mul(e, q, g_exp_t);

    
    
    
    
    
    
    

    
    
    
    var g_last:i64= p_view_4d(e, g_cs, 1, ne(g_cs,1), ne(g_cs,2), ne(g_cs,3),
            nb(g_cs,1),
            nb(g_cs,2),
            nb(g_cs,3),
            row(g_cs, ne(g_cs,0) - 1));
    observe(e,g_last,60, il);

    
    var g_last_exp_t:i64= p_transpose(e, p_exp(e, g_last));
    observe(e,g_last_exp_t,61, il);

    
    var g_diff:i64= p_neg(e, p_sub(e, g_cs, g_last));
    observe(e,g_diff,62, il);

    var g_diff_exp_t:i64= p_cont(e, p_transpose(e, p_exp(e, g_diff)));

    
    var kg:i64= p_mul(e, k, g_diff_exp_t);
    observe(e,kg,63, il);

    
    var kg_t:i64= p_cont(e, p_transpose(e, kg));
    observe(e,kg_t,64, il);

    s = p_reshape_4d(e, s, S_v, S_v, 1, H_v * n_seqs);
    observe(e,s,65, il);

    
    var v_t:i64= p_cont(e, p_transpose(e, v));

    for chunk in n_chunks {
        var ch_k_cd:i64= get_slice_2d(e, k_cd,    chunk); 
        var ch_v_t:i64= get_slice_2d(e, v_t,     chunk); 
        var ch_kq:i64= get_slice_2d(e, kq,      chunk); 
        var ch_q_g_exp:i64= get_slice_2d(e, q_g_exp, chunk); 
        var ch_kg_t:i64= get_slice_2d(e, kg_t,    chunk); 

        
        var v_t_p:i64= p_mul_mat(e, ch_k_cd, s);
        observe(e,v_t_p,66, il);

        
        var v_t_new:i64= p_sub(e, ch_v_t, v_t_p);
        observe(e,v_t_new,67, il);

        
        var v_attn:i64= p_mul_mat(e, v_t_new, ch_kq);
        observe(e,v_attn,68, il);

        
        var attn_inter:i64= p_mul_mat(e, s, ch_q_g_exp);
        observe(e,attn_inter,69, il);

        
        var o_ch:i64= p_add(e, attn_inter, v_attn);
        observe(e,o_ch,70, il);

        v = p_set_inplace(e, v, o_ch, nb(v,1), nb(v,2), nb(v,3), chunk * nb(v,2));

        
        
        var kgv:i64= p_mul_mat(e, ch_kg_t, v_t_new); 

        
        var ch_g_last_exp_t:i64= get_slice_2d(e, g_last_exp_t, chunk);

        s = p_mul(e, s, ch_g_last_exp_t);
        s = p_add(e, s, kgv);
        observe(e,s,65, il);
    }

    
    var o:i64= p_view_4d(e, v,
            S_v, n_tokens, H_v, n_seqs,
            row(v, S_v),
            row(v, S_v * CS * n_chunks),
            row(v, S_v * CS * n_chunks * H_v), 0);
    o = p_permute(e, o, 0, 2, 1, 3); 
    s = p_reshape_4d(e, s, S_v, S_v, H_v, n_seqs);
    observe(e,s,71, il);

    return Pair { a:o,b:s };

}

fn build_delta_net_autoregressive(e_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,g_arg:i64,b_arg:i64,s_arg:i64,il_arg:i64)->Pair {var e:i64=e_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var g:i64=g_arg;var b:i64=b_arg;var s:i64=s_arg;var il:i64=il_arg;

    var S_k:i64= ne(q,0);
    var H_k:i64= ne(q,1);
    var n_tokens:i64= ne(q,2);
    var n_seqs:i64= ne(q,3);

    var S_v:i64= ne(v,0);
    var H_v:i64= ne(v,1);

    assert(n_tokens == 1);

    assert(S_k == S_v);
    assert(H_v % H_k == 0);

    assert(ne(q,0) == S_k && ne(q,1) == H_k && ne(q,2) == n_tokens && ne(q,3) == n_seqs);
    assert(ne(k,0) == S_k && ne(k,1) == H_k && ne(k,2) == n_tokens && ne(k,3) == n_seqs);
    assert(ne(v,0) == S_v && ne(v,1) == H_v && ne(v,2) == n_tokens && ne(v,3) == n_seqs);

    assert(ne(g,0) == 1   || ne(g,0) == S_v);
    assert(                   ne(g,1) == H_v && ne(g,2) == n_tokens && ne(g,3) == n_seqs);
    assert(ne(b,0) == 1   && ne(b,1) == H_v && ne(b,2) == n_tokens && ne(b,3) == n_seqs);
    assert(ne(s,0) == S_v && ne(s,1) == S_v && ne(s,2) == H_v      && ne(s,3) == n_seqs);

    var scale:f32= 1.0 / fsqrt(S_k as f32);

    q = p_scale(e, q, scale);

    q = p_permute(e, q, 0, 2, 1, 3); 
    k = p_permute(e, k, 0, 2, 1, 3); 
    v = p_permute(e, v, 0, 2, 1, 3); 

    observe(e,q,44, il);
    observe(e,k,45, il);
    observe(e,v,46, il);
    observe(e,b,47, il);
    observe(e,g,48, il);

    
    
    g = p_reshape_4d(e, g, 1, ne(g,0), H_v, n_seqs);
    b = p_reshape_4d(e, b, 1,        1, H_v, n_seqs);

    
    g = p_exp(e, g);
    s = p_mul(e, s, g);

    
    var sk:i64=0;
    sk = p_mul(e, s, k);
    sk = p_sum_rows(e, sk);

    
    var d:i64=0;
    d = p_sub(e, v, p_transpose(e, sk));
    d = p_mul(e, d, b);

    
    var d_t:i64=0;
    d_t = p_transpose(e, d);

    
    var kd:i64=0;
    k  = p_repeat(e, k, s);
    kd = p_mul(e, k, d_t);

    s = p_add(e, s, kd);

    observe(e,s,72, il);

    var s_q:i64= p_mul(e, s, q);
    var o:i64= p_sum_rows(e, s_q);

    o = p_permute(e, o, 2, 0, 1, 3); 

    return Pair { a:o,b:s };

}

fn build_delta_net_fused(e_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,g_arg:i64,b_arg:i64,s_arg:i64,il_arg:i64)->Pair {var e:i64=e_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var g:i64=g_arg;var b:i64=b_arg;var s:i64=s_arg;var il:i64=il_arg;

    var S_k:i64= ne(q,0);
    var H_k:i64= ne(q,1);
    var n_tokens:i64= ne(q,2);
    var n_seqs:i64= ne(q,3);

    var S_v:i64= ne(v,0);
    var H_v:i64= ne(v,1);

    assert(S_k == S_v);
    assert(H_v % H_k == 0);

    assert(ne(q,0) == S_k && ne(q,1) == H_k && ne(q,2) == n_tokens && ne(q,3) == n_seqs);
    assert(ne(k,0) == S_k && ne(k,1) == H_k && ne(k,2) == n_tokens && ne(k,3) == n_seqs);
    assert(ne(v,0) == S_v && ne(v,1) == H_v && ne(v,2) == n_tokens && ne(v,3) == n_seqs);

    assert(ne(g,0) == 1   || ne(g,0) == S_v);
    assert(                   ne(g,1) == H_v && ne(g,2) == n_tokens && ne(g,3) == n_seqs);
    assert(ne(b,0) == 1   && ne(b,1) == H_v && ne(b,2) == n_tokens && ne(b,3) == n_seqs);
    assert(ne(s,0) == S_v && ne(s,1) == S_v && ne(s,2) == H_v      && ne(s,3) == n_seqs);

    
    var result:i64= p_gated_delta_net(e, q, k, v, g, b, s, 1);
    if (n_tokens == 1) {
        fused(e,1,result,il);
    } else {
        fused(e,2,result,il);
    }

    var output:i64= p_view_4d(e, result,
            S_v, H_v, n_tokens, n_seqs,
            row(result, S_v),
            row(result, S_v * H_v),
            row(result, S_v * H_v * n_tokens), 0);

    var new_state:i64= p_view_4d(e, result,
            S_v, S_v, H_v, n_seqs,
            row(result, S_v),
            row(result, S_v * S_v),
            row(result, S_v * S_v * H_v),
            row(result, S_v * H_v * n_tokens * n_seqs));

    return Pair { a:output,b:new_state };

}

fn build_delta_net(e_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,g_arg:i64,b_arg:i64,s_arg:i64,il_arg:i64)->Pair {var e:i64=e_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var g:i64=g_arg;var b:i64=b_arg;var s:i64=s_arg;var il:i64=il_arg;

    var n_seq_tokens:i64= ne(q,2);

    if (n_seq_tokens == 1) {
        if (get(e,20,0)!=0) {
            return build_delta_net_fused(e,q, k, v, g, b, s, il);
        }
        return build_delta_net_autoregressive(e,q, k, v, g, b, s, il);
    }

    if (get(e,21,0)!=0) {
        return build_delta_net_fused(e,q, k, v, g, b, s, il);
    }

    return build_delta_net_chunking(e,q, k, v, g, b, s, il);

}

fn get_slice_2d(e_arg:i64,t_arg:i64,c_arg:i64)->i64 {var e:i64=e_arg;var t:i64=t_arg;var c:i64=c_arg;

    return p_view_4d(e, t, ne(t,0), ne(t,1), 1, ne(t,3),
        nb(t,1), nb(t,2), nb(t,3), nb(t,2) * c);

}

fn build_conv_state(e_arg:i64,inp_arg:i64,conv_states_all_arg:i64,qkv_mixed_arg:i64,conv_kernel_size_arg:i64,conv_channels_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var inp:i64=inp_arg;var conv_states_all:i64=conv_states_all_arg;var qkv_mixed:i64=qkv_mixed_arg;var conv_kernel_size:i64=conv_kernel_size_arg;var conv_channels:i64=conv_channels_arg;var il:i64=il_arg;

    var mctx_cur= service(e,5,inp,0,0);

    var kv_head= service(e,6,mctx_cur,0,0);
    var mem_size= service(e,7,mctx_cur,0,0);

    var n_seqs:i64= get(e,7,0);

    var conv_states:i64= build_rs(e,inp, conv_states_all, get(e,4,0), n_seqs);
    observe(e,conv_states,73, il);

    conv_states = p_reshape_3d(e, conv_states, conv_kernel_size - 1, conv_channels, n_seqs);
    observe(e,conv_states,74, il);

    qkv_mixed = p_transpose(e, qkv_mixed);
    observe(e,qkv_mixed,75, il);

    var conv_input:i64= p_concat(e, conv_states, qkv_mixed, 0);
    observe(e,conv_input,76, il);

    var row_count:i64= (conv_kernel_size - 1) * conv_channels;

    var row_size:i64= row(conv_states_all, row_count);

    if (get(e,24,0) == 0) {
        var s_idx:i64= ne(conv_input,0) - ne(conv_states,0);
        var s_slot:i64= 0;

        var conv_state_last:i64=
            p_view_3d(e, conv_input,
                    conv_kernel_size - 1, conv_channels, n_seqs,
                    nb(conv_input,1), nb(conv_input,2),
                    row(conv_input, s_idx));
        observe(e,conv_state_last,77, il);

        var conv_state_update:i64=
            p_view_2d(e, conv_states_all,
                    row_count, n_seqs, nb(conv_states_all,1),
                    (s_slot * mem_size + kv_head) * row_size);
        observe(e,conv_state_update,78, il);

        p_build_forward_expand(e, p_cpy(e, conv_state_last, conv_state_update));
    } else {
        
        
        

        var K:i64= get(e,24,0) + 1;

        for t0 in K { let t=t0+1;
            var s_idx:i64= maxi(0, ne(conv_input,0) - ne(conv_states,0) - K + t);
            var s_slot:i64= K - t;

            var conv_state_last:i64=
                p_view_3d(e, conv_input,
                        conv_kernel_size - 1, conv_channels, n_seqs,
                        nb(conv_input,1), nb(conv_input,2),
                        row(conv_input, s_idx));

            var conv_state_update:i64=
                p_view_2d(e,
                        conv_states_all, row_count, n_seqs,
                        nb(conv_states_all,1),
                        (s_slot * mem_size + kv_head) * row_size);

            p_build_forward_expand(e, p_cpy(e, conv_state_last, conv_state_update));
        }
    }

    return conv_input;

}

fn build_recurrent_attn(e_arg:i64,inp_arg:i64,ssm_states_all_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,g_arg:i64,b_arg:i64,s_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var inp:i64=inp_arg;var ssm_states_all:i64=ssm_states_all_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var g:i64=g_arg;var b:i64=b_arg;var s:i64=s_arg;var il:i64=il_arg;

    var mctx_cur= service(e,5,inp,0,0);
    var kv_head= service(e,6,mctx_cur,0,0);
    var mem_size:i64= service(e,7,mctx_cur,0,0);

    var S_v:i64= ne(s,0);
    var H_v:i64= ne(s,2);
    var n_seqs:i64= ne(s,3);
    var n_seq_tokens:i64= ne(q,2);

    var keep:bool= get(e,24,0) > 0;

    if (!keep) {
        var attn_out= build_delta_net(e,q, k, v, g, b, s, il);
        var output:i64= attn_out.a;
        var new_state:i64= attn_out.b;
        observe(e,output,23, il);
        observe(e,new_state,79, il);

        p_build_forward_expand(e,
                p_cpy(e, new_state,
                    p_view_2d(e, ssm_states_all, get(e,3,0), n_seqs, nb(ssm_states_all,1),
                        kv_head * get(e,3,0) * elem(ssm_states_all))));

        return output;
    }

    var D:i64= S_v * S_v * H_v;
    var K:i64= get(e,24,0) + 1;

    
    var gdn_out:i64= p_gated_delta_net(e, q, k, v, g, b, s, K);
    if (n_seq_tokens > 1) {
        fused(e,2,gdn_out,il);
    } else {
        fused(e,1,gdn_out,il);
    }

    var attn_score_elems:i64= S_v * H_v * n_seq_tokens * n_seqs;
    var state_size_per_snap:i64= S_v * S_v * H_v * n_seqs;

    var output:i64= p_view_4d(e, gdn_out,
        S_v, H_v, n_seq_tokens, n_seqs,
        row(gdn_out, S_v),
        row(gdn_out, S_v * H_v),
        row(gdn_out, S_v * H_v * n_seq_tokens),
        0);
    observe(e,output,23, il);

    var row_size:i64= get(e,3,0) * elem(ssm_states_all);

    
    var n_written:i64= mini(n_seq_tokens, K);

    
    var src:i64= p_view_3d(e, gdn_out,
        D, n_seqs, n_written,
        row(gdn_out, D),
        row(gdn_out, state_size_per_snap),
        row(gdn_out, attn_score_elems));

    var dst:i64= p_view_3d(e, ssm_states_all,
        D, n_seqs, n_written,
        nb(ssm_states_all,1),
        mem_size * row_size,
        kv_head * row_size);

    p_build_forward_expand(e, p_cpy(e, src, dst));

    return output;

}

fn flow_main(e_arg:i64)->i64 {var e:i64=e_arg;

    var n_embd_head:i64= get(e,1,0);

    assert(n_embd_head == get(e,2,0));

    let sections:i64=0;
    

    var cur:i64=0;
    var inpL:i64=0;

    inpL = input_embed(e,wt(e,-1,0));

    observe(e,inpL,0, -1);

    var inp:i64=0;

    var inp_pos:i64= position_input(e);
    var inp_out_ids:i64= outid_input(e);

    
    for il in get(e,6,0) {
        put(e,0,il,inpL);

        var inpSA:i64= inpL;

        cur = build_norm(e,inpL, wt(e,il,1), 0, 1, il);
        observe(e,cur,1, il);

        p_build_forward_expand(e, cur);

        
        if (get(e,5,il)!=0) {
            
            cur = service(e,71,cur,il,1);
        } else {
            
            cur = service(e,71,cur,il,0);
        }

        if (il == get(e,6,0) - 1 && crop_before(e,inp_out_ids)) {
            cur   = p_get_rows(e, cur, inp_out_ids);
            inpSA = p_get_rows(e, inpSA, inp_out_ids);
        }

        
        cur = p_add(e, cur, inpSA);
        observe(e,cur,2, il);

        
        var ffn_residual:i64= cur;

        
        var attn_post_norm:i64= build_norm(e,cur, wt(e,il,2), 0, 1, il);
        observe(e,attn_post_norm,3, il);

        
        cur = build_layer_ffn(e,attn_post_norm, il);
        observe(e,cur,4, il);

        
        cur = p_add(e, cur, ffn_residual);
        observe(e,cur,5, il);

        cur = service(e,10,cur,il,0);
        observe(e,cur,6, il);

        
        inpL = cur;
    }
    cur = inpL;

    
    cur = build_norm(e,cur, wt(e,-1,3), 0, 1, -1);

    observe(e,cur,7, -1);
    put(e,1,0,cur);

    if (crop_after(e,inp_out_ids)) {
        cur = p_get_rows(e, cur, inp_out_ids);
    }

    observe(e,cur,8, -1);
    put(e,2,0,cur);

    
    cur = build_lora_mm(e,wt(e,-1,5), cur, wt(e,-1,4));

    observe(e,cur,9, -1);
    put(e,3,0,cur);

return cur;
}

struct Pair { a:i64,b:i64 }
struct Triple { a:i64,b:i64,c:i64 }
fn maxi(a_arg:i64,b_arg:i64)->i64 {var a:i64=a_arg;var b:i64=b_arg;return if a>b {a} else {b};}
fn mini(a_arg:i64,b_arg:i64)->i64 {var a:i64=a_arg;var b:i64=b_arg;return if a<b {a} else {b};}
fn crop_before(e_arg:i64,ids_arg:i64)->bool {var e:i64=e_arg;var ids:i64=ids_arg;return ids!=0&&(get(e,25,0)==0||get(e,26,0)!=0);}
fn crop_after(e_arg:i64,ids_arg:i64)->bool {var e:i64=e_arg;var ids:i64=ids_arg;return ids!=0&&get(e,25,0)!=0&&get(e,26,0)==0;}
fn build_norm(e_arg:i64,cur_arg:i64,w_arg:i64,b_arg:i64,mode_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var cur:i64=cur_arg;var w:i64=w_arg;var b:i64=b_arg;var mode:i64=mode_arg;var il:i64=il_arg;
 assert(mode==1);cur=p_rms_norm(e,cur,fget(e,7,0));
 if w!=0||b!=0 {observe(e,cur,80,il);}if w!=0 {cur=p_mul(e,cur,w);if b!=0 {observe(e,cur,81,il);}}if b!=0 {cur=p_add(e,cur,b);}return cur;
}
fn build_gdn_l2_norm(e_arg:i64,x_arg:i64,eps_arg:f32)->i64 {var e:i64=e_arg;var x:i64=x_arg;var eps:f32=eps_arg;let n:f32=ne(x,0) as f32;return p_scale(e,p_rms_norm(e,x,eps/n),1.0/fsqrt(n));}
fn build_lora_mm(e_arg:i64,w_arg:i64,cur_arg:i64,scale_arg:i64)->i64 {var e:i64=e_arg;var w:i64=w_arg;var cur:i64=cur_arg;var scale:i64=scale_arg;
 cur=hadamard_input(e,w,cur);var out=p_mul_mat(e,w,cur);let ignored=service(e,12,out,w,0);
 if scale!=0 {out=p_mul(e,out,scale);}for i in service(e,13,0,0,0) {let a=service(e,14,w,i,0);if a!=0 {let b=service(e,15,w,i,0);var ab=p_mul_mat(e,b,p_mul_mat(e,a,cur));ab=p_scale(e,ab,adapter_scale(e,w,i,0));out=p_add(e,out,ab);}}
 return out;
}
fn build_lora_mm_id(e_arg:i64,w_arg:i64,cur_arg:i64,ids_arg:i64,scale_arg:i64,slots_arg:i64)->i64 {var e:i64=e_arg;var w:i64=w_arg;var cur:i64=cur_arg;var ids:i64=ids_arg;var scale:i64=scale_arg;var slots:i64=slots_arg;
 cur=hadamard_input(e,w,cur);var out:i64=0;if slots==0 {out=p_mul_mat_id(e,w,cur,ids);}else {out=p_mul_mat_id(e,service(e,16,w,0,0),cur,slots);}let ignored=service(e,12,out,w,0);
 if scale!=0 {let n=ne(scale,0);var s=p_reshape_3d(e,scale,1,n,1);s=p_repeat_4d(e,s,1,n,ne(cur,2),1);s=p_get_rows(e,s,ids);out=p_mul(e,out,s);}
 for i in service(e,13,0,0,0) {let a=service(e,14,w,i,0);if a!=0 {let b=service(e,15,w,i,0);var ab=p_mul_mat_id(e,b,p_mul_mat_id(e,a,cur,ids),ids);ab=p_scale(e,ab,adapter_scale(e,w,i,1));out=p_add(e,out,ab);}}
 return out;
}
fn build_qkv(e_arg:i64,layer_arg:i64,cur_arg:i64,dq_arg:i64,hq_arg:i64,dk_arg:i64,hk_arg:i64,dv_arg:i64,hv_arg:i64,il_arg:i64,reshape_arg:bool)->Triple {var e:i64=e_arg;var layer:i64=layer_arg;var cur:i64=cur_arg;var dq:i64=dq_arg;var hq:i64=hq_arg;var dk:i64=dk_arg;var hk:i64=hk_arg;var dv:i64=dv_arg;var hv:i64=hv_arg;var il:i64=il_arg;var reshape:bool=reshape_arg;
 assert(!reshape);let qdim=dq*hq;let kdim=dk*hk;var Q:i64=0;var K:i64=0;var V:i64=0;
 if wt(e,il,7)!=0 {
 var mixed=build_lora_mm(e,wt(e,il,7),cur,wt(e,il,8));observe(e,mixed,82,il);
 if wt(e,il,39)!=0 {mixed=p_add(e,mixed,wt(e,il,39));observe(e,mixed,83,il);}else {if wt(e,il,40)!=0&&wt(e,il,41)!=0&&wt(e,il,42)!=0 {let bias=p_concat(e,p_concat(e,wt(e,il,40),wt(e,il,41),0),wt(e,il,42),0);mixed=p_add(e,mixed,bias);observe(e,mixed,83,il);}}
 Q=p_view_2d(e,mixed,qdim,get(e,11,0),nb(mixed,1),0);K=p_view_2d(e,mixed,kdim,get(e,11,0),nb(mixed,1),row(mixed,qdim));V=p_view_2d(e,mixed,dv*hv,get(e,11,0),nb(mixed,1),row(mixed,qdim+kdim));Q=p_cont(e,Q);K=p_cont(e,K);V=p_cont(e,V);
 }else {
 Q=build_lora_mm(e,wt(e,il,43),cur,wt(e,il,44));if wt(e,il,40)!=0 {Q=p_add(e,Q,wt(e,il,40));}
 K=build_lora_mm(e,wt(e,il,45),cur,wt(e,il,46));if wt(e,il,41)!=0 {K=p_add(e,K,wt(e,il,41));}
 V=build_lora_mm(e,wt(e,il,47),cur,wt(e,il,48));if wt(e,il,42)!=0 {V=p_add(e,V,wt(e,il,42));}
 }return Triple {a:Q,b:K,c:V};
}
fn build_ffn(e_arg:i64,cur_arg:i64,up_arg:i64,up_b_arg:i64,up_s_arg:i64,gate_arg:i64,gate_b_arg:i64,gate_s_arg:i64,down_arg:i64,down_b_arg:i64,down_s_arg:i64,act_arg:i64,op_arg:i64,mode_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var cur:i64=cur_arg;var up:i64=up_arg;var up_b:i64=up_b_arg;var up_s:i64=up_s_arg;var gate:i64=gate_arg;var gate_b:i64=gate_b_arg;var gate_s:i64=gate_s_arg;var down:i64=down_arg;var down_b:i64=down_b_arg;var down_s:i64=down_s_arg;var act:i64=act_arg;var op:i64=op_arg;var mode:i64=mode_arg;var il:i64=il_arg;
 assert(op==1&&mode==1&&up_b==0&&gate_b==0&&down_b==0&&act==0);
 var tmp=cur;if up!=0 {tmp=build_lora_mm(e,up,cur,0);}observe(e,tmp,84,il);if up_s!=0 {tmp=p_mul(e,tmp,up_s);observe(e,tmp,85,il);}
 if gate!=0 {cur=build_lora_mm(e,gate,cur,0);observe(e,cur,86,il);if gate_s!=0 {cur=p_mul(e,cur,gate_s);observe(e,cur,87,il);}}else {cur=tmp;}
 if gate!=0 {let limit=layer_float(e,0,il);if limit>0.000001 {tmp=p_clamp(e,tmp,-limit,limit);observe(e,tmp,88,il);var activated=p_silu(e,cur);observe(e,activated,89,il);activated=p_clamp(e,activated,-infinity(),limit);observe(e,activated,90,il);cur=p_mul(e,activated,tmp);observe(e,cur,91,il);}else {cur=p_swiglu_split(e,cur,tmp);observe(e,cur,92,il);}}else {cur=p_silu(e,cur);observe(e,cur,89,il);}
 if down!=0 {cur=build_lora_mm(e,down,cur,0);}if down_s!=0 {cur=p_mul(e,cur,down_s);observe(e,cur,93,il);}return cur;
}
fn build_moe_ffn(e_arg:i64,cur_arg:i64,gate_inp_arg:i64,up_exps_arg:i64,gate_exps_arg:i64,down_exps_arg:i64,prob_bias_arg:i64,nexp_arg:i64,nused_arg:i64,op_arg:i64,norm_arg:bool,wscale_arg:f32,gating_arg:i64,il_arg:i64,probs_in_arg:i64,gate_up_arg:i64,up_s_arg:i64,gate_s_arg:i64,down_s_arg:i64)->i64 {var e:i64=e_arg;var cur:i64=cur_arg;var gate_inp:i64=gate_inp_arg;var up_exps:i64=up_exps_arg;var gate_exps:i64=gate_exps_arg;var down_exps:i64=down_exps_arg;var prob_bias:i64=prob_bias_arg;var nexp:i64=nexp_arg;var nused:i64=nused_arg;var op:i64=op_arg;var norm:bool=norm_arg;var wscale:f32=wscale_arg;var gating:i64=gating_arg;var il:i64=il_arg;var probs_in:i64=probs_in_arg;var gate_up:i64=gate_up_arg;var up_s:i64=up_s_arg;var gate_s:i64=gate_s_arg;var down_s:i64=down_s_arg;
 assert(prob_bias==0&&probs_in==0&&op==1&&gating==1&&get(e,27,0)<=1);
 let dim=ne(cur,0);let nt=ne(cur,1);let logits=build_lora_mm(e,gate_inp,cur,0);observe(e,logits,94,il);var probs=p_soft_max(e,logits);observe(e,probs,95,il);
 let ids=p_argsort_top_k(e,probs,nused);observe(e,src(ids,0),96,il);observe(e,ids,97,il);probs=p_reshape_3d(e,probs,1,nexp,nt);var weights=p_get_rows(e,probs,ids);observe(e,weights,98,il);
 if norm {weights=p_reshape_2d(e,weights,nused,nt);var sums=p_sum_rows(e,weights);observe(e,sums,99,il);sums=p_clamp(e,sums,0.00006103515625,infinity());observe(e,sums,100,il);weights=p_div(e,weights,sums);observe(e,weights,101,il);weights=p_reshape_3d(e,weights,1,nused,nt);}
 if wscale!=0.0&&wscale!=1.0 {weights=p_scale(e,weights,wscale);observe(e,weights,102,il);}p_build_forward_expand(e,weights);
 let slots=cache_slots(e,ids,up_exps,gate_exps,down_exps,gate_up,il);cur=p_reshape_3d(e,cur,dim,1,nt);var up:i64=0;
 if gate_up!=0 {let both=build_lora_mm_id(e,gate_up,cur,ids,up_s,slots);observe(e,both,103,il);if up_s!=0 {observe(e,both,104,il);}let ff=ne(both,0)/2;cur=p_view_3d(e,both,ff,ne(both,1),ne(both,2),nb(both,1),nb(both,2),0);observe(e,cur,105,il);up=p_view_3d(e,both,ff,ne(both,1),ne(both,2),nb(both,1),nb(both,2),ff*nb(both,0));observe(e,up,106,il);
 }else {up=build_lora_mm_id(e,up_exps,cur,ids,up_s,slots);observe(e,up,106,il);if up_s!=0 {observe(e,up,107,il);}if gate_exps!=0 {cur=build_lora_mm_id(e,gate_exps,cur,ids,gate_s,slots);observe(e,cur,105,il);}else {cur=up;}if gate_s!=0 {observe(e,cur,108,il);}}
 let hasgate=gate_exps!=0||gate_up!=0;var clamped=false;if gate_exps!=0&&il>=0 {let limit=layer_float(e,1,il);if limit>0.000001 {up=p_clamp(e,up,-limit,limit);observe(e,up,109,il);var a=p_silu(e,cur);observe(e,a,110,il);a=p_clamp(e,a,-infinity(),limit);observe(e,a,111,il);cur=p_mul(e,a,up);observe(e,cur,112,il);clamped=true;}}
 if !clamped {if hasgate {cur=p_swiglu_split(e,cur,up);observe(e,cur,113,il);}else {cur=p_silu(e,cur);observe(e,cur,110,il);}}
 var experts=build_lora_mm_id(e,down_exps,cur,ids,down_s,slots);observe(e,experts,114,il);if down_s!=0 {observe(e,experts,115,il);}experts=p_mul(e,experts,weights);observe(e,experts,116,il);p_build_forward_expand(e,experts);
 let n=layer_int(e,0,il);assert(n>0&&n<=256);var views:i64[256]=default<i64[256]>();for i in n {views[i]=p_view_2d(e,experts,dim,nt,nb(experts,2),i*nb(experts,1));p_build_forward_expand(e,views[i]);}
 var out=views[0];for j in n-1 {out=p_add(e,out,views[j+1]);p_build_forward_expand(e,out);}if n==1 {out=p_cont(e,out);}observe(e,out,39,il);return out;
}
fn build_attn(e_arg:i64,inp_arg:i64,wo_arg:i64,wo_b_arg:i64,wo_s_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,bias_arg:i64,sinks_arg:i64,mla_arg:i64,scale_arg:f32,il_arg:i64)->i64 {var e:i64=e_arg;var inp:i64=inp_arg;var wo:i64=wo_arg;var wo_b:i64=wo_b_arg;var wo_s:i64=wo_s_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var bias:i64=bias_arg;var sinks:i64=sinks_arg;var mla:i64=mla_arg;var scale:f32=scale_arg;var il:i64=il_arg;
 assert(mla==0);let kr=service(e,30,inp,0,0);let vr=service(e,31,inp,0,0);if kr!=0 {q=service(e,32,q,kr,0);k=service(e,32,k,kr,0);}if vr!=0 {v=service(e,32,v,vr,0);}p_build_forward_expand(e,q);p_build_forward_expand(e,v);p_build_forward_expand(e,k);
 if get(e,28,0)!=0 {assert(service(e,33,inp,0,0)==get(e,11,0));}else {p_build_forward_expand(e,service(e,34,inp,k,il));p_build_forward_expand(e,service(e,35,inp,v,il));k=service(e,36,inp,il,0);v=service(e,37,inp,il,0);}
 var out=build_attn_mha(e,q,k,v,bias,service(e,38,inp,0,0),sinks,mla,0,scale,il);observe(e,out,117,il);if vr!=0 {out=service(e,32,out,vr,0);}if wo!=0 {out=build_lora_mm(e,wo,out,wo_s);}if wo_b!=0 {out=p_add(e,out,wo_b);}return out;
}
fn build_attn_mha(e_arg:i64,q_arg:i64,k_arg:i64,v_arg:i64,bias_arg:i64,mask_arg:i64,sinks_arg:i64,mla_arg:i64,nkv_arg:i64,scale_arg:f32,il_arg:i64)->i64 {var e:i64=e_arg;var q:i64=q_arg;var k:i64=k_arg;var v:i64=v_arg;var bias:i64=bias_arg;var mask:i64=mask_arg;var sinks:i64=sinks_arg;var mla:i64=mla_arg;var nkv:i64=nkv_arg;var scale:f32=scale_arg;var il:i64=il_arg;
 assert(mla==0&&bias==0&&nkv==0);let trans=nb(v,1)>nb(v,2);let ns=ne(k,3);q=p_view_4d(e,q,ne(q,0),ne(q,1),ne(q,2)/ns,ns,nb(q,1),nb(q,2),nb(q,2)*(ne(q,2)/ns),0);q=p_permute(e,q,0,2,1,3);k=p_permute(e,k,0,2,1,3);v=p_permute(e,v,0,2,1,3);var out:i64=0;
 if get(e,29,0)!=0 {if trans {v=p_transpose(e,v);}if typ(k)==0 {k=p_cast(e,k,1);}if typ(v)==0 {v=p_cast(e,v,1);}var cap:f32=0.0;if get(e,30,0)!=0 {cap=fget(e,9,0);}out=p_flash_attn_ext(e,q,k,v,mask,scale,fget(e,10,0),cap);fused(e,0,out,il);p_flash_attn_ext_add_sinks(out,sinks);p_flash_attn_ext_set_n_kv_max(out,nkv);p_prec_set_acc(out,10);out=p_reshape_2d(e,out,ne(out,0)*ne(out,1),ne(out,2)*ne(out,3));
 }else {var kq=p_mul_mat(e,k,q);observe(e,kq,53,il);p_prec_set_acc(kq,10);if get(e,30,0)!=0 {kq=p_scale(e,kq,1.0/fget(e,9,0));observe(e,kq,118,il);kq=p_tanh(e,kq);observe(e,kq,119,il);kq=p_scale(e,kq,fget(e,9,0));observe(e,kq,120,il);}kq=p_soft_max_ext(e,kq,mask,scale,fget(e,10,0));p_soft_max_add_sinks(kq,sinks);observe(e,kq,121,il);if !trans {v=p_cont(e,p_transpose(e,v));observe(e,v,122,il);}let kqv=p_mul_mat(e,v,kq);observe(e,kqv,123,il);out=p_permute(e,kqv,0,2,1,3);out=p_cont_2d(e,out,ne(out,0)*ne(out,1),ne(out,2)*ne(out,3));if get(e,31,0)==0 {let ignored=service(e,39,out,0,0);}}
 p_build_forward_expand(e,out);return out;
}
fn build_rs(e_arg:i64,inp_arg:i64,s_arg:i64,dim_arg:i64,nseq_arg:i64)->i64 {var e:i64=e_arg;var inp:i64=inp_arg;var s:i64=s_arg;var dim:i64=dim_arg;var nseq:i64=nseq_arg;
 let copy=service(e,40,inp,0,0);let head=service(e,41,inp,0,0);let nr=service(e,42,inp,0,0);let rz=service(e,43,inp,0,0);var states=p_reshape_2d(e,s,dim,ne(s,1));var count:i64=0;if rz>=0 {count=1;}let zero=p_view_1d(e,states,dim*count,rz*nb(states,1)*count);p_build_forward_expand(e,p_scale_inplace(e,zero,0.0));let i0=nr-ne(copy,0);let all=p_get_rows(e,states,copy);let out=p_view_2d(e,all,dim,nseq,nb(all,1),0);p_build_forward_expand(e,out);let extra=p_view_2d(e,all,dim,nr-nseq,nb(all,1),(nseq-i0)*nb(all,1));p_build_forward_expand(e,p_cpy(e,extra,p_view_2d(e,s,dim,nr-nseq,nb(s,1),(head+nseq)*nb(s,1))));return out;
}
fn input_embed(e_arg:i64,table_arg:i64)->i64 {var e:i64=e_arg;var table:i64=table_arg;
 let dim=get(e,32,0);let emb=get(e,33,0);let nt=get(e,19,0);assert(dim>=emb);let inp=service(e,20,0,0,0);let mixed=get(e,34,0)==0;var nm:i64=0;if mixed {nm=service(e,44,0,0,0);}
 let tokens=p_new_tensor_1d(e,26,nt);observe(e,tokens,124,-1);p_set_input(tokens);put(e,4,0,tokens);putin(e,inp,0,tokens);
 let embeddings=p_new_tensor_2d(e,0,dim,nt);observe(e,embeddings,125,-1);p_set_input(embeddings);putin(e,inp,1,embeddings);var mt:i64=0;if mixed {mt=p_new_tensor_1d(e,26,nm);observe(e,mt,126,-1);p_set_input(mt);putin(e,inp,2,mt);}
 var a=p_get_rows(e,table,tokens);p_build_forward_order(e,a);var c:i64=0;if mixed {c=p_get_rows(e,table,mt);p_build_forward_order(e,c);}a=token_embed(e,table,a,tokens,dim,emb);var b=embeddings;
 if mixed {let slots=p_new_tensor_1d(e,27,nm);observe(e,slots,127,-1);p_set_input(slots);putin(e,inp,3,slots);let me=p_new_tensor_2d(e,0,dim,nt);observe(e,me,128,-1);p_set_input(me);putin(e,inp,4,me);c=p_set_rows(e,p_dup(e,me),token_embed(e,table,c,mt,dim,emb),slots);}
 let idx=service(e,45,0,0,0);var cur=select_input(e,a,b,c,if mixed {3} else {2},idx);if dim!=emb {cur=p_view_2d(e,cur,emb,get(e,11,0),nb(cur,1),0);}put(e,5,0,cur);
 let token_only=fget(e,11,0)!=0.0&&get(e,35,0)>0;if fget(e,11,0)!=0.0&&!token_only {if !p_is_contiguous(cur) {cur=p_cont(e,cur);}cur=p_scale(e,cur,fget(e,11,0));}
 var ts:f32=1.0;if token_only {ts=fget(e,11,0);}input_scale(e,inp,ts);if ts!=1.0 {let s=p_new_tensor_2d(e,0,1,nt);observe(e,s,129,-1);p_set_input(s);putin(e,inp,5,s);if !p_is_contiguous(cur) {cur=p_cont(e,cur);}cur=p_mul(e,cur,s);}
 observe(e,cur,130,-1);let ignored=service(e,46,inp,0,0);p_build_forward_expand(e,cur);return cur;
}
fn token_embed(e_arg:i64,table_arg:i64,cur_arg:i64,ids_arg:i64,dim_arg:i64,emb_arg:i64)->i64 {var e:i64=e_arg;var table:i64=table_arg;var cur:i64=cur_arg;var ids:i64=ids_arg;var dim:i64=dim_arg;var emb:i64=emb_arg;
 let rot=service(e,63,table,0,0);if rot!=0 {cur=service(e,32,cur,rot,0);let signs=service(e,63,table,1,0);if signs!=0 {cur=p_mul(e,cur,signs);}}for i in service(e,13,0,0,0) {let a=service(e,14,table,i,0);if a!=0 {let b=service(e,15,table,i,0);let delta=p_scale(e,p_mul_mat(e,b,p_get_rows(e,a,ids)),adapter_scale(e,table,i,0));cur=p_add(e,cur,delta);}}if dim!=emb {cur=p_pad(e,cur,dim-emb,0,0,0);}return cur;
}
fn entry(e_arg:i64)->i64 {var e:i64=e_arg;assert(get(e,6,0)==40);return build_main(e);}
fn main() {let result=entry(0);print(result);let p=probe(0,0);print(p);binding_entry(0,0);}
fn probe(e_arg:i64,mode_arg:i64)->i64 {var e:i64=e_arg;var mode:i64=mode_arg;
 if mode==0 {let pair=build_delta_net(e,service(e,70,0,0,0),service(e,70,1,0,0),service(e,70,2,0,0),service(e,70,3,0,0),service(e,70,4,0,0),service(e,70,5,0,0),0);put(e,6,7,pair.b);return pair.a;}
 if mode==1 {return build_layer_ffn(e,service(e,70,0,0,0),0);}
 if mode==2 {return build_attn_mha(e,service(e,70,0,0,0),service(e,70,1,0,0),service(e,70,2,0,0),0,service(e,70,3,0,0),0,0,0,0.0625,3);}
 if mode==3 {let triple=build_qkv(e,3,service(e,70,0,0,0),512,16,256,2,256,2,3,false);put(e,6,6,triple.b);put(e,6,7,triple.c);return triple.a;}
 if mode==4 {return input_embed(e,wt(e,-1,0));}
 if mode==7 {return flow_main(e);}
 if mode==5 {return build_recurrent_attn(e,service(e,70,7,0,0),service(e,70,6,0,0),service(e,70,0,0,0),service(e,70,1,0,0),service(e,70,2,0,0),service(e,70,3,0,0),service(e,70,4,0,0),service(e,70,5,0,0),0);}
 if mode==6 {return build_conv_state(e,service(e,70,7,0,0),service(e,70,6,0,0),service(e,70,0,0,0),4,8192,0);}
 return 0;
}
fn hadamard_input(e_arg:i64,w_arg:i64,cur_arg:i64)->i64 {var e:i64=e_arg;var w:i64=w_arg;var cur:i64=cur_arg;
 let rot=service(e,60,w,0,0);if rot==0 {return cur;}let cached=service(e,61,cur,rot,0);if cached!=0 {return cached;}var out=cur;let rep=service(e,60,w,2,0);if rep>1 {if !p_is_contiguous(out) {out=p_cont(e,out);}let n1=ne(out,1);let n2=ne(out,2);let n3=ne(out,3);let hd=service(e,60,w,3,0);let nk=service(e,60,w,4,0);out=p_reshape_4d(e,out,hd,nk,rep,n1*n2*n3);out=p_cont(e,p_permute(e,out,0,2,1,3));out=p_reshape_4d(e,out,hd*nk*rep,n1,n2,n3);}let signs=service(e,60,w,1,0);if signs!=0 {out=p_mul(e,out,signs);}out=service(e,32,out,rot,0);let ignored=service(e,62,cur,rot,out);return out;
}
fn cache_slots(e_arg:i64,ids_arg:i64,up_arg:i64,gate_arg:i64,down_arg:i64,both_arg:i64,il_arg:i64)->i64 {var e:i64=e_arg;var ids:i64=ids_arg;var up:i64=up_arg;var gate:i64=gate_arg;var down:i64=down_arg;var both:i64=both_arg;var il:i64=il_arg;
 if service(e,50,0,0,0)==0 {return 0;}let map=service(e,51,il,ne(ids,1),ne(ids,0));if map==0 {return 0;}var weights:i64[4]=default<i64[4]>();weights[0]=up;weights[1]=gate;weights[2]=down;weights[3]=both;for i in 4 {if weights[i]!=0&&service(e,16,weights[i],0,0)==0 {return 0;}}
 let original=ids;if !p_is_contiguous(ids) {ids=p_cont(e,ids);}ids=p_reshape_1d(e,ids,p_nelements(ids));let slots=p_get_rows(e,map,ids);if service(e,53,il,slots,0)==0 {return 0;}let ignored=service(e,54,il,slots,0);observe(e,slots,131,il);p_build_forward_expand(e,slots);return p_reshape_2d(e,slots,ne(original,0),ne(original,1));
}
fn position_input(e_arg:i64)->i64 {var e:i64=e_arg;
 let inp=service(e,80,0,0,0);let pos=p_new_tensor_1d(e,26,get(e,11,0)*get(e,36,0));let stored=service(e,81,inp,pos,0);p_set_input(pos);observe(e,pos,132,-1);let ignored=service(e,84,inp,0,0);return pos;
}
fn outid_input(e_arg:i64)->i64 {var e:i64=e_arg;
 let inp=service(e,80,1,0,0);let ids=p_new_tensor_1d(e,26,get(e,37,0));let stored=service(e,81,inp,ids,1);p_set_input(ids);let named=service(e,83,ids,0,0);let ignored=service(e,84,inp,0,0);return ids;
}

extern "fg153_am_lget" fn lget(e:i64,k:i64)->i64;
extern "fg153_am_bind" fn binding(e:i64,k:i64,il:i64,nd:i64,a:i64,b:i64,c:i64,flags:i64)->i64;
extern "fg153_am_recurrent" fn recurrent(e:i64,il:i64,v:bool);
fn bind_qkv(e:i64,il:i64,dim:i64,q:i64,k:i64,v:i64,flags:i64) {
 let nr=lget(e,20);let skip=lget(e,21);let virt=lget(e,22);let total=q+k+v;
 if (flags&skip)!=0 {let f=nr|skip;let a=binding(e,7,il,2,dim,total,1,f|virt);let b=binding(e,39,il,1,total,1,1,f|virt);let c=binding(e,43,il,2,dim,q,1,f);let d=binding(e,45,il,2,dim,k,1,f);let e0=binding(e,47,il,2,dim,v,1,f);let f0=binding(e,40,il,1,q,1,1,f);let g=binding(e,41,il,1,k,1,1,f);let h=binding(e,42,il,1,v,1,1,f);return;}
 let fused=binding(e,7,il,2,dim,total,1,nr|virt);if fused!=0 {let b=binding(e,39,il,1,total,1,1,nr|virt);if b==0 {let q0=binding(e,40,il,1,q,1,1,nr);let k0=binding(e,41,il,1,k,1,1,nr);let v0=binding(e,42,il,1,v,1,1,nr);}}
 else {let q0=binding(e,43,il,2,dim,q,1,flags);let k0=binding(e,45,il,2,dim,k,1,flags);let v0=binding(e,47,il,2,dim,v,1,flags);let qb=binding(e,40,il,1,q,1,1,nr);let kb=binding(e,41,il,1,k,1,1,nr);let vb=binding(e,42,il,1,v,1,1,nr);}
}
fn bind_experts(e:i64,il:i64,dim:i64,ff:i64,n:i64,flags:i64) {
 let nr=lget(e,20);let skip=lget(e,21);let virt=lget(e,22);if (flags&skip)!=0 {let f=nr|skip;let a=binding(e,29,il,3,dim,2*ff,n,f|virt);let b=binding(e,27,il,3,dim,ff,n,f);let c=binding(e,26,il,3,dim,ff,n,f);return;}
 let both=binding(e,29,il,3,dim,2*ff,n,nr);if both==0 {let a=binding(e,27,il,3,dim,ff,n,flags);let b=binding(e,26,il,3,dim,ff,n,flags);}
}
fn bind_main(e:i64) {
 let dim=lget(e,0);let vocab=lget(e,1);let nl=lget(e,2);let nexp=lget(e,3);let nused=lget(e,4);let ff=lget(e,5);let ffexp=lget(e,6);let shared=lget(e,7);let head=lget(e,8);let heads=lget(e,9);let kd=lget(e,10);let vd=lget(e,11);let state=lget(e,12);let groups=lget(e,13);let rank=lget(e,14);let conv=lget(e,15);let flags=lget(e,16);let nr=lget(e,20);let dup=lget(e,23);
 let embedding=binding(e,0,-1,2,dim,vocab,1,0);let norm=binding(e,3,-1,1,dim,1,1,0);let out=binding(e,5,-1,2,dim,vocab,1,nr);if out==0 {let tied=binding(e,5,-1,2,dim,vocab,1,dup);}
 for il in nl {let expdim=if ffexp!=0 {ffexp} else {ff/nused};let shareddim=if shared!=0 {shared} else {ff};let keydim=state*groups;let valdim=state*rank;let convdim=2*keydim+valdim;
 let norm1=binding(e,1,il,1,dim,1,1,flags);let norm2=binding(e,2,il,1,dim,1,1,flags);
 if lget(e,100+il)==0 {bind_qkv(e,il,dim,2*head*heads,kd,vd,flags);let a=binding(e,13,il,2,head*heads,dim,1,flags);let b=binding(e,11,il,1,head,1,1,flags);let c=binding(e,12,il,1,head,1,1,flags);}
 else {let a=binding(e,7,il,2,dim,2*keydim+valdim,1,nr);let b=binding(e,9,il,2,dim,valdim,1,nr);let c=binding(e,21,il,2,conv,convdim,1,flags);let d=binding(e,19,il,1,rank,1,1,flags);let f=binding(e,20,il,1,rank,1,1,flags);let g=binding(e,15,il,2,dim,rank,1,flags);let h=binding(e,17,il,2,dim,rank,1,flags);let j=binding(e,22,il,1,state,1,1,flags);let k=binding(e,23,il,2,valdim,dim,1,flags);}
 let r=binding(e,25,il,2,dim,nexp,1,flags);let d=binding(e,28,il,3,expdim,dim,nexp,flags);bind_experts(e,il,dim,expdim,nexp,flags);let a=binding(e,6,il,1,dim,1,1,flags);let b=binding(e,35,il,2,dim,shareddim,1,flags);let c=binding(e,33,il,2,dim,shareddim,1,flags);let f=binding(e,37,il,2,shareddim,dim,1,flags);
 }
}
fn recurrent_policy(e:i64) {let all=lget(e,30);let main=lget(e,2);let interval=lget(e,31);assert(interval>0);for il in all {recurrent(e,il,il<main&&(il+1)%interval!=0);}}
fn binding_entry(e:i64,mode:i64) {if mode==0 {bind_main(e);}else {recurrent_policy(e);}}
