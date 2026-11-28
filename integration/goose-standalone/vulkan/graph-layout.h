#include "graph.h"

_Static_assert(sizeof(fg_vk_graph) == 384, "Goose graph ABI: fg_vk_graph");
_Static_assert(offsetof(fg_vk_graph, vk) == 0, "Goose graph ABI: fg_vk_graph.vk");
_Static_assert(offsetof(fg_vk_graph, sets) == 8, "Goose graph ABI: fg_vk_graph.sets");
_Static_assert(offsetof(fg_vk_graph, layout) == 16, "Goose graph ABI: fg_vk_graph.layout");
_Static_assert(offsetof(fg_vk_graph, pipelines) == 24, "Goose graph ABI: fg_vk_graph.pipelines");
_Static_assert(offsetof(fg_vk_graph, descriptors) == 328, "Goose graph ABI: fg_vk_graph.descriptors");
_Static_assert(offsetof(fg_vk_graph, pool) == 336, "Goose graph ABI: fg_vk_graph.pool");
_Static_assert(offsetof(fg_vk_graph, cmd) == 344, "Goose graph ABI: fg_vk_graph.cmd");
_Static_assert(offsetof(fg_vk_graph, done) == 352, "Goose graph ABI: fg_vk_graph.done");
_Static_assert(offsetof(fg_vk_graph, epoch) == 360, "Goose graph ABI: fg_vk_graph.epoch");
_Static_assert(offsetof(fg_vk_graph, capacity) == 368, "Goose graph ABI: fg_vk_graph.capacity");
_Static_assert(offsetof(fg_vk_graph, used) == 372, "Goose graph ABI: fg_vk_graph.used");
_Static_assert(offsetof(fg_vk_graph, recording) == 376, "Goose graph ABI: fg_vk_graph.recording");
_Static_assert(offsetof(fg_vk_device, properties.limits.maxComputeWorkGroupCount) == 556, "Goose graph ABI: fg_vk_device.properties.limits.maxComputeWorkGroupCount");
_Static_assert(offsetof(fg_vk_device, CreateDescriptorSetLayout) == 2256, "Goose graph ABI: fg_vk_device.CreateDescriptorSetLayout");
_Static_assert(offsetof(fg_vk_device, DestroyDescriptorSetLayout) == 2264, "Goose graph ABI: fg_vk_device.DestroyDescriptorSetLayout");
_Static_assert(offsetof(fg_vk_device, CreatePipelineLayout) == 2272, "Goose graph ABI: fg_vk_device.CreatePipelineLayout");
_Static_assert(offsetof(fg_vk_device, DestroyPipelineLayout) == 2280, "Goose graph ABI: fg_vk_device.DestroyPipelineLayout");
_Static_assert(offsetof(fg_vk_device, CreateShaderModule) == 2288, "Goose graph ABI: fg_vk_device.CreateShaderModule");
_Static_assert(offsetof(fg_vk_device, DestroyShaderModule) == 2296, "Goose graph ABI: fg_vk_device.DestroyShaderModule");
_Static_assert(offsetof(fg_vk_device, CreateComputePipelines) == 2304, "Goose graph ABI: fg_vk_device.CreateComputePipelines");
_Static_assert(offsetof(fg_vk_device, DestroyPipeline) == 2312, "Goose graph ABI: fg_vk_device.DestroyPipeline");
_Static_assert(offsetof(fg_vk_device, CreateDescriptorPool) == 2320, "Goose graph ABI: fg_vk_device.CreateDescriptorPool");
_Static_assert(offsetof(fg_vk_device, DestroyDescriptorPool) == 2328, "Goose graph ABI: fg_vk_device.DestroyDescriptorPool");
_Static_assert(offsetof(fg_vk_device, ResetDescriptorPool) == 2336, "Goose graph ABI: fg_vk_device.ResetDescriptorPool");
_Static_assert(offsetof(fg_vk_device, AllocateDescriptorSets) == 2344, "Goose graph ABI: fg_vk_device.AllocateDescriptorSets");
_Static_assert(offsetof(fg_vk_device, UpdateDescriptorSets) == 2352, "Goose graph ABI: fg_vk_device.UpdateDescriptorSets");
_Static_assert(offsetof(fg_vk_device, CmdBindPipeline) == 2360, "Goose graph ABI: fg_vk_device.CmdBindPipeline");
_Static_assert(offsetof(fg_vk_device, CmdBindDescriptorSets) == 2368, "Goose graph ABI: fg_vk_device.CmdBindDescriptorSets");
_Static_assert(offsetof(fg_vk_device, CmdPushConstants) == 2376, "Goose graph ABI: fg_vk_device.CmdPushConstants");
_Static_assert(offsetof(fg_vk_device, CmdDispatch) == 2384, "Goose graph ABI: fg_vk_device.CmdDispatch");
_Static_assert(sizeof(VkShaderModuleCreateInfo) == 40, "Goose graph ABI: VkShaderModuleCreateInfo");
_Static_assert(offsetof(VkShaderModuleCreateInfo, codeSize) == 24, "Goose graph ABI: VkShaderModuleCreateInfo.codeSize");
_Static_assert(offsetof(VkShaderModuleCreateInfo, pCode) == 32, "Goose graph ABI: VkShaderModuleCreateInfo.pCode");
_Static_assert(sizeof(VkComputePipelineCreateInfo) == 96, "Goose graph ABI: VkComputePipelineCreateInfo");
_Static_assert(offsetof(VkComputePipelineCreateInfo, stage) == 24, "Goose graph ABI: VkComputePipelineCreateInfo.stage");
_Static_assert(offsetof(VkComputePipelineCreateInfo, layout) == 72, "Goose graph ABI: VkComputePipelineCreateInfo.layout");
_Static_assert(sizeof(VkPipelineShaderStageCreateInfo) == 48, "Goose graph ABI: VkPipelineShaderStageCreateInfo");
_Static_assert(offsetof(VkPipelineShaderStageCreateInfo, stage) == 20, "Goose graph ABI: VkPipelineShaderStageCreateInfo.stage");
_Static_assert(offsetof(VkPipelineShaderStageCreateInfo, module) == 24, "Goose graph ABI: VkPipelineShaderStageCreateInfo.module");
_Static_assert(offsetof(VkPipelineShaderStageCreateInfo, pName) == 32, "Goose graph ABI: VkPipelineShaderStageCreateInfo.pName");
_Static_assert(sizeof(VkDescriptorSetLayoutBinding) == 24, "Goose graph ABI: VkDescriptorSetLayoutBinding");
_Static_assert(offsetof(VkDescriptorSetLayoutBinding, descriptorCount) == 8, "Goose graph ABI: VkDescriptorSetLayoutBinding.descriptorCount");
_Static_assert(sizeof(VkDescriptorSetLayoutCreateInfo) == 32, "Goose graph ABI: VkDescriptorSetLayoutCreateInfo");
_Static_assert(offsetof(VkDescriptorSetLayoutCreateInfo, bindingCount) == 20, "Goose graph ABI: VkDescriptorSetLayoutCreateInfo.bindingCount");
_Static_assert(offsetof(VkDescriptorSetLayoutCreateInfo, pBindings) == 24, "Goose graph ABI: VkDescriptorSetLayoutCreateInfo.pBindings");
_Static_assert(sizeof(VkPushConstantRange) == 12, "Goose graph ABI: VkPushConstantRange");
_Static_assert(offsetof(VkPushConstantRange, size) == 8, "Goose graph ABI: VkPushConstantRange.size");
_Static_assert(sizeof(VkPipelineLayoutCreateInfo) == 48, "Goose graph ABI: VkPipelineLayoutCreateInfo");
_Static_assert(offsetof(VkPipelineLayoutCreateInfo, setLayoutCount) == 20, "Goose graph ABI: VkPipelineLayoutCreateInfo.setLayoutCount");
_Static_assert(offsetof(VkPipelineLayoutCreateInfo, pSetLayouts) == 24, "Goose graph ABI: VkPipelineLayoutCreateInfo.pSetLayouts");
_Static_assert(offsetof(VkPipelineLayoutCreateInfo, pPushConstantRanges) == 40, "Goose graph ABI: VkPipelineLayoutCreateInfo.pPushConstantRanges");
_Static_assert(sizeof(VkDescriptorPoolCreateInfo) == 40, "Goose graph ABI: VkDescriptorPoolCreateInfo");
_Static_assert(offsetof(VkDescriptorPoolCreateInfo, maxSets) == 20, "Goose graph ABI: VkDescriptorPoolCreateInfo.maxSets");
_Static_assert(offsetof(VkDescriptorPoolCreateInfo, pPoolSizes) == 32, "Goose graph ABI: VkDescriptorPoolCreateInfo.pPoolSizes");
_Static_assert(sizeof(VkDescriptorSetAllocateInfo) == 40, "Goose graph ABI: VkDescriptorSetAllocateInfo");
_Static_assert(offsetof(VkDescriptorSetAllocateInfo, descriptorSetCount) == 24, "Goose graph ABI: VkDescriptorSetAllocateInfo.descriptorSetCount");
_Static_assert(offsetof(VkDescriptorSetAllocateInfo, pSetLayouts) == 32, "Goose graph ABI: VkDescriptorSetAllocateInfo.pSetLayouts");
_Static_assert(sizeof(VkWriteDescriptorSet) == 64, "Goose graph ABI: VkWriteDescriptorSet");
_Static_assert(offsetof(VkWriteDescriptorSet, dstBinding) == 24, "Goose graph ABI: VkWriteDescriptorSet.dstBinding");
_Static_assert(offsetof(VkWriteDescriptorSet, descriptorCount) == 32, "Goose graph ABI: VkWriteDescriptorSet.descriptorCount");
_Static_assert(offsetof(VkWriteDescriptorSet, pBufferInfo) == 48, "Goose graph ABI: VkWriteDescriptorSet.pBufferInfo");
_Static_assert(sizeof(VkDescriptorBufferInfo) == 24, "Goose graph ABI: VkDescriptorBufferInfo");
_Static_assert(sizeof(VkMemoryBarrier2) == 48, "Goose graph ABI: VkMemoryBarrier2");
_Static_assert(offsetof(VkDependencyInfo, memoryBarrierCount) == 20, "Goose graph ABI: VkDependencyInfo.memoryBarrierCount");
_Static_assert(offsetof(VkDependencyInfo, pMemoryBarriers) == 24, "Goose graph ABI: VkDependencyInfo.pMemoryBarriers");
_Static_assert(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO == 16 &&
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO == 18 &&
    VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO == 29 &&
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO == 30 &&
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO == 32 &&
    VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO == 33 &&
    VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO == 34 &&
    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET == 35 &&
    VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 == 1000314000 &&
    VK_SHADER_STAGE_COMPUTE_BIT == 32 && VK_DESCRIPTOR_TYPE_STORAGE_BUFFER == 7 &&
    VK_PIPELINE_BIND_POINT_COMPUTE == 1 && FG_VK_OP_COUNT == 38, "Goose graph constants");
_Static_assert(sizeof(fg_vk_view) == 24 && offsetof(fg_vk_view, buffer) == 0 &&
    offsetof(fg_vk_view, offset) == 8 && offsetof(fg_vk_view, size) == 16 &&
    sizeof(fg_vk_shape) == 36 && offsetof(fg_vk_shape, epsilon) == 12 &&
    offsetof(fg_vk_shape, theta) == 28 && offsetof(fg_vk_shape, rotary) == 32, "Goose graph view and push constants ABI");
