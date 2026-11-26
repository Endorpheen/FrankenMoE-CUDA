#ifndef FG_VULKAN_UPLOAD_H
#define FG_VULKAN_UPLOAD_H

#include "device.h"

typedef struct {
    fg_vk_device *vk;
    fg_vk_buffer staging;
    VkCommandPool pool;
    VkCommandBuffer cmd;
    VkSemaphore done;
    uint64_t epoch;
} fg_vk_upload;

int fg_vk_upload_open(fg_vk_upload *upload, fg_vk_device *vk, VkDeviceSize capacity);
void fg_vk_upload_close(fg_vk_upload *upload);
int fg_vk_upload_write(fg_vk_upload *upload, const fg_vk_buffer *dest,
    VkDeviceSize offset, void *data, size_t size);

#endif
