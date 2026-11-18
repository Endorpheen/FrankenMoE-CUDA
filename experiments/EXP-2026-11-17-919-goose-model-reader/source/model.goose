// Qwen3.5 model planning from validated borrowed GGUF records, not native GGUF.
// Mapped bytes and table storage stay borrowed. This module owns GGML context/
// tensor lifetime; the caller must keep the mapping/tables until model close.
// Owner12: magic9191,map_base,file_bytes,weight_ctx,state_ctx,layers,live_frames,
// hidden,vocab,generation,capacity,bound_weights. Generation survives close.

fn mr_integer(data: const u8[:], st: i64[:], kv: i64[:], slots: i64[:],
                key: const u8[:], missing: i64, scratch: i64[:]) -> i64 {
    let index: i64 = gr_lookup(data, st, kv, slots, key, 8, 2);
    if index < 0 { return missing; }
    if gr_meta_int_impl(data, st, kv, index, scratch) != 0 { return -1; }
    return scratch[0];
}

fn mr_float(data: const u8[:], st: i64[:], kv: i64[:], slots: i64[:],
              key: const u8[:], missing: i64, scratch: i64[:]) -> i64 {
    let index: i64 = gr_lookup(data, st, kv, slots, key, 8, 2);
    if index < 0 { return missing; }
    if gr_meta_f32_bits_impl(data, st, kv, index, scratch) != 0 { return -1; }
    return scratch[0];
}

fn mr_text(data: const u8[:], st: i64[:], kv: i64[:], slots: i64[:], key: const u8[:], expected: const u8[:]) -> bool {
    let index: i64 = gr_lookup(data, st, kv, slots, key, 8, 2);
    if index < 0 || kv[index * 8 + 2] != 8 { return false; }
    let at: i64 = kv[index * 8 + 4]; let n: i64 = kv[index * 8 + 6];
    return gr_fits(data, at, n) && data[at .. at + n] == expected;
}

fn mr_no_clamp(data: const u8[:], st: i64[:], kv: i64[:], slots: i64[:], key: const u8[:]) -> bool {
    let index: i64 = gr_lookup(data, st, kv, slots, key, 8, 2);
    if index < 0 { return true; }
    let row: i64 = index * 8; let wire: i64 = kv[row + 2];
    if wire != 6 && !(wire == 9 && kv[row + 3] == 6) { return false; }
    let n: i64 = if wire == 6 { 1 } else { kv[row + 5] };
    let at: i64 = kv[row + 4]; if n < 0 || n > data.len / 4 || !gr_fits(data, at, n * 4) { return false; }
    var i: i64 = 0;
    while i < n { if !(float_bits(gr_bits(data, at + i * 4, 4) as i64) <= 0.000001) { return false; } i += 1; }
    return true;
}

fn mr_name(layer: i64, suffix: const u8[:], out: u8[:]) -> i64 {
    if layer < 0 || layer > 127 || out.len < suffix.len + 8 { return -1; }
    out[0] = 98; out[1] = 108; out[2] = 107; out[3] = 46;
    var div: i64 = if layer >= 100 { 100 } else { if layer >= 10 { 10 } else { 1 } };
    var at: i64 = 4;
    while div > 0 { out[at] = (48 + (layer / div) % 10) as u8; div /= 10; at += 1; }
    out[at] = 46; at += 1; var i: i64 = 0;
    while i < suffix.len { out[at] = suffix[i]; at += 1; i += 1; }
    return at;
}

fn mr_branch_name(kind: i64, i: i64) -> const u8[:] {
    if i == 0 { return "attn_norm.weight"; }
    if kind == 0 {
        if i == 1 { return "attn_q.weight"; } if i == 2 { return "attn_k.weight"; }
        if i == 3 { return "attn_v.weight"; } if i == 4 { return "attn_output.weight"; }
        if i == 5 { return "attn_q_norm.weight"; } if i == 6 { return "attn_k_norm.weight"; }
        return "post_attention_norm.weight";
    }
    if i == 1 { return "attn_qkv.weight"; } if i == 2 { return "attn_gate.weight"; }
    if i == 3 { return "ssm_beta.weight"; } if i == 4 { return "ssm_alpha.weight"; }
    if i == 5 { return "ssm_dt.bias"; } if i == 6 { return "ssm_a"; }
    if i == 7 { return "ssm_conv1d.weight"; } if i == 8 { return "ssm_norm.weight"; }
    if i == 9 { return "ssm_out.weight"; } return "post_attention_norm.weight";
}

fn mr_moe_name(i: i64) -> const u8[:] {
    if i == 0 { return "ffn_gate_inp.weight"; } if i == 1 { return "ffn_up_exps.weight"; }
    if i == 2 { return "ffn_gate_exps.weight"; } if i == 3 { return "ffn_down_exps.weight"; }
    if i == 4 { return "ffn_up_shexp.weight"; } if i == 5 { return "ffn_gate_shexp.weight"; }
    if i == 6 { return "ffn_down_shexp.weight"; } return "ffn_gate_inp_shexp.weight";
}

fn mr_bind(data: const u8[:], st: i64[:], tensors: i64[:], slots: i64[:], key: const u8[:],
             c: i64, base: u64, bytes: i64, usage: i64[:]) -> i64 {
    let index: i64 = gr_lookup(data, st, tensors, slots, key, 12, 3);
    if index < 0 { return 0; }
    if index >= usage.len { return 0; }
    usage[index] = 1;
    let row: i64 = index * 12;
    return q35_bind_weight(c, base, bytes, tensors[row + 10], tensors[row + 2], tensors[row + 4 .. row + 8]);
}

// Arithmetic qualification for the current baseline family. The format reader
// remains generic; scaled/other unqualified quantizations are not silently used.
fn mr_matrix_type(t: i64) -> bool {
    let dtype: i64 = prop(t, 0);
    return dtype == 0 || dtype == 1 || dtype == 12 || dtype == 14 || dtype == 30;
}

fn mr_layer_ok(row: i64[:], hidden: i64, attn_head: i64, attn: i64[:], gdn: i64[:], moe: i64[:], ff: i64, shared: i64) -> bool {
    if !qm_shape(row[1], hidden, 1, 1) || prop(row[1], 0) != 0 { return false; }
    var j: i64 = 2;
    if row[0] == 1 {
        let h: i64 = gdn[0]; let nk: i64 = gdn[1]; let nv: i64 = gdn[2]; let channels: i64 = h * (2 * nk + nv);
        if !qm_shape(row[2], hidden, channels, 1) || !qm_shape(row[3], hidden, h * nv, 1) ||
           !qm_shape(row[4], hidden, nv, 1) || !qm_shape(row[5], hidden, nv, 1) ||
           !qm_shape(row[6], nv, 1, 1) || !qm_shape(row[7], nv, 1, 1) || !qm_shape(row[8], gdn[3], channels, 1) ||
           !qm_shape(row[9], h, 1, 1) || !qm_shape(row[10], h * nv, hidden, 1) || !qm_shape(row[11], hidden, 1, 1) { return false; }
        while j <= 5 { if !mr_matrix_type(row[j]) { return false; } j += 1; }
        j = 6; while j <= 9 { if prop(row[j], 0) != 0 { return false; } j += 1; }
        if !mr_matrix_type(row[10]) || prop(row[11], 0) != 0 { return false; }
    } else {
        let hd: i64 = attn_head;
        if hd <= 0 || !qm_shape(row[2], hidden, hd * attn[0] * 2, 1) || !qm_shape(row[3], hidden, hd * attn[1], 1) ||
           !qm_shape(row[4], hidden, hd * attn[1], 1) || !qm_shape(row[5], hd * attn[0], hidden, 1) ||
           !qm_shape(row[6], hd, 1, 1) || !qm_shape(row[7], hd, 1, 1) || !qm_shape(row[8], hidden, 1, 1) { return false; }
        while j <= 5 { if !mr_matrix_type(row[j]) { return false; } j += 1; }
        j = 6; while j <= 8 { if prop(row[j], 0) != 0 { return false; } j += 1; }
    }
    if !qm_shape(row[12], hidden, moe[0], 1) || prop(row[12], 0) != 0 ||
       !qm_shape(row[13], hidden, ff, moe[0]) || !qm_shape(row[14], hidden, ff, moe[0]) || !qm_shape(row[15], ff, hidden, moe[0]) ||
       !qm_shape(row[16], hidden, shared, 1) || !qm_shape(row[17], hidden, shared, 1) || !qm_shape(row[18], shared, hidden, 1) ||
       !qm_shape(row[19], hidden, 1, 1) || prop(row[19], 0) != 0 { return false; }
    j = 13; while j < 19 { if !mr_matrix_type(row[j]) { return false; } j += 1; }
    return true;
}

fn mr_close_contexts(owner: i64[:]) {
    let states: i64 = owner[4]; let weights: i64 = owner[3];
    owner[0] = 0; owner[3] = 0; owner[4] = 0; qo_context_free(states); qo_context_free(weights);
}

// Output tables retain the EXP918 record/config ABI. No mmap call or native
// GGUF accessor is hidden here; unsupported graph features fail explicitly.
export fn qr_model_open(data: const u8[:], mapping: u64, file_bytes: i64, parsed: i64[:],
                         kv: i64[:], tensors: i64[:], kv_slots: i64[:], tensor_slots: i64[:],
                         capacity: i64, state_budget: i64, owner: i64[:], layers: i64[:], handles: i64[:],
                         attn: i64[:], gdn: i64[:], moe: i64[:], sections: i64[:], session: i64[:],
                         name_work: u8[:], scratch: i64[:]) -> i64 {
    if owner.len < 12 || owner[0] != 0 || owner[9] < 0 || owner[9] == 9223372036854775807 || parsed.len < 8 || parsed[0] != 919 ||
       handles.len < 4 || attn.len < 14 || gdn.len < 7 || moe.len < 3 || sections.len < 4 || session.len < 6 ||
       parsed[2] < 0 || parsed[2] > kv.len / 8 || parsed[3] < 0 || parsed[3] > tensors.len / 12 ||
       name_work.len < 64 || scratch.len < 4 + parsed[3] || mapping == 0 || mapping == 18446744073709551615 ||
       file_bytes < data.len || parsed[5] > file_bytes || parsed[6] > file_bytes - parsed[5] ||
       capacity <= 0 || capacity > 2147483647 || capacity % 32 != 0 || state_budget < 1048576 || state_budget > 2147483647 { return -1; }
    if !mr_text(data, parsed, kv, kv_slots, "general.architecture", "qwen35moe") { return -2; }
    let all: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.block_count", -1, scratch);
    let mtp: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.nextn_predict_layers", 0, scratch);
    if all <= 0 || all > 128 || mtp < 0 || mtp > 1 || mtp >= all { return -2; }
    let count: i64 = all - mtp;
    let hidden: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.embedding_length", -1, scratch);
    let heads: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.attention.head_count", -1, scratch);
    let hk: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.attention.head_count_kv", -1, scratch);
    let hd: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.attention.key_length", -1, scratch);
    let hv: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.attention.value_length", hd, scratch);
    let head: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.ssm.state_size", -1, scratch);
    let nk: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.ssm.group_count", -1, scratch);
    let nv: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.ssm.time_step_rank", -1, scratch);
    let kernel: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.ssm.conv_kernel", -1, scratch);
    let inner: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.ssm.inner_size", -1, scratch);
    let experts: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.expert_count", -1, scratch);
    let used: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.expert_used_count", -1, scratch);
    let group: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.expert_group_count", 1, scratch);
    let ff: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.expert_feed_forward_length", -1, scratch);
    let shared: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.expert_shared_feed_forward_length", -1, scratch);
    let epsilon: i64 = mr_float(data, parsed, kv, kv_slots, "qwen35moe.attention.layer_norm_rms_epsilon", -1, scratch);
    let base_freq: i64 = mr_float(data, parsed, kv, kv_slots, "qwen35moe.rope.freq_base", -1, scratch);
    let original: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.context_length", -1, scratch);
    let rotated: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.rope.dimension_count", -1, scratch);
    let scale: i64 = mr_float(data, parsed, kv, kv_slots, "qwen35moe.expert_weights_scale", 0, scratch);
    if layers.len / 32 < count || hidden <= 0 || hidden > 65536 || heads <= 0 || heads > 512 || hk <= 0 || hk > 512 || heads % hk != 0 ||
       hd <= 0 || hd > 4096 || hd % 32 != 0 || hv != hd || experts <= 0 || experts > 4096 || used <= 0 || used > 64 || used > experts ||
       group < 0 || group > 1 || original <= 0 || original > 2147483647 || rotated <= 0 || rotated > hd || rotated % 2 != 0 ||
       ff <= 0 || ff > 65536 || shared <= 0 || shared > 65536 || epsilon < 0 || base_freq < 0 || !(float_bits(base_freq) > 0.0) ||
       !(float_bits(base_freq) <= 3.4028234663852886e38) || scale < 0 || !(float_bits(scale) >= 0.0) || !(float_bits(scale) <= 3.4028234663852886e38) ||
       !mr_no_clamp(data, parsed, kv, kv_slots, "qwen35moe.swiglu_clamp_exp") ||
       !mr_no_clamp(data, parsed, kv, kv_slots, "qwen35moe.swiglu_clamp_shexp") { return -2; }
    if gr_lookup(data, parsed, kv, kv_slots, "qwen35moe.rope.scaling.type", 8, 2) >= 0 &&
       !mr_text(data, parsed, kv, kv_slots, "qwen35moe.rope.scaling.type", "none") { return -2; }
    if q35_gdn_config(head, nk, nv, kernel, epsilon, gdn) != 7 || inner != head * nv { return -2; }
    attn[0] = heads; attn[1] = hk; attn[2] = rotated; attn[3] = 40; attn[4] = original; attn[5] = epsilon; attn[6] = base_freq;
    attn[7] = 1065353216; attn[8] = 0; attn[9] = 1065353216; attn[10] = 1107296256; attn[11] = 1065353216;
    attn[12] = qg_positive_bits(1.0 / qg_sqrt(hd as f32)); attn[13] = 10;
    moe[0] = experts; moe[1] = used; moe[2] = scale;
    let rs: i64 = gr_lookup(data, parsed, kv, kv_slots, "qwen35moe.rope.dimension_sections", 8, 2);
    if rs < 0 || kv[rs * 8 + 2] != 9 || kv[rs * 8 + 5] != 4 { return -2; }
    var i: i64 = 0; var section_sum: i64 = 0;
    while i < 4 { if gr_array_int_impl(data, parsed, kv, rs, i, scratch) != 0 || scratch[0] < 0 || scratch[0] > rotated { return -2; } sections[i] = scratch[0]; section_sum += scratch[0]; i += 1; }
    if section_sum * 2 != rotated { return -2; }
    let flags: i64 = gr_lookup(data, parsed, kv, kv_slots, "qwen35moe.attention.recurrent_layers", 8, 2);
    let interval: i64 = mr_integer(data, parsed, kv, kv_slots, "qwen35moe.full_attention_interval", 4, scratch);
    if interval <= 0 || (flags >= 0 && (kv[flags * 8 + 2] != 9 || kv[flags * 8 + 3] != 7 || kv[flags * 8 + 5] < count)) { return -2; }
    let conv_elements: i64 = (kernel - 1) * head * (2 * nk + nv);
    let matrix_elements: i64 = head * head * nv;
    let cache_bytes: i64 = (hd * hk / 32) * 18 * capacity;
    if conv_elements > 9223372036854775807 / 4 || matrix_elements > 9223372036854775807 / 4 || cache_bytes > 9223372036854775807 / 2 { return -2; }
    let recurrent_bytes: i64 = (conv_elements + matrix_elements) * 4;
    var needed: i64 = 1048576; i = 0;
    while i < count {
        var kind: i64 = if (i + 1) % interval != 0 { 1 } else { 0 };
        if flags >= 0 { if gr_array_int_impl(data, parsed, kv, flags, i, scratch) != 0 { return -2; } kind = scratch[0]; }
        if kind != 0 && kind != 1 { return -2; }
        let bytes: i64 = if kind == 1 { recurrent_bytes } else { cache_bytes * 2 };
        if bytes > state_budget - needed { return -3; }
        needed += bytes; layers[i * 32] = kind; i += 1;
    }
    i = 0; while i < parsed[3] { scratch[4 + i] = 0; i += 1; }
    owner[3] = qo_context_new(8388608, 1);
    if owner[3] == 0 { return -3; }
    owner[4] = qo_context_new(state_budget, 0);
    if owner[4] == 0 { mr_close_contexts(owner); return -3; }
    handles[0] = mr_bind(data, parsed, tensors, tensor_slots, "token_embd.weight", owner[3], mapping, file_bytes, scratch[4 .. 4 + parsed[3]]);
    if gr_lookup(data, parsed, tensors, tensor_slots, "output.weight", 12, 3) < 0 { handles[1] = handles[0]; }
    else { handles[1] = mr_bind(data, parsed, tensors, tensor_slots, "output.weight", owner[3], mapping, file_bytes, scratch[4 .. 4 + parsed[3]]); }
    handles[2] = mr_bind(data, parsed, tensors, tensor_slots, "output_norm.weight", owner[3], mapping, file_bytes, scratch[4 .. 4 + parsed[3]]);
    if handles[0] == 0 || handles[1] == 0 || handles[2] == 0 { mr_close_contexts(owner); return -4; }
    let vocab: i64 = ne(handles[0], 1);
    if !qm_shape(handles[0], hidden, vocab, 1) || !qm_shape(handles[1], hidden, vocab, 1) ||
       !qm_shape(handles[2], hidden, 1, 1) || prop(handles[2], 0) != 0 || !mr_matrix_type(handles[0]) || !mr_matrix_type(handles[1]) { mr_close_contexts(owner); return -4; }
    var bound: i64 = if handles[0] == handles[1] { 2 } else { 3 };
    i = 0;
    while i < count {
        let row: i64 = i * 32; let kind: i64 = layers[row];
        var j: i64 = 1; while j < 32 { layers[row + j] = 0; j += 1; }
        let n: i64 = if kind == 1 { 11 } else { 8 }; j = 0;
        while j < n {
            let len: i64 = mr_name(i, mr_branch_name(kind, j), name_work);
            let weight: i64 = mr_bind(data, parsed, tensors, tensor_slots, name_work[0 .. len], owner[3], mapping, file_bytes, scratch[4 .. 4 + parsed[3]]);
            if weight == 0 { mr_close_contexts(owner); return -4; }
            layers[row + 1 + j] = weight; bound += 1; j += 1;
        }
        j = 0;
        while j < 8 {
            let len: i64 = mr_name(i, mr_moe_name(j), name_work);
            let weight: i64 = mr_bind(data, parsed, tensors, tensor_slots, name_work[0 .. len], owner[3], mapping, file_bytes, scratch[4 .. 4 + parsed[3]]);
            if weight == 0 { mr_close_contexts(owner); return -4; }
            layers[row + 12 + j] = weight; bound += 1; j += 1;
        }
        if !mr_layer_ok(layers[row .. row + 32], hidden, hd, attn, gdn, moe, ff, shared) { mr_close_contexts(owner); return -4; }
        if kind == 1 {
            layers[row + 20] = fresh(owner[4], 0, kernel - 1, head * (2 * nk + nv), 1, 1);
            layers[row + 21] = fresh(owner[4], 0, head, head, nv, 1);
            qo_tensor_zero(layers[row + 20]); qo_tensor_zero(layers[row + 21]);
        } else {
            layers[row + 22] = fresh(owner[4], 2, hd * hk, capacity, 1, 1);
            layers[row + 23] = fresh(owner[4], 2, hd * hk, capacity, 1, 1);
            qo_tensor_zero(layers[row + 22]); qo_tensor_zero(layers[row + 23]);
        }
        i += 1;
    }
    // Every file tensor must belong to this trunk or an explicitly skipped
    // MTP layer. Never turn an invalid/renamed output into a silent tied head.
    let skip_len: i64 = mr_name(count, "", name_work);
    i = 0;
    while i < parsed[3] {
        if scratch[4 + i] == 0 {
            let at: i64 = tensors[i * 12]; let n: i64 = tensors[i * 12 + 1];
            if mtp == 0 || n < skip_len || !gr_fits(data, at, n) || data[at .. at + skip_len] != name_work[0 .. skip_len] {
                mr_close_contexts(owner); return -4;
            }
        }
        i += 1;
    }
    // Native RoPE parameters read four I32 words. Keep them in model-owned
    // storage rather than requiring a caller's temporary native array.
    let rope_storage: i64 = fresh(owner[4], 26, 4, 1, 1, 1);
    let rope_bytes: u64 = qa_get(rope_storage, 4, 0);
    i = 0;
    while i < 4 {
        var j: i64 = 0;
        while j < 4 { qo_store_u8(rope_bytes + ((i * 4 + j) as u64), (sections[i] >> (8 * j)) & 255); j += 1; }
        i += 1;
    }
    handles[3] = rope_bytes as! i64;
    if q35_kv_init(capacity, 32, session) != 0 { mr_close_contexts(owner); return -4; }
    owner[1] = mapping as! i64; owner[2] = file_bytes; owner[5] = count; owner[6] = 0;
    owner[7] = hidden; owner[8] = ne(handles[0], 1); owner[9] += 1; owner[10] = capacity; owner[11] = bound; owner[0] = 9191;
    return count;
}

export fn qr_model_close(owner: i64[:]) -> i64 {
    if owner.len < 12 || owner[0] != 9191 || owner[6] != 0 { return -1; }
    mr_close_contexts(owner); return 0;
}

// Frame4: magic9192,context,generation,model mapping identity. Never copy an
// active frame record. Close refuses stale/wrong-owner and double releases.
export fn qr_frame_open(owner: i64[:], arena_bytes: i64, frame: i64[:]) -> i64 {
    if owner.len < 12 || owner[0] != 9191 || owner[6] < 0 || owner[6] == 9223372036854775807 ||
       frame.len < 4 || frame[0] != 0 || arena_bytes <= 0 || arena_bytes > 2147483647 { return 0; }
    let c: i64 = qo_context_new(arena_bytes, 0); if c == 0 { return 0; }
    owner[6] += 1; frame[1] = c; frame[2] = owner[9]; frame[3] = owner[1]; frame[0] = 9192; return c;
}

export fn qr_frame_close(owner: i64[:], frame: i64[:]) -> i64 {
    if owner.len < 12 || owner[0] != 9191 || owner[6] <= 0 || frame.len < 4 || frame[0] != 9192 ||
       frame[2] != owner[9] || frame[3] != owner[1] { return -1; }
    let c: i64 = frame[1]; frame[0] = 0; frame[1] = 0; owner[6] -= 1; qo_context_free(c); return 0;
}
