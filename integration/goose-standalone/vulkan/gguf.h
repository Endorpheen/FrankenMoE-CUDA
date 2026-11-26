#ifndef FG_VULKAN_GGUF_H
#define FG_VULKAN_GGUF_H

#include "qwen2_unit.h"
#include "upload.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    sl_u8 data;
    int64_t state[8], metadata[8 * 256], tensors[12 * 1024], meta_slots[512], tensor_slots[2048];
} fg_gguf;

static inline sl_u8 fg_gguf_key(const char *text) {
    return (sl_u8){(uint8_t *)text, (int64_t)strlen(text)};
}

static inline sl_i64 fg_gguf_words(int64_t *data, size_t count) {
    return (sl_i64){data, (int64_t)count};
}

#define FG_GGUF_WORDS(array) fg_gguf_words((array), sizeof(array) / sizeof(*(array)))

int64_t fg_gguf_metadata(fg_gguf *file, const char *name);
int fg_gguf_metadata_is(fg_gguf *file, const char *name, const char *value);
int64_t fg_gguf_read_header(fg_gguf *file, FILE *model);
int fg_gguf_upload(FILE *file, fg_vk_upload *upload, const fg_vk_buffer *dest, VkDeviceSize offset,
    uint64_t file_offset, uint64_t bytes, unsigned char *chunk);

#endif
