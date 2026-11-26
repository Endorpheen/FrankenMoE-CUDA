// The decoder visits shared weights by physical layer. KV and short-conv
// state are indexed by cache slot, including one slot per extra core visit.
const I2_VISIT_WORDS: i64 = 4;
const I2_MAX_LAYERS: i64 = 128;

fn i2_build_visits_impl(physical: i64, start: i64, end: i64, passes: i64, block_size: i64,
                          sliding: const i64[:], visits: i64[:]) -> i64 {
    if physical < 1 || physical > I2_MAX_LAYERS || start <= 0 || end <= start ||
       end > physical || passes < 2 || passes > I2_MAX_LAYERS || block_size < 1 ||
       start % block_size != 0 || end % block_size != 0 || sliding.len != physical { return -1; }

    let core = end - start;
    let count = physical + (passes - 1) * core;
    if count > I2_MAX_LAYERS || visits.len / I2_VISIT_WORDS < count { return -1; }
    for i in physical {
        if sliding[i] < 0 || sliding[i] > 1 { return -1; }
    }

    for i in count {
        var layer = i;
        var slot = i;
        var pass: i64 = -1;
        if i >= start && i < start + passes * core {
            pass = (i - start) / core;
            layer = start + (i - start) % core;
            slot = if pass == 0 { layer } else { physical + (pass - 1) * core + layer - start };
        } else if i >= start + passes * core {
            layer = i - (passes - 1) * core;
            slot = layer;
        }
        let at = i * I2_VISIT_WORDS;
        visits[at] = layer;
        visits[at + 1] = slot;
        visits[at + 2] = pass;
        visits[at + 3] = sliding[layer];
    }
    return count;
}

export fn i2_build_visits(physical: i64, start: i64, end: i64, passes: i64, block_size: i64,
                          sliding: const i64[:], visits: i64[:]) -> i64 {
    return i2_build_visits_impl(physical, start, end, passes, block_size, sliding, visits);
}

fn i2_find_key(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
               prefix: const u8[:], suffix: const u8[:]) -> i64 {
    var key: u8[96] = [0; 96];
    if prefix.len + suffix.len > key.len { return -1; }
    for i in prefix.len { key[i] = prefix[i]; }
    for i in suffix.len { key[prefix.len + i] = suffix[i]; }
    return gr_lookup(data, state, kv, slots, key[0 .. prefix.len + suffix.len], 8, 2);
}

fn i2_integer(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
              prefix: const u8[:], suffix: const u8[:], scratch: i64[:]) -> i64 {
    let id = i2_find_key(data, state, kv, slots, prefix, suffix);
    if gr_meta_int_impl(data, state, kv, id, scratch) != 0 { return -1; }
    return scratch[0];
}

// config: physical layers, core start/end, passes, gate rank, residual block size,
// sliding window, model capacity and execution visits.
export fn i2_read_layout(data: const u8[:], state: i64[:], kv: i64[:], slots: i64[:],
                         config: i64[:], visits: i64[:]) -> i64 {
    let legacy = mr_text(data, state, kv, slots, "general.architecture", "inkling2");
    if config.len < 9 || (!legacy && !mr_text(data, state, kv, slots, "general.architecture", "inkling2_loop")) {
        return -1;
    }
    let prefix = if legacy { "inkling2." } else { "inkling2_loop." };

    var scalar: i64[2] = [0; 2];
    let stored = i2_integer(data, state, kv, slots, prefix, "block_count", scalar);
    let start_key = if legacy { "loop.core_start" } else { "loop.start" };
    let end_key = if legacy { "loop.core_end" } else { "loop.end" };
    let pass_key = if legacy { "num_loops" } else { "loop.count" };
    let start = i2_integer(data, state, kv, slots, prefix, start_key, scalar);
    let end = i2_integer(data, state, kv, slots, prefix, end_key, scalar);
    let passes = i2_integer(data, state, kv, slots, prefix, pass_key, scalar);
    if stored < 1 || stored > I2_MAX_LAYERS || start < 1 || end <= start ||
       end > stored || passes < 2 || passes > I2_MAX_LAYERS { return -1; }
    let extra = (passes - 1) * (end - start);
    let physical = if legacy { stored - extra } else { stored };
    let rank = i2_integer(data, state, kv, slots, prefix, "loop.gate_rank", scalar);
    let block_size = i2_integer(data, state, kv, slots, prefix, "attn_res.block_size", scalar);
    let window = i2_integer(data, state, kv, slots, prefix, "attention.sliding_window", scalar);
    let capacity = i2_integer(data, state, kv, slots, prefix, "context_length", scalar);
    if physical < 1 || physical > I2_MAX_LAYERS || rank < 1 || window < 1 || capacity < window {
        return -1;
    }

    let pattern = i2_find_key(data, state, kv, slots, prefix, "attention.sliding_window_pattern");
    if gr_arr_n_impl(state, kv, pattern) != stored || kv[pattern * 8 + 3] != 7 {
        return -1;
    }
    var sliding: i64[128] = [0; 128];
    for i in physical {
        let logical = if legacy && i >= end { i + extra } else { i };
        if gr_array_int_impl(data, state, kv, pattern, logical, scalar) != 0 { return -1; }
        sliding[i] = scalar[0];
    }
    let count = i2_build_visits_impl(physical, start, end, passes, block_size, sliding[0 .. physical], visits);
    if count < 0 { return -1; }
    if legacy {
        if count != stored { return -1; }
        for i in count {
            let at = i * I2_VISIT_WORDS;
            if gr_array_int_impl(data, state, kv, pattern, i, scalar) != 0 ||
               scalar[0] != visits[at + 3] { return -1; }
            visits[at + 1] = i;
        }
    }

    config[0] = physical;
    config[1] = start;
    config[2] = end;
    config[3] = passes;
    config[4] = rank;
    config[5] = block_size;
    config[6] = window;
    config[7] = capacity;
    config[8] = count;
    return count;
}

// Weight bindings point at validated GGUF tensor records. They do not own
// memory. Type checks here qualify the selected AD-IQ4_XS text checkpoint.
const I2_TEXT_GLOBALS: i64 = 5;
const I2_TEXT_LAYER_WEIGHTS: i64 = 10;

fn i2_expected_tensor(data: const u8[:], state: i64[:], tensors: i64[:], slots: i64[:], name: const u8[:],
                      kind: i64, n0: i64, n1: i64, n2: i64) -> i64 {
    let id = gr_lookup(data, state, tensors, slots, name, 12, 3);
    if id < 0 { return -1; }
    let base = 12 * id;
    if tensors[base + 2] != kind || tensors[base + 4] != n0 || tensors[base + 5] != n1 ||
       tensors[base + 6] != n2 || tensors[base + 7] != 1 { return -1; }
    return id;
}

export fn i2_bind_text_weights(data: const u8[:], state: i64[:], kv: i64[:], kv_slots: i64[:],
                              tensors: i64[:], tensor_slots: i64[:], layout: const i64[:],
                              globals: i64[:], layers: i64[:]) -> i64 {
    let legacy = mr_text(data, state, kv, kv_slots, "general.architecture", "inkling2");
    if layout.len < 9 || globals.len < I2_TEXT_GLOBALS ||
       (!legacy && !mr_text(data, state, kv, kv_slots, "general.architecture", "inkling2_loop")) { return -1; }

    let prefix = if legacy { "inkling2." } else { "inkling2_loop." };
    var scratch: i64[2] = [0; 2];
    let stored = i2_integer(data, state, kv, kv_slots, prefix, "block_count", scratch);
    let physical = layout[0];
    if physical < 1 || physical > I2_MAX_LAYERS || stored < physical || stored > I2_MAX_LAYERS ||
       stored != (if legacy { layout[8] } else { physical }) || layout[2] < 1 || layout[2] > physical ||
       layers.len / I2_TEXT_LAYER_WEIGHTS < physical { return -1; }
    let width = i2_integer(data, state, kv, kv_slots, prefix, "embedding_length", scratch);
    let vocabulary = i2_integer(data, state, kv, kv_slots, prefix, "vocab_size", scratch);
    let experts = i2_integer(data, state, kv, kv_slots, prefix, "expert_count", scratch);
    let keep = i2_integer(data, state, kv, kv_slots, prefix, "expert_used_count", scratch);
    let ff = i2_integer(data, state, kv, kv_slots, prefix, "expert_feed_forward_length", scratch);
    let key_width = i2_integer(data, state, kv, kv_slots, prefix, "attention.key_length", scratch);
    let value_width = i2_integer(data, state, kv, kv_slots, prefix, "attention.value_length", scratch);
    if width != 2304 || vocabulary != 201024 || experts != 128 || keep != 8 || ff != 1152 ||
       key_width != 128 || value_width != 128 { return -1; }
    let q_heads_key = i2_find_key(data, state, kv, kv_slots, prefix, "attention.head_count");
    let kv_heads_key = i2_find_key(data, state, kv, kv_slots, prefix, "attention.head_count_kv");
    if gr_arr_n_impl(state, kv, q_heads_key) != stored || gr_arr_n_impl(state, kv, kv_heads_key) != stored {
        return -1;
    }

    globals[0] = i2_expected_tensor(data, state, tensors, tensor_slots, "token_embd.weight", 8, width, vocabulary, 1);
    globals[1] = i2_expected_tensor(data, state, tensors, tensor_slots, "token_embd_norm.weight", 0, width, 1, 1);
    globals[2] = i2_expected_tensor(data, state, tensors, tensor_slots, "output.weight", 8, width, vocabulary, 1);
    globals[3] = i2_expected_tensor(data, state, tensors, tensor_slots, "output_norm.weight", 0, width, 1, 1);
    globals[4] = i2_expected_tensor(data, state, tensors, tensor_slots, "output_res_query.weight", 0, width, 1, 1);
    for i in I2_TEXT_GLOBALS { if globals[i] < 0 { return -1; } }

    var name: u8[96] = [0; 96];
    for layer in physical {
        let logical = if legacy && layer >= layout[2] { layer + stored - physical } else { layer };
        if gr_array_int_impl(data, state, kv, q_heads_key, logical, scratch) != 0 || scratch[0] != 24 {
            return -1;
        }
        if gr_array_int_impl(data, state, kv, kv_heads_key, logical, scratch) != 0 || scratch[0] != 8 {
            return -1;
        }
        let q_width = 24 * key_width;
        let kv_width = 8 * key_width;
        for i in I2_TEXT_LAYER_WEIGHTS {
            let suffix = if i == 0 { "attn_q.weight" }
                else if i == 1 { "attn_k.weight" }
                else if i == 2 { "attn_v.weight" }
                else if i == 3 { "attn_output.weight" }
                else if i == 4 { "attn_gate.weight" }
                else if i == 5 { "ffn_gate_inp.weight" }
                else if i == 6 { "ffn_gate_up_exps.weight" }
                else if i == 7 { "ffn_down_exps.weight" }
                else if i == 8 { "exp_probs_b.bias" }
                else { "ffn_norm.weight" };
            let name_size = mr_name(layer, suffix, name);
            if name_size < 0 { return -1; }
            let kind = if i < 5 { 8 } else if i == 6 { 23 } else if i == 7 { 20 } else { 0 };
            let n0 = if i == 3 { q_width } else if i == 7 { ff } else if i == 8 { experts } else { width };
            let n1 = if i == 0 || i == 4 { q_width }
                else if i == 1 || i == 2 { kv_width }
                else if i == 3 || i == 7 { width }
                else if i == 5 { experts }
                else if i == 6 { 2 * ff }
                else { 1 };
            let n2 = if i == 6 || i == 7 { experts } else { 1 };
            let id = i2_expected_tensor(data, state, tensors, tensor_slots, name[0 .. name_size], kind, n0, n1, n2);
            if id < 0 { return -1; }
            layers[I2_TEXT_LAYER_WEIGHTS * layer + i] = id;
        }
    }
    return physical;
}

// Canonical Inkling auxiliaries: shared convolution and relative attention,
// optional residual query, local/global input paths, and loop controllers.
// Each physical layer gets 30 slots; -1 denotes an inapplicable tensor.
const I2_TEXT_AUX_WEIGHTS: i64 = 30;

export fn i2_bind_text_aux(data: const u8[:], state: i64[:], kv: i64[:], kv_slots: i64[:],
                         tensors: i64[:], tensor_slots: i64[:], layout: const i64[:],
                         layers: i64[:]) -> i64 {
    if layout.len < 9 || layout[0] != 24 || layout[1] != 6 || layout[2] != 18 || layout[4] != 64 ||
       layers.len / I2_TEXT_AUX_WEIGHTS < layout[0] ||
       !mr_text(data, state, kv, kv_slots, "general.architecture", "inkling2_loop") { return -1; }
    let pattern = gr_lookup(data, state, kv, kv_slots, "inkling2_loop.attention.sliding_window_pattern", 8, 2);
    if gr_arr_n_impl(state, kv, pattern) != layout[0] || kv[pattern * 8 + 3] != 7 { return -1; }
    var scratch: i64[2] = [0; 2];
    var name: u8[96] = [0; 96];
    for layer in layout[0] {
        if gr_array_int_impl(data, state, kv, pattern, layer, scratch) != 0 { return -1; }
        let sliding = scratch[0] != 0;
        let core = layer >= layout[1] && layer < layout[2];
        for i in I2_TEXT_AUX_WEIGHTS {
            let at = layer * I2_TEXT_AUX_WEIGHTS + i;
            layers[at] = -1;
            if (i == 10 && layer == 0) || (i == 11 && !sliding) ||
               (i >= 12 && i < 17 && sliding) || (i >= 17 && !core) { continue; }
            let suffix = if i == 0 { "attn_k_conv.weight" }
            else if i == 1 { "attn_v_conv.weight" }
            else if i == 2 { "attn_post_conv.weight" }
            else if i == 3 { "ffn_post_conv.weight" }
            else if i == 4 { "attn_q_clip.weight" }
            else if i == 5 { "attn_rel.weight" }
            else if i == 6 { "attn_rel_bias.weight" }
            else if i == 7 { "attn_rel_clip.weight" }
            else if i == 8 { "attn_sink_proj.weight" }
            else if i == 9 { "ffn_res_query.weight" }
            else if i == 10 { "attn_res_query.weight" }
            else if i == 11 { "attn_norm.weight" }
            else if i == 12 { "attn_q_inp_norm.weight" }
            else if i == 13 { "attn_k_inp_norm.weight" }
            else if i == 14 { "attn_v_inp_norm.weight" }
            else if i == 15 { "attn_k_conv_proj.weight" }
            else if i == 16 { "attn_v_conv_proj.weight" }
            else if i == 17 { "loop_msg_query.weight" }
            else if i == 18 { "loop_q_ctrl_down.weight" }
            else if i == 19 { "loop_q_ctrl_forget.weight" }
            else if i == 20 { "loop_q_ctrl_gain.weight" }
            else if i == 21 { "loop_k_ctrl_down.weight" }
            else if i == 22 { "loop_k_ctrl_forget.weight" }
            else if i == 23 { "loop_k_ctrl_gain.weight" }
            else if i == 24 { "loop_v_ctrl_down.weight" }
            else if i == 25 { "loop_v_ctrl_forget.weight" }
            else if i == 26 { "loop_v_ctrl_gain.weight" }
            else if i == 27 { "loop_ffn_ctrl_down.weight" }
            else if i == 28 { "loop_ffn_ctrl_forget.weight" }
            else { "loop_ffn_ctrl_gain.weight" };
            let kind = if i == 0 || i == 1 || i == 2 || i == 3 || i == 4 || i == 6 || i == 7 || i == 9 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 17 { 0 }
            else { 8 };
            let n0 = if i == 0 || i == 1 || i == 2 || i == 3 { 4 }
            else if i == 4 { 128 }
            else if i == 5 || i == 8 || i == 9 || i == 10 || i == 11 || i == 12 || i == 13 || i == 14 || i == 15 || i == 16 || i == 17 { 2304 }
            else if i == 6 || i == 7 { 16 }
            else if i == 18 || i == 21 || i == 24 || i == 27 { 4608 }
            else { 64 };
            let n1 = if i == 0 || i == 1 || i == 6 { 1024 }
            else if i == 2 || i == 3 || i == 19 || i == 20 || i == 22 || i == 23 || i == 25 || i == 26 || i == 28 || i == 29 { 2304 }
            else if i == 4 || i == 7 || i == 8 { 24 }
            else if i == 5 { 384 }
            else if i == 9 || i == 11 || i == 12 || i == 13 || i == 14 { 1 }
            else if i == 10 { (if sliding { 1 } else { 3 }) }
            else if i == 15 || i == 16 { 32 }
            else if i == 17 { 4 }
            else { 64 };
            let size = mr_name(layer, suffix, name);
            if size < 0 { return -1; }
            let id = i2_expected_tensor(data, state, tensors, tensor_slots, name[0 .. size], kind, n0, n1, 1);
            if id < 0 { return -1; }
            layers[at] = id;
        }
    }
    return layout[0];
}
