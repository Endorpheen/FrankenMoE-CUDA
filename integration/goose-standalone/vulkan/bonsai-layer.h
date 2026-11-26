#ifndef FG_VULKAN_BONSAI_LAYER_H
#define FG_VULKAN_BONSAI_LAYER_H

#include "graph.h"

enum { FG_BONSAI_HIDDEN = 5120, FG_BONSAI_DIM = 128, FG_BONSAI_KEY_HEADS = 16, FG_BONSAI_HEADS = 48,
       FG_BONSAI_VALUES = FG_BONSAI_HEADS * FG_BONSAI_DIM,
       FG_BONSAI_MIX = (FG_BONSAI_HEADS + 2 * FG_BONSAI_KEY_HEADS) * FG_BONSAI_DIM,
       FG_BONSAI_FFN = 17408 };

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

int fg_vk_bonsai_recurrent_layer(fg_vk_graph *graph, const fg_vk_bonsai_layer *layer,
    const fg_vk_view scratch[BS_COUNT], fg_vk_view input, fg_vk_view output, float epsilon);

#endif
