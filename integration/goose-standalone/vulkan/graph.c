#include "graph.h"
#include "shader_words.h"

_Static_assert(sizeof(fg_vk_shape) == 36, "Vulkan push constant layout");
enum { FG_VK_BINDINGS = 4 };

typedef struct {
    const uint32_t *words;
    size_t bytes;
} fg_vk_shader;

static const fg_vk_shader shaders[FG_VK_OP_COUNT] = {
    [FG_VK_RMSNORM] = { fg_rmsnorm, sizeof(fg_rmsnorm) },
    [FG_VK_Q4_0] = { fg_q4_0, sizeof(fg_q4_0) },
    [FG_VK_Q4_K] = { fg_q4_k, sizeof(fg_q4_k) },
    [FG_VK_Q6_K] = { fg_q6_k, sizeof(fg_q6_k) },
    [FG_VK_GATHER_Q4_K] = { fg_gather_q4_k, sizeof(fg_gather_q4_k) },
    [FG_VK_IQ4_NL] = { fg_iq4_nl, sizeof(fg_iq4_nl) },
    [FG_VK_IQ4_XS] = { fg_iq4_xs, sizeof(fg_iq4_xs) },
    [FG_VK_SWIGLU] = { fg_swiglu, sizeof(fg_swiglu) },
    [FG_VK_ROPE] = { fg_rope, sizeof(fg_rope) },
    [FG_VK_KV_STORE] = { fg_kv_store, sizeof(fg_kv_store) },
    [FG_VK_ATTENTION] = { fg_attention, sizeof(fg_attention) },
    [FG_VK_Q8_0] = { fg_q8_0, sizeof(fg_q8_0) },
    [FG_VK_GATHER_Q8_0] = { fg_gather_q8_0, sizeof(fg_gather_q8_0) },
    [FG_VK_GATHER_Q4_0] = { fg_gather_q4_0, sizeof(fg_gather_q4_0) },
    [FG_VK_BIAS] = { fg_bias, sizeof(fg_bias) },
    [FG_VK_ADD] = { fg_add, sizeof(fg_add) },
    [FG_VK_PQ2_0] = { fg_pq2_0, sizeof(fg_pq2_0) },
    [FG_VK_GATHER_PQ2_0] = { fg_gather_pq2_0, sizeof(fg_gather_pq2_0) },
    [FG_VK_FWHT] = { fg_fwht, sizeof(fg_fwht) },
    [FG_VK_CAUSAL_CONV] = { fg_causal_conv, sizeof(fg_causal_conv) },
    [FG_VK_QK_NORM] = { fg_qk_norm, sizeof(fg_qk_norm) },
    [FG_VK_GDN] = { fg_gdn, sizeof(fg_gdn) },
    [FG_VK_GDN_GATES] = { fg_gdn_gates, sizeof(fg_gdn_gates) },
    [FG_VK_GATED_RMS] = { fg_gated_rms, sizeof(fg_gated_rms) },
    [FG_VK_HEAD_SLICE] = { fg_head_slice, sizeof(fg_head_slice) },
    [FG_VK_SIGMOID_GATE] = { fg_sigmoid_gate, sizeof(fg_sigmoid_gate) },
    [FG_VK_KV_STORE_Q4] = { fg_kv_store_q4, sizeof(fg_kv_store_q4) },
    [FG_VK_ATTENTION_Q4] = { fg_attention_q4, sizeof(fg_attention_q4) },
    [FG_VK_F32_PROJECT] = { fg_f32_project, sizeof(fg_f32_project) },
    [FG_VK_MOE_ROUTE] = { fg_moe_route, sizeof(fg_moe_route) },
    [FG_VK_MOE_WEIGHTED] = { fg_moe_weighted, sizeof(fg_moe_weighted) },
    [FG_VK_APEX_QK_NORM] = { fg_apex_qk_norm, sizeof(fg_apex_qk_norm) },
    [FG_VK_GDN_APPLY] = { fg_gdn_apply, sizeof(fg_gdn_apply) },
    [FG_VK_SIGMOID_GATE_SCALAR] = { fg_sigmoid_gate_scalar, sizeof(fg_sigmoid_gate_scalar) },
    [FG_VK_KV_FWHT] = { fg_kv_fwht, sizeof(fg_kv_fwht) },
    [FG_VK_INKLING_ROUTE] = { fg_inkling_route, sizeof(fg_inkling_route) },
    [FG_VK_INKLING_SITU] = { fg_inkling_situ, sizeof(fg_inkling_situ) },
    [FG_VK_INKLING_ATTENTION] = { fg_inkling_attention, sizeof(fg_inkling_attention) },
};

static VkResult create_pipeline(fg_vk_graph *graph, fg_vk_op op) {
    fg_vk_device *vk = graph->vk;
    const VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = shaders[op].bytes, .pCode = shaders[op].words,
    };
    VkShaderModule module;
    VkResult result = vk->CreateShaderModule(vk->device, &info, NULL, &module);
    if (result != VK_SUCCESS) return result;
    const VkComputePipelineCreateInfo pipeline = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .layout = graph->layout,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = module, .pName = "main",
        },
    };
    result = vk->CreateComputePipelines(vk->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &graph->pipelines[op]);
    vk->DestroyShaderModule(vk->device, module, NULL);
    return result;
}

void fg_vk_graph_close(fg_vk_graph *graph) {
    fg_vk_device *vk = graph->vk;
    if (!vk) return;
    if (graph->pool) vk->DestroyCommandPool(vk->device, graph->pool, NULL);
    if (graph->done) vk->DestroySemaphore(vk->device, graph->done, NULL);
    if (graph->descriptors) vk->DestroyDescriptorPool(vk->device, graph->descriptors, NULL);
    for (int i = 0; i < FG_VK_OP_COUNT; ++i) {
        if (graph->pipelines[i]) vk->DestroyPipeline(vk->device, graph->pipelines[i], NULL);
    }
    if (graph->layout) vk->DestroyPipelineLayout(vk->device, graph->layout, NULL);
    if (graph->sets) vk->DestroyDescriptorSetLayout(vk->device, graph->sets, NULL);
    *graph = (fg_vk_graph){0};
}

int fg_vk_graph_open(fg_vk_graph *graph, fg_vk_device *vk, uint32_t capacity) {
    *graph = (fg_vk_graph){.vk = vk, .capacity = capacity};
    if (!capacity || capacity > UINT32_MAX / FG_VK_BINDINGS) return fg_vk_error("invalid dispatch capacity", VK_ERROR_INITIALIZATION_FAILED);
    VkDescriptorSetLayoutBinding bindings[FG_VK_BINDINGS] = {0};
    for (uint32_t i = 0; i < FG_VK_BINDINGS; ++i) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = i, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
    }
    const VkDescriptorSetLayoutCreateInfo sets = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = FG_VK_BINDINGS, .pBindings = bindings,
    };
    const VkPushConstantRange push = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(fg_vk_shape)};
    const VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &graph->sets,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push,
    };
    const VkDescriptorPoolSize slots = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = capacity * FG_VK_BINDINGS};
    const VkDescriptorPoolCreateInfo descriptors = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = capacity,
        .poolSizeCount = 1, .pPoolSizes = &slots,
    };
    const VkCommandPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = vk->family,
    };
    const VkCommandBufferAllocateInfo cmd = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1,
    };
    const VkSemaphoreTypeCreateInfo timeline = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    };
    const VkSemaphoreCreateInfo done = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timeline,
    };
    VkResult result = vk->CreateDescriptorSetLayout(vk->device, &sets, NULL, &graph->sets);
    if (result == VK_SUCCESS) result = vk->CreatePipelineLayout(vk->device, &layout, NULL, &graph->layout);
    if (result == VK_SUCCESS) result = vk->CreateDescriptorPool(vk->device, &descriptors, NULL, &graph->descriptors);
    if (result == VK_SUCCESS) result = vk->CreateCommandPool(vk->device, &pool, NULL, &graph->pool);
    if (result == VK_SUCCESS) {
        VkCommandBufferAllocateInfo alloc = cmd;
        alloc.commandPool = graph->pool;
        result = vk->AllocateCommandBuffers(vk->device, &alloc, &graph->cmd);
    }
    if (result == VK_SUCCESS) result = vk->CreateSemaphore(vk->device, &done, NULL, &graph->done);
    for (fg_vk_op i = 0; i < FG_VK_OP_COUNT && result == VK_SUCCESS; ++i) result = create_pipeline(graph, i);
    if (result == VK_SUCCESS) return 1;
    fg_vk_graph_close(graph);
    return fg_vk_error("initialize compute graph", result);
}

int fg_vk_graph_begin(fg_vk_graph *graph) {
    if (!graph->cmd || graph->recording || graph->epoch == UINT64_MAX) return 0;
    fg_vk_device *vk = graph->vk;
    VkResult result = vk->ResetCommandPool(vk->device, graph->pool, 0);
    if (result == VK_SUCCESS) result = vk->ResetDescriptorPool(vk->device, graph->descriptors, 0);
    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (result == VK_SUCCESS) result = vk->BeginCommandBuffer(graph->cmd, &begin);
    if (result != VK_SUCCESS) return fg_vk_error("begin compute graph", result);
    graph->recording = 1;
    graph->used = 0;
    return 1;
}

static int valid_view(const fg_vk_device *vk, fg_vk_view v, VkDeviceSize required) {
    return v.buffer && v.buffer->buffer && (v.buffer->usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) &&
        v.offset % vk->properties.limits.minStorageBufferOffsetAlignment == 0 &&
        v.size >= required && v.size <= vk->properties.limits.maxStorageBufferRange &&
        v.offset <= v.buffer->size && v.size <= v.buffer->size - v.offset;
}

static int overlap(fg_vk_view a, fg_vk_view b) {
    return a.buffer->buffer == b.buffer->buffer && a.offset < b.offset + b.size && b.offset < a.offset + a.size;
}

extern int fg_vk_dispatch_plan(fg_vk_op op, fg_vk_shape shape, int aliased_state, uint64_t plan[6]);

static void compute_barrier(fg_vk_graph *graph, VkPipelineStageFlags2 stage, VkAccessFlags2 access) {
    const VkMemoryBarrier2 memory = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = stage, .dstAccessMask = access,
    };
    const VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &memory,
    };
    graph->vk->CmdPipelineBarrier2(graph->cmd, &deps);
}

static int record(fg_vk_graph *graph, fg_vk_op op, fg_vk_shape shape, fg_vk_view input,
    fg_vk_view weight, fg_vk_view output, fg_vk_view values) {
    if (!graph->recording || graph->used >= graph->capacity || (unsigned)op >= FG_VK_OP_COUNT) return 0;
    const int aliased_state =
        (op == FG_VK_CAUSAL_CONV && (overlap(input, values) || overlap(weight, values))) ||
        (op == FG_VK_GDN && (overlap(input, weight) || overlap(weight, values)));
    uint64_t plan[6];
    if (!fg_vk_dispatch_plan(op, shape, aliased_state, plan)) return 0;
    const uint64_t input_bytes = plan[0], weight_bytes = plan[1], output_bytes = plan[2], value_bytes = plan[3];
    const uint32_t x = (uint32_t)plan[4], y = (uint32_t)plan[5];
    fg_vk_device *vk = graph->vk;
    if (x > vk->properties.limits.maxComputeWorkGroupCount[0] || y > vk->properties.limits.maxComputeWorkGroupCount[1] ||
        !valid_view(vk, input, input_bytes) || !valid_view(vk, weight, weight_bytes) ||
        !valid_view(vk, output, output_bytes) || !valid_view(vk, values, value_bytes) ||
        overlap(input, output) || overlap(weight, output) || overlap(values, output)) return 0;

    VkDescriptorSet set;
    const VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = graph->descriptors, .descriptorSetCount = 1, .pSetLayouts = &graph->sets,
    };
    VkResult result = vk->AllocateDescriptorSets(vk->device, &alloc, &set);
    if (result != VK_SUCCESS) return fg_vk_error("allocate dispatch descriptors", result);
    const fg_vk_view views[] = {input, weight, output, values};
    VkDescriptorBufferInfo buffers[FG_VK_BINDINGS];
    VkWriteDescriptorSet writes[FG_VK_BINDINGS];
    for (uint32_t i = 0; i < FG_VK_BINDINGS; ++i) {
        buffers[i] = (VkDescriptorBufferInfo){ .buffer = views[i].buffer->buffer, .offset = views[i].offset, .range = views[i].size };
        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = i,
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffers[i],
        };
    }
    vk->UpdateDescriptorSets(vk->device, FG_VK_BINDINGS, writes, 0, NULL);
    if (graph->used || graph->epoch) compute_barrier(graph, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
    vk->CmdBindPipeline(graph->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, graph->pipelines[op]);
    vk->CmdBindDescriptorSets(graph->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, graph->layout, 0, 1, &set, 0, NULL);
    vk->CmdPushConstants(graph->cmd, graph->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(shape), &shape);
    vk->CmdDispatch(graph->cmd, x, y, 1);
    ++graph->used;
    return 1;
}

int fg_vk_graph_record(fg_vk_graph *graph, fg_vk_op op, fg_vk_shape shape,
    fg_vk_view input, fg_vk_view weight, fg_vk_view output) {
    if (op == FG_VK_ATTENTION || op == FG_VK_ATTENTION_Q4 || op == FG_VK_INKLING_ATTENTION ||
        op == FG_VK_CAUSAL_CONV || op == FG_VK_GDN ||
        op == FG_VK_GDN_GATES || op == FG_VK_GDN_APPLY || op == FG_VK_GATED_RMS) return 0;
    return record(graph, op, shape, input, weight, output, input);
}

int fg_vk_graph_conv(fg_vk_graph *graph, uint32_t channels, fg_vk_view input,
    fg_vk_view coefficients, fg_vk_view history, fg_vk_view output) {
    if (!input.buffer || !coefficients.buffer || !history.buffer) return 0;
    return record(graph, FG_VK_CAUSAL_CONV, (fg_vk_shape){.width = channels, .batch = 1},
        input, coefficients, output, history);
}

int fg_vk_graph_gdn(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view qkv,
    fg_vk_view state, fg_vk_view gates, fg_vk_view output) {
    if (!qkv.buffer || !state.buffer || !gates.buffer) return 0;
    return record(graph, FG_VK_GDN, shape, qkv, state, output, gates);
}

int fg_vk_graph_gdn_gates(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view input,
    fg_vk_view weights, fg_vk_view bias_scale, fg_vk_view output) {
    return record(graph, FG_VK_GDN_GATES, shape, input, weights, output, bias_scale);
}

int fg_vk_graph_gdn_apply(fg_vk_graph *graph, uint32_t heads, fg_vk_view alpha,
    fg_vk_view beta, fg_vk_view bias_scale, fg_vk_view output) {
    return record(graph, FG_VK_GDN_APPLY, (fg_vk_shape){.width = heads, .batch = 1},
        alpha, beta, output, bias_scale);
}

int fg_vk_graph_gated_rms(fg_vk_graph *graph, fg_vk_shape shape, fg_vk_view input,
    fg_vk_view weights, fg_vk_view gate, fg_vk_view output) {
    return record(graph, FG_VK_GATED_RMS, shape, input, weights, output, gate);
}

int fg_vk_graph_attention(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output) {
    return record(graph, FG_VK_ATTENTION, shape, query, keys, output, values);
}

int fg_vk_graph_attention_q4(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output) {
    return record(graph, FG_VK_ATTENTION_Q4, shape, query, keys, output, values);
}

int fg_vk_graph_inkling_attention(fg_vk_graph *graph, fg_vk_shape shape,
    fg_vk_view query, fg_vk_view keys, fg_vk_view values, fg_vk_view output) {
    return record(graph, FG_VK_INKLING_ATTENTION, shape, query, keys, output, values);
}
int fg_vk_graph_submit(fg_vk_graph *graph) {
    if (!graph->recording || !graph->used) return 0;
    compute_barrier(graph, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    graph->recording = 0;
    VkResult result = graph->vk->EndCommandBuffer(graph->cmd);
    if (result != VK_SUCCESS) return fg_vk_error("end compute graph", result);
    return fg_vk_submit_wait(graph->vk, graph->cmd, graph->done, ++graph->epoch);
}
