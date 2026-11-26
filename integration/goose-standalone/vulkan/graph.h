#ifndef FG_VULKAN_GRAPH_H
#define FG_VULKAN_GRAPH_H

#include "device.h"

typedef enum {
    FG_VK_RMSNORM, FG_VK_Q4_0, FG_VK_SWIGLU, FG_VK_ROPE, FG_VK_KV_STORE,
    FG_VK_ATTENTION, FG_VK_OP_COUNT
} fg_vk_op;

typedef struct {
    const fg_vk_buffer *buffer;
    VkDeviceSize offset;
    VkDeviceSize size;
} fg_vk_view;

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
int fg_vk_graph_submit(fg_vk_graph *graph);

#endif
