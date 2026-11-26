#include "bonsai-layer.h"

static int run(fg_vk_graph *graph, fg_vk_op op, uint32_t width, uint32_t rows,
    float epsilon, fg_vk_view input, fg_vk_view weight, fg_vk_view output) {
    return fg_vk_graph_record(graph, op, (fg_vk_shape){.width = width, .rows = rows, .batch = 1, .epsilon = epsilon},
        input, weight, output);
}

int fg_vk_bonsai_recurrent_layer(fg_vk_graph *graph, const fg_vk_bonsai_layer *layer,
    const fg_vk_view s[BS_COUNT], fg_vk_view input, fg_vk_view output, float epsilon) {
    const fg_vk_view *w = layer->weights;
    const uint32_t hidden = FG_BONSAI_HIDDEN, mix = FG_BONSAI_MIX, values = FG_BONSAI_VALUES, ffn = FG_BONSAI_FFN;
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
        run(graph, FG_VK_ADD, hidden, 0, 0, input, s[BS_PROJECTED], s[BS_RESIDUAL]) &&
        run(graph, FG_VK_RMSNORM, hidden, 0, epsilon, s[BS_RESIDUAL], w[BW_POST_NORM], s[BS_NORM]) &&
        run(graph, FG_VK_FWHT, hidden, 0, 0, s[BS_NORM], w[BW_SIGN_H], s[BS_ROTATED]) &&
        run(graph, FG_VK_PQ2_0, hidden, ffn, 0, s[BS_ROTATED], w[BW_FFN_GATE], s[BS_GATE]) &&
        run(graph, FG_VK_PQ2_0, hidden, ffn, 0, s[BS_ROTATED], w[BW_FFN_UP], s[BS_UP]) &&
        run(graph, FG_VK_SWIGLU, ffn, 0, 0, s[BS_GATE], s[BS_UP], s[BS_PRODUCT]) &&
        run(graph, FG_VK_FWHT, ffn, 0, 0, s[BS_PRODUCT], w[BW_SIGN_F], s[BS_ROTATED_F]) &&
        run(graph, FG_VK_PQ2_0, ffn, hidden, 0, s[BS_ROTATED_F], w[BW_FFN_DOWN], s[BS_PROJECTED]) &&
        run(graph, FG_VK_ADD, hidden, 0, 0, s[BS_RESIDUAL], s[BS_PROJECTED], output);
}
