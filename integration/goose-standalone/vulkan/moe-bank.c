#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "moe-bank.h"

#include <stdlib.h>
#include <string.h>

void fg_vk_expert_bank_close(fg_vk_expert_bank *bank) {
    if (bank->upload.vk) fg_vk_free_buffer(bank->upload.vk, &bank->device);
    fg_vk_upload_close(&bank->upload);
    free(bank->host);
    *bank = (fg_vk_expert_bank){0};
}

int fg_vk_expert_bank_open(fg_vk_expert_bank *bank, fg_vk_device *vk, uint32_t slots,
    const uint64_t max_bytes[FG_VK_EXPERT_PLANES]) {
    *bank = (fg_vk_expert_bank){0};
    if (!slots || slots > FG_VK_EXPERT_SLOTS) return 0;
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize end = 0;
    for (uint32_t slot = 0; slot < slots; ++slot) {
        for (unsigned p = 0; p < FG_VK_EXPERT_PLANES; ++p) {
            if (!max_bytes[p]) continue;
            if (max_bytes[p] > vk->properties.limits.maxStorageBufferRange) return 0;
            const VkDeviceSize size = (max_bytes[p] + 3u) & ~(VkDeviceSize)3;
            if (size > vk->properties.limits.maxStorageBufferRange) return 0;
            const VkDeviceSize at = (end + alignment - 1) & ~(alignment - 1);
            bank->plane[slot][p] = (fg_vk_view){&bank->device, at, size};
            end = at + size;
        }
    }
    if (!end || end > SIZE_MAX) return 0;
    if (!fg_vk_upload_open(&bank->upload, vk, end)) return 0;
    bank->host = calloc(1, (size_t)end);
    VkResult result = fg_vk_alloc_buffer(vk, &bank->device, end,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    if (result != VK_SUCCESS || !bank->host) {
        fg_vk_expert_bank_close(bank);
        return result == VK_SUCCESS ? 0 : fg_vk_error("allocate Vulkan expert bank", result);
    }
    bank->slots = slots;
    return 1;
}

int fg_vk_expert_bank_stage(fg_vk_expert_bank *bank, FILE *file, uint64_t file_bytes,
    uint32_t experts, const fg_vk_expert_plane planes[FG_VK_EXPERT_PLANES], const uint32_t *ids, uint32_t count) {
    bank->loaded = 0;
    if (!experts || !count || count > bank->slots || file_bytes > INT64_MAX) return 0;
    for (unsigned p = 0; p < FG_VK_EXPERT_PLANES; ++p) {
        const fg_vk_expert_plane plane = planes[p];
        if (plane.file_offset > file_bytes || plane.expert_bytes > bank->plane[0][p].size ||
            plane.expert_bytes > (file_bytes - plane.file_offset) / experts) return 0;
    }
    VkDeviceSize end = 0;
    for (uint32_t slot = 0; slot < count; ++slot) {
        if (ids[slot] >= experts) return 0;
        for (unsigned p = 0; p < FG_VK_EXPERT_PLANES; ++p) {
            const fg_vk_expert_plane plane = planes[p];
            if (!plane.expert_bytes) continue;
            const fg_vk_view view = bank->plane[slot][p];
            const size_t bytes = (size_t)plane.expert_bytes;
            const uint64_t start = plane.file_offset + plane.expert_bytes * ids[slot];
            unsigned char *dest = bank->host + view.offset;
            if (fseeko(file, (int64_t)start, SEEK_SET) || fread(dest, 1, bytes, file) != bytes) return 0;
            const size_t padded = (bytes + 3u) & ~(size_t)3;
            memset(dest + bytes, 0, padded - bytes);
            end = view.offset + padded;
        }
    }
    if (!end || !fg_vk_upload_write(&bank->upload, &bank->device, 0, bank->host, (size_t)end)) return 0;
    bank->loaded = count;
    return 1;
}

int fg_vk_expert_bank_record_split(fg_vk_graph *graph, const fg_vk_expert_bank *bank,
    uint32_t hidden, uint32_t ff, const fg_vk_op ops[FG_VK_EXPERT_PLANES],
    fg_vk_view input, const fg_vk_view scratch[3], fg_vk_view branches) {
    const VkDeviceSize row_size = (VkDeviceSize)hidden * sizeof(float);
    if (!bank->loaded || bank->upload.vk != graph->vk || !row_size ||
        branches.size / row_size < bank->loaded ||
        branches.offset > UINT64_MAX - row_size * bank->loaded ||
        (bank->loaded > 1 && row_size % graph->vk->properties.limits.minStorageBufferOffsetAlignment)) return 0;
    const fg_vk_shape up = {.width = hidden, .rows = ff, .batch = 1};
    const fg_vk_shape down = {.width = ff, .rows = hidden, .batch = 1};
    const fg_vk_shape activate = {.width = ff, .batch = 1};
    for (uint32_t slot = 0; slot < bank->loaded; ++slot) {
        const fg_vk_view *w = bank->plane[slot];
        const fg_vk_view output = {branches.buffer, branches.offset + slot * row_size, row_size};
        if (!fg_vk_graph_record(graph, ops[0], up, input, w[0], scratch[0]) ||
            !fg_vk_graph_record(graph, ops[1], up, input, w[1], scratch[1]) ||
            !fg_vk_graph_record(graph, FG_VK_SWIGLU, activate, scratch[0], scratch[1], scratch[2]) ||
            !fg_vk_graph_record(graph, ops[2], down, scratch[2], w[2], output)) return 0;
    }
    return 1;
}
