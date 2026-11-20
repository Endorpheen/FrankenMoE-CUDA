// Borrow a real read-only mapping into a persistent metadata-only GGML context.
// Context and mapping ownership remain with the model/session, not each graph.
// Reuses the canonical GGUF type table instead of duplicating quant layouts.
extern "ggml_get_no_alloc" fn qm_noalloc(c: i64) -> bool;

export fn q35_bind_weight(c: i64, mapping: u64, mapping_bytes: i64, offset: i64,
                          dtype: i64, shape: i64[:]) -> i64 {
    if c == 0 || mapping == 0 || mapping == 18446744073709551615 || mapping_bytes <= 0 ||
       offset < 0 || offset > mapping_bytes || offset % 8 != 0 || shape.len < 4 || !qm_noalloc(c) { return 0; }
    let block_size: i64 = t_blck_size(dtype);
    let size: i64 = t_type_size(dtype);
    if block_size <= 0 || size <= 0 || shape[0] <= 0 || shape[0] % block_size != 0 { return 0; }
    let blocks: i64 = shape[0] / block_size;
    if blocks > 9223372036854775807 / size { return 0; }
    var bytes: i64 = blocks * size;
    var elements: i64 = shape[0];
    var i: i64 = 1;
    while i < 4 {
        if shape[i] <= 0 || bytes > 9223372036854775807 / shape[i] || elements > 9223372036854775807 / shape[i] { return 0; }
        bytes *= shape[i]; elements *= shape[i]; i += 1;
    }
    if bytes > mapping_bytes - offset || mapping > 18446744073709551615 - ((offset + bytes) as u64) { return 0; }
    let tensor: i64 = fresh(c, dtype, shape[0], shape[1], shape[2], shape[3]);
    qa_set(tensor, 4, 0, mapping + (offset as u64));
    return tensor;
}

fn qm_shape(t: i64, n0: i64, n1: i64, n2: i64) -> bool {
    return t != 0 && ne(t, 0) == n0 && ne(t, 1) == n1 && ne(t, 2) == n2 && ne(t, 3) == 1 && pred(0, t, 0) != 0;
}

fn qm_mm_id(c: i64, w: i64, x: i64, ids: i64, work: i64[:]) -> i64 {
    qa_begin(work, 12, c, w, x, ids, 30);
    return gb_common_args(work);
}

fn qm_swiglu(c: i64, gate: i64, up: i64, work: i64[:]) -> i64 {
    qa_begin(work, 10, c, gate, up, 0, 2);
    return gb_common_args(work);
}

extern "fg_q35_moe_weighted_node" fn qm_weighted(c: i64, input: i64, weights: i64) -> i64;
extern "fg_q35_moe_norm_node" fn qm_route_weights(c: i64, input: i64, ids: i64, scale_bits: i64) -> i64;

// Weights: router, up_exps, gate_exps, down_exps, shared_up, shared_gate,
// shared_down, shared_router. cfg: expert_count, used_count, weight_scale_bits.
// No groups, biases, LoRA, split scaling or activation clamps in this path.
// Required nodes length=26. Return includes the gated shared expert.
export fn q35_moe_ffn(c: i64, x: i64, weights: i64[:], cfg: i64[:],
                      work: i64[:], nodes: i64[:]) -> i64 {
    if c == 0 || x == 0 || weights.len < 8 || cfg.len < 3 || work.len < 16 { return 0; }
    let experts: i64 = cfg[0]; let used: i64 = cfg[1];
    if experts <= 0 || experts > 4096 || used <= 0 || used > experts || used > 64 || nodes.len < 26 { return 0; }
    let scale: f32 = float_bits(cfg[2]);
    if !(scale >= 0.0) || !(scale <= 3.4028234663852886e38) { return 0; }
    let hidden: i64 = ne(x, 0); let count: i64 = ne(x, 1);
    if hidden <= 0 || count <= 0 || count > 4096 || prop(x, 0) != 0 || !qm_shape(x, hidden, count, 1) { return 0; }
    var i: i64 = 0; while i < 8 { if weights[i] == 0 { return 0; } i += 1; }
    let ff: i64 = ne(weights[1], 1); let shared: i64 = ne(weights[4], 1);
    if ff <= 0 || shared <= 0 || !qm_shape(weights[0], hidden, experts, 1) || prop(weights[0], 0) != 0 ||
       !qm_shape(weights[1], hidden, ff, experts) || !qm_shape(weights[2], hidden, ff, experts) ||
       !qm_shape(weights[3], ff, hidden, experts) || !qm_shape(weights[4], hidden, shared, 1) ||
       !qm_shape(weights[5], hidden, shared, 1) || !qm_shape(weights[6], shared, hidden, 1) ||
       !qm_shape(weights[7], hidden, 1, 1) || prop(weights[7], 0) != 0 { return 0; }
    nodes[0] = qa_mm(c, weights[0], x, work);
    qa_begin(work, 20, c, nodes[0], 0, 0, 0); work[7] = 1065353216;
    nodes[1] = gb_common_args(work);
    qa_begin(work, 30, c, nodes[1], 0, 0, used); nodes[2] = gb_common_args(work);
    nodes[3] = nodes[2];
    nodes[11] = qm_route_weights(c, nodes[1], nodes[3], cfg[2]);
    if nodes[11] == 0 { return 0; }
    nodes[12] = qa_reshape(c, x, hidden, 1, count, 1);
    nodes[13] = qm_mm_id(c, weights[1], nodes[12], nodes[3], work);
    nodes[14] = qm_mm_id(c, weights[2], nodes[12], nodes[3], work);
    nodes[15] = qm_swiglu(c, nodes[14], nodes[13], work);
    nodes[16] = qm_mm_id(c, weights[3], nodes[15], nodes[3], work);
    let aggregate = qm_weighted(c, nodes[16], nodes[11]);
    if aggregate == 0 { return 0; }
    let base: i64 = 17;
    nodes[base] = aggregate;
    nodes[base + 1] = qa_mm(c, weights[4], x, work);
    nodes[base + 2] = qa_mm(c, weights[5], x, work);
    nodes[base + 3] = qm_swiglu(c, nodes[base + 2], nodes[base + 1], work);
    nodes[base + 4] = qa_mm(c, weights[6], nodes[base + 3], work);
    nodes[base + 5] = qa_mm(c, weights[7], x, work);
    qa_begin(work, 3, c, nodes[base + 5], 0, 0, 7); nodes[base + 6] = gb_common_args(work);
    nodes[base + 7] = qa_binary(c, nodes[base + 4], nodes[base + 6], 7, work);
    nodes[base + 8] = qa_binary(c, aggregate, nodes[base + 7], 2, work);
    return nodes[base + 8];
}

// Layer records have stride32: kind (0 attention,1 GDN), branch weights[11],
// MoE weights[8], conv,state,K,V handles, then eight reserved zero words.
// Branch weights use q35_attn_cached's first8 or q35_gdn_layer's eleven order.
// One model-step ticket is shared across layers. This builder never commits it.
// roots=[count,handles...]; generic executor expands every returned root, waits,
// and commits once. On failure discard this unexecuted graph/context.
export fn q35_trunk_output(c: i64, ids: i64, embedding: i64, head_weight: i64, head_norm: i64,
                    layers: i64[:], layer_count: i64, positions: i64, mask: i64, indices: i64,
                    attn_cfg: i64[:], gdn_cfg: i64[:], moe_cfg: i64[:], sections: i64,
                    state: i64[:], ticket: i64, work: i64[:], branch_nodes: i64[:],
                    moe_nodes: i64[:], frames: i64[:], roots: i64[:], logit_rows: i64) -> i64 {
    if c == 0 || ids == 0 || embedding == 0 || head_norm == 0 || head_weight == 0 ||
       layer_count <= 0 || layer_count > 128 || layers.len < layer_count * 32 ||
       frames.len < 4 * layer_count + 4 || roots.len < 2 * layer_count + 2 ||
       work.len < 16 || branch_nodes.len < 40 || attn_cfg.len < 14 || gdn_cfg.len < 7 || moe_cfg.len < 3 ||
       !qk_state_ok(state) || state[4] <= 0 || state[5] != ticket || positions == 0 ||
       indices == 0 || sections == 0 || logit_rows < -1 || logit_rows > 1 { return 0; }
    let count: i64 = ne(ids, 0);
    let hidden: i64 = ne(embedding, 0);
    if count != state[4] || prop(ids, 0) != 26 || ne(ids, 1) != 1 || ne(ids, 2) != 1 || ne(ids, 3) != 1 ||
       ne(embedding, 2) != 1 || ne(embedding, 3) != 1 || ne(head_weight, 0) != hidden || ne(head_weight, 1) != ne(embedding, 1) ||
       !qm_shape(head_norm, hidden, 1, 1) || prop(head_norm, 0) != 0 { return 0; }
    var layer: i64 = 0;
    while layer < layer_count {
        let base: i64 = layer * 32;
        if layers[base] != 0 && layers[base] != 1 { return 0; }
        var w: i64 = 1; let n: i64 = if layers[base] == 0 { 8 } else { 11 };
        while w <= n { if layers[base + w] == 0 { return 0; } w += 1; }
        w = 12; while w < 20 { if layers[base + w] == 0 { return 0; } w += 1; }
        if layers[base] == 0 {
            if layers[base + 22] == 0 || layers[base + 23] == 0 { return 0; }
        } else {
            if layers[base + 20] == 0 || layers[base + 21] == 0 { return 0; }
        }
        layer += 1;
    }
    qa_begin(work, 18, c, embedding, ids, 0, 0);
    var current: i64 = gb_common_args(work);
    frames[0] = current;
    var root_count: i64 = 0;
    layer = 0;
    while layer < layer_count {
        let base: i64 = layer * 32;
        let kind: i64 = layers[base];
        var normed: i64 = 0; var residual: i64 = 0;
        if kind == 0 {
            normed = q35_attn_cached(c, current, positions, mask, layers[base + 1 .. base + 9], attn_cfg, sections,
                                     layers[base + 22], layers[base + 23], indices, state, ticket, work, branch_nodes);
            residual = branch_nodes[26];
        } else {
            normed = q35_gdn_layer(c, current, layers[base + 20], layers[base + 21],
                                    layers[base + 1 .. base + 12], gdn_cfg, work, branch_nodes);
            residual = branch_nodes[37];
            roots[root_count + 1] = branch_nodes[16]; roots[root_count + 2] = branch_nodes[29]; root_count += 2;
        }
        if normed == 0 { return 0; }
        let ffn: i64 = q35_moe_ffn(c, normed, layers[base + 12 .. base + 20], moe_cfg, work, moe_nodes);
        if ffn == 0 { return 0; }
        current = qa_binary(c, ffn, residual, 2, work);
        frames[1 + 4 * layer] = normed; frames[2 + 4 * layer] = moe_nodes[3];
        frames[3 + 4 * layer] = ffn; frames[4 + 4 * layer] = current;
        layer += 1;
    }
    let last: i64 = 1 + 4 * layer_count;
    if logit_rows == 0 {
        roots[root_count + 1] = current; roots[0] = root_count + 1;
        return current;
    }
    if logit_rows == 1 && count > 1 {
        let row_stride = qa_get(current, 1, 1) as! i64;
        current = view2(c, current, hidden, 1, row_stride, (count - 1) * row_stride);
    }
    frames[last] = qa_rms(c, current, attn_cfg[5], work);
    frames[last + 1] = qa_binary(c, frames[last], head_norm, 7, work);
    frames[last + 2] = qa_mm(c, head_weight, frames[last + 1], work);
    roots[root_count + 1] = frames[last + 2]; roots[0] = root_count + 1;
    return frames[last + 2];
}

export fn q35_trunk(c: i64, ids: i64, embedding: i64, head_weight: i64, head_norm: i64,
                    layers: i64[:], layer_count: i64, positions: i64, mask: i64, indices: i64,
                    attn_cfg: i64[:], gdn_cfg: i64[:], moe_cfg: i64[:], sections: i64,
                    state: i64[:], ticket: i64, work: i64[:], branch_nodes: i64[:],
                    moe_nodes: i64[:], frames: i64[:], roots: i64[:]) -> i64 {
    return q35_trunk_output(c, ids, embedding, head_weight, head_norm, layers, layer_count,
        positions, mask, indices, attn_cfg, gdn_cfg, moe_cfg, sections, state, ticket, work,
        branch_nodes, moe_nodes, frames, roots, -1);
}
