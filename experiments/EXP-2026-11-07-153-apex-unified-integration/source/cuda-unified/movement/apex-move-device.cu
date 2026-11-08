// worker-10: CUDA launch layer for the generated Goose movement kernels.
// Launch shapes and the entry ids below come from the real generated emit;
// no kernel computation is reimplemented here.
#define FG153_AM_DEVICE_PRIMITIVES 1
#include "apex-move-cuda.h"
#include "common.cuh"

#include "apex-move-device-primitives.cuh"
#include "generated/apex-move-device-functions.cuh"
#include "generated/apex-move-entry-ids.h"

#define FG153_AM_JOIN2(a, b) a ## b
#define FG153_AM_FN(id) FG153_AM_JOIN2(gg_function_, id)

static __global__ void k_copy_scalar(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_COPY_SCALAR_DEVICE)((int64_t) (uintptr_t) &q, 0);
}
static __global__ void k_copy_contig(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_COPY_CONTIG_DEVICE)((int64_t) (uintptr_t) &q, 0);
}
static __global__ void k_copy_blocks(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_COPY_BLOCKS_DEVICE)((int64_t) (uintptr_t) &q, 0);
}
static __global__ void k_transpose(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_TRANSPOSE_DEVICE)((int64_t) (uintptr_t) &q, 0);
}
static __global__ void k_getrows(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_GETROWS_DEVICE)((int64_t) (uintptr_t) &q, 0);
}
static __global__ void k_setrows(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_SETROWS_DEVICE)((int64_t) (uintptr_t) &q, 0);
}
static __global__ void k_q4_dequant(fg153_am_params p) {
    fg153_am_params q = p;
    FG153_AM_FN(FG153_AM_FN_Q4_DEQUANT_DEVICE)((int64_t) (uintptr_t) &q, 0);
}

static fg153_am_params make_params(const ggml_tensor * a, const ggml_tensor * b) {
    fg153_am_params p = {};
    p.v[FG153_AM_P_SRC] = (int64_t) (uintptr_t) a->data;
    p.v[FG153_AM_P_DST] = (int64_t) (uintptr_t) b->data;
    p.v[FG153_AM_P_ST]  = (int64_t) a->type;
    p.v[FG153_AM_P_DT]  = (int64_t) b->type;
    p.v[FG153_AM_P_NE00] = a->ne[0];
    p.v[FG153_AM_P_NE01] = a->ne[1];
    p.v[FG153_AM_P_NE02] = a->ne[2];
    p.v[FG153_AM_P_NE03] = a->ne[3];
    p.v[FG153_AM_P_NB00] = (int64_t) a->nb[0];
    p.v[FG153_AM_P_NB01] = (int64_t) a->nb[1];
    p.v[FG153_AM_P_NB02] = (int64_t) a->nb[2];
    p.v[FG153_AM_P_NB03] = (int64_t) a->nb[3];
    p.v[FG153_AM_P_NE10] = b->ne[0];
    p.v[FG153_AM_P_NE11] = b->ne[1];
    p.v[FG153_AM_P_NE12] = b->ne[2];
    p.v[FG153_AM_P_NE13] = b->ne[3];
    p.v[FG153_AM_P_NB10] = (int64_t) b->nb[0];
    p.v[FG153_AM_P_NB11] = (int64_t) b->nb[1];
    p.v[FG153_AM_P_NB12] = (int64_t) b->nb[2];
    p.v[FG153_AM_P_NB13] = (int64_t) b->nb[3];
    p.v[FG153_AM_P_N] = ggml_nelements(a);
    return p;
}

static int types_float(int t) { return t == GGML_TYPE_F32 || t == GGML_TYPE_F16; }

extern "C" int fg153_apex_move_cpy(ggml_backend_cuda_context & ctx, const ggml_tensor * src0, ggml_tensor * src1) {
    if (src0 == nullptr || src1 == nullptr) return 0;
    const int64_t ne = ggml_nelements(src0);
    if (ne != ggml_nelements(src1)) return 0;

    const int st = (int) src0->type;
    const int dt = (int) src1->type;
    cudaStream_t stream = ctx.stream();

    if (st == GGML_TYPE_Q4_0 || dt == GGML_TYPE_Q4_0) {
        if (!((st == GGML_TYPE_F32 && dt == GGML_TYPE_Q4_0) ||
              (st == GGML_TYPE_Q4_0 && dt == GGML_TYPE_F32))) return 0;
        if (ne % 32 != 0) return 0;
        if ((int64_t) src0->nb[0] != ggml_type_size(src0->type) ||
            (int64_t) src1->nb[0] != ggml_type_size(src1->type)) return 0;

        fg153_am_params p = make_params(src0, src1);
        p.v[FG153_AM_P_N] = ne;
        if (!fg153_am_can_run(FG153_AM_OP_COPY_BLOCKS, &p)) return 0;
        const int64_t num_blocks = (ne / 32 + 63) / 64;
        if (num_blocks > INT_MAX) return 0;
        k_copy_blocks<<<(unsigned) num_blocks, 64, 0, stream>>>(p);
        CUDA_CHECK(cudaGetLastError());
        return 1;
    }

    if (!types_float(st) || !types_float(dt)) return 0;
    if (src0->ne[0] <= 0 || src0->ne[1] <= 0 || src0->ne[2] <= 0 || src0->ne[3] <= 0) return 0;

    const bool contiguous = ggml_is_contiguous(src0) && ggml_is_contiguous(src1);
    if (contiguous) {
        fg153_am_params p = make_params(src0, src1);
        p.v[FG153_AM_P_N] = ne;
        if (!fg153_am_can_run(FG153_AM_OP_COPY_CONTIG, &p)) return 0;
        const int64_t num_blocks = (ne + 63) / 64;
        if (num_blocks > INT_MAX) return 0;
        k_copy_contig<<<(unsigned) num_blocks, 64, 0, stream>>>(p);
        CUDA_CHECK(cudaGetLastError());
        return 1;
    }

    const bool can_be_transposed = (int64_t) src0->nb[1] == (int64_t) ggml_element_size(src0) &&
        src0->ne[3] == 1 &&
        (int64_t) src0->nb[2] == src0->ne[0] * src0->ne[1] * (int64_t) ggml_element_size(src0) &&
        ggml_is_contiguous(src1) &&
        src0->nb[0] <= src0->nb[2];
    if (can_be_transposed) {
        fg153_am_params p = make_params(src0, src1);
        p.v[FG153_AM_P_N] = ne;
        if (!fg153_am_can_run(FG153_AM_OP_TRANSPOSE, &p)) return 0;
        const int64_t ne00 = src0->ne[0];
        const int64_t ne01 = src0->ne[1];
        const int64_t gx = (ne01 + 31) / 32;
        const int64_t gy = (ne00 + 31) / 32;
        const int64_t gz = (ne / (ne00 * ne01) + 7) / 8;
        if (gx > INT_MAX || gy > USHRT_MAX || gz > USHRT_MAX) return 0;
        k_transpose<<<dim3((unsigned) gx, (unsigned) gy, (unsigned) gz), dim3(32, 8, 1), 0, stream>>>(p);
        CUDA_CHECK(cudaGetLastError());
        return 1;
    }

    fg153_am_params p = make_params(src0, src1);
    p.v[FG153_AM_P_N] = ne;
    if (!fg153_am_can_run(FG153_AM_OP_COPY_SCALAR, &p)) return 0;
    const int64_t num_blocks = (ne + 63) / 64;
    if (num_blocks > INT_MAX) return 0;
    k_copy_scalar<<<(unsigned) num_blocks, 64, 0, stream>>>(p);
    CUDA_CHECK(cudaGetLastError());
    return 1;
}

extern "C" int fg153_apex_move_get_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];
    if (!src0 || !src1) return 0;
    if (!((src0->type == GGML_TYPE_F32 || src0->type == GGML_TYPE_F16 || src0->type == GGML_TYPE_Q4_0) &&
          (dst->type == GGML_TYPE_F32 || dst->type == GGML_TYPE_F16))) return 0;
    if (src1->type != GGML_TYPE_I32) return 0;
    if (src1->ne[3] != 1) return 0;
    if ((int64_t) src0->nb[0] != ggml_type_size(src0->type)) return 0;
    if ((int64_t) src1->nb[0] != ggml_type_size(src1->type)) return 0;
    if ((int64_t) dst->nb[0] != ggml_type_size(dst->type)) return 0;

    fg153_am_params p = {};
    p.v[FG153_AM_P_SRC] = (int64_t) (uintptr_t) src0->data;
    p.v[FG153_AM_P_DST] = (int64_t) (uintptr_t) dst->data;
    p.v[FG153_AM_P_IDX] = (int64_t) (uintptr_t) src1->data;
    p.v[FG153_AM_P_ST] = (int64_t) src0->type;
    p.v[FG153_AM_P_DT] = (int64_t) dst->type;
    p.v[FG153_AM_P_IT] = (int64_t) src1->type;
    p.v[FG153_AM_P_NE00] = src0->ne[0];
    p.v[FG153_AM_P_NE01] = src0->ne[1];
    p.v[FG153_AM_P_NE02] = src0->ne[2];
    p.v[FG153_AM_P_NE03] = src0->ne[3];
    p.v[FG153_AM_P_NB00] = (int64_t) src0->nb[0];
    p.v[FG153_AM_P_NB01] = (int64_t) src0->nb[1];
    p.v[FG153_AM_P_NB02] = (int64_t) src0->nb[2];
    p.v[FG153_AM_P_NB03] = (int64_t) src0->nb[3];
    p.v[FG153_AM_P_NE10] = src1->ne[0];
    p.v[FG153_AM_P_NE11] = src1->ne[1];
    p.v[FG153_AM_P_NE12] = src1->ne[2];
    p.v[FG153_AM_P_NE13] = src1->ne[3];
    p.v[FG153_AM_P_NB10] = (int64_t) src1->nb[0];
    p.v[FG153_AM_P_NB11] = (int64_t) src1->nb[1];
    p.v[FG153_AM_P_NB12] = (int64_t) src1->nb[2];
    p.v[FG153_AM_P_NB13] = (int64_t) src1->nb[3];
    p.v[FG153_AM_P_NB1] = (int64_t) dst->nb[1];
    p.v[FG153_AM_P_NB2] = (int64_t) dst->nb[2];
    p.v[FG153_AM_P_NB3] = (int64_t) dst->nb[3];
    p.v[FG153_AM_P_N] = 0;
    p.v[FG153_AM_P_AUX0] = src1->ne[0];
    p.v[FG153_AM_P_AUX2] = src0->type == GGML_TYPE_Q4_0 ? 1 : 0;
    if (!fg153_am_can_run(FG153_AM_OP_GETROWS, &p)) return 0;

    const int64_t ne00 = src0->ne[0];
    const int64_t block_num_y = src0->type == GGML_TYPE_Q4_0
        ? (ne00 + 2 * 256 - 1) / (2 * 256)
        : (ne00 + 256 - 1) / 256;
    const dim3 grid((unsigned) src1->ne[0], (unsigned) MIN(block_num_y, (int64_t) UINT16_MAX),
                    (unsigned) MIN(src1->ne[1] * src1->ne[2], (int64_t) UINT16_MAX));
    k_getrows<<<grid, dim3(256, 1, 1), 0, ctx.stream()>>>(p);
    CUDA_CHECK(cudaGetLastError());
    return 1;
}

extern "C" int fg153_apex_move_set_rows(ggml_backend_cuda_context & ctx, ggml_tensor * dst) {
    const ggml_tensor * src0 = dst->src[0];
    const ggml_tensor * src1 = dst->src[1];
    if (!src0 || !src1) return 0;
    if (!((src0->type == GGML_TYPE_F32 &&
           (dst->type == GGML_TYPE_F32 || dst->type == GGML_TYPE_F16 || dst->type == GGML_TYPE_Q4_0)) ||
          (src0->type == GGML_TYPE_F16 && dst->type == GGML_TYPE_F16))) return 0;
    if (src1->type != GGML_TYPE_I64 && src1->type != GGML_TYPE_I32) return 0;
    if ((int64_t) src0->nb[0] != ggml_type_size(src0->type)) return 0;
    if ((int64_t) src1->nb[0] != ggml_type_size(src1->type)) return 0;
    if (dst->type == GGML_TYPE_Q4_0 && (int64_t) dst->nb[0] != ggml_type_size(dst->type)) return 0;

    fg153_am_params p = {};
    p.v[FG153_AM_P_SRC] = (int64_t) (uintptr_t) src0->data;
    p.v[FG153_AM_P_DST] = (int64_t) (uintptr_t) dst->data;
    p.v[FG153_AM_P_IDX] = (int64_t) (uintptr_t) src1->data;
    p.v[FG153_AM_P_ST] = (int64_t) src0->type;
    p.v[FG153_AM_P_DT] = (int64_t) dst->type;
    p.v[FG153_AM_P_IT] = (int64_t) src1->type;
    p.v[FG153_AM_P_NE00] = src0->ne[0];
    p.v[FG153_AM_P_NE01] = src0->ne[1];
    p.v[FG153_AM_P_NE02] = src0->ne[2];
    p.v[FG153_AM_P_NE03] = src0->ne[3];
    p.v[FG153_AM_P_NB01] = (int64_t) src0->nb[1];
    p.v[FG153_AM_P_NB02] = (int64_t) src0->nb[2];
    p.v[FG153_AM_P_NB03] = (int64_t) src0->nb[3];
    p.v[FG153_AM_P_NE10] = src1->ne[0];
    p.v[FG153_AM_P_NE11] = src1->ne[1];
    p.v[FG153_AM_P_NE12] = src1->ne[2];
    p.v[FG153_AM_P_NE13] = src1->ne[3];
    p.v[FG153_AM_P_NB10] = (int64_t) src1->nb[0];
    p.v[FG153_AM_P_NB11] = (int64_t) src1->nb[1];
    p.v[FG153_AM_P_NB12] = (int64_t) src1->nb[2];
    p.v[FG153_AM_P_NB13] = (int64_t) src1->nb[3];
    p.v[FG153_AM_P_NB1] = (int64_t) dst->nb[1];
    p.v[FG153_AM_P_NB2] = (int64_t) dst->nb[2];
    p.v[FG153_AM_P_NB3] = (int64_t) dst->nb[3];
    const int64_t ne = ggml_nelements(src0);
    p.v[FG153_AM_P_N] = dst->type == GGML_TYPE_Q4_0 ? ne / 32 : ne;
    if (!fg153_am_can_run(FG153_AM_OP_SETROWS, &p)) return 0;

    const int64_t num_blocks = (p.v[FG153_AM_P_N] + 255) / 256;
    if (num_blocks > INT_MAX) return 0;
    k_setrows<<<(unsigned) num_blocks, 256, 0, ctx.stream()>>>(p);
    CUDA_CHECK(cudaGetLastError());
    return 1;
}

extern "C" void fg153_apex_move_to_fp32_q4_0(const void * vx, float * y, int64_t k, cudaStream_t stream) {
    fg153_am_params p = {};
    p.v[FG153_AM_P_SRC] = (int64_t) (uintptr_t) vx;
    p.v[FG153_AM_P_DST] = (int64_t) (uintptr_t) y;
    p.v[FG153_AM_P_N] = k / 32;
    const int64_t nb = (k + 255) / 256;
    if (nb <= 0) return;
    k_q4_dequant<<<(unsigned) nb, 32, 0, stream>>>(p);
    CUDA_CHECK(cudaGetLastError());
}
