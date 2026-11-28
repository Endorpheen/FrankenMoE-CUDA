#include "qwen2.h"

_Static_assert(sizeof(fg_vk_qwen2) == 11408 && offsetof(fg_vk_qwen2, position) == 11396 &&
    offsetof(fg_vk_qwen2, graph) + offsetof(fg_vk_graph, recording) == 2768 &&
    offsetof(fg_vk_qwen2, vk) + offsetof(fg_vk_device, properties.deviceName) == 60 &&
    offsetof(fg_vk_qwen2, context) == 11392, "Goose Vulkan Qwen2 ABI");
