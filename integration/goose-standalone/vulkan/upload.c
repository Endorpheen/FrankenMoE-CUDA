#include "upload.h"

void fg_vk_upload_close(fg_vk_upload *upload) {
    fg_vk_device *vk = upload->vk;
    if (!vk) return;
    if (upload->pool) vk->DestroyCommandPool(vk->device, upload->pool, NULL);
    if (upload->done) vk->DestroySemaphore(vk->device, upload->done, NULL);
    fg_vk_free_buffer(vk, &upload->staging);
    *upload = (fg_vk_upload){0};
}

int fg_vk_upload_open(fg_vk_upload *upload, fg_vk_device *vk, VkDeviceSize capacity) {
    *upload = (fg_vk_upload){.vk = vk};
    const VkCommandPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = vk->family,
    };
    const VkSemaphoreTypeCreateInfo timeline = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
    };
    const VkSemaphoreCreateInfo done = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &timeline,
    };
    VkResult result = fg_vk_alloc_buffer(vk, &upload->staging, capacity, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (result == VK_SUCCESS) result = vk->CreateCommandPool(vk->device, &pool, NULL, &upload->pool);
    if (result == VK_SUCCESS) {
        const VkCommandBufferAllocateInfo cmd = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = upload->pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1,
        };
        result = vk->AllocateCommandBuffers(vk->device, &cmd, &upload->cmd);
    }
    if (result == VK_SUCCESS) result = vk->CreateSemaphore(vk->device, &done, NULL, &upload->done);
    if (result == VK_SUCCESS) return 1;
    fg_vk_upload_close(upload);
    return fg_vk_error("create weight upload queue", result);
}

int fg_vk_upload_write(fg_vk_upload *upload, const fg_vk_buffer *dest,
    VkDeviceSize offset, void *data, size_t size) {
    if (!size || (size | offset) % 4 || !(dest->usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) ||
        offset > dest->size || size > dest->size - offset || size > upload->staging.size ||
        upload->epoch == UINT64_MAX) return 0;
    fg_vk_device *vk = upload->vk;
    VkResult result = fg_vk_access_buffer(vk, &upload->staging, 0, data, size, 1);
    if (result == VK_SUCCESS) result = vk->ResetCommandPool(vk->device, upload->pool, 0);
    const VkCommandBufferBeginInfo begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (result == VK_SUCCESS) result = vk->BeginCommandBuffer(upload->cmd, &begin);
    if (result != VK_SUCCESS) return fg_vk_error("begin weight upload", result);
    const VkBufferCopy copy = {.dstOffset = offset, .size = size};
    vk->CmdCopyBuffer(upload->cmd, upload->staging.buffer, dest->buffer, 1, &copy);
    const VkBufferMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT, .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = dest->buffer, .offset = offset, .size = size,
    };
    const VkDependencyInfo deps = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .bufferMemoryBarrierCount = 1, .pBufferMemoryBarriers = &barrier,
    };
    vk->CmdPipelineBarrier2(upload->cmd, &deps);
    result = vk->EndCommandBuffer(upload->cmd);
    if (result != VK_SUCCESS) return fg_vk_error("end weight upload", result);
    return fg_vk_submit_wait(vk, upload->cmd, upload->done, ++upload->epoch);
}
