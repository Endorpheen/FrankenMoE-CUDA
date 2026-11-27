#ifndef FG_VULKAN_APEX_MODEL_H
#define FG_VULKAN_APEX_MODEL_H

#include <stdint.h>

enum { FG_APEX_VOCAB = 248320 };
typedef struct fg_vk_apex fg_vk_apex;

fg_vk_apex *fg_vk_apex_open(const char *path, uint32_t capacity, uint32_t device, int allow_cpu);
int fg_vk_apex_step(fg_vk_apex *model, uint32_t token, float *logits);
int fg_vk_apex_reset(fg_vk_apex *model);
void fg_vk_apex_close(fg_vk_apex *model);

#endif
