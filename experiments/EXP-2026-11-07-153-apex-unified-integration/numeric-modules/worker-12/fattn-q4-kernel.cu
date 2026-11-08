// worker-12: CUDA launch surface for the Goose-emitted Q4_0 FA vectors.
// No compute lives here; the body is a mechanical projection of the frozen
// emitter's gg_function entry onto the existing fattn_kernel_t signature.
#include "generated/fattn-q4-device.cu"

#define FG012_PROJECT(Q, K, V, mask, sinks, KV_max, dst, dst_meta,                   \
        scale, max_bias, m0, m1, n_head_log2, logit_softcap,                        \
        ne00, ne01, ne02, ne03, nb01, nb02, nb03,                                   \
        ne10, ne11, ne12, ne13, nb11, nb12, nb13,                                   \
        nb21, nb22, nb23, ne31, ne32, ne33, nb31, nb32, nb33, ncols_value)          \
    gg_function_6(                                                                  \
        (int64_t)(intptr_t)(Q), (int64_t)(intptr_t)(K), (int64_t)(intptr_t)(V),     \
        (int64_t)(intptr_t)(mask), (int64_t)(intptr_t)(sinks),                      \
        (int64_t)(intptr_t)(KV_max), (int64_t)(intptr_t)(dst),                      \
        (int64_t)(intptr_t)(dst_meta),                                              \
        (scale), (max_bias), (m0), (m1), (int64_t)(n_head_log2), (logit_softcap),   \
        (int64_t)(ne00), (int64_t)((ne01).z), (int64_t)(ne02), (int64_t)(ne03),     \
        (int64_t)(nb01), (int64_t)(nb02), (int64_t)(nb03),                          \
        (int64_t)(ne10), (int64_t)(ne11), (int64_t)(ne12), (int64_t)(ne13),         \
        (int64_t)(nb11), (int64_t)(nb12), (int64_t)(nb13),                          \
        (int64_t)(nb21), (int64_t)(nb22), (int64_t)(nb23),                          \
        (int64_t)(ne31), (int64_t)(ne32), (int64_t)(ne33),                          \
        (int64_t)(nb31), (int64_t)(nb32), (int64_t)(nb33),                          \
        (int64_t)(ncols_value), (int64_t)((mask) != nullptr),                       \
        (int64_t)((sinks) != nullptr), (int64_t)((KV_max) != nullptr),              \
        (int64_t)((logit_softcap) != 0.0f), 0)

extern "C" __launch_bounds__(128, 1) __global__ void fg012_flash_attn_ext_vec_q4_ncols1(
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
        const int32_t nb31, const int32_t nb32, const int64_t nb33) {
    FG012_PROJECT(Q, K, V, mask, sinks, KV_max, dst, dst_meta,
        scale, max_bias, m0, m1, n_head_log2, logit_softcap,
        ne00, ne01, ne02, ne03, nb01, nb02, nb03,
        ne10, ne11, ne12, ne13, nb11, nb12, nb13,
        nb21, nb22, nb23, ne31, ne32, ne33, nb31, nb32, nb33, 1);
}

extern "C" __launch_bounds__(128, 1) __global__ void fg012_flash_attn_ext_vec_q4_ncols2(
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
        const int32_t nb31, const int32_t nb32, const int64_t nb33) {
    FG012_PROJECT(Q, K, V, mask, sinks, KV_max, dst, dst_meta,
        scale, max_bias, m0, m1, n_head_log2, logit_softcap,
        ne00, ne01, ne02, ne03, nb01, nb02, nb03,
        ne10, ne11, ne12, ne13, nb11, nb12, nb13,
        nb21, nb22, nb23, ne31, ne32, ne33, nb31, nb32, nb33, 2);
}
