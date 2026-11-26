#ifndef FG_VULKAN_BONSAI_LAYER_H
#define FG_VULKAN_BONSAI_LAYER_H

#include "graph.h"

enum { FG_BONSAI_HIDDEN = 5120, FG_BONSAI_DIM = 128, FG_BONSAI_KEY_HEADS = 16, FG_BONSAI_HEADS = 48,
       FG_BONSAI_VALUES = FG_BONSAI_HEADS * FG_BONSAI_DIM,
       FG_BONSAI_MIX = (FG_BONSAI_HEADS + 2 * FG_BONSAI_KEY_HEADS) * FG_BONSAI_DIM,
       FG_BONSAI_FFN = 17408, FG_BONSAI_QUERY_HEADS = 24, FG_BONSAI_KV_HEADS = 4,
       FG_BONSAI_HEAD_WIDTH = 256, FG_BONSAI_KV = FG_BONSAI_KV_HEADS * FG_BONSAI_HEAD_WIDTH,
       FG_BONSAI_KV_BYTES = FG_BONSAI_KV / 32 * 18 };

enum {
    BW_SIGN_H, BW_SIGN_V, BW_SIGN_F, BW_ATTN_NORM, BW_MIX, BW_Z, BW_ALPHA_BETA,
    BW_DT_A, BW_CONV, BW_GDN_NORM, BW_SSM_OUT, BW_POST_NORM, BW_FFN_GATE,
    BW_FFN_UP, BW_FFN_DOWN, BW_COUNT
};

enum {
    BS_NORM, BS_ROTATED, BS_MIX, BS_Z, BS_GATES, BS_CONV, BS_QKV, BS_SCORE,
    BS_GATED, BS_ROTATED_V, BS_PROJECTED, BS_RESIDUAL, BS_GATE, BS_UP, BS_PRODUCT, BS_ROTATED_F, BS_COUNT
};

typedef struct {
    fg_vk_view weights[BW_COUNT];
    fg_vk_view history, state;
} fg_vk_bonsai_layer;

enum { BA_QG, BA_K, BA_V, BA_QN, BA_KN, BA_O, BA_COUNT };
typedef struct {
    fg_vk_view common[BW_COUNT], attention[BA_COUNT];
    fg_vk_view keys, values;
} fg_vk_bonsai_attention;

int fg_vk_bonsai_recurrent_layer(fg_vk_graph *graph, const fg_vk_bonsai_layer *layer,
    const fg_vk_view scratch[BS_COUNT], fg_vk_view input, fg_vk_view output, float epsilon);
int fg_vk_bonsai_attention_prepare(fg_vk_graph *graph, const fg_vk_bonsai_attention *layer,
    const fg_vk_view scratch[BS_COUNT], fg_vk_view input, float epsilon, uint32_t position);
int fg_vk_bonsai_attention_finish(fg_vk_graph *graph, const fg_vk_bonsai_attention *layer,
    const fg_vk_view scratch[BS_COUNT], fg_vk_view input, fg_vk_view output,
    float epsilon, uint32_t position, uint32_t capacity);
int fg_vk_bonsai_attention_layer(fg_vk_graph *graph, const fg_vk_bonsai_attention *layer,
    const fg_vk_view scratch[BS_COUNT], fg_vk_view input, fg_vk_view output,
    float epsilon, uint32_t position, uint32_t capacity);

#endif
