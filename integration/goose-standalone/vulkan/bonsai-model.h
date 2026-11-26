#ifndef FG_VULKAN_BONSAI_MODEL_H
#define FG_VULKAN_BONSAI_MODEL_H

#include <stdint.h>

enum { FG_BONSAI_VOCAB = 248320 };
typedef struct fg_vk_bonsai fg_vk_bonsai;

fg_vk_bonsai *fg_vk_bonsai_open(const char *path, uint32_t capacity, uint32_t device, int allow_cpu);
int fg_vk_bonsai_step(fg_vk_bonsai *model, uint32_t token, float *logits);
int fg_vk_bonsai_is_eog(const fg_vk_bonsai *model, uint32_t token);
void fg_vk_bonsai_close(fg_vk_bonsai *model);

#endif
