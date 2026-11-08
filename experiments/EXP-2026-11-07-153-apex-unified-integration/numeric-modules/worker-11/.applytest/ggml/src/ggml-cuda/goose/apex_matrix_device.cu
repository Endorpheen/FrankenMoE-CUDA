// Device unit: the qualified Goose-generated C plus the CUDA launch shells.
// Every arithmetic statement comes from apex_matrix_generated.cu (emitted by
// the frozen Goose compiler and mechanically qualified); the shells below only
// supply thread indices, slices and launch geometry.
#include <cstdlib>
#include <cstring>
#include "apex_matrix_prims.cuh"
#include "apex_matrix_glue.h"

#define main fgm_generated_unused_main
#include "apex_matrix_generated.cu"
#undef main

static __device__ __forceinline__ int64_t fgm_call(int64_t mode,
        const void * w, int64_t wb, const void * x, int64_t xb,
        float * y, int64_t yl, int64_t p0, int64_t p1, int64_t p2, int64_t p3) {
    return gg_function_1(mode, gg_view((void *) w, wb), gg_view((void *) x, xb),
                         gg_view(y, yl), p0, p1, p2, p3, 0);
}

static __host__ __device__ __forceinline__ int64_t fgm_block_bytes(int64_t kind) {
    return kind == 12 ? 144 : (kind == 14 ? 210 : 0);
}

__global__ void fgm_k_dequant(int64_t kind, const void * w, float * y, int64_t lanes, int64_t wb) {
    int64_t i = (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= lanes) return;
    float tmp[1];
    int64_t mode = kind == 12 ? 0 : 1;
    fgm_call(mode, w, wb, nullptr, 0, tmp, 1, i, 0, 0, 0);
    y[i] = tmp[0];
}

__global__ void fgm_k_gemv(const void * w, int is_f16, int64_t nrows, int64_t k,
                           const float * x, float * y, int64_t wb, int64_t xb) {
    int64_t row = (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= nrows) return;
    float tmp[1];
    fgm_call(is_f16 ? 3 : 2, w, wb, x, xb, tmp, 1, nrows, k, row, 0);
    y[row] = tmp[0];
}

__global__ void fgm_k_gemm(const void * w, int is_f16, int64_t m, int64_t n, int64_t k,
                           const float * b, float * c, int64_t wb, int64_t bbytes) {
    int64_t row = (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= m) return;
    fgm_call(is_f16 ? 5 : 4, w, wb, b, bbytes, c + row * n, n, m, n, k, row);
}

// MMVQ: one thread per output row, super-blocks traversed in native order so
// the per-row floating accumulation order is deterministic.
__global__ void fgm_k_mmvq_kq(int64_t kind, const void * w, const void * q8, float * dst,
                              int64_t k, int64_t row_low, int64_t row_high,
                              int64_t stride_row_x, int64_t stride_col_y) {
    int64_t local = (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
    if (local >= row_high - row_low) return;
    int64_t bs = fgm_block_bytes(kind);
    int64_t blocks = k / 256;
    const char * rowp = (const char *) w + (row_low + local) * stride_row_x * bs;
    const char * q8p = (const char *) q8;
    float acc = 0.0f;
    float tmp[1];
    for (int64_t sb = 0; sb < blocks; ++sb) {
        fgm_call(kind == 12 ? 6 : 7, rowp + sb * bs, bs, q8p + sb * 8 * 36, 8 * 36,
                 tmp, 1, 0, 0, 0, 0);
        acc += tmp[0];
    }
    dst[local] = acc;
    (void) stride_col_y;
}

// MMQ-shaped path: one thread per (row, column).  It keeps the same q8_1 column
// layout and per-output numerics; the tensor-core MMA issue stays vendor-side.
__global__ void fgm_k_mmq_kq(int64_t kind, const void * w, const void * q8, float * dst,
                             int64_t nrows, int64_t k, int64_t ncols,
                             int64_t stride_row_x, int64_t q8_col_blocks) {
    int64_t row = (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
    int64_t col = blockIdx.y;
    if (row >= nrows || col >= ncols) return;
    int64_t bs = fgm_block_bytes(kind);
    int64_t blocks = k / 256;
    const char * rowp = (const char *) w + row * stride_row_x * bs;
    const char * colp = (const char *) q8 + col * q8_col_blocks * 8 * 36;
    float acc = 0.0f;
    float tmp[1];
    for (int64_t sb = 0; sb < blocks; ++sb) {
        fgm_call(kind == 12 ? 6 : 7, rowp + sb * bs, bs, colp + sb * 8 * 36, 8 * 36,
                 tmp, 1, 0, 0, 0, 0);
        acc += tmp[0];
    }
    dst[col * nrows + row] = acc;
}

// Shared-memory bound probe: Goose decides whether the dp4a tile fits.
__global__ void fgm_k_sram_probe(int64_t kind, int64_t I, int64_t stride, int64_t * out) {
    extern __shared__ int smem[];
    int64_t cap = I * stride;
    for (int64_t i = threadIdx.x; i < cap; i += blockDim.x) smem[i] = (int) (i & 0x7fffffff);
    __syncthreads();
    if (threadIdx.x == 0) {
        float tmp[6];
        int64_t ok = fgm_call(15, nullptr, 0, nullptr, 0, tmp, 6, kind, I, stride, 0);
        out[0] = (int64_t) tmp[3];
        out[1] = (int64_t) tmp[4];
        out[2] = ok;
    }
}

extern "C" int fgm_cuda_enabled(void) {
    const char * v = getenv("GGML_GOOSE_CUDA_MATRIX");
    return !v || strcmp(v, "0") != 0;
}

extern "C" int64_t fgm_cuda_matrix_entry(int64_t mode, const void * w, int64_t wbytes,
                                         const void * x, int64_t xbytes, float * y, int64_t ylen,
                                         int64_t p0, int64_t p1, int64_t p2, int64_t p3) {
    return gg_function_1(mode, gg_view((void *) w, wbytes), gg_view((void *) x, xbytes),
                         gg_view(y, ylen), p0, p1, p2, p3, 0);
}

extern "C" int fgm_cuda_dequant(int64_t kind, const void * w, float * y, int64_t lanes, cudaStream_t stream) {
    if (!fgm_cuda_enabled()) return 1;
    if (kind != 12 && kind != 14) return 2;
    if (!w || !y || lanes <= 0 || lanes % 256 != 0) return 3;
    int64_t blocks = lanes / 256;
    int64_t wb = blocks * fgm_block_bytes(kind);
    int threads = 256;
    int grid = (int) ((lanes + threads - 1) / threads);
    fgm_k_dequant<<<grid, threads, 0, stream>>>(kind, w, y, lanes, wb);
    return 0;
}

extern "C" int fgm_cuda_gemv(const void * w, int is_f16, int64_t nrows, int64_t k,
                             const float * x, float * y, cudaStream_t stream) {
    if (!fgm_cuda_enabled()) return 1;
    if (!w || !x || !y || nrows <= 0 || k <= 0) return 3;
    int64_t wb = nrows * k * (is_f16 ? 2 : 4);
    int64_t xb = k * 4;
    int threads = 128;
    int grid = (int) ((nrows + threads - 1) / threads);
    fgm_k_gemv<<<grid, threads, 0, stream>>>(w, is_f16, nrows, k, x, y, wb, xb);
    return 0;
}

extern "C" int fgm_cuda_gemm(const void * w, int is_f16, int64_t m, int64_t n, int64_t k,
                             const float * b, float * c, cudaStream_t stream) {
    if (!fgm_cuda_enabled()) return 1;
    if (!w || !b || !c || m <= 0 || n <= 0 || k <= 0) return 3;
    int64_t wb = m * k * (is_f16 ? 2 : 4);
    int64_t bbytes = k * n * 4;
    int threads = 128;
    int grid = (int) ((m + threads - 1) / threads);
    fgm_k_gemm<<<grid, threads, 0, stream>>>(w, is_f16, m, n, k, b, c, wb, bbytes);
    return 0;
}

extern "C" int fgm_cuda_mul_mat_vec_kq(int64_t kind, const void * w, const void * q8, float * dst,
                                       int64_t k, int64_t row_low, int64_t row_high,
                                       int64_t stride_row_x, int64_t stride_col_y, cudaStream_t stream) {
    if (!fgm_cuda_enabled()) return 1;
    if (kind != 12 && kind != 14) return 2;
    if (!w || !q8 || !dst || k <= 0 || k % 256 != 0 || row_high <= row_low) return 3;
    int64_t rows = row_high - row_low;
    int threads = 128;
    int grid = (int) ((rows + threads - 1) / threads);
    fgm_k_mmvq_kq<<<grid, threads, 0, stream>>>(kind, w, q8, dst, k, row_low, row_high,
                                                stride_row_x, stride_col_y);
    return 0;
}

extern "C" int fgm_cuda_mmq_kq(int64_t kind, const void * w, const void * q8, float * dst,
                               int64_t nrows, int64_t k, int64_t ncols,
                               int64_t stride_row_x, int64_t q8_col_blocks, cudaStream_t stream) {
    if (!fgm_cuda_enabled()) return 1;
    if (kind != 12 && kind != 14) return 2;
    if (!w || !q8 || !dst || nrows <= 0 || ncols <= 0 || k <= 0 || k % 256 != 0) return 3;
    int threads = 64;
    dim3 grid((unsigned) ((nrows + threads - 1) / threads), (unsigned) ncols, 1);
    fgm_k_mmq_kq<<<grid, threads, 0, stream>>>(kind, w, q8, dst, nrows, k, ncols,
                                               stride_row_x, q8_col_blocks);
    return 0;
}

extern "C" int fgm_cuda_mmq_sram_probe(int64_t kind, int64_t I, int64_t stride,
                                       int64_t * qs, int64_t * need, int64_t * cap, cudaStream_t stream) {
    if (!fgm_cuda_enabled()) return 1;
    if (kind != 12 && kind != 14) return 2;
    if (I <= 0 || I % 32 != 0 || stride <= 0) return 3;
    size_t bytes = (size_t) I * (size_t) stride * sizeof(int);
    if (bytes > 48u * 1024u) return 4;
    int64_t * d = nullptr;
    if (cudaMalloc(&d, 3 * sizeof(int64_t)) != cudaSuccess) return 5;
    fgm_k_sram_probe<<<1, 128, bytes, stream>>>(kind, I, stride, d);
    cudaError_t e = cudaMemcpyAsync(qs, d, 3 * sizeof(int64_t), cudaMemcpyDeviceToHost, stream);
    if (e == cudaSuccess) e = cudaStreamSynchronize(stream);
    cudaFree(d);
    if (e != cudaSuccess) return 6;
    *need = qs[1]; *cap = qs[2];
    return 0;
}
