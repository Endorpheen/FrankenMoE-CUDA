// fg153-moe-cuda.h - default-on bridge to the Goose MoE routing kernels.
// Rollback: GGML_CUDA_GOOSE_ARGSORT=0, GGML_CUDA_GOOSE_TOP_K=0,
// GGML_CUDA_GOOSE_MOE_REDUCTION=0.  Launch helpers only fill the ABI struct
// and enqueue on the caller's stream; no host synchronisation is added.
#ifndef FG153_MOE_CUDA_H
#define FG153_MOE_CUDA_H
#include <cuda_runtime.h>
#include <cstdint>
#include <cstdlib>
#include "moe-routing/fg153-moe-abi.h"

extern "C" __global__ void fg153_goose_argsort_kernel(fg153_moe_sort_cfg cfg);
extern "C" __global__ void fg153_goose_moe_reduction_kernel(fg153_moe_reduce_cfg cfg);

static inline bool fg153_goose_argsort_enabled(void) {
    const char *e = std::getenv("GGML_CUDA_GOOSE_ARGSORT");
    return !(e != nullptr && e[0] == '0' && e[1] == '\0');
}
static inline bool fg153_goose_top_k_enabled(void) {
    const char *e = std::getenv("GGML_CUDA_GOOSE_TOP_K");
    return !(e != nullptr && e[0] == '0' && e[1] == '\0');
}
static inline bool fg153_goose_moe_reduction_enabled(void) {
    const char *e = std::getenv("GGML_CUDA_GOOSE_MOE_REDUCTION");
    return !(e != nullptr && e[0] == '0' && e[1] == '\0');
}

static inline int64_t fg153_goose_next_pow2(int64_t x) {
    int64_t n = 1;
    while (n < x) n *= 2;
    return n;
}

static inline bool fg153_goose_sort_launch(const float * x, int * dst,
        int64_t ncols, int64_t nrows, int64_t outcols, int order,
        int kind, cudaStream_t stream) {
    if (x == nullptr || dst == nullptr) return false;
    if (ncols <= 0 || nrows <= 0 || outcols < 1 || outcols > ncols) return false;
    if (order != 0 && order != 1) return false;
    const int64_t npad = fg153_goose_next_pow2(ncols);
    if (npad < ncols || npad > 12288) return false;
    const size_t smem = (size_t) npad * sizeof(int);
    if (smem > 48 * 1024) return false;
    fg153_moe_sort_cfg c{};
    c.v[FG153_MOE_X] = (int64_t)(uintptr_t) x;
    c.v[FG153_MOE_DST] = (int64_t)(uintptr_t) dst;
    c.v[FG153_MOE_NCOLS] = ncols;
    c.v[FG153_MOE_NPAD] = npad;
    c.v[FG153_MOE_ROWS] = nrows;
    c.v[FG153_MOE_OUTCOLS] = outcols;
    c.v[FG153_MOE_ORDER] = order;
    c.v[FG153_MOE_KIND] = kind;
    c.v[FG153_MOE_DSTSTRIDE] = outcols;
    const int64_t bd = npad < 1024 ? npad : 1024;
    dim3 grid((unsigned) nrows, 1, 1);
    dim3 block((unsigned) bd, 1, 1);
    fg153_goose_argsort_kernel<<<grid, block, smem, stream>>>(c);
    return cudaGetLastError() == cudaSuccess;
}

// Native ggml_cuda_op_argsort uses the bitonic branch only when ncols <= 1024
// (with CUB) or unconditionally without CUB; restrict default-on to the
// common, reachable <=1024 bitonic branch and fall back otherwise.
static inline bool fg153_goose_argsort_launch(const float * x, int * dst,
        int64_t ncols, int64_t nrows, int order, cudaStream_t stream) {
    if (!fg153_goose_argsort_enabled()) return false;
    if (ncols > 1024) return false;
    return fg153_goose_sort_launch(x, dst, ncols, nrows, ncols, order, 0, stream);
}

// Native top-k without CUB uses the bitonic argsort branch for ncols <= 512;
// with CUB it also requires nrows > 1.  Other branches stay native.
static inline bool fg153_goose_top_k_launch(const float * x, int * dst,
        int64_t ncols, int64_t nrows, int64_t k, cudaStream_t stream) {
    if (!fg153_goose_top_k_enabled()) return false;
    if (ncols <= 0 || ncols > 512 || k < 1 || k > ncols) return false;
#ifdef GGML_CUDA_USE_CUB
    if (nrows <= 1) return false;
#endif
    return fg153_goose_sort_launch(x, dst, ncols, nrows, k, 1, 1, stream);
}

static inline bool fg153_goose_moe_reduction_launch(const float * experts,
        const float * expert_scale, const float * weights, float * dst,
        int64_t n_embd, int64_t n_tokens, int n_expert_used, cudaStream_t stream) {
    if (!fg153_goose_moe_reduction_enabled()) return false;
    if (experts == nullptr || weights == nullptr || dst == nullptr) return false;
    if (n_embd <= 0 || n_tokens < 0 || n_expert_used < 1) return false;
    constexpr int threads = 256;
    fg153_moe_reduce_cfg c{};
    c.v[FG153_MR_EXPERTS] = (int64_t)(uintptr_t) experts;
    c.v[FG153_MR_SCALE] = (int64_t)(uintptr_t) expert_scale;
    c.v[FG153_MR_WEIGHTS] = (int64_t)(uintptr_t) weights;
    c.v[FG153_MR_DST] = (int64_t)(uintptr_t) dst;
    c.v[FG153_MR_NEMBD] = n_embd;
    c.v[FG153_MR_NTOK] = n_tokens;
    c.v[FG153_MR_NUSED] = n_expert_used;
    dim3 grid((unsigned) n_tokens, (unsigned) ((n_embd + threads - 1) / threads), 1);
    dim3 block((unsigned) threads, 1, 1);
    fg153_goose_moe_reduction_kernel<<<grid, block, 0, stream>>>(c);
    return cudaGetLastError() == cudaSuccess;
}
#endif
