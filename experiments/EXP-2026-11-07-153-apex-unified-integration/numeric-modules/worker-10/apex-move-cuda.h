#pragma once
/* C++/CUDA-only declarations for the default-on native source patch. */
#include <cuda_runtime.h>
#include "apex-move-bridge.h"

#ifdef __cplusplus
struct ggml_backend_cuda_context;
struct ggml_tensor;

extern "C" {
int fg153_apex_move_cpy(ggml_backend_cuda_context & ctx, const ggml_tensor * src0, ggml_tensor * src1);
int fg153_apex_move_get_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
int fg153_apex_move_set_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
void fg153_apex_move_to_fp32_q4_0(const void * vx, float * y, int64_t k, cudaStream_t stream);
}
#endif
