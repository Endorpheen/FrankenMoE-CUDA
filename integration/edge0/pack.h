#pragma once
#include "ggml.h"
#include <memory>
#include <string>
struct franken_edge0_pack;
std::shared_ptr<franken_edge0_pack> franken_edge0_open(const std::string & path);
void franken_edge0_bind(franken_edge0_pack & pack, int layer, ggml_tensor * router,
                       ggml_tensor * gate, ggml_tensor * up, ggml_tensor * down, ggml_tensor * shared_router);
ggml_tensor * franken_edge0_ffn(franken_edge0_pack & pack, ggml_context * ctx, ggml_tensor * x, int layer);
void franken_edge0_ple(franken_edge0_pack & pack, const int32_t * rows, int64_t count, float * output);
