#ifndef FG_VULKAN_INKLING_MODEL_H
#define FG_VULKAN_INKLING_MODEL_H

#include <stdint.h>

enum { FG_INKLING_VOCAB = 200058, FG_INKLING_ROWS = 201024 };
typedef struct fg_vk_inkling fg_vk_inkling;

fg_vk_inkling *fg_vk_inkling_open(const char *path, uint32_t capacity, uint32_t device, int allow_cpu);
int fg_vk_inkling_step(fg_vk_inkling *model, uint32_t token, float *logits);
int fg_vk_inkling_reset(fg_vk_inkling *model);
void fg_vk_inkling_close(fg_vk_inkling *model);

#endif
