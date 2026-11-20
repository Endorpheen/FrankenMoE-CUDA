// Depends on the accepted typed graph builder and qg_split. Handles, immutable
// weights/config, argument workspace and node workspace belong to the caller.
extern "fg156_tget" fn qa_get(t: i64, field: i64, index: i64) -> u64;
extern "fg156_tset" fn qa_set(t: i64, field: i64, index: i64, value: u64);
extern "fg156_name" fn qa_name(t: i64, source: i64, style: i64);
extern "fg_q35_fwht_node" fn qa_fwht_node(c: i64, input: i64, width: i64) -> i64;

fn qa_begin(work: i64[:], mode: i64, c: i64, a: i64, b: i64, d: i64, op: i64) {
    var i: i64 = 0;
    while i < 16 {
        work[i] = 0;
        i += 1;
    }
    work[0] = mode;
    work[1] = c;
    work[2] = a;
    work[3] = b;
    work[4] = d;
    work[5] = op;
}

fn qa_binary(c: i64, a: i64, b: i64, op: i64, work: i64[:]) -> i64 {
    qa_begin(work, 1, c, a, b, 0, op);
    return gb_common_args(work);
}

fn qa_mm(c: i64, weight: i64, input: i64, work: i64[:]) -> i64 {
    qa_begin(work, 11, c, weight, input, 0, gb_op(1));
    return gb_common_args(work);
}

fn qa_rms(c: i64, input: i64, epsilon: i64, work: i64[:]) -> i64 {
    qa_begin(work, 4, c, input, 0, 0, 25);
    work[7] = epsilon;
    return gb_common_args(work);
}

// The accepted EXP156 reshape policy, using an existing native view allocation
// primitive and POD projections. Only contiguous F32 intermediates reach it.
fn qa_reshape(c: i64, a: i64, n0: i64, n1: i64, n2: i64, n3: i64) -> i64 {
    ensure(prop(a, 0) == 0 && pred(0, a, 0) != 0, 901);
    ensure(times(times(n0, n1), times(n2, n3)) == pred(13, a, 0), 902);
    let r = view(c, a);
    qa_set(r, 0, 0, n0 as u64);
    qa_set(r, 0, 1, n1 as u64);
    qa_set(r, 0, 2, n2 as u64);
    qa_set(r, 0, 3, n3 as u64);
    qa_set(r, 1, 0, 4 as u64);
    qa_set(r, 1, 1, times(n0, 4) as u64);
    qa_set(r, 1, 2, times(times(n0, n1), 4) as u64);
    qa_set(r, 1, 3, times(times(times(n0, n1), n2), 4) as u64);
    qa_set(r, 7, 0, 36 as u64);
    qa_set(r, 8, 0, a as u64);
    qa_name(r, a, 1);
    return r;
}

// Exact fixed-axis specialization of the accepted EXP156 permutation policy.
// Unlike a contiguous copy this preserves strides, view roots and op params.
fn qa_permute(c: i64, a: i64) -> i64 {
    let r = view(c, a);
    var i: i64 = 0;
    while i < 4 {
        let axis: i64 = if i == 1 { 2 } else { if i == 2 { 1 } else { i } };
        qa_set(r, 0, axis, qa_get(a, 0, i));
        qa_set(r, 1, axis, qa_get(a, 1, i));
        qa_set(r, 10, i, axis as u64);
        i += 1;
    }
    qa_set(r, 7, 0, 38 as u64);
    qa_set(r, 8, 0, a as u64);
    qa_name(r, a, 2);
    return r;
}

fn qa_rope(c: i64, a: i64, positions: i64, cfg: i64[:], sections: i64, work: i64[:]) -> i64 {
    qa_begin(work, 25, c, a, positions, 0, cfg[3]);
    work[7] = cfg[2];
    work[8] = cfg[4];
    var j: i64 = 0;
    while j < 6 {
        work[9 + j] = cfg[6 + j];
        j += 1;
    }
    work[15] = sections;
    return gb_common_args(work);
}

// weights: pre_norm, QG, K, V, O, Q_norm, K_norm, post_norm.
// cfg: heads, KV_heads, n_rot, rope_type, original_context, epsilon_bits,
// base/scale/ext/attn/beta_fast/beta_slow bits, attention_scale_bits, F32_prec.
export fn q35_attn_project(c: i64, x: i64, positions: i64, weights: i64[:], cfg: i64[:],
                          sections: i64, work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || x == 0 || positions == 0 || sections == 0 || weights.len < 8 || cfg.len < 14 || work.len < 16 || nodes.len < 29 {
        return 0;
    }
    var j: i64 = 0;
    while j < 8 {
        if weights[j] == 0 { return 0; }
        j += 1;
    }
    let h = cfg[0];
    let hk = cfg[1];
    let k = ne(x, 0);
    let t = ne(x, 1);
    if h <= 0 || hk <= 0 || h % hk != 0 || k <= 0 || t <= 0 || ne(x, 2) != 1 || ne(x, 3) != 1 || prop(x, 0) != 0 {
        return 0;
    }
    if ne(weights[1], 1) % times(h, 2) != 0 { return 0; }
    let hd = ne(weights[1], 1) / times(h, 2);
    if hd <= 0 || cfg[2] <= 0 || cfg[2] % 2 != 0 || cfg[2] > hd || cfg[3] != 40 || cfg[4] <= 0 {
        return 0;
    }
    if !(float_bits(cfg[5]) >= 0.0) || !(float_bits(cfg[6]) > 0.0) || !(float_bits(cfg[7]) > 0.0) {
        return 0;
    }
    if prop(positions, 0) != 26 || ne(positions, 0) != times(t, 4) {
        return 0;
    }
    if ne(weights[0], 0) != k || ne(weights[7], 0) != k || ne(weights[5], 0) != hd || ne(weights[6], 0) != hd {
        return 0;
    }
    if ne(weights[1], 0) != k || ne(weights[2], 0) != k || ne(weights[3], 0) != k ||
       ne(weights[2], 1) != times(hd, hk) || ne(weights[3], 1) != times(hd, hk) ||
       ne(weights[4], 0) != times(hd, h) || ne(weights[4], 1) != k {
        return 0;
    }
    nodes[0] = qa_rms(c, x, cfg[5], work);
    nodes[1] = qa_binary(c, nodes[0], weights[0], 7, work);
    nodes[2] = qa_mm(c, weights[1], nodes[1], work);
    nodes[3] = qa_mm(c, weights[2], nodes[1], work);
    nodes[4] = qa_mm(c, weights[3], nodes[1], work);
    if qg_split(c, nodes[2], hd, h, t, 4, nodes[5 .. 7]) != 2 { return 0; }
    nodes[7] = qa_reshape(c, nodes[3], hd, hk, t, 1);
    nodes[8] = qa_reshape(c, nodes[4], hd, hk, t, 1);
    nodes[9] = qa_rms(c, nodes[5], cfg[5], work);
    nodes[10] = qa_binary(c, nodes[9], weights[5], 7, work);
    nodes[11] = qa_rms(c, nodes[7], cfg[5], work);
    nodes[12] = qa_binary(c, nodes[11], weights[6], 7, work);
    nodes[13] = qa_rope(c, nodes[10], positions, cfg, sections, work);
    nodes[14] = qa_rope(c, nodes[12], positions, cfg, sections, work);
    qa_begin(work, 24, c, nodes[6], 0, 0, 35);
    work[7] = times(hd, h);
    work[8] = t;
    work[9] = 1;
    work[10] = 1;
    nodes[15] = gb_common_args(work);
    return nodes[13];
}

// K/V may be fresh projection tensors or later cache views. The caller inserts
// the state update between project and finish; this function owns the MHA path.
fn qa_attn_finish(c: i64, x: i64, q: i64, k: i64, v: i64, gate: i64, mask: i64,
                  weights: i64[:], cfg: i64[:], work: i64[:], nodes: i64[:], value_rotation: i64) -> i64 {
    if c == 0 || x == 0 || q == 0 || k == 0 || v == 0 || gate == 0 ||
       weights.len < 8 || cfg.len < 14 || work.len < 16 || nodes.len < 29 {
        return 0;
    }
    nodes[16] = qa_permute(c, q);
    nodes[17] = qa_permute(c, k);
    nodes[18] = qa_permute(c, v);
    nodes[19] = nodes[17];
    nodes[20] = nodes[18];
    if prop(nodes[17], 0) == 0 {
        qa_begin(work, 22, c, nodes[17], 0, 0, gb_type(1));
        nodes[19] = gb_common_args(work);
    }
    if prop(nodes[18], 0) == 0 {
        qa_begin(work, 22, c, nodes[18], 0, 0, gb_type(1));
        nodes[20] = gb_common_args(work);
    }
    qa_begin(work, 26, c, nodes[16], nodes[19], nodes[20], 0);
    work[7] = mask;
    work[8] = cfg[12];
    nodes[21] = gb_common_args(work);
    qa_begin(work, 35, c, nodes[21], 0, 0, 2);
    work[7] = cfg[13];
    if gb_common_args(work) != 1 { return 0; }
    let attention = if value_rotation == 0 { nodes[21] } else { qa_fwht_node(c, nodes[21], value_rotation) };
    if attention == 0 { return 0; }
    nodes[22] = qa_reshape(c, attention, times(ne(q, 0), ne(q, 1)), ne(q, 2), 1, 1);
    qa_begin(work, 3, c, gate, 0, 0, 7);
    nodes[23] = gb_common_args(work);
    nodes[24] = qa_binary(c, nodes[22], nodes[23], 7, work);
    nodes[25] = qa_mm(c, weights[4], nodes[24], work);
    nodes[26] = qa_binary(c, nodes[25], x, 2, work);
    nodes[27] = qa_rms(c, nodes[26], cfg[5], work);
    nodes[28] = qa_binary(c, nodes[27], weights[7], 7, work);
    return nodes[28];
}

export fn q35_attn_finish(c: i64, x: i64, q: i64, k: i64, v: i64, gate: i64, mask: i64,
                         weights: i64[:], cfg: i64[:], work: i64[:], nodes: i64[:]) -> i64 {
    return qa_attn_finish(c, x, q, k, v, gate, mask, weights, cfg, work, nodes, 0);
}

export fn q35_attn_block(c: i64, x: i64, positions: i64, mask: i64, weights: i64[:], cfg: i64[:],
                        sections: i64, work: i64[:], nodes: i64[:]) -> i64 {
    if mask == 0 { return 0; }
    if q35_attn_project(c, x, positions, weights, cfg, sections, work, nodes) == 0 { return 0; }
    return q35_attn_finish(c, x, nodes[13], nodes[14], nodes[8], nodes[15], mask, weights, cfg, work, nodes);
}
