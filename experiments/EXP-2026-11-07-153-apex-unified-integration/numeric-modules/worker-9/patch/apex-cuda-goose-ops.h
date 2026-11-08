// Apex CUDA Goose operation hooks (worker-9).
//
// The patch inserts a single default-on hook at the top of the native
// ggml_cuda_op_* entry points. Each hook extracts the same metadata the native
// launcher reads, calls the Goose-generated device code, and returns true.
// Unsupported types/layouts return false so the original native path runs.
// Rollback: compile with -DAPEX_CUDA_GOOSE_OPS=0 (no other source change).
#pragma once

#ifndef APEX_CUDA_GOOSE_OPS
#define APEX_CUDA_GOOSE_OPS 1
#endif

#if APEX_CUDA_GOOSE_OPS

#include <cstdint>
#include <cstddef>
#include <cstring>

// Provided by worker-9 gen/device/*.cu (apex_launch_* host entry points).
struct ggml_backend_cuda_context;
struct ggml_tensor;
typedef struct CUstream_st * cudaStream_t;

void apex_launch_unary_f32(int op, const float * x, float * dst, int64_t k, cudaStream_t stream);
void apex_launch_glu_swiglu_f32(const float * x, const float * g, float * dst,
                                int64_t k, int64_t n, int64_t o0, int64_t o1, cudaStream_t stream);
void apex_launch_bin_bcast_f32(int op, const float * s0, const float * s1, float * dst,
        int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3,
        int64_t ne10, int64_t ne11, int64_t ne12, int64_t ne13,
        int64_t d1, int64_t d2, int64_t d3,
        int64_t s00, int64_t s01, int64_t s02, int64_t s03,
        int64_t s10, int64_t s11, int64_t s12, int64_t s13, cudaStream_t stream);
void apex_launch_norm_f32(const float * x, float * dst, int64_t ncols, int64_t nrows,
        int64_t nchannels, int64_t nsamples, int64_t srow, int64_t sch, int64_t ssamp,
        float eps, cudaStream_t stream);
void apex_launch_group_norm_f32(const float * x, float * dst, int64_t num_groups,
        int64_t group_size, int64_t ne_elements, float eps, cudaStream_t stream);
void apex_launch_rms_norm_f32(const float * x, float * dst, int64_t ncols, int64_t nrows,
        int64_t nchannels, int64_t nsamples, int64_t srow, int64_t sch, int64_t ssamp,
        float eps, int alignment_ok, cudaStream_t stream);
void apex_launch_l2_norm_f32(const float * x, float * dst, int64_t ncols, int64_t nrows,
        int64_t nchannels, int64_t nsamples, int64_t srow, int64_t sch, int64_t ssamp,
        float eps, cudaStream_t stream);
void apex_launch_soft_max_f32(const float * x, const uint8_t * mask, int mkind, float * dst,
        int64_t ncols, int64_t ne01, int64_t ne02, int64_t ne03, int64_t ne12, int64_t ne13,
        int64_t mb11, int64_t mb12, int64_t mb13,
        float scale, float max_bias, int64_t n_head, cudaStream_t stream);
void apex_launch_rope_multi_f32(const float * x, float * dst, const int32_t * pos,
        int64_t ne00, int64_t ne01, int64_t ne02, int64_t nr,
        int64_t s01, int64_t s02, int64_t s03, int64_t s1, int64_t s2, int64_t s3,
        int n_dims, int n_offs, int mode, int n_ctx_orig,
        float freq_base, float freq_scale, float ext_factor, float attn_factor,
        float beta_fast, float beta_slow, const int sections[4], cudaStream_t stream);
void apex_launch_rope_multi_f32_f16(const float * x, uint16_t * dst, const int32_t * pos,
        int64_t ne00, int64_t ne01, int64_t ne02, int64_t nr,
        int64_t s01, int64_t s02, int64_t s03, int64_t s1, int64_t s2, int64_t s3,
        int n_dims, int n_offs, int mode, int n_ctx_orig,
        float freq_base, float freq_scale, float ext_factor, float attn_factor,
        float beta_fast, float beta_slow, const int sections[4], cudaStream_t stream);

namespace apex_cuda_goose {

static inline bool aligned16(const void * p) { return ((uintptr_t) p & 0xFu) == 0; }

static inline bool unary(ggml_backend_cuda_context & ctx, ggml_tensor * dst, int op) {
    const ggml_tensor * src0 = dst->src[0];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    if (!ggml_is_contiguous(src0) || !ggml_is_contiguous(dst)) { return false; }
    apex_launch_unary_f32(op, (const float *) src0->data, (float *) dst->data,
                          ggml_nelements(src0), ctx.stream());
    return true;
}

static inline bool swiglu(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    if (src1 && src1->type != GGML_TYPE_F32) { return false; }
    if (!ggml_is_contiguous_1(src0) || !ggml_is_contiguous(dst)) { return false; }
    if (src1 && !ggml_is_contiguous_1(src1)) { return false; }
    const int64_t nc = src1 ? src0->ne[0] : src0->ne[0] / 2;
    if (dst->ne[0] != nc) { return false; }
    const int32_t swapped = ((const int32_t *) dst->op_params)[1];
    const float * x = (const float *) src0->data;
    const float * g = src1 ? (const float *) src1->data : x;
    const int64_t o0 = src0->nb[1] / (int64_t) sizeof(float);
    const int64_t o1 = src1 ? src1->nb[1] / (int64_t) sizeof(float) : o0;
    if (!src1) { x += swapped ? nc : 0; g += swapped ? 0 : nc; }
    apex_launch_glu_swiglu_f32(x, g, (float *) dst->data, ggml_nelements(dst), nc, o0, o1, ctx.stream());
    return true;
}

static inline void collapse(int64_t cne[4]) {
    cne[0] *= cne[1]; cne[1] = cne[2]; cne[2] = cne[3]; cne[3] = 1;
}
static inline void collapse_nb(int64_t cnb[4], const int64_t cne[4]) {
    cnb[1] *= cne[1]; cnb[2] *= cne[2]; cnb[3] *= cne[3];
}

static inline bool bin_bcast(ggml_backend_cuda_context & ctx, ggml_tensor * dst, int op) {
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];
    if (!src1) { return false; }
    if (src0->type != GGML_TYPE_F32 || src1->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }

    int64_t cne[4]  = { dst->ne[0], dst->ne[1], dst->ne[2], dst->ne[3] };
    int64_t cne0[4] = { src0->ne[0], src0->ne[1], src0->ne[2], src0->ne[3] };
    int64_t cne1[4] = { src1->ne[0], src1->ne[1], src1->ne[2], src1->ne[3] };
    int64_t cnb[4]  = { dst->nb[0], dst->nb[1], dst->nb[2], dst->nb[3] };
    int64_t cnb0[4] = { src0->nb[0], src0->nb[1], src0->nb[2], src0->nb[3] };
    int64_t cnb1[4] = { src1->nb[0], src1->nb[1], src1->nb[2], src1->nb[3] };
    int64_t nr[4] = { cne1[0] / cne[0], cne1[1] / cne[1], cne1[2] / cne[2], cne1[3] / cne[3] };
    const bool contiguous = ggml_is_contiguous(src0) && ggml_is_contiguous(src1) &&
                            !ggml_is_permuted(src0) && !ggml_is_permuted(src1);
    if (contiguous) {
        for (int i = 0; i < 4; i++) {
            if (nr[i] != 1) { break; }
            if (i > 0) {
                collapse_nb(cnb, cne); collapse_nb(cnb0, cne0); collapse_nb(cnb1, cne1);
                collapse(cne); collapse(cne0); collapse(cne1);
            }
        }
    }
    const int64_t ts = (int64_t) sizeof(float);
    apex_launch_bin_bcast_f32(op,
        (const float *) src0->data, (const float *) src1->data, (float *) dst->data,
        cne[0], cne[1], cne[2], cne[3], cne1[0], cne1[1], cne1[2], cne1[3],
        cnb[1] / ts, cnb[2] / ts, cnb[3] / ts,
        cnb0[0] / ts, cnb0[1] / ts, cnb0[2] / ts, cnb0[3] / ts,
        cnb1[0] / ts, cnb1[1] / ts, cnb1[2] / ts, cnb1[3] / ts, ctx.stream());
    return true;
}

static inline bool norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    if (src0->nb[0] != (int64_t) sizeof(float)) { return false; }
    float eps; memcpy(&eps, dst->op_params, sizeof(float));
    const int64_t ts = (int64_t) sizeof(float);
    apex_launch_norm_f32((const float *) src0->data, (float *) dst->data,
        src0->ne[0], src0->ne[1], src0->ne[2], src0->ne[3],
        src0->nb[1] / ts, src0->nb[2] / ts, src0->nb[3] / ts, eps, ctx.stream());
    return true;
}

static inline bool group_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    const int num_groups = dst->op_params[0];
    if (num_groups <= 0) { return false; }
    float eps; memcpy(&eps, dst->op_params + 1, sizeof(float));
    const int64_t group_size = src0->ne[0] * src0->ne[1] * ((src0->ne[2] + num_groups - 1) / num_groups);
    apex_launch_group_norm_f32((const float *) src0->data, (float *) dst->data,
        (int64_t) num_groups * src0->ne[3], group_size, ggml_nelements(src0), eps, ctx.stream());
    return true;
}

static inline bool rms_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    if (src0->nb[0] != (int64_t) sizeof(float)) { return false; }
    float eps; memcpy(&eps, dst->op_params, sizeof(float));
    const int64_t ts = (int64_t) sizeof(float);
    const bool aok = aligned16(src0->data) && aligned16(dst->data);
    apex_launch_rms_norm_f32((const float *) src0->data, (float *) dst->data,
        src0->ne[0], src0->ne[1], src0->ne[2], src0->ne[3],
        src0->nb[1] / ts, src0->nb[2] / ts, src0->nb[3] / ts, eps, aok ? 1 : 0, ctx.stream());
    return true;
}

static inline bool l2_norm(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    if (src0->nb[0] != (int64_t) sizeof(float)) { return false; }
    float eps; memcpy(&eps, dst->op_params, sizeof(float));
    const int64_t ts = (int64_t) sizeof(float);
    apex_launch_l2_norm_f32((const float *) src0->data, (float *) dst->data,
        src0->ne[0], src0->ne[1], src0->ne[2], src0->ne[3],
        src0->nb[1] / ts, src0->nb[2] / ts, src0->nb[3] / ts, eps, ctx.stream());
    return true;
}

static inline bool soft_max(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];
    const ggml_tensor * src2 = dst->src[2];
    if (src0->type != GGML_TYPE_F32 || dst->type != GGML_TYPE_F32) { return false; }
    if (src2 && src2->data) { return false; } // attention sinks: native fallback
    if (src1 && src1->type != GGML_TYPE_F32 && src1->type != GGML_TYPE_F16) { return false; }
    float scale = 1.0f, max_bias = 0.0f;
    memcpy(&scale, (const float *) dst->op_params + 0, sizeof(float));
    memcpy(&max_bias, (const float *) dst->op_params + 1, sizeof(float));
    const int mkind = src1 ? (src1->type == GGML_TYPE_F16 ? 0 : 1) : -1;
    const int64_t esz = src1 ? (int64_t) ggml_element_size(src1) : 1;
    const int64_t ne12 = src1 ? src1->ne[2] : 1;
    const int64_t ne13 = src1 ? src1->ne[3] : 1;
    const int64_t mb11 = src1 ? src1->nb[1] / esz : 1;
    const int64_t mb12 = src1 ? src1->nb[2] / esz : 1;
    const int64_t mb13 = src1 ? src1->nb[3] / esz : 1;
    apex_launch_soft_max_f32((const float *) src0->data,
        src1 ? (const uint8_t *) src1->data : nullptr, mkind, (float *) dst->data,
        src0->ne[0], src0->ne[1], src0->ne[2], src0->ne[3], ne12, ne13,
        mb11, mb12, mb13, scale, max_bias, (int64_t) src0->ne[2], ctx.stream());
    return true;
}

static inline bool rope(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];
    const ggml_tensor * src2 = dst->src[2];
    if (src0->type != GGML_TYPE_F32) { return false; }
    if (!src1 || src1->type != GGML_TYPE_I32) { return false; }
    if (src0->ne[0] % 2 != 0) { return false; }
    if (dst->type != GGML_TYPE_F32 && dst->type != GGML_TYPE_F16) { return false; }
    if (src2 && src2->data) { return false; } // freq_factors: native fallback
    const int n_dims = ((const int32_t *) dst->op_params)[1];
    const int mode   = ((const int32_t *) dst->op_params)[2];
    const int n_ctx_orig = ((const int32_t *) dst->op_params)[4];
    const int n_offs = ((const int32_t *) dst->op_params)[15];
    if (mode != GGML_ROPE_TYPE_IMROPE) { return false; } // other modes: native fallback
    float freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow;
    memcpy(&freq_base,   (const int32_t *) dst->op_params + 5, sizeof(float));
    memcpy(&freq_scale,  (const int32_t *) dst->op_params + 6, sizeof(float));
    memcpy(&ext_factor,  (const int32_t *) dst->op_params + 7, sizeof(float));
    memcpy(&attn_factor, (const int32_t *) dst->op_params + 8, sizeof(float));
    memcpy(&beta_fast,   (const int32_t *) dst->op_params + 9, sizeof(float));
    memcpy(&beta_slow,   (const int32_t *) dst->op_params + 10, sizeof(float));
    int sections[4];
    memcpy(sections, (const int32_t *) dst->op_params + 11, sizeof(int) * 4);
    const int64_t ts0 = (int64_t) ggml_type_size(src0->type);
    const int64_t tsd = (int64_t) ggml_type_size(dst->type);
    const int64_t s01 = src0->nb[1] / ts0, s02 = src0->nb[2] / ts0, s03 = src0->nb[3] / ts0;
    const int64_t s1  = dst->nb[1] / tsd, s2 = dst->nb[2] / tsd, s3 = dst->nb[3] / tsd;
    const int64_t nr = ggml_nrows(src0);
    const int32_t * pos = (const int32_t *) src1->data;
    if (dst->type == GGML_TYPE_F32) {
        apex_launch_rope_multi_f32((const float *) src0->data, (float *) dst->data, pos,
            src0->ne[0], src0->ne[1], src0->ne[2], nr, s01, s02, s03, s1, s2, s3,
            n_dims, n_offs, mode, n_ctx_orig, freq_base, freq_scale, ext_factor, attn_factor,
            beta_fast, beta_slow, sections, ctx.stream());
    } else {
        apex_launch_rope_multi_f32_f16((const float *) src0->data, (uint16_t *) dst->data, pos,
            src0->ne[0], src0->ne[1], src0->ne[2], nr, s01, s02, s03, s1, s2, s3,
            n_dims, n_offs, mode, n_ctx_orig, freq_base, freq_scale, ext_factor, attn_factor,
            beta_fast, beta_slow, sections, ctx.stream());
    }
    return true;
}

} // namespace apex_cuda_goose

#endif // APEX_CUDA_GOOSE_OPS
