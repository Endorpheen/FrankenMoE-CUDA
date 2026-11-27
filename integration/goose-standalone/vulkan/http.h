#ifndef FG_VULKAN_HTTP_H
#define FG_VULKAN_HTTP_H

#include <stdint.h>
#include "tokenizer.h"

int fg_vk_http_run(const char *model_file, uint32_t capacity, uint32_t index, int cpu,
    uint32_t port, uint32_t requests, fg_vk_model_kind kind);

#endif
