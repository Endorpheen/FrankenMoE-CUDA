#include "device.h"

#include <string.h>

static void buffer_barrier(fg_vk_device *vk, VkCommandBuffer cmd, VkBuffer buffer,
    VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    const VkBufferMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT, .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = dst_stage, .dstAccessMask = dst_access,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer, .offset = 0, .size = VK_WHOLE_SIZE,
    };
    const VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(cmd, &deps);
}

int fg_vk_roundtrip(fg_vk_device *vk, const void *input, void *output, size_t size) {
    if (!input || !output || !size || size > SIZE_MAX / 2 || size % 4) {
        return fg_vk_error("roundtrip size must be a nonzero multiple of four", VK_ERROR_INITIALIZATION_FAILED);
    }
    fg_vk_buffer stage = {0}, local = {0};
    VkCommandPool pool = VK_NULL_HANDLE;
    VkSemaphore done = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    void *mapped = NULL;
    const VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    const VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = vk->family,
    };
    const VkSemaphoreTypeCreateInfo timeline = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    };
    const VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timeline,
    };
    const char *action = "allocate staging buffer";
    VkResult result = fg_vk_alloc_buffer(vk, &stage, (VkDeviceSize)size * 2, usage,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (result != VK_SUCCESS) goto end;
    action = "allocate device-local buffer";
    result = fg_vk_alloc_buffer(vk, &local, size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    if (result != VK_SUCCESS) goto end;
    action = "map upload and readback";
    result = vk->MapMemory(vk->device, stage.memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (result != VK_SUCCESS) goto end;
    memcpy(mapped, input, size);
    const VkMappedMemoryRange mapped_range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, .memory = stage.memory, .size = VK_WHOLE_SIZE,
    };
    const int coherent = (stage.properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    if (!coherent) {
        action = "flush upload memory";
        result = vk->FlushMappedMemoryRanges(vk->device, 1, &mapped_range);
        if (result != VK_SUCCESS) goto end;
    }

    action = "create transfer command pool";
    result = vk->CreateCommandPool(vk->device, &pool_info, NULL, &pool);
    if (result != VK_SUCCESS) goto end;
    action = "create transfer timeline";
    result = vk->CreateSemaphore(vk->device, &semaphore_info, NULL, &done);
    if (result != VK_SUCCESS) goto end;
    const VkCommandBufferAllocateInfo cmd_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1,
    };
    action = "allocate transfer command buffer";
    result = vk->AllocateCommandBuffers(vk->device, &cmd_info, &cmd);
    if (result != VK_SUCCESS) goto end;
    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    action = "begin transfer command buffer";
    result = vk->BeginCommandBuffer(cmd, &begin);
    if (result != VK_SUCCESS) goto end;
    const VkBufferCopy upload = { .size = size };
    const VkBufferCopy download = { .dstOffset = size, .size = size };
    vk->CmdCopyBuffer(cmd, stage.buffer, local.buffer, 1, &upload);
    buffer_barrier(vk, cmd, local.buffer, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
    vk->CmdCopyBuffer(cmd, local.buffer, stage.buffer, 1, &download);
    buffer_barrier(vk, cmd, stage.buffer, VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT);
    action = "end transfer command buffer";
    result = vk->EndCommandBuffer(cmd);
    if (result != VK_SUCCESS) goto end;

    const VkCommandBufferSubmitInfo command = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd,
    };
    const uint64_t value = 1;
    const VkSemaphoreSubmitInfo signal = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = done, .value = value,
        .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
    };
    const VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .commandBufferInfoCount = 1, .pCommandBufferInfos = &command,
        .signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos = &signal,
    };
    action = "submit transfer";
    result = vk->QueueSubmit2(vk->queue, 1, &submit, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) goto end;
    const VkSemaphoreWaitInfo wait = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
        .semaphoreCount = 1, .pSemaphores = &done, .pValues = &value,
    };
    action = "wait for transfer";
    result = vk->WaitSemaphores(vk->device, &wait, UINT64_MAX);
    if (result != VK_SUCCESS) {
        (void)vk->DeviceWaitIdle(vk->device);
        goto end;
    }
    if (!coherent) {
        action = "invalidate readback memory";
        result = vk->InvalidateMappedMemoryRanges(vk->device, 1, &mapped_range);
        if (result != VK_SUCCESS) goto end;
    }
    memcpy(output, (const char *)mapped + size, size);
end:
    if (done) vk->DestroySemaphore(vk->device, done, NULL);
    if (pool) vk->DestroyCommandPool(vk->device, pool, NULL);
    if (mapped) vk->UnmapMemory(vk->device, stage.memory);
    fg_vk_free_buffer(vk, &local);
    fg_vk_free_buffer(vk, &stage);
    return result == VK_SUCCESS || fg_vk_error(action, result);
}
