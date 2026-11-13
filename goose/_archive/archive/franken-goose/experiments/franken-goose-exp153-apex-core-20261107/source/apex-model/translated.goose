fn build_main(e:i64)->i64 {

    var n_embd_head:i64= get(e,1,0);

    assert(n_embd_head == get(e,2,0));

    let sections:i64=0;
    

    var cur:i64=0;
    var inpL:i64=0;

    inpL = input_embed(e,wt(e,-1,0));

    observe(e,inpL,0, -1);

    var inp:i64=service(e,0,0,0,0);

    var inp_pos:i64= service(e,1,0,0,0);
    var inp_out_ids:i64= service(e,2,0,0,0);

    
    for il in get(e,6,0) {
        put(e,0,il,inpL);

        var inpSA:i64= inpL;

        cur = build_norm(e,inpL, wt(e,il,1), 0, LLM_NORM_RMS, il);
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

        
        var attn_post_norm:i64= build_norm(e,cur, wt(e,il,2), 0, LLM_NORM_RMS, il);
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

    
    cur = build_norm(e,cur, wt(e,-1,3), 0, LLM_NORM_RMS, -1);

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

fn build_qkvz(e:i64,input:i64,il:i64)->Pair {

    var n_seqs:i64= get(e,7,0);
    var n_seq_tokens:i64= get(e,8,0);

    var qkv_mixed:i64= build_lora_mm(e,wt(e,il,7), input, wt(e,il,8));
    qkv_mixed = p_reshape_3d(e, qkv_mixed, ne(qkv_mixed,0), n_seq_tokens, n_seqs);
    observe(e,qkv_mixed,10, il);

    var z:i64= build_lora_mm(e,wt(e,il,9), input, wt(e,il,10));
    observe(e,z,11, il);

    return Pair { a:qkv_mixed,b:z };

}

fn build_norm_gated(e:i64,input:i64,weights:i64,gate:i64,layer:i64)->i64 {

    var normalized:i64= build_norm(e,input, weights, 0, LLM_NORM_RMS, layer);
    var gated_silu:i64= p_silu(e, gate);

    return p_mul(e, normalized, gated_silu);

}

fn build_layer_attn(e:i64,inp:i64,cur:i64,inp_pos:i64,sections:i64,il:i64)->i64 {

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

    
    Qcur = build_norm(e,Qcur, wt(e,il,11), 0, LLM_NORM_RMS, il);
    observe(e,Qcur,16, il);

    
    Kcur = p_reshape_3d(e, Kcur, n_embd_head, get(e,10,0), get(e,11,0));
    Kcur = build_norm(e,Kcur, wt(e,il,12), 0, LLM_NORM_RMS, il);
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

    
    var kq_scale:f32= fget(e,6,0) == 0.0 ? 1.0 / fsqrt((n_embd_head as f32)) : fget(e,6,0);

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

fn build_layer_attn_linear(e:i64,inp:i64,cur:i64,il:i64)->i64 {

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

    
    
    

    
    
    if (num_k_heads != num_v_heads && (!get(e,20,0) || !get(e,21,0))) {
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

fn build_layer_ffn(e:i64,cur:i64,il:i64)->i64 {

    
    assert(wt(e,il,25) != 0);

    var moe_out:i64=
        build_moe_ffn(e,cur,
            wt(e,il,25),
            wt(e,il,26),
            wt(e,il,27),
            wt(e,il,28),
            0,
            get(e,22,0), get(e,23,0),
            LLM_FFN_SILU, true,
            fget(e,8,0),
            LLAMA_EXPERT_GATING_FUNC_TYPE_SOFTMAX, il,
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
                LLM_FFN_SILU, LLM_FFN_PAR, il);
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

fn build_delta_net_chunking(e:i64,q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64)->Pair {

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

    var scale:f32= 1.0 / fsqrt(S_k);

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

    var pad:i64= (CS - n_tokens % CS) % CS;
    var n_chunks:i64= (n_tokens + pad) / CS;

    q = p_pad(e, q, 0, pad, 0, 0);
    k = p_pad(e, k, 0, pad, 0, 0);
    v = p_pad(e, v, 0, pad, 0, 0);
    g = p_pad(e, g, 0, pad, 0, 0);
    b = p_pad(e, b, 0, pad, 0, 0);

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
        decay_mask = p_tri(e, decay_mask, GGML_TRI_TYPE_LOWER_DIAG);
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
        decay_mask = p_tri(e, decay_mask, GGML_TRI_TYPE_LOWER_DIAG);
        decay_mask = p_exp(e, decay_mask);
        observe(e,decay_mask,52, il);

        
        kb = p_mul_mat(e, k,  k_b);
        kb = p_mul(e, kb, decay_mask);

        
        kq = p_mul_mat(e, k, q);
        kq = p_mul(e, kq, decay_mask);
    }

    kq = p_tri(e, kq, GGML_TRI_TYPE_LOWER_DIAG);
    observe(e,kq,53, il);

    
    var attn:i64=0;
    attn = p_tri(e, kb, GGML_TRI_TYPE_LOWER);
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

fn build_delta_net_autoregressive(e:i64,q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64)->Pair {

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

    var scale:f32= 1.0 / fsqrt(S_k);

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

fn build_delta_net_fused(e:i64,q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64)->Pair {

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
        fused(e,LLM_FUSED_OP_GDN_AR,result,il);
    } else {
        fused(e,LLM_FUSED_OP_GDN_CH,result,il);
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

fn build_delta_net(e:i64,q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64)->Pair {

    var n_seq_tokens:i64= ne(q,2);

    if (n_seq_tokens == 1) {
        if (get(e,20,0)) {
            return build_delta_net_fused(e,q, k, v, g, b, s, il);
        }
        return build_delta_net_autoregressive(e,q, k, v, g, b, s, il);
    }

    if (get(e,21,0)) {
        return build_delta_net_fused(e,q, k, v, g, b, s, il);
    }

    return build_delta_net_chunking(e,q, k, v, g, b, s, il);

}

fn get_slice_2d(e:i64,t:i64,c:i64)->i64 {

    return p_view_4d(e, t, ne(t,0), ne(t,1), 1, ne(t,3),
        nb(t,1), nb(t,2), nb(t,3), nb(t,2) * c);

}

fn build_conv_state(e:i64,inp:i64,conv_states_all:i64,qkv_mixed:i64,conv_kernel_size:i64,conv_channels:i64,il:i64)->i64 {

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

fn build_recurrent_attn(e:i64,inp:i64,ssm_states_all:i64,q:i64,k:i64,v:i64,g:i64,b:i64,s:i64,il:i64)->i64 {

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
        fused(e,LLM_FUSED_OP_GDN_CH,gdn_out,il);
    } else {
        fused(e,LLM_FUSED_OP_GDN_AR,gdn_out,il);
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

fn flow_main(e:i64)->i64 {

    var n_embd_head:i64= get(e,1,0);

    assert(n_embd_head == get(e,2,0));

    let sections:i64=0;
    

    var cur:i64=0;
    var inpL:i64=0;

    inpL = input_embed(e,wt(e,-1,0));

    observe(e,inpL,0, -1);

    var inp:i64=0;

    var inp_pos:i64= service(e,1,0,0,0);
    var inp_out_ids:i64= service(e,2,0,0,0);

    
    for il in get(e,6,0) {
        put(e,0,il,inpL);

        var inpSA:i64= inpL;

        cur = build_norm(e,inpL, wt(e,il,1), 0, LLM_NORM_RMS, il);
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

        
        var attn_post_norm:i64= build_norm(e,cur, wt(e,il,2), 0, LLM_NORM_RMS, il);
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

    
    cur = build_norm(e,cur, wt(e,-1,3), 0, LLM_NORM_RMS, -1);

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
