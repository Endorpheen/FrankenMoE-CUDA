#ifndef FG_VULKAN_QWEN2_H
#define FG_VULKAN_QWEN2_H

#include "graph.h"

enum { FG_QWEN_LAYERS = 24, FG_QWEN_VOCAB = 151936 };

typedef enum {
    WT_AN, WT_Q, WT_K, WT_V, WT_QB, WT_KB, WT_VB, WT_O, WT_FN, WT_GATE, WT_UP, WT_DOWN, WT_COUNT
} fg_qwen_weights;
typedef enum { WT_EMBED, WT_NORM, WT_LOGITS, WT_GLOBAL_COUNT } fg_qwen_global;
typedef enum {
    T_IDS, T_HIDDEN, T_NORM, T_Q, T_K, T_V, T_Q_BIAS, T_K_BIAS, T_V_BIAS,
    T_ATTN, T_PROJECTED, T_RESIDUAL, T_GATE, T_UP, T_PRODUCT, T_LOGITS, T_COUNT
} fg_qwen_temp;

typedef struct {
    fg_vk_device vk;
    fg_vk_graph graph;
    fg_vk_buffer weights, work, host;
    fg_vk_view layers[FG_QWEN_LAYERS][WT_COUNT];
    fg_vk_view globals[WT_GLOBAL_COUNT], scratch[T_COUNT];
    fg_vk_view keys[FG_QWEN_LAYERS], values[FG_QWEN_LAYERS];
    uint32_t context, position;
    float epsilon, theta;
} fg_vk_qwen2;

int fg_vk_qwen2_open(fg_vk_qwen2 *model, const char *path, uint32_t context,
    uint32_t index, int allow_cpu);
void fg_vk_qwen2_close(fg_vk_qwen2 *model);
int fg_vk_qwen2_step(fg_vk_qwen2 *model, uint32_t id, float *logits);

#endif
