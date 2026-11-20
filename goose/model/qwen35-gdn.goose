// Real recurrent-layer orchestration. GGML remains the allocation/math substrate.
extern "sqrtf" fn qg_sqrt(x: f32) -> f32;

// Exact encoding of nonnegative normal F32 parameters, without a new bitcast
// adapter. Binary power-of-two normalization preserves their significands.
fn qg_positive_bits(input: f32) -> i64 {
    if input == 0.0 { return 0; }
    if !(input >= 1.1754943508222875e-38) || !(input <= 3.4028234663852886e38) { return -1; }
    var v: f32 = input;
    var exponent: i64 = 127;
    while v >= 2.0 { v = v * 0.5; exponent += 1; }
    while v < 1.0 { v = v * 2.0; exponent -= 1; }
    let fraction: i64 = ((v - 1.0) * 8388608.0) as i64;
    return exponent * 8388608 + fraction;
}

// cfg: head width, key heads, value heads, kernel, RMS epsilon bits,
// RMS-based L2 epsilon bits, RMS-based L2 scale bits. One sequence, fused K=1.
export fn q35_gdn_config(head: i64, key_heads: i64, value_heads: i64, kernel: i64,
                          epsilon_bits: i64, out: i64[:]) -> i64 {
    if out.len < 7 || head <= 0 || head > 4096 || key_heads <= 0 || value_heads <= 0 ||
       key_heads > 512 || value_heads > 512 || value_heads % key_heads != 0 || kernel < 2 || kernel > 64 { return -1; }
    let epsilon: f32 = float_bits(epsilon_bits);
    if !(epsilon > 0.0) || qg_positive_bits(epsilon) != epsilon_bits { return -1; }
    let e: i64 = qg_positive_bits(epsilon / (head as f32));
    let s: i64 = qg_positive_bits(1.0 / qg_sqrt(head as f32));
    if e < 0 || s < 0 { return -1; }
    out[0] = head; out[1] = key_heads; out[2] = value_heads; out[3] = kernel;
    out[4] = epsilon_bits; out[5] = e; out[6] = s;
    return 7;
}

fn qg_shape(t: i64, n0: i64, n1: i64, n2: i64) -> bool {
    return t != 0 && ne(t, 0) == n0 && ne(t, 1) == n1 && ne(t, 2) == n2 && ne(t, 3) == 1 && pred(0, t, 0) != 0;
}

fn qg_cfg_ok(cfg: i64[:]) -> bool {
    if cfg.len < 7 || cfg[0] <= 0 || cfg[0] > 4096 || cfg[1] <= 0 || cfg[1] > 512 ||
       cfg[2] <= 0 || cfg[2] > 512 || cfg[2] % cfg[1] != 0 || cfg[3] < 2 || cfg[3] > 64 { return false; }
    let epsilon: f32 = float_bits(cfg[4]);
    return epsilon > 0.0 && qg_positive_bits(epsilon) == cfg[4] &&
           qg_positive_bits(epsilon / (cfg[0] as f32)) == cfg[5] &&
           qg_positive_bits(1.0 / qg_sqrt(cfg[0] as f32)) == cfg[6];
}

// Quiescent host-addressable disjoint buffers only; no device pointer access.
// Metadata is the magic followed by the exact seven-word arithmetic config.
export fn q35_gdn_host_save(conv: f32[:], state: f32[:], cfg: i64[:], meta: i64[:], out: f32[:]) -> i64 {
    if !qg_cfg_ok(cfg) || meta.len < 8 { return -1; }
    let nc: i64 = (cfg[3] - 1) * cfg[0] * (2 * cfg[1] + cfg[2]);
    let ns: i64 = cfg[0] * cfg[0] * cfg[2];
    if conv.len != nc || state.len != ns || out.len < nc + ns { return -1; }
    var i: i64 = 0;
    while i < nc { out[i] = conv[i]; i += 1; }
    i = 0; while i < ns { out[nc + i] = state[i]; i += 1; }
    meta[0] = 917;
    i = 0; while i < 7 { meta[i + 1] = cfg[i]; i += 1; }
    return nc + ns;
}

export fn q35_gdn_host_restore(conv: f32[:], state: f32[:], cfg: i64[:], meta: i64[:], data: f32[:]) -> i64 {
    if !qg_cfg_ok(cfg) || meta.len < 8 || meta[0] != 917 { return -1; }
    var i: i64 = 0; while i < 7 { if meta[i + 1] != cfg[i] { return -1; } i += 1; }
    let nc: i64 = (cfg[3] - 1) * cfg[0] * (2 * cfg[1] + cfg[2]);
    let ns: i64 = cfg[0] * cfg[0] * cfg[2];
    if conv.len != nc || state.len != ns || data.len < nc + ns { return -1; }
    i = 0; while i < nc { conv[i] = data[i]; i += 1; }
    i = 0; while i < ns { state[i] = data[nc + i]; i += 1; }
    return nc + ns;
}

extern "fg_q35_ssm_silu_enabled" fn qg_ssm_silu_enabled() -> bool;

fn qg_unary(c: i64, a: i64, op: i64, w: i64[:]) -> i64 {
    qa_begin(w, 3, c, a, 0, 0, op);
    return gb_common_args(w);
}

// weights: pre_norm,QKV,Z,beta,alpha,dt,A,conv,norm,O,post_norm.
// Persistent conv=[kernel-1,channels], state=[head,head,value_heads,1], F32.
// Roots: nodes[16] conv copy, nodes[29] state copy, returned nodes[39] output.
export fn q35_gdn_layer(c: i64, x: i64, conv: i64, state: i64, weights: i64[:], cfg: i64[:],
                         work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || x == 0 || conv == 0 || state == 0 || weights.len < 11 || cfg.len < 7 || work.len < 16 || nodes.len < 40 { return 0; }
    let head: i64 = cfg[0]; let nk: i64 = cfg[1]; let nv: i64 = cfg[2]; let kernel: i64 = cfg[3];
    if !qg_cfg_ok(cfg) { return 0; }
    let hidden: i64 = ne(x, 0); let count: i64 = ne(x, 1);
    let vd: i64 = head * nv; let channels: i64 = head * (2 * nk + nv);
    if hidden <= 0 || count <= 0 || count > 4096 || !qg_shape(x, hidden, count, 1) || prop(x, 0) != 0 ||
       !qg_shape(conv, kernel - 1, channels, 1) || prop(conv, 0) != 0 ||
       !qg_shape(state, head, head, nv) || prop(state, 0) != 0 { return 0; }
    var i: i64 = 0;
    while i < 11 { if weights[i] == 0 { return 0; } i += 1; }
    if !qg_shape(weights[0], hidden, 1, 1) || !qg_shape(weights[10], hidden, 1, 1) ||
       !qg_shape(weights[1], hidden, channels, 1) || !qg_shape(weights[2], hidden, vd, 1) ||
       !qg_shape(weights[3], hidden, nv, 1) || !qg_shape(weights[4], hidden, nv, 1) ||
       !qg_shape(weights[5], nv, 1, 1) || !qg_shape(weights[6], nv, 1, 1) ||
       !qg_shape(weights[7], kernel, channels, 1) || !qg_shape(weights[8], head, 1, 1) ||
       !qg_shape(weights[9], vd, hidden, 1) { return 0; }
    if prop(weights[0], 0) != 0 || prop(weights[10], 0) != 0 || prop(weights[5], 0) != 0 ||
       prop(weights[6], 0) != 0 || prop(weights[7], 0) != 0 || prop(weights[8], 0) != 0 { return 0; }
    nodes[1] = qa_weighted_rms(c, x, weights[0], cfg[4], work);
    nodes[2] = qa_mm(c, weights[1], nodes[1], work);
    nodes[3] = qa_reshape(c, nodes[2], channels, count, 1, 1);
    nodes[4] = qa_mm(c, weights[2], nodes[1], work);
    nodes[5] = qa_mm(c, weights[3], nodes[1], work);
    nodes[6] = qa_reshape(c, nodes[5], 1, nv, count, 1);
    nodes[7] = qg_unary(c, nodes[6], 7, work);
    nodes[8] = qa_mm(c, weights[4], nodes[1], work);
    nodes[9] = qa_reshape(c, nodes[8], nv, count, 1, 1);
    nodes[10] = qa_binary(c, nodes[9], weights[5], 2, work);
    nodes[11] = qg_unary(c, nodes[10], 15, work);
    nodes[12] = qa_binary(c, nodes[11], weights[6], 7, work);
    nodes[13] = qa_reshape(c, nodes[12], 1, nv, count, 1);
    nodes[14] = gdn_conv_input(c, conv, nodes[3], 4);
    nodes[15] = gdn_conv_tail(c, nodes[14], kernel, channels, 1, 4);
    nodes[16] = gdn_conv_store(c, nodes[15], conv);
    qa_begin(work, 27, c, nodes[14], weights[7], 0, 0);
    nodes[17] = gb_common_args(work);
    if qg_ssm_silu_enabled() {
        qa_set(nodes[17], 10, 0, 1);
        nodes[18] = nodes[17];
    } else {
        nodes[18] = qg_unary(c, nodes[17], 10, work);
    }
    nodes[19] = gdn_qkv_view(c, nodes[18], 0, head, nk, head, nv, count, 1, 4);
    nodes[20] = gdn_qkv_view(c, nodes[18], 1, head, nk, head, nv, count, 1, 4);
    nodes[21] = gdn_qkv_view(c, nodes[18], 2, head, nk, head, nv, count, 1, 4);
    nodes[22] = qa_rms(c, nodes[19], cfg[5], work);
    qa_begin(work, 8, c, nodes[22], 0, 0, 32); work[7] = cfg[6]; nodes[23] = gb_common_args(work);
    nodes[24] = qa_rms(c, nodes[20], cfg[5], work);
    qa_begin(work, 8, c, nodes[24], 0, 0, 32); work[7] = cfg[6]; nodes[25] = gb_common_args(work);
    qa_begin(work, 28, c, nodes[23], nodes[25], nodes[21], 0);
    work[7] = nodes[13]; work[8] = nodes[7]; work[9] = state; work[10] = 1;
    nodes[26] = gb_common_args(work);
    // The only consumer of the state tail is the copy to this same model
    // state tensor below. A resident backend can publish that tail in place.
    qa_set(nodes[26], 10, 4, 1);
    nodes[27] = view4(c, nodes[26], head, nv, count, 1, head * 4, vd * 4, vd * count * 4, 0);
    nodes[28] = view4(c, nodes[26], head, head, nv, 1, head * 4, head * head * 4, head * head * nv * 4, vd * count * 4);
    nodes[29] = gdn_conv_store(c, nodes[28], state);
    nodes[30] = qa_reshape(c, nodes[4], head, nv, count, 1);
    nodes[32] = qa_weighted_rms(c, nodes[27], weights[8], cfg[4], work);
    nodes[33] = qg_unary(c, nodes[30], 10, work);
    nodes[34] = qa_binary(c, nodes[32], nodes[33], 7, work);
    nodes[35] = qa_reshape(c, nodes[34], vd, count, 1, 1);
    nodes[36] = qa_mm(c, weights[9], nodes[35], work);
    nodes[37] = qa_binary(c, nodes[36], x, 2, work);
    nodes[39] = qa_weighted_rms(c, nodes[37], weights[10], cfg[4], work);
    return nodes[39];
}
