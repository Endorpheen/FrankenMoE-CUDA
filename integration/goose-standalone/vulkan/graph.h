#ifndef FG_VULKAN_GRAPH_H
#define FG_VULKAN_GRAPH_H

#include "device.h"

typedef enum {
    FG_VK_RMSNORM, FG_VK_Q4_0, FG_VK_SWIGLU, FG_VK_ROPE, FG_VK_KV_STORE,
    FG_VK_ATTENTION, FG_VK_Q8_0, FG_VK_GATHER_Q4_0, FG_VK_BIAS, FG_VK_ADD,
    FG_VK_PQ2_0, FG_VK_GATHER_PQ2_0, FG_VK_FWHT,
    FG_VK_CAUSAL_CONV, FG_VK_QK_NORM, FG_VK_GDN, FG_VK_OP_COUNT
} fg_vk_op;

typedef struct {
    const fg_vk_buffer *buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
} fg_vk_view;

/* For a quantized projection stripe, position is the first output column and
 * context is the full output stride. The weight view contains only the stripe.
 * A gather uses position as the first token ID in the weight view.
 * For FWHT, position selects forward or inverse; rows, kv_heads and context
 * describe head width, key head count and repeat count for grouped input. */
typedef struct {
    uint32_t width, rows, batch;
    float epsilon;
    uint32_t position, kv_heads, context;
    float theta;
} fg_vk_shape;

typedef struct {
    fg_vk_device *vk;
    VkDescriptorSetLayout sets;
    VkPipelineLayout layout;
    VkPipeline pipelines[FG_VK_OP_COUNT];
    VkDescriptorPool descriptors;
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkSemaphore done;
    uint64_t epoch;
    uint32_t capacity, used;
    int recording;
} fg_vk_graph;

/* Graph recording, submission and host buffer access use one owning thread. */
int fg_vk_graph_open(fg_vk_graph *graph, fg_vk_device *vk, uint32_t capacity);
void fg_vk_graph_close(fg_vk_graph *graph);
int fg_vk_graph_begin(fg_vk_graph *graph);
int fg_vk_graph_record(fg_vk_graph *graph, fg_vk_op op, fg_vk_shape shape,
    fg_vk_view input, fg_vk_view weight, fg_vk_view output);
int fg_vk_graph_attention(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output);
/* The convolution updates [time][channel] history in place, oldest first. */
int fg_vk_graph_conv(fg_vk_graph *graph, uint32_t channels, fg_vk_view input,
    fg_vk_view coefficients, fg_vk_view history, fg_vk_view output);
/* Q/K/V is one view. Gates packs log decay then beta; state uses [head][value][key]. */
int fg_vk_graph_gdn(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view qkv,
    fg_vk_view state, fg_vk_view gates, fg_vk_view output);
int fg_vk_graph_submit(fg_vk_graph *graph);

#endif
