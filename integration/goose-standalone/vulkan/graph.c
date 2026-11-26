#include "graph.h"
#include "shader_words.h"

#include <math.h>

_Static_assert(sizeof(fg_vk_shape) == 16, "Vulkan push constant layout");

typedef struct {
    const uint32_t *words;
    size_t bytes;
} fg_vk_shader;

static const fg_vk_shader shaders[FG_VK_OP_COUNT] = {
    [FG_VK_RMSNORM] = { fg_rmsnorm, sizeof(fg_rmsnorm) },
    [FG_VK_Q4_0] = { fg_q4_0, sizeof(fg_q4_0) },
    [FG_VK_SWIGLU] = { fg_swiglu, sizeof(fg_swiglu) },
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
    if (!capacity || capacity > UINT32_MAX / 3) return fg_vk_error("invalid dispatch capacity", VK_ERROR_INITIALIZATION_FAILED);
    VkDescriptorSetLayoutBinding bindings[3] = {0};
    for (uint32_t i = 0; i < 3; ++i) {
        bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding = i, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        };
    }
    const VkDescriptorSetLayoutCreateInfo sets = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3, .pBindings = bindings,
    };
    const VkPushConstantRange push = {.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .size = sizeof(fg_vk_shape)};
    const VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &graph->sets,
        .pushConstantRangeCount = 1, .pPushConstantRanges = &push,
    };
    const VkDescriptorPoolSize slots = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = capacity * 3};
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

int fg_vk_graph_record(fg_vk_graph *graph, fg_vk_op op, fg_vk_shape shape,
    fg_vk_view input, fg_vk_view weight, fg_vk_view output) {
    if (!graph->recording || graph->used >= graph->capacity || (unsigned)op >= FG_VK_OP_COUNT) return 0;
    const uint64_t elements = (uint64_t)shape.width * shape.batch;
    if (!elements || elements > UINT32_MAX) return 0;
    VkDeviceSize input_bytes = elements * 4, weight_bytes, output_bytes = input_bytes;
    uint32_t x, y = 1;
    switch (op) {
    case FG_VK_RMSNORM:
        if (!isfinite(shape.epsilon) || shape.epsilon <= 0.0f) return 0;
        x = shape.batch;
        weight_bytes = (VkDeviceSize)shape.width * 4;
        break;
    case FG_VK_Q4_0:
        if (shape.width % 32 || !shape.rows || (uint64_t)shape.rows * shape.batch > UINT32_MAX) return 0;
        x = shape.rows;
        y = shape.batch;
        weight_bytes = (uint64_t)(shape.width / 32) * shape.rows * 18;
        if (weight_bytes > UINT32_MAX - 3) return 0;
        weight_bytes = (weight_bytes + 3) & ~(VkDeviceSize)3;
        output_bytes = (uint64_t)shape.rows * shape.batch * 4;
        break;
    case FG_VK_SWIGLU:
        x = (uint32_t)((elements + 63) / 64);
        weight_bytes = input_bytes;
        break;
    default:
        return 0;
    }
    fg_vk_device *vk = graph->vk;
    if (x > vk->properties.limits.maxComputeWorkGroupCount[0] || y > vk->properties.limits.maxComputeWorkGroupCount[1] ||
        !valid_view(vk, input, input_bytes) || !valid_view(vk, weight, weight_bytes) ||
        !valid_view(vk, output, output_bytes) || overlap(input, output) || overlap(weight, output)) return 0;

    VkDescriptorSet set;
    const VkDescriptorSetAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = graph->descriptors, .descriptorSetCount = 1, .pSetLayouts = &graph->sets,
    };
    VkResult result = vk->AllocateDescriptorSets(vk->device, &alloc, &set);
    if (result != VK_SUCCESS) return fg_vk_error("allocate dispatch descriptors", result);
    const fg_vk_view views[] = {input, weight, output};
    VkDescriptorBufferInfo buffers[3];
    VkWriteDescriptorSet writes[3];
    for (uint32_t i = 0; i < 3; ++i) {
        buffers[i] = (VkDescriptorBufferInfo){ .buffer = views[i].buffer->buffer, .offset = views[i].offset, .range = views[i].size };
        writes[i] = (VkWriteDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = i,
            .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffers[i],
        };
    }
    vk->UpdateDescriptorSets(vk->device, 3, writes, 0, NULL);
    if (graph->used || graph->epoch) compute_barrier(graph, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                    VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);
    vk->CmdBindPipeline(graph->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, graph->pipelines[op]);
    vk->CmdBindDescriptorSets(graph->cmd, VK_PIPELINE_BIND_POINT_COMPUTE, graph->layout, 0, 1, &set, 0, NULL);
    vk->CmdPushConstants(graph->cmd, graph->layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(shape), &shape);
    vk->CmdDispatch(graph->cmd, x, y, 1);
    ++graph->used;
    return 1;
}

int fg_vk_graph_submit(fg_vk_graph *graph) {
    if (!graph->recording || !graph->used) return 0;
    compute_barrier(graph, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    graph->recording = 0;
    VkResult result = graph->vk->EndCommandBuffer(graph->cmd);
    if (result != VK_SUCCESS) return fg_vk_error("end compute graph", result);
    return fg_vk_submit_wait(graph->vk, graph->cmd, graph->done, ++graph->epoch);
}
