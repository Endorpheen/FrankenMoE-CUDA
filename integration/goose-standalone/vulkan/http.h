#ifndef FG_VULKAN_HTTP_H
#define FG_VULKAN_HTTP_H

#include <stdint.h>
#include "qwen2.h"

_Static_assert(sizeof(fg_vk_qwen2) == 11408 && offsetof(fg_vk_qwen2, position) == 11396 &&
    offsetof(fg_vk_qwen2, graph) + offsetof(fg_vk_graph, recording) == 2768 &&
    offsetof(fg_vk_qwen2, vk) + offsetof(fg_vk_device, properties.deviceName) == 60 &&
    offsetof(fg_vk_qwen2, context) == 11392, "Goose Vulkan HTTP Qwen2 ABI");

int fg_vk_http_run(const char *model_file, uint32_t capacity, uint32_t index, int cpu,
    uint32_t port, uint32_t requests, fg_vk_model_kind kind);

#endif
