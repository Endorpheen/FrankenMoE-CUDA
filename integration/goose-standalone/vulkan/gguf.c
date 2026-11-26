#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "gguf.h"

#include <stdlib.h>

int64_t fg_gguf_metadata(fg_gguf *file, const char *name) {
    return gr_find_kv(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata), FG_GGUF_WORDS(file->meta_slots), fg_gguf_key(name));
}

int fg_gguf_metadata_is(fg_gguf *file, const char *name, const char *value) {
    int64_t span[4];
    return !gr_val_span(FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata), fg_gguf_metadata(file, name), FG_GGUF_WORDS(span)) &&
        span[0] == 8 && span[2] == (int64_t)strlen(value) &&
        !memcmp(file->data.data + span[1], value, (size_t)span[2]);
}

int64_t fg_gguf_read_header(fg_gguf *file, FILE *model) {
    if (fseeko(model, 0, SEEK_END)) return -1;
    const int64_t size = ftello(model);
    if (size < 24) return -1;
    const size_t prefix = size > (64 << 20) ? 64 << 20 : (size_t)size;
    file->data = (sl_u8){malloc(prefix), (int64_t)prefix};
    if (!file->data.data || fseeko(model, 0, SEEK_SET) || fread(file->data.data, 1, prefix, model) != prefix ||
        gr_parse(file->data, size, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata), FG_GGUF_WORDS(file->tensors),
                 FG_GGUF_WORDS(file->meta_slots), FG_GGUF_WORDS(file->tensor_slots))) return -1;
    return size;
}

int fg_gguf_upload(FILE *file, fg_vk_upload *upload, const fg_vk_buffer *dest, VkDeviceSize offset,
    uint64_t file_offset, uint64_t bytes, unsigned char *chunk) {
    const size_t capacity = (size_t)upload->staging.size;
    if (!capacity || capacity % 4 || file_offset > INT64_MAX || fseeko(file, (int64_t)file_offset, SEEK_SET)) return 0;
    for (uint64_t at = 0; at < bytes;) {
        const size_t count = bytes - at > capacity ? capacity : (size_t)(bytes - at);
        const size_t padded = (count + 3) & ~(size_t)3;
        if (fread(chunk, 1, count, file) != count) return 0;
        memset(chunk + count, 0, padded - count);
        if (!fg_vk_upload_write(upload, dest, offset + at, chunk, padded)) return 0;
        at += count;
    }
    return 1;
}
