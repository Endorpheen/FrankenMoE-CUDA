#pragma once
//
// EXP168 normalization fixture - native oracle entry points.
//
// The parent build compiles the untouched upstream normalization
// implementation (inputs/native-norm.cu) with every public symbol prefixed
// by "fg168_native_" (see inputs/CONTRACT.md). This header declares those
// oracle entry points; signatures must match inputs/native-norm.cuh exactly.
// Must be included after "norm.cuh", mirroring the fg167 example fixture
// idiom (inputs/example-broadcast-fixture-api.h). The candidate (Goose)
// entry points are declared by the SDK header "norm.cuh" under their
// original unprefixed names.
//
void fg168_native_ggml_cuda_op_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
void fg168_native_ggml_cuda_op_group_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
void fg168_native_ggml_cuda_op_rms_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
void fg168_native_ggml_cuda_op_rms_norm_fused(ggml_backend_cuda_context & ctx, ggml_tensor * dst, ggml_tensor * mul_tensor);
void fg168_native_ggml_cuda_op_rms_norm_scale_fused(ggml_backend_cuda_context & ctx, ggml_tensor * dst, ggml_tensor * scale_tensor);
void fg168_native_ggml_cuda_op_rms_norm_fused_add(ggml_backend_cuda_context & ctx,
                                                  ggml_tensor *               dst,
                                                  ggml_tensor *               mul_tensor,
                                                  ggml_tensor *               add_tensor);
void fg168_native_ggml_cuda_op_rms_norm_back(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
void fg168_native_ggml_cuda_op_l2_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst);
