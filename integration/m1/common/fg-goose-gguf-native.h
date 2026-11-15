// M1 (P11-01): the GGUF writer entries kept compiled in the patched gguf.cpp.
//
// gguf_write_to_buf is private to gguf.cpp (gguf.h does not declare it), so the
// staged S1.2 gate gets both the public entry and its native oracle here.
// gguf_get_meta_size / gguf_get_meta_data are public and stay declared in
// gguf.h; only their *_native twins are added.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

struct gguf_context;

void   gguf_write_to_buf(const struct gguf_context * ctx, std::vector<int8_t> & buf, bool only_meta);
void   gguf_write_to_buf_native(const struct gguf_context * ctx, std::vector<int8_t> & buf, bool only_meta);
size_t gguf_get_meta_size_native(const struct gguf_context * ctx);
void   gguf_get_meta_data_native(const struct gguf_context * ctx, void * data);
