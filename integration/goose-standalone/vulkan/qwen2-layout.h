#include "qwen2.h"

_Static_assert(sizeof(fg_vk_qwen2) == 11408 && offsetof(fg_vk_qwen2, position) == 11396 &&
    offsetof(fg_vk_qwen2, graph) + offsetof(fg_vk_graph, recording) == 2768 &&
    offsetof(fg_vk_qwen2, vk) + offsetof(fg_vk_device, properties.deviceName) == 60 &&
    offsetof(fg_vk_qwen2, context) == 11392, "Goose Vulkan Qwen2 ABI");

_Static_assert(offsetof(fg_vk_qwen2, graph) == 2392 && offsetof(fg_vk_qwen2, weights) == 2776 &&
    offsetof(fg_vk_qwen2, work) == 2808 && offsetof(fg_vk_qwen2, host) == 2840 &&
    offsetof(fg_vk_qwen2, layers) == 2872 && offsetof(fg_vk_qwen2, globals) == 9784 &&
    offsetof(fg_vk_qwen2, scratch) == 9856 && offsetof(fg_vk_qwen2, keys) == 10240 &&
    offsetof(fg_vk_qwen2, values) == 10816 && offsetof(fg_vk_qwen2, epsilon) == 11400 &&
    offsetof(fg_vk_qwen2, theta) == 11404, "Goose Qwen2 model views ABI");
