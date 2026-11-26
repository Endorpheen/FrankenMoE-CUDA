#include "bonsai-layer.h"

static int run(fg_vk_graph *graph, fg_vk_op op, uint32_t width, uint32_t rows,
    float epsilon, fg_vk_view input, fg_vk_view weight, fg_vk_view output) {
    return fg_vk_graph_record(graph, op, (fg_vk_shape){.width = width, .rows = rows, .batch = 1, .epsilon = epsilon},
        input, weight, output);
}

static int finish(fg_vk_graph *graph, const fg_vk_view *w, const fg_vk_view *s,
    fg_vk_view input, fg_vk_view output, float epsilon) {
    const uint32_t hidden = FG_BONSAI_HIDDEN, ffn = FG_BONSAI_FFN;
    return run(graph, FG_VK_ADD, hidden, 0, 0, input, s[BS_PROJECTED], s[BS_RESIDUAL]) &&
        run(graph, FG_VK_RMSNORM, hidden, 0, epsilon, s[BS_RESIDUAL], w[BW_POST_NORM], s[BS_NORM]) &&
        run(graph, FG_VK_FWHT, hidden, 0, 0, s[BS_NORM], w[BW_SIGN_H], s[BS_ROTATED]) &&
        run(graph, FG_VK_PQ2_0, hidden, ffn, 0, s[BS_ROTATED], w[BW_FFN_GATE], s[BS_GATE]) &&
        run(graph, FG_VK_PQ2_0, hidden, ffn, 0, s[BS_ROTATED], w[BW_FFN_UP], s[BS_UP]) &&
        run(graph, FG_VK_SWIGLU, ffn, 0, 0, s[BS_GATE], s[BS_UP], s[BS_PRODUCT]) &&
        run(graph, FG_VK_FWHT, ffn, 0, 0, s[BS_PRODUCT], w[BW_SIGN_F], s[BS_ROTATED_F]) &&
        run(graph, FG_VK_PQ2_0, ffn, hidden, 0, s[BS_ROTATED_F], w[BW_FFN_DOWN], s[BS_PROJECTED]) &&
        run(graph, FG_VK_ADD, hidden, 0, 0, s[BS_RESIDUAL], s[BS_PROJECTED], output);
}

int fg_vk_bonsai_recurrent_layer(fg_vk_graph *graph, const fg_vk_bonsai_layer *layer,
    const fg_vk_view s[BS_COUNT], fg_vk_view input, fg_vk_view output, float epsilon) {
    const fg_vk_view *w = layer->weights;
    const uint32_t hidden = FG_BONSAI_HIDDEN, mix = FG_BONSAI_MIX, values = FG_BONSAI_VALUES;
    const fg_vk_shape gates = {.width = FG_BONSAI_HIDDEN, .rows = FG_BONSAI_HEADS, .batch = 1};
    const fg_vk_shape recurrence = {.width = FG_BONSAI_DIM, .rows = FG_BONSAI_HEADS,
        .kv_heads = FG_BONSAI_KEY_HEADS, .batch = 1};
    fg_vk_shape normalize = recurrence;
    normalize.rows += 2 * normalize.kv_heads;
    normalize.epsilon = epsilon;
    const fg_vk_shape gated_rms = {.width = FG_BONSAI_DIM, .batch = FG_BONSAI_HEADS, .epsilon = epsilon};
    const fg_vk_shape grouped = {.width = FG_BONSAI_VALUES, .rows = FG_BONSAI_DIM,
        .kv_heads = FG_BONSAI_KEY_HEADS, .context = FG_BONSAI_HEADS / FG_BONSAI_KEY_HEADS, .batch = 1};

    return run(graph, FG_VK_RMSNORM, hidden, 0, epsilon, input, w[BW_ATTN_NORM], s[BS_NORM]) &&
        run(graph, FG_VK_FWHT, hidden, 0, 0, s[BS_NORM], w[BW_SIGN_H], s[BS_ROTATED]) &&
        run(graph, FG_VK_PQ2_0, hidden, mix, 0, s[BS_ROTATED], w[BW_MIX], s[BS_MIX]) &&
        run(graph, FG_VK_PQ2_0, hidden, values, 0, s[BS_ROTATED], w[BW_Z], s[BS_Z]) &&
        fg_vk_graph_gdn_gates(graph, gates, s[BS_NORM], w[BW_ALPHA_BETA], w[BW_DT_A], s[BS_GATES]) &&
        fg_vk_graph_conv(graph, mix, s[BS_MIX], w[BW_CONV], layer->history, s[BS_CONV]) &&
        fg_vk_graph_record(graph, FG_VK_QK_NORM, normalize, s[BS_CONV], s[BS_CONV], s[BS_QKV]) &&
        fg_vk_graph_gdn(graph, recurrence, s[BS_QKV], layer->state, s[BS_GATES], s[BS_SCORE]) &&
        fg_vk_graph_gated_rms(graph, gated_rms, s[BS_SCORE], w[BW_GDN_NORM], s[BS_Z], s[BS_GATED]) &&
        fg_vk_graph_record(graph, FG_VK_FWHT, grouped, s[BS_GATED], w[BW_SIGN_V], s[BS_ROTATED_V]) &&
        run(graph, FG_VK_PQ2_0, values, hidden, 0, s[BS_ROTATED_V], w[BW_SSM_OUT], s[BS_PROJECTED]) &&
        finish(graph, w, s, input, output, epsilon);
}

int fg_vk_bonsai_attention_prepare(fg_vk_graph *graph, const fg_vk_bonsai_attention *layer,
    const fg_vk_view s[BS_COUNT], fg_vk_view input, float epsilon, uint32_t position) {
    const fg_vk_view *w = layer->common, *a = layer->attention;
    const uint32_t h = FG_BONSAI_HIDDEN, q = FG_BONSAI_VALUES, kv = FG_BONSAI_KV;
    fg_vk_shape split = {.width = FG_BONSAI_HEAD_WIDTH, .rows = FG_BONSAI_QUERY_HEADS, .batch = 1};
    fg_vk_shape gate = split;
    gate.position = 1;
    const fg_vk_shape norm_q = {.width = FG_BONSAI_HEAD_WIDTH, .batch = FG_BONSAI_QUERY_HEADS, .epsilon = epsilon};
    fg_vk_shape norm_k = norm_q;
    norm_k.batch = FG_BONSAI_KV_HEADS;
    fg_vk_shape rope_q = split;
    rope_q.position = position;
    rope_q.theta = 1e7f;
    rope_q.rotary = 64;
    fg_vk_shape rope_k = rope_q;
    rope_k.rows = FG_BONSAI_KV_HEADS;

    return run(graph, FG_VK_RMSNORM, h, 0, epsilon, input, w[BW_ATTN_NORM], s[BS_NORM]) &&
        run(graph, FG_VK_FWHT, h, 0, 0, s[BS_NORM], w[BW_SIGN_H], s[BS_ROTATED]) &&
        run(graph, FG_VK_PQ2_0, h, 2 * q, 0, s[BS_ROTATED], a[BA_QG], s[BS_MIX]) &&
        run(graph, FG_VK_PQ2_0, h, kv, 0, s[BS_ROTATED], a[BA_K], s[BS_Z]) &&
        run(graph, FG_VK_PQ2_0, h, kv, 0, s[BS_ROTATED], a[BA_V], s[BS_CONV]) &&
        fg_vk_graph_record(graph, FG_VK_HEAD_SLICE, split, s[BS_MIX], s[BS_MIX], s[BS_GATED]) &&
        fg_vk_graph_record(graph, FG_VK_HEAD_SLICE, gate, s[BS_MIX], s[BS_MIX], s[BS_ROTATED_V]) &&
        fg_vk_graph_record(graph, FG_VK_RMSNORM, norm_q, s[BS_GATED], a[BA_QN], s[BS_SCORE]) &&
        fg_vk_graph_record(graph, FG_VK_RMSNORM, norm_k, s[BS_Z], a[BA_KN], s[BS_GATES]) &&
        fg_vk_graph_record(graph, FG_VK_ROPE, rope_q, s[BS_SCORE], s[BS_SCORE], s[BS_GATED]) &&
        fg_vk_graph_record(graph, FG_VK_ROPE, rope_k, s[BS_GATES], s[BS_GATES], s[BS_QKV]);
}

int fg_vk_bonsai_attention_finish(fg_vk_graph *graph, const fg_vk_bonsai_attention *layer,
    const fg_vk_view s[BS_COUNT], fg_vk_view input, fg_vk_view output,
    float epsilon, uint32_t position, uint32_t capacity) {
    const fg_vk_shape attend = {.width = FG_BONSAI_HEAD_WIDTH, .rows = FG_BONSAI_QUERY_HEADS,
        .batch = 1, .position = position, .context = capacity, .kv_heads = FG_BONSAI_KV_HEADS};
    const uint32_t q = FG_BONSAI_VALUES;
    return fg_vk_graph_attention_q4(graph, attend, s[BS_GATED], layer->keys, layer->values, s[BS_PRODUCT]) &&
        run(graph, FG_VK_SIGMOID_GATE, q, 0, 0, s[BS_PRODUCT], s[BS_ROTATED_V], s[BS_SCORE]) &&
        run(graph, FG_VK_FWHT, q, 0, 0, s[BS_SCORE], layer->common[BW_SIGN_V], s[BS_ROTATED_V]) &&
        run(graph, FG_VK_PQ2_0, q, FG_BONSAI_HIDDEN, 0, s[BS_ROTATED_V], layer->attention[BA_O], s[BS_PROJECTED]) &&
        finish(graph, layer->common, s, input, output, epsilon);
}

int fg_vk_bonsai_attention_layer(fg_vk_graph *graph, const fg_vk_bonsai_attention *layer,
    const fg_vk_view s[BS_COUNT], fg_vk_view input, fg_vk_view output,
    float epsilon, uint32_t position, uint32_t capacity) {
    const fg_vk_shape store = {.width = FG_BONSAI_HEAD_WIDTH, .rows = FG_BONSAI_KV_HEADS,
        .batch = 1, .position = position, .context = capacity};
    return fg_vk_bonsai_attention_prepare(graph, layer, s, input, epsilon, position) &&
        fg_vk_graph_record(graph, FG_VK_KV_STORE_Q4, store, s[BS_QKV], s[BS_QKV], layer->keys) &&
        fg_vk_graph_record(graph, FG_VK_KV_STORE_Q4, store, s[BS_CONV], s[BS_CONV], layer->values) &&
        fg_vk_bonsai_attention_finish(graph, layer, s, input, output, epsilon, position, capacity);
}
