// worker-12 public device entry points for the Apex Q4_0 FlashAttention vec path.
// The declarations are byte-for-byte the existing fattn_kernel_t argument list
// so the patched dispatcher can bind them without a wrapper or ABI change.
#pragma once
#include <cstdint>
#include <cuda_runtime.h>

extern "C" __global__ void fg012_flash_attn_ext_vec_q4_ncols1(
        const char * Q, const char * K, const char * V,
        const char * mask, const char * sinks, const int * KV_max,
        float * dst, float2 * dst_meta,
        const float scale, const float max_bias, const float m0, const float m1,
        const uint32_t n_head_log2, const float logit_softcap,
        const int32_t ne00, const uint3 ne01, const int32_t ne02, const int32_t ne03,
        const int32_t nb01, const int32_t nb02, const int32_t nb03,
        const int32_t ne10, const int32_t ne11, const int32_t ne12, const int32_t ne13,
        const int32_t nb11, const int32_t nb12, const int64_t nb13,
        const int32_t nb21, const int32_t nb22, const int64_t nb23,
        const int32_t ne31, const int32_t ne32, const int32_t ne33,
        const int32_t nb31, const int32_t nb32, const int64_t nb33);

extern "C" __global__ void fg012_flash_attn_ext_vec_q4_ncols2(
        const char * Q, const char * K, const char * V,
        const char * mask, const char * sinks, const int * KV_max,
        float * dst, float2 * dst_meta,
        const float scale, const float max_bias, const float m0, const float m1,
        const uint32_t n_head_log2, const float logit_softcap,
        const int32_t ne00, const uint3 ne01, const int32_t ne02, const int32_t ne03,
        const int32_t nb01, const int32_t nb02, const int32_t nb03,
        const int32_t ne10, const int32_t ne11, const int32_t ne12, const int32_t ne13,
        const int32_t nb11, const int32_t nb12, const int64_t nb13,
        const int32_t nb21, const int32_t nb22, const int64_t nb23,
        const int32_t ne31, const int32_t ne32, const int32_t ne33,
        const int32_t nb31, const int32_t nb32, const int64_t nb33);
