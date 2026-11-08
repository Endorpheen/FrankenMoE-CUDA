// matrix-closure device unit: qualified Goose C + launch shells.
//
// All arithmetic, q8_1 packing decisions and MMQ tile planning come from
// closure_generated.cu (frozen Goose compiler output, mechanically qualified).
// These shells only supply thread indices, warp reductions, slices and launch
// geometry.  cuBLAS is never called here; the F16/F32 route reports it as an
// external vendor primitive.
#include <cstdlib>
#include <cstring>

#include "prims.cuh"
#include "emitter_ids.h"
#include "apex_closure_glue.h"

#define main fgc_generated_unused_main
#include "closure_generated.cu"
#undef main

// -------------------------- Goose call wrappers -----------------------------
static __device__ __forceinline__ int64_t dev_q8_elem(int64_t mode, const float * x, int64_t xlen,
        int64_t i, float amax, unsigned char * out, int64_t olen, int64_t off) {
    return FGC_FN_Q8_ELEM(mode, gg_view((void *) x, xlen), i, amax,
                                      gg_view(out, olen), off, 0);
}
static __device__ __forceinline__ int64_t dev_mmvq_header(float amax, float sum,
        unsigned char * out, int64_t olen, int64_t off) {
    return FGC_FN_MMVQ_HEADER(amax, sum, gg_view(out, olen), off, 0);
}
static __device__ __forceinline__ int64_t dev_mmq_header(float amax, float sum, int64_t layout,
        unsigned char * out, int64_t olen, int64_t off, int64_t gi) {
    return FGC_FN_MMQ_HEADER(amax, sum, layout, gg_view(out, olen), off, gi, 0);
}
static __device__ __forceinline__ int64_t dev_mmq_dot(int64_t kind, const void * w, int64_t wlen,
        int64_t wbase, const void * q8, int64_t q8len, int64_t qbase, float * out) {
    return FGC_FN_MMQ_DOT(kind, gg_view((void *) w, wlen), wbase,
                                      gg_view((void *) q8, q8len), qbase, gg_view(out, 1), 0);
}
static __device__ __forceinline__ int64_t dev_stage_weights(int64_t kind, const void * w, int64_t wlen,
        int64_t s01, int64_t kbx0, int64_t ibase, int64_t rows, unsigned char * dst, int64_t dlen, int64_t rs) {
    return FGC_FN_STAGE_WEIGHTS(kind, gg_view((void *) w, wlen), s01, kbx0, ibase, rows,
                                            gg_view(dst, dlen), rs, 0);
}
static __device__ __forceinline__ int64_t dev_stage_q8(const void * q8, int64_t q8len, int64_t ncols,
        int64_t kblock0, int64_t jbase, int64_t cols, unsigned char * dst, int64_t dlen) {
    return FGC_FN_STAGE_Q8(gg_view((void *) q8, q8len), ncols, kblock0, jbase, cols,
                                       gg_view(dst, dlen), 0);
}

// --------------------------- quantize kernels -------------------------------
__global__ void fgc_k_quant_row(const float * x, void * vy,
        int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
        int64_t ne0, int64_t ne1, int64_t ne2, int64_t n3, int64_t ylen) {
    int64_t i0 = (int64_t) blockDim.x * blockIdx.x + threadIdx.x;
    if (i0 >= ne0) return;
    int64_t i3 = blockIdx.z / ne2;
    int64_t i2 = blockIdx.z - i3 * ne2;
    int64_t i1 = blockIdx.y;
    int64_t icont = ((i3 * ne2 + i2) * ne1 + i1) * ne0 + i0;
    int64_t ib = icont / 32;
    int64_t iqs = icont % 32;
    const float * row = x + i3 * s03 + i2 * s02 + i1 * s01;
    float xi = i0 < ne00 ? row[i0] : 0.0f;
    float amax = fabsf(xi);
    float sum = xi;
#pragma unroll
    for (int off = 16; off > 0; off >>= 1) amax = fmaxf(amax, __shfl_xor_sync(0xffffffffu, amax, off, 32));
#pragma unroll
    for (int off = 16; off > 0; off >>= 1) sum  = sum + __shfl_xor_sync(0xffffffffu, sum, off, 32);
    int64_t ii = i0 < ne00 ? iqs : 0;
    unsigned char * y = (unsigned char *) vy;
    dev_q8_elem(0, row, ne00, ii, amax, y, ylen, ib * 36 + 4 + iqs);
    if (iqs == 0) dev_mmvq_header(amax, sum, y, ylen, ib * 36);
    (void) n3;
}

__global__ void fgc_k_quant_mmq(const float * x, void * vy,
        int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
        int64_t ne0, int64_t ne1, int64_t ne2, int64_t layout, int64_t ylen) {
    int64_t i0 = ((int64_t) blockDim.x * blockIdx.y + threadIdx.x) * 4;
    if (i0 >= ne0) return;
    int64_t i2 = blockIdx.z % ne2;
    int64_t i3 = blockIdx.z / ne2;
    int64_t base = i3 * s03 + i2 * s02 + (int64_t) blockIdx.x * s01;
    const float * row = x + base;
    float v0 = i0 + 0 < ne00 ? row[i0 + 0] : 0.0f;
    float v1 = i0 + 1 < ne00 ? row[i0 + 1] : 0.0f;
    float v2 = i0 + 2 < ne00 ? row[i0 + 2] : 0.0f;
    float v3 = i0 + 3 < ne00 ? row[i0 + 3] : 0.0f;
    float amax = fmaxf(fmaxf(fabsf(v0), fabsf(v1)), fmaxf(fabsf(v2), fabsf(v3)));
    float sum = v0 + v1 + v2 + v3;
#pragma unroll
    for (int off = 4; off > 0; off >>= 1) amax = fmaxf(amax, __shfl_xor_sync(0xffffffffu, amax, off, 32));
#pragma unroll
    for (int off = 4; off > 0; off >>= 1) sum  = sum + __shfl_xor_sync(0xffffffffu, sum, off, 32);
    int64_t kblock = i0 / 128;
    int64_t iqs = i0 % 128;
    int64_t yblock = kblock * ne1 + blockIdx.x;
    unsigned char * y = (unsigned char *) vy;
    int64_t ii = i0 < ne00 ? iqs : 0;
    dev_q8_elem(1, row, ne00, ii + 0, amax, y, ylen, yblock * 144 + 16 + iqs + 0);
    dev_q8_elem(1, row, ne00, ii + 1, amax, y, ylen, yblock * 144 + 16 + iqs + 1);
    dev_q8_elem(1, row, ne00, ii + 2, amax, y, ylen, yblock * 144 + 16 + iqs + 2);
    dev_q8_elem(1, row, ne00, ii + 3, amax, y, ylen, yblock * 144 + 16 + iqs + 3);
    if (iqs % 32 == 0) dev_mmq_header(amax, sum, layout, y, ylen, yblock * 144, iqs / 32);
}

// ------------------------------- MMQ kernel ---------------------------------
// Goose tiles: 32 weight rows x 8 activation columns.  Weights and q8_1 blocks
// are staged by Goose; the dot is Goose.  Static shared bytes are checked by
// sram_bound (>= 9472 for Q6_K).
__global__ void fgc_k_mmq(int64_t kind, const void * w, int64_t wlen, const void * q8, int64_t q8len,
        float * dst, int64_t ne00, int64_t ne01, int64_t ne11,
        int64_t s01, int64_t dstride, int64_t q8_ncols) {
    __shared__ unsigned char smem[9472];
    const int64_t I = 32;
    const int64_t J = 8;
    const int64_t rs = kind == 12 ? 160 : 224;
    unsigned char * wsh = smem;
    unsigned char * qsh = smem + I * rs;
    int64_t ti = threadIdx.x / 8;
    int64_t tj = threadIdx.x % 8;
    int64_t ibase = (int64_t) blockIdx.x * I;
    int64_t jbase = (int64_t) blockIdx.y * J;
    int64_t rows = ne01 - ibase; if (rows > I) rows = I;
    int64_t cols = ne11 - jbase; if (cols > J) cols = J;
    float acc = 0.0f;
    float tmp[1];
    int64_t nsb = ne00 / 256;
    for (int64_t sb = 0; sb < nsb; ++sb) {
        if (tj == 0 && ti < rows) {
            dev_stage_weights(kind, w, wlen, s01, sb, ibase + ti, 1, wsh + ti * rs, rs, rs);
        }
        if (ti == 0 && tj < cols) {
            dev_stage_q8(q8, q8len, q8_ncols, 2 * sb, jbase + tj, 1, qsh + tj * 288, 288);
        }
        __syncthreads();
        if (ti < rows && tj < cols) {
            dev_mmq_dot(kind, wsh, I * rs, ti * rs, qsh, J * 288, tj * 288, tmp);
            acc += tmp[0];
        }
        __syncthreads();
    }
    if (ti < rows && tj < cols) {
        dst[(jbase + tj) * dstride + ibase + ti] = acc;
    }
}

// ------------------------------ GEMM kernel ---------------------------------
__global__ void fgc_k_gemm(int64_t kind, const void * a, const void * b, float * c,
        int64_t m, int64_t n, int64_t k, int64_t bstride, int64_t cstride,
        int64_t alen, int64_t blen) {
    int64_t row = (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= m) return;
    FGC_FN_GEMM_ROW(kind, gg_view((void *) a, alen), gg_view((void *) b, blen),
                                gg_view(c + row, (n - 1) * cstride + 1), m, n, k, row, cstride, 0);
    (void) bstride;
}

// ----------------------------- host launchers -------------------------------
extern "C" int fgc_cuda_enabled(void) {
    const char * v = getenv("GGML_GOOSE_CUDA_MATRIX");
    return !v || strcmp(v, "0") != 0;
}

static int64_t fgc_route_host(int64_t kind, int64_t batch, int64_t dense, int64_t cc,
                              int64_t smpbo, int64_t ne00, int64_t ne11, int64_t nsm, int64_t * reason) {
    float out[2] = {0.0f, 0.0f};
    int64_t r = FGC_FN_ROUTE_MATMUL(kind, batch, dense, cc, smpbo, ne00, ne11, nsm,
                                                gg_view(out, 2), 0);
    if (reason) *reason = (int64_t) out[0];
    return r;
}

extern "C" int64_t fgc_cuda_route(int64_t kind, int64_t batch, int64_t dense, int64_t cc,
                                  int64_t smpbo, int64_t ne00, int64_t ne11, int64_t nsm, int64_t * reason) {
    return fgc_route_host(kind, batch, dense, cc, smpbo, ne00, ne11, nsm, reason);
}

extern "C" int64_t fgc_cuda_plan(int64_t kind, int64_t ne00, int64_t ne01, int64_t ne11,
                                 int64_t smpbo, int64_t cc, int64_t nsm, int64_t * out, int64_t nout) {
    if (!out || nout < 24) return 0;
    float f[24];
    int64_t ok = FGC_FN_PLAN_MMQ(kind, ne00, ne01, ne11, smpbo, cc, nsm, gg_view(f, 24), 0);
    for (int i = 0; i < 24; ++i) out[i] = (int64_t) f[i];
    return ok;
}

extern "C" int fgc_cuda_quant_row_q8_1(const float * x, void * vy,
        int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
        int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, cudaStream_t stream) {
    if (!fgc_cuda_enabled()) return 1;
    if (!x || !vy || ne0 <= 0 || ne0 % 32 != 0 || ne1 <= 0 || ne2 <= 0 || ne3 <= 0) return 2;
    int64_t ylen = ((int64_t) ne0 * ne1 * ne2 * ne3) / 32 * 36;
    dim3 grid((unsigned) ((ne0 + 255) / 256), (unsigned) ne1, (unsigned) (ne2 * ne3));
    fgc_k_quant_row<<<grid, 256, 0, stream>>>(x, vy, ne00, s01, s02, s03, ne0, ne1, ne2, ne3, ylen);
    return 0;
}

extern "C" int fgc_cuda_quant_mmq_q8_1(const float * x, void * vy,
        int64_t ne00, int64_t s01, int64_t s02, int64_t s03,
        int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, int64_t layout, cudaStream_t stream) {
    if (!fgc_cuda_enabled()) return 1;
    if (!x || !vy || ne00 % 4 != 0 || ne0 <= 0 || ne0 % 128 != 0 || ne1 <= 0 || ne2 <= 0 || ne3 <= 0) return 2;
    if (layout != 0 && layout != 1) return 3;
    int64_t ylen = ((int64_t) ne0 / 128) * ne1 * ne2 * ne3 * 144;
    int64_t bn_y = (ne0 + 511) / 512;
    dim3 grid((unsigned) ne1, (unsigned) bn_y, (unsigned) (ne2 * ne3));
    fgc_k_quant_mmq<<<grid, 128, 0, stream>>>(x, vy, ne00, s01, s02, s03, ne0, ne1, ne2, layout, ylen);
    return 0;
}

extern "C" int fgc_cuda_mmq_dense(int64_t kind, const void * w, const float * x, float * dst,
        int64_t ne00, int64_t ne01, int64_t ne11,
        int64_t s01, int64_t dst_stride,
        int64_t s11, int64_t s12, int64_t s13,
        int64_t ne12, int64_t ne13,
        int cc, int64_t smpbo, int64_t nsm, cudaStream_t stream) {
    if (!fgc_cuda_enabled()) return 1;
    if (kind != 12 && kind != 14) return 2;
    if (!w || !x || !dst) return 3;
    if (ne00 <= 0 || ne00 % 256 != 0 || ne01 <= 0 || ne11 <= 0) return 3;
    if (ne12 != 1 || ne13 != 1) return 4; // MoE ids and multi-sample stay native
    int64_t reason = 0;
    if (fgc_route_host(kind, ne11, 1, cc, smpbo, ne00, ne11, nsm, &reason) != 3) return 5;
    float plan[24];
    if (FGC_FN_PLAN_MMQ(kind, ne00, ne01, ne11, smpbo, cc, nsm, gg_view(plan, 24), 0) != 1) return 6;
    int64_t gi = (int64_t) plan[13];
    int64_t gj = (int64_t) plan[14];
    if (gi != 32 || gj != 8) return 7;
    int64_t ne10p = (int64_t) plan[17];
    int64_t gnty = (int64_t) plan[11];
    int64_t gntx = (int64_t) plan[12];
    int64_t layout = (int64_t) plan[4];
    int64_t q8blocks = (ne10p / 128) * ne11;
    int64_t q8bytes = q8blocks * 144;
    char * q8 = nullptr;
    if (cudaMalloc(&q8, (size_t) q8bytes) != cudaSuccess) return 8;
    int rc = fgc_cuda_quant_mmq_q8_1(x, q8, ne00, s11, s12, s13, ne10p, ne11, 1, 1, layout, stream);
    if (rc != 0) { cudaFree(q8); return 9; }
    int64_t bs = kind == 12 ? 144 : 210;
    int64_t wlen = ne01 * s01 * bs;
    int64_t q8len = q8bytes;
    dim3 grid((unsigned) gnty, (unsigned) gntx, 1);
    fgc_k_mmq<<<grid, 256, 0, stream>>>(kind, w, wlen, q8, q8len, dst, ne00, ne01, ne11,
                                        s01, dst_stride, ne11);
    cudaError_t err = cudaGetLastError();
    cudaFree(q8);
    if (err != cudaSuccess) return 10;
    return 0;
}

extern "C" int fgc_cuda_gemm_dense(int64_t kind, const void * a, const float * b, float * c,
        int64_t m, int64_t n, int64_t k, int64_t bstride, cudaStream_t stream) {
    if (!fgc_cuda_enabled()) return 1;
    const char * opt = getenv("GGML_GOOSE_CUDA_GEMM");
    int64_t reason = 0;
    int64_t r = fgc_route_host(kind, n, 1, 800, 64 * 1024, k, n, 1, &reason);
    if (r != 2) return 2; // only the explicit F16/F32 prefill route is served here
    if (!opt || strcmp(opt, "1") != 0) return 3; // cuBLAS is the default vendor primitive
    if (!a || !b || !c || m <= 0 || n <= 0 || k <= 0) return 4;
    int64_t ab = kind == 1 ? 2 : 4;
    int64_t alen = m * k * ab;
    int64_t blen = n * k * 4;
    int64_t cstride = m;
    int64_t grid = (m + 127) / 128;
    fgc_k_gemm<<<(unsigned) grid, 128, 0, stream>>>(kind, a, b, c, m, n, k, bstride, cstride, alen, blen);
    return 0;
}
