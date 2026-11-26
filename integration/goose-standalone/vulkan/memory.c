#include "device.h"

void fg_vk_free_buffer(fg_vk_device *vk, fg_vk_buffer *buffer) {
    if (buffer->buffer) vk->DestroyBuffer(vk->device, buffer->buffer, NULL);
    if (buffer->memory) vk->FreeMemory(vk->device, buffer->memory, NULL);
    *buffer = (fg_vk_buffer){0};
}

VkResult fg_vk_alloc_buffer(fg_vk_device *vk, fg_vk_buffer *buffer, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred) {
    *buffer = (fg_vk_buffer){0};
    if (!size) return VK_ERROR_INITIALIZATION_FAILED;
    const VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkResult result = vk->CreateBuffer(vk->device, &info, NULL, &buffer->buffer);
    if (result != VK_SUCCESS) return result;
    VkMemoryRequirements req;
    vk->GetBufferMemoryRequirements(vk->device, buffer->buffer, &req);

    uint32_t type = UINT32_MAX;
    for (uint32_t i = 0; i < vk->memory.memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = vk->memory.memoryTypes[i].propertyFlags;
        if (!(req.memoryTypeBits & (1u << i)) || (flags & required) != required) continue;
        if (type == UINT32_MAX) type = i;
        if ((flags & preferred) == preferred) {
            type = i;
            break;
        }
    }
    if (type == UINT32_MAX) {
        result = VK_ERROR_FEATURE_NOT_PRESENT;
        goto fail;
    }
    const VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type,
    };
    result = vk->AllocateMemory(vk->device, &alloc, NULL, &buffer->memory);
    if (result == VK_SUCCESS) result = vk->BindBufferMemory(vk->device, buffer->buffer, buffer->memory, 0);
    if (result == VK_SUCCESS) {
        buffer->size = size;
        buffer->properties = vk->memory.memoryTypes[type].propertyFlags;
        return result;
    }
fail:
    fg_vk_free_buffer(vk, buffer);
    return result;
}
