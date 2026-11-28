#ifndef FG_VULKAN_GRAPH_H
#define FG_VULKAN_GRAPH_H

#include "device.h"

typedef enum {
    FG_VK_RMSNORM, FG_VK_Q4_0, FG_VK_SWIGLU, FG_VK_ROPE, FG_VK_KV_STORE,
    FG_VK_ATTENTION, FG_VK_Q8_0, FG_VK_GATHER_Q4_0, FG_VK_BIAS, FG_VK_ADD,
    FG_VK_PQ2_0, FG_VK_GATHER_PQ2_0, FG_VK_FWHT,
    FG_VK_CAUSAL_CONV, FG_VK_QK_NORM, FG_VK_GDN, FG_VK_GDN_GATES, FG_VK_GATED_RMS,
    FG_VK_HEAD_SLICE, FG_VK_SIGMOID_GATE, FG_VK_KV_STORE_Q4, FG_VK_ATTENTION_Q4,
    FG_VK_Q4_K, FG_VK_Q6_K, FG_VK_GATHER_Q4_K, FG_VK_IQ4_NL, FG_VK_IQ4_XS,
    FG_VK_GATHER_Q8_0, FG_VK_F32_PROJECT, FG_VK_MOE_ROUTE, FG_VK_MOE_WEIGHTED,
    FG_VK_APEX_QK_NORM, FG_VK_GDN_APPLY, FG_VK_SIGMOID_GATE_SCALAR, FG_VK_KV_FWHT,
    FG_VK_INKLING_ROUTE, FG_VK_INKLING_SITU, FG_VK_INKLING_ATTENTION, FG_VK_OP_COUNT
} fg_vk_op;

typedef struct {
    const fg_vk_buffer *buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
} fg_vk_view;

/* For a quantized projection stripe, position is the first output column and
 * context is the full output stride. The weight view contains only the stripe.
 * A gather uses position as the first token ID in the weight view.
 * A MoE route uses width=experts, rows=selected, theta=weight scale.
 * The output row contains selected uint32 IDs followed by their FP32 weights.
 * MoE weighted uses width=hidden, rows=selected and consumes the same route
 * layout with expert outputs in [batch][selected][hidden] order.
 * For FWHT, position selects forward or inverse; rows, kv_heads and context
 * describe head width, key head count and repeat count for grouped input. */
typedef struct {
    uint32_t width, rows, batch;
    float epsilon;
    uint32_t position, kv_heads, context;
    float theta;
    uint32_t rotary;
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

_Static_assert(FG_VK_RMSNORM == 0 &&
    FG_VK_Q4_0 == 1 &&
    FG_VK_SWIGLU == 2 &&
    FG_VK_ROPE == 3 &&
    FG_VK_KV_STORE == 4 &&
    FG_VK_ATTENTION == 5 &&
    FG_VK_Q8_0 == 6 &&
    FG_VK_GATHER_Q4_0 == 7 &&
    FG_VK_BIAS == 8 &&
    FG_VK_ADD == 9 &&
    FG_VK_PQ2_0 == 10 &&
    FG_VK_GATHER_PQ2_0 == 11 &&
    FG_VK_FWHT == 12 &&
    FG_VK_CAUSAL_CONV == 13 &&
    FG_VK_QK_NORM == 14 &&
    FG_VK_GDN == 15 &&
    FG_VK_GDN_GATES == 16 &&
    FG_VK_GATED_RMS == 17 &&
    FG_VK_HEAD_SLICE == 18 &&
    FG_VK_SIGMOID_GATE == 19 &&
    FG_VK_KV_STORE_Q4 == 20 &&
    FG_VK_ATTENTION_Q4 == 21 &&
    FG_VK_Q4_K == 22 &&
    FG_VK_Q6_K == 23 &&
    FG_VK_GATHER_Q4_K == 24 &&
    FG_VK_IQ4_NL == 25 &&
    FG_VK_IQ4_XS == 26 &&
    FG_VK_GATHER_Q8_0 == 27 &&
    FG_VK_F32_PROJECT == 28 &&
    FG_VK_MOE_ROUTE == 29 &&
    FG_VK_MOE_WEIGHTED == 30 &&
    FG_VK_APEX_QK_NORM == 31 &&
    FG_VK_GDN_APPLY == 32 &&
    FG_VK_SIGMOID_GATE_SCALAR == 33 &&
    FG_VK_KV_FWHT == 34 &&
    FG_VK_INKLING_ROUTE == 35 &&
    FG_VK_INKLING_SITU == 36 &&
    FG_VK_INKLING_ATTENTION == 37, "Goose Vulkan dispatch operation ABI");

/* Graph recording, submission and host buffer access use one owning thread. */
int fg_vk_graph_open(fg_vk_graph *graph, fg_vk_device *vk, uint32_t capacity);
void fg_vk_graph_close(fg_vk_graph *graph);
int fg_vk_graph_begin(fg_vk_graph *graph);
int fg_vk_graph_record(fg_vk_graph *graph, fg_vk_op op, fg_vk_shape shape,
    fg_vk_view input, fg_vk_view weight, fg_vk_view output);
int fg_vk_graph_attention(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output);
int fg_vk_graph_attention_q4(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output);
/* Inkling query storage is Q, relative query, sinks, then the bias table.
 * The rotary field is zero for full attention or the active sliding window. */
int fg_vk_graph_inkling_attention(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output);
/* The convolution updates [time][channel] history in place, oldest first. */
int fg_vk_graph_conv(fg_vk_graph *graph, uint32_t channels, fg_vk_view input,
    fg_vk_view coefficients, fg_vk_view history, fg_vk_view output);
/* Q/K/V is one view. Gates packs log decay then beta; state uses [head][value][key]. */
int fg_vk_graph_gdn(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view qkv,
    fg_vk_view state, fg_vk_view gates, fg_vk_view output);
int fg_vk_graph_gdn_gates(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view input,
    fg_vk_view weights, fg_vk_view bias_scale, fg_vk_view output);
int fg_vk_graph_gdn_apply(fg_vk_graph *graph, uint32_t heads, fg_vk_view alpha,
    fg_vk_view beta, fg_vk_view bias_scale, fg_vk_view output);
int fg_vk_graph_gated_rms(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view input,
    fg_vk_view weights, fg_vk_view gate, fg_vk_view output);
int fg_vk_graph_submit(fg_vk_graph *graph);

#endif
