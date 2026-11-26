#ifndef FG_VULKAN_MOE_BANK_H
#define FG_VULKAN_MOE_BANK_H

#include "graph.h"
#include "upload.h"

#include <stdio.h>

enum { FG_VK_EXPERT_PLANES = 3, FG_VK_EXPERT_SLOTS = 64 };

typedef struct {
    uint64_t file_offset, expert_bytes;
} fg_vk_expert_plane;

typedef struct {
    fg_vk_upload upload;
    fg_vk_buffer device;
    unsigned char *host;
    fg_vk_view plane[FG_VK_EXPERT_SLOTS][FG_VK_EXPERT_PLANES];
    uint32_t slots, loaded;
} fg_vk_expert_bank;

int fg_vk_expert_bank_open(fg_vk_expert_bank *bank, fg_vk_device *vk, uint32_t slots,
    const uint64_t max_bytes[FG_VK_EXPERT_PLANES]);
void fg_vk_expert_bank_close(fg_vk_expert_bank *bank);
int fg_vk_expert_bank_stage(fg_vk_expert_bank *bank, FILE *file, uint64_t file_bytes,
    uint32_t experts, const fg_vk_expert_plane planes[FG_VK_EXPERT_PLANES], const uint32_t *ids, uint32_t count);
int fg_vk_expert_bank_record_split(fg_vk_graph *graph, const fg_vk_expert_bank *bank,
    uint32_t hidden, uint32_t ff, const fg_vk_op ops[FG_VK_EXPERT_PLANES],
    fg_vk_view input, const fg_vk_view scratch[3], fg_vk_view branches);

#endif
