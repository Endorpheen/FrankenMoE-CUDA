fn giga_bind_one(data: const u8[:], state: i64[:], tensors: i64[:], slots: i64[:],
                 layer: i64, suffix: const u8[:], d0: i64, d1: i64, d2: i64, f32_only: bool) -> i64 {
    var name: u8[64] = [0; 64];
    var n: i64 = 0;
    if layer >= 0 {
        name[0] = 98; name[1] = 108; name[2] = 107; name[3] = 46;
        n = 4;
        if layer >= 10 { name[n] = (48 + layer / 10) as u8; n += 1; }
        name[n] = (48 + layer % 10) as u8; n += 1;
        name[n] = 46; n += 1;
    }
    if suffix.len > name.len - n { return -1; }
    for i in suffix.len { name[n + i] = suffix[i]; }
    let id = gr_find_tensor(data, state, tensors, slots, name[0 .. n + suffix.len]);
    var shape: i64[9] = [0; 9];
    if gr_tensor_info(state, tensors, id, shape) != 0 ||
       shape[2] != d0 || shape[3] != d1 || shape[4] != d2 || shape[5] != 1 { return -1; }
    let ty = shape[0];
    if ty != 0 && (f32_only || (ty != 6 && ty != 12 && ty != 14)) { return -1; }
    return id;
}

export fn giga_bind_tensors(data: const u8[:], state: i64[:], tensors: i64[:],
                             slots: i64[:], kinds: i64[:], ids: i64[:], offsets: i64[:]) -> i64 {
    if kinds.len < 33 || ids.len < 1036 || offsets.len < 34 { return -1; }
    if kinds[32] != 2 { return -1; }
    var recurrent: i64 = 0;
    for layer in 32 {
        if kinds[layer] < 0 || kinds[layer] > 1 { return -1; }
        if kinds[layer] == 0 { recurrent += 1; }
    }
    if recurrent != 24 { return -1; }
    var n: i64 = 0;
    ids[n] = giga_bind_one(data, state, tensors, slots, -1, "output_norm.weight", 2560, 1, 1, true); n += 1;
    ids[n] = giga_bind_one(data, state, tensors, slots, -1, "output_norm_gate_up.weight", 2560, 16, 1, true); n += 1;
    ids[n] = giga_bind_one(data, state, tensors, slots, -1, "output_norm_gate_down.weight", 16, 2560, 1, true); n += 1;
    ids[n] = giga_bind_one(data, state, tensors, slots, -1, "token_embd.weight", 2560, 128256, 1, false); n += 1;
    ids[n] = giga_bind_one(data, state, tensors, slots, -1, "output.weight", 2560, 128256, 1, false); n += 1;
    for layer in 33 {
        offsets[layer] = n;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_norm.weight", 2560, 1, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_norm_gate_up.weight", 2560, 16, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_norm_gate_down.weight", 16, 2560, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "post_attention_norm.weight", 2560, 1, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "post_attention_norm_gate_up.weight", 2560, 16, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "post_attention_norm_gate_down.weight", 16, 2560, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_norm.weight", 2560, 1, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_norm_gate_up.weight", 2560, 16, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_norm_gate_down.weight", 16, 2560, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "post_ffw_norm.weight", 2560, 1, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "post_ffw_norm_gate_up.weight", 2560, 16, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "post_ffw_norm_gate_down.weight", 16, 2560, 1, true); n += 1;
        ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_gate.weight", 2560, 4096, 1, false); n += 1;
        if kinds[layer] == 0 {
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_qkv.weight", 2560, 8192, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_conv1d.weight", 4, 8192, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_a", 32, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_dt.bias", 32, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_alpha.weight", 2560, 32, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_beta.weight", 2560, 32, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_norm.weight", 128, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ssm_out.weight", 4096, 2560, 1, false); n += 1;
        } else {
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_q_a.weight", 2560, 1536, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_q_b.weight", 1536, 6144, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_kv_a_mqa.weight", 2560, 576, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_k_b.weight", 128, 512, 32, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_v_b.weight", 512, 128, 32, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_output.weight", 4096, 2560, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_q_a_norm.weight", 1536, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_q_a_norm_gate_up.weight", 1536, 16, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_q_a_norm_gate_down.weight", 16, 1536, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_kv_a_norm.weight", 512, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_kv_a_norm_gate_up.weight", 512, 16, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "attn_kv_a_norm_gate_down.weight", 16, 512, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "indexer.attn_q_b.weight", 1536, 8192, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "indexer.attn_k.weight", 2560, 128, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "indexer.k_norm.weight", 128, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "indexer.proj.weight", 2560, 64, 1, false); n += 1;
        }
        if layer == 0 || kinds[layer] == 2 {
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_gate.weight", 2560, 8192, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_up.weight", 2560, 8192, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_down.weight", 8192, 2560, 1, false); n += 1;
        } else {
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_gate_inp.weight", 2560, 192, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "exp_probs_b.bias", 192, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_gate_exps.weight", 2560, 768, 192, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_up_exps.weight", 2560, 768, 192, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_down_exps.weight", 768, 2560, 192, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_gate_shexp.weight", 2560, 768, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_up_shexp.weight", 2560, 768, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "ffn_down_shexp.weight", 768, 2560, 1, false); n += 1;
        }
        if kinds[layer] == 2 {
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.enorm.weight", 2560, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.enorm_gate_up.weight", 2560, 16, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.enorm_gate_down.weight", 16, 2560, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.hnorm.weight", 2560, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.hnorm_gate_up.weight", 2560, 16, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.hnorm_gate_down.weight", 16, 2560, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.shared_head_norm.weight", 2560, 1, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.shared_head_norm_gate_up.weight", 2560, 16, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.shared_head_norm_gate_down.weight", 16, 2560, 1, true); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.eh_proj.weight", 5120, 2560, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.embed_tokens.weight", 2560, 128256, 1, false); n += 1;
            ids[n] = giga_bind_one(data, state, tensors, slots, layer, "nextn.shared_head_head.weight", 2560, 128256, 1, false); n += 1;
        }
    }
    offsets[33] = n;
    if n != 1036 { return -2; }
    for i in n { if ids[i] < 0 { return -(i + 3); } }
    return n;
}
