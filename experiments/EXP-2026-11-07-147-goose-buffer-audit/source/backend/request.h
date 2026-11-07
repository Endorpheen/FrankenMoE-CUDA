#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
struct fg145_request {ggml_backend_t backend;ggml_backend_cpu_context*cpu;ggml_backend_plan_cpu*plan;ggml_cgraph*graph;ggml_backend_dev_t dev;const ggml_tensor*op;ggml_backend_buffer_type_t buft;const char*name;int64_t name_len;int64_t value;void*pointer;void*data;std::vector<ggml_backend_buffer_type_t>*extras;std::vector<ggml_backend_feature>*features;ggml_cplan temporary;size_t*free_mem;size_t*total_mem;ggml_backend_buffer_t buffer;const ggml_tensor*tensor2;void*data2;size_t buf_size;size_t buf_offset;int64_t buf_value;int64_t buf_kind;struct ggml_compute_params*params;size_t*size_ptr;};
static_assert(sizeof(uintptr_t)==sizeof(int64_t),"opaque ABI requires 64-bit pointers");
