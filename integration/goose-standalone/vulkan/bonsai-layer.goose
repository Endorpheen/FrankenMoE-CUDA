const BW_SIGN_H: u64 = 0;
const BW_SIGN_V: u64 = 1;
const BW_SIGN_F: u64 = 2;
const BW_ATTN_NORM: u64 = 3;
const BW_MIX: u64 = 4;
const BW_Z: u64 = 5;
const BW_ALPHA_BETA: u64 = 6;
const BW_DT_A: u64 = 7;
const BW_CONV: u64 = 8;
const BW_GDN_NORM: u64 = 9;
const BW_SSM_OUT: u64 = 10;
const BW_POST_NORM: u64 = 11;
const BW_FFN_GATE: u64 = 12;
const BW_FFN_UP: u64 = 13;
const BW_FFN_DOWN: u64 = 14;
const BS_NORM: u64 = 0;
const BS_ROTATED: u64 = 1;
const BS_MIX: u64 = 2;
const BS_Z: u64 = 3;
const BS_GATES: u64 = 4;
const BS_CONV: u64 = 5;
const BS_QKV: u64 = 6;
const BS_SCORE: u64 = 7;
const BS_GATED: u64 = 8;
const BS_ROTATED_V: u64 = 9;
const BS_PROJECTED: u64 = 10;
const BS_RESIDUAL: u64 = 11;
const BS_GATE: u64 = 12;
const BS_UP: u64 = 13;
const BS_PRODUCT: u64 = 14;
const BS_ROTATED_F: u64 = 15;
const BA_QG: u64 = 0;
const BA_K: u64 = 1;
const BA_V: u64 = 2;
const BA_QN: u64 = 3;
const BA_KN: u64 = 4;
const BA_O: u64 = 5;
const BONSAI_OP_RMSNORM: u32 = 0;
const BONSAI_OP_SWIGLU: u32 = 2;
const BONSAI_OP_ROPE: u32 = 3;
const BONSAI_OP_ADD: u32 = 9;
const BONSAI_OP_PQ2_0: u32 = 10;
const BONSAI_OP_FWHT: u32 = 12;
const BONSAI_OP_QK_NORM: u32 = 14;
const BONSAI_OP_HEAD_SLICE: u32 = 18;
const BONSAI_OP_SIGMOID_GATE: u32 = 19;
const BONSAI_OP_KV_STORE_Q4: u32 = 20;

const BONSAI_HIDDEN: u32 = 5120;
const BONSAI_DIM: u32 = 128;
const BONSAI_HEADS: u32 = 48;
const BONSAI_KEY_HEADS: u32 = 16;
const BONSAI_VALUES: u32 = 6144;
const BONSAI_MIX: u32 = 10240;
const BONSAI_FFN: u32 = 17408;
const BONSAI_QUERY_HEADS: u32 = 24;
const BONSAI_KV_HEADS: u32 = 4;
const BONSAI_HEAD_WIDTH: u32 = 256;
const BONSAI_KV: u32 = 1024;
const BONSAI_WEIGHTS_BYTES: u64 = 360;
const BONSAI_ATTENTION_BYTES: u64 = 144;


fn bonsai_view(base: u64, index: u64) -> BankView { return bank_view(base + index * 24); }

fn bonsai_run(graph: u64, op: u32, width: u32, rows: u32, epsilon: f32,
        input: BankView, weight: BankView, output: BankView) -> bool {
    var shape = bank_shape(width, rows);
    shape.epsilon = epsilon;
    return fg_vk_graph_record(graph, op, shape, input, weight, output) != 0;
}

fn bonsai_finish(graph: u64, w: u64, s: u64, input: BankView, output: BankView, epsilon: f32) -> bool {
    let hidden = BONSAI_HIDDEN;
    let ffn = BONSAI_FFN;
    let zero = 0.0 as! f32;
    return bonsai_run(graph, BONSAI_OP_ADD, hidden, 0, zero, input, bonsai_view(s, BS_PROJECTED), bonsai_view(s, BS_RESIDUAL)) &&
        bonsai_run(graph, BONSAI_OP_RMSNORM, hidden, 0, epsilon, bonsai_view(s, BS_RESIDUAL), bonsai_view(w, BW_POST_NORM), bonsai_view(s, BS_NORM)) &&
        bonsai_run(graph, BONSAI_OP_FWHT, hidden, 0, zero, bonsai_view(s, BS_NORM), bonsai_view(w, BW_SIGN_H), bonsai_view(s, BS_ROTATED)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, hidden, ffn, zero, bonsai_view(s, BS_ROTATED), bonsai_view(w, BW_FFN_GATE), bonsai_view(s, BS_GATE)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, hidden, ffn, zero, bonsai_view(s, BS_ROTATED), bonsai_view(w, BW_FFN_UP), bonsai_view(s, BS_UP)) &&
        bonsai_run(graph, BONSAI_OP_SWIGLU, ffn, 0, zero, bonsai_view(s, BS_GATE), bonsai_view(s, BS_UP), bonsai_view(s, BS_PRODUCT)) &&
        bonsai_run(graph, BONSAI_OP_FWHT, ffn, 0, zero, bonsai_view(s, BS_PRODUCT), bonsai_view(w, BW_SIGN_F), bonsai_view(s, BS_ROTATED_F)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, ffn, hidden, zero, bonsai_view(s, BS_ROTATED_F), bonsai_view(w, BW_FFN_DOWN), bonsai_view(s, BS_PROJECTED)) &&
        bonsai_run(graph, BONSAI_OP_ADD, hidden, 0, zero, bonsai_view(s, BS_RESIDUAL), bonsai_view(s, BS_PROJECTED), output);
}

export fn fg_vk_bonsai_recurrent_layer(graph: u64, layer: u64, s: u64, input: BankView, output: BankView, epsilon: f32) -> i32 {
    let hidden = BONSAI_HIDDEN;
    let zero = 0.0 as! f32;
    let gates = bank_shape(hidden, BONSAI_HEADS);
    var recurrence = bank_shape(BONSAI_DIM, BONSAI_HEADS);
    recurrence.kv_heads = BONSAI_KEY_HEADS;
    var normalize = recurrence;
    normalize.rows += 2 * normalize.kv_heads;
    normalize.epsilon = epsilon;
    var gated_rms = bank_shape(BONSAI_DIM, 0);
    gated_rms.batch = BONSAI_HEADS;
    gated_rms.epsilon = epsilon;
    var grouped = bank_shape(BONSAI_VALUES, BONSAI_DIM);
    grouped.kv_heads = BONSAI_KEY_HEADS;
    grouped.context = BONSAI_HEADS / BONSAI_KEY_HEADS;
    let history = bank_view(layer + BONSAI_WEIGHTS_BYTES);
    let state = bank_view(layer + BONSAI_WEIGHTS_BYTES + 24);

    let ok = bonsai_run(graph, BONSAI_OP_RMSNORM, hidden, 0, epsilon, input, bonsai_view(layer, BW_ATTN_NORM), bonsai_view(s, BS_NORM)) &&
        bonsai_run(graph, BONSAI_OP_FWHT, hidden, 0, zero, bonsai_view(s, BS_NORM), bonsai_view(layer, BW_SIGN_H), bonsai_view(s, BS_ROTATED)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, hidden, BONSAI_MIX, zero, bonsai_view(s, BS_ROTATED), bonsai_view(layer, BW_MIX), bonsai_view(s, BS_MIX)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, hidden, BONSAI_VALUES, zero, bonsai_view(s, BS_ROTATED), bonsai_view(layer, BW_Z), bonsai_view(s, BS_Z)) &&
        fg_vk_graph_gdn_gates(graph, gates, bonsai_view(s, BS_NORM), bonsai_view(layer, BW_ALPHA_BETA), bonsai_view(layer, BW_DT_A), bonsai_view(s, BS_GATES)) != 0 &&
        fg_vk_graph_conv(graph, BONSAI_MIX, bonsai_view(s, BS_MIX), bonsai_view(layer, BW_CONV), history, bonsai_view(s, BS_CONV)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_QK_NORM, normalize, bonsai_view(s, BS_CONV), bonsai_view(s, BS_CONV), bonsai_view(s, BS_QKV)) != 0 &&
        fg_vk_graph_gdn(graph, recurrence, bonsai_view(s, BS_QKV), state, bonsai_view(s, BS_GATES), bonsai_view(s, BS_SCORE)) != 0 &&
        fg_vk_graph_gated_rms(graph, gated_rms, bonsai_view(s, BS_SCORE), bonsai_view(layer, BW_GDN_NORM), bonsai_view(s, BS_Z), bonsai_view(s, BS_GATED)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_FWHT, grouped, bonsai_view(s, BS_GATED), bonsai_view(layer, BW_SIGN_V), bonsai_view(s, BS_ROTATED_V)) != 0 &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, BONSAI_VALUES, hidden, zero, bonsai_view(s, BS_ROTATED_V), bonsai_view(layer, BW_SSM_OUT), bonsai_view(s, BS_PROJECTED)) &&
        bonsai_finish(graph, layer, s, input, output, epsilon);
    return if ok { 1 } else { 0 };
}

export fn fg_vk_bonsai_attention_prepare(graph: u64, layer: u64, s: u64, input: BankView, epsilon: f32, position: u32) -> i32 {
    let a = layer + BONSAI_WEIGHTS_BYTES;
    let h = BONSAI_HIDDEN;
    let zero = 0.0 as! f32;
    let split = bank_shape(BONSAI_HEAD_WIDTH, BONSAI_QUERY_HEADS);
    var gate = split;
    gate.position = 1;
    var norm_q = bank_shape(BONSAI_HEAD_WIDTH, 0);
    norm_q.batch = BONSAI_QUERY_HEADS;
    norm_q.epsilon = epsilon;
    var norm_k = norm_q;
    norm_k.batch = BONSAI_KV_HEADS;
    var rope_q = split;
    rope_q.position = position;
    rope_q.theta = 10000000.0 as! f32;
    rope_q.rotary = 64;
    var rope_k = rope_q;
    rope_k.rows = BONSAI_KV_HEADS;
    let mixed = bonsai_view(s, BS_MIX);
    let ok = bonsai_run(graph, BONSAI_OP_RMSNORM, h, 0, epsilon, input, bonsai_view(layer, BW_ATTN_NORM), bonsai_view(s, BS_NORM)) &&
        bonsai_run(graph, BONSAI_OP_FWHT, h, 0, zero, bonsai_view(s, BS_NORM), bonsai_view(layer, BW_SIGN_H), bonsai_view(s, BS_ROTATED)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, h, 2 * BONSAI_VALUES, zero, bonsai_view(s, BS_ROTATED), bonsai_view(a, BA_QG), mixed) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, h, BONSAI_KV, zero, bonsai_view(s, BS_ROTATED), bonsai_view(a, BA_K), bonsai_view(s, BS_Z)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, h, BONSAI_KV, zero, bonsai_view(s, BS_ROTATED), bonsai_view(a, BA_V), bonsai_view(s, BS_CONV)) &&
        fg_vk_graph_record(graph, BONSAI_OP_HEAD_SLICE, split, mixed, mixed, bonsai_view(s, BS_GATED)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_HEAD_SLICE, gate, mixed, mixed, bonsai_view(s, BS_ROTATED_V)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_RMSNORM, norm_q, bonsai_view(s, BS_GATED), bonsai_view(a, BA_QN), bonsai_view(s, BS_SCORE)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_RMSNORM, norm_k, bonsai_view(s, BS_Z), bonsai_view(a, BA_KN), bonsai_view(s, BS_GATES)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_ROPE, rope_q, bonsai_view(s, BS_SCORE), bonsai_view(s, BS_SCORE), bonsai_view(s, BS_GATED)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_ROPE, rope_k, bonsai_view(s, BS_GATES), bonsai_view(s, BS_GATES), bonsai_view(s, BS_QKV)) != 0;
    return if ok { 1 } else { 0 };
}

fn bonsai_keys(layer: u64) -> BankView { return bank_view(layer + BONSAI_WEIGHTS_BYTES + BONSAI_ATTENTION_BYTES); }
fn bonsai_values(layer: u64) -> BankView { return bank_view(layer + BONSAI_WEIGHTS_BYTES + BONSAI_ATTENTION_BYTES + 24); }

export fn fg_vk_bonsai_attention_finish(graph: u64, layer: u64, s: u64, input: BankView, output: BankView,
        epsilon: f32, position: u32, capacity: u32) -> i32 {
    var attend = bank_shape(BONSAI_HEAD_WIDTH, BONSAI_QUERY_HEADS);
    attend.position = position;
    attend.context = capacity;
    attend.kv_heads = BONSAI_KV_HEADS;
    let q = BONSAI_VALUES;
    let zero = 0.0 as! f32;
    let ok = fg_vk_graph_attention_q4(graph, attend, bonsai_view(s, BS_GATED), bonsai_keys(layer), bonsai_values(layer), bonsai_view(s, BS_PRODUCT)) != 0 &&
        bonsai_run(graph, BONSAI_OP_SIGMOID_GATE, q, 0, zero, bonsai_view(s, BS_PRODUCT), bonsai_view(s, BS_ROTATED_V), bonsai_view(s, BS_SCORE)) &&
        bonsai_run(graph, BONSAI_OP_FWHT, q, 0, zero, bonsai_view(s, BS_SCORE), bonsai_view(layer, BW_SIGN_V), bonsai_view(s, BS_ROTATED_V)) &&
        bonsai_run(graph, BONSAI_OP_PQ2_0, q, BONSAI_HIDDEN, zero, bonsai_view(s, BS_ROTATED_V),
            bonsai_view(layer + BONSAI_WEIGHTS_BYTES, BA_O), bonsai_view(s, BS_PROJECTED)) &&
        bonsai_finish(graph, layer, s, input, output, epsilon);
    return if ok { 1 } else { 0 };
}

export fn fg_vk_bonsai_attention_layer(graph: u64, layer: u64, s: u64, input: BankView, output: BankView,
        epsilon: f32, position: u32, capacity: u32) -> i32 {
    var store = bank_shape(BONSAI_HEAD_WIDTH, BONSAI_KV_HEADS);
    store.position = position;
    store.context = capacity;
    let keys = bonsai_view(s, BS_QKV);
    let values = bonsai_view(s, BS_CONV);
    let ok = fg_vk_bonsai_attention_prepare(graph, layer, s, input, epsilon, position) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_KV_STORE_Q4, store, keys, keys, bonsai_keys(layer)) != 0 &&
        fg_vk_graph_record(graph, BONSAI_OP_KV_STORE_Q4, store, values, values, bonsai_values(layer)) != 0 &&
        fg_vk_bonsai_attention_finish(graph, layer, s, input, output, epsilon, position, capacity) != 0;
    return if ok { 1 } else { 0 };
}
