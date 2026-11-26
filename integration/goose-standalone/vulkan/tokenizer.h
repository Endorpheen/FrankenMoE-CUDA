#ifndef FG_VULKAN_TOKENIZER_H
#define FG_VULKAN_TOKENIZER_H

#include <stddef.h>
#include <stdint.h>

enum { FG_VK_TEXT_BYTES = 65536 };
typedef enum { FG_VK_MODEL_QWEN2, FG_VK_MODEL_BONSAI } fg_vk_model_kind;
typedef struct fg_vk_tokenizer fg_vk_tokenizer;

fg_vk_tokenizer *fg_vk_model_tokenizer_open(const char *path, fg_vk_model_kind kind);
void fg_vk_tokenizer_close(fg_vk_tokenizer *tokenizer);
int fg_vk_tokenizer_encode(const fg_vk_tokenizer *tokenizer, const char *text, size_t length,
    int special, uint32_t *ids, uint32_t capacity);
int fg_vk_tokenizer_piece(const fg_vk_tokenizer *tokenizer, uint32_t token, char *out, size_t bytes);
int fg_vk_tokenizer_is_eog(const fg_vk_tokenizer *tokenizer, uint32_t token);

#endif
