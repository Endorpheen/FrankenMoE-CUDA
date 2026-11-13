// Differential fixture for Q4_K GET_ROWS (F32 destination).
#include "getrows.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>
#include <random>

// ---- oracle: verbatim k_get_rows_kq from getrows.cu ------------------------
template<typename dst_t, dequantize_kq_t<dst_t> dequantize_kq>
static __global__ void k_get_rows_kq(
        const void * __restrict__ src0, const int32_t * __restrict__ src1, dst_t * __restrict__ dst,
        const int64_t ne00, const int64_t ne11, const uint3 ne12_fdv,
        const size_t s1, const size_t s2, const size_t s3,
        const size_t nb01, const size_t nb02, const size_t nb03,
        const size_t s10, const size_t s11, const size_t s12) {
    ggml_cuda_pdl_sync();
    const int64_t nsb = ne00/QK_K;
    for (int64_t z = blockIdx.z; z < ne11*(int64_t)ne12_fdv.z; z += gridDim.z) {
        const int i10 = blockIdx.x;
        const uint2 dm  = fast_div_modulo((uint32_t)z, ne12_fdv);
        const int i11 = dm.x;
        const int i12 = dm.y;
        const int i01 = src1[i10*s10 + i11*s11 + i12*s12];
        dst_t * dst_row = dst + i10*s1 + i11*s2 + i12*s3;
        const void * src0_row = (const char *) src0 + i01*nb01 + i11*nb02 + i12*nb03;
        for (int64_t ib = blockIdx.y; ib < nsb; ib += gridDim.y) {
            dequantize_kq(src0_row, ib, dst_row + ib*QK_K, threadIdx.x);
        }
    }
}

// ---- goose arm --------------------------------------------------------------
static __device__ __forceinline__ uint32_t fg435_ld_u8(uint64_t p, uint64_t o) {
    return ((const uint8_t *) (uintptr_t) p)[o];
}
static __device__ __forceinline__ float fg435_ld_half(uint64_t p, uint64_t o) {
    return __half2float(*(const __half *) ((const char *) (uintptr_t) p + o));
}
static __device__ __forceinline__ int64_t fg435_ld_i32(uint64_t p, uint64_t o) {
    return (int64_t) *(const int32_t *) ((const char *) (uintptr_t) p + o);
}
static __device__ __forceinline__ void fg435_st_f32(uint64_t p, uint64_t o, float v) {
    *(float *) ((char *) (uintptr_t) p + o) = v;
}
static __device__ __forceinline__ void fg435_fail(int64_t) { asm("trap;"); }
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_getrows(const void * src0, const int32_t * src1, float * dst,
        int64_t ne00, int64_t ne11, int64_t ne12, int64_t ne10,
        int64_t s1, int64_t s2, int64_t s3, int64_t nb01, int64_t nb02, int64_t nb03,
        int64_t s10, int64_t s11, int64_t s12) {
    FG435_CB_getrows((uint64_t) (uintptr_t) src0, (uint64_t) (uintptr_t) src1,
        (uint64_t) (uintptr_t) dst, ne00, ne11, ne12, s1, s2, s3, nb01, nb02, nb03,
        s10, s11, s12, (int64_t) blockIdx.x, (int64_t) blockIdx.y, (int64_t) blockIdx.z,
        (int64_t) threadIdx.x, (int64_t) gridDim.y, (int64_t) gridDim.z, 0);
}

struct Case { int64_t ne00, ne10, nrows_src; const char * name; };

int main(void) {
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) { fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(435001);
    const Case cases[9] = {
        {256, 1, 4, "one-token"}, {256, 4, 4, "four-tokens"}, {512, 17, 17, "many-tokens"},
        {1024, 3, 8, "two-superblocks"}, {256, 32, 100, "rows100"},
        {512, 8, 2, "rows2"}, {768, 5, 6, "three-superblocks"},
        {256, 64, 1, "one-row"}, {1024, 16, 32, "wide"},
    };
    int checks = 0, fails = 0, launch_fail = 0;
    for (int c = 0; c < 9; ++c) {
        const Case & k = cases[c];
        const int64_t nsb = k.ne00 / 256;
        const size_t row_bytes = (size_t) nsb * 144;
        std::vector<uint8_t> src((size_t) k.nrows_src * row_bytes);
        for (auto & b : src) b = (uint8_t) (rng() & 0xff);
        std::vector<int32_t> idx((size_t) k.ne10);
        for (auto & v : idx) v = (int32_t) (rng() % (uint64_t) k.nrows_src);
        const size_t dst_elems = (size_t) k.ne10 * k.ne00;
        std::vector<float> zero(dst_elems, 0.0f);

        void * dsrc = nullptr; int32_t * didx = nullptr; float * dref = nullptr; float * dgo = nullptr;
        cudaMalloc(&dsrc, src.size()); cudaMalloc(&didx, idx.size() * 4);
        cudaMalloc(&dref, dst_elems * 4); cudaMalloc(&dgo, dst_elems * 4);
        cudaMemcpy(dsrc, src.data(), src.size(), cudaMemcpyHostToDevice);
        cudaMemcpy(didx, idx.data(), idx.size() * 4, cudaMemcpyHostToDevice);
        cudaMemset(dref, 0x7F, dst_elems * 4); cudaMemset(dgo, 0x7F, dst_elems * 4);

        const int64_t s1 = k.ne00, s2 = s1 * k.ne10, s3 = s2;
        const int64_t nb01 = (int64_t) row_bytes, nb02 = nb01 * k.nrows_src, nb03 = nb02;
        const uint3 ne12_fdv = init_fastdiv_values(1);
        const dim3 grid((unsigned) k.ne10, (unsigned) (nsb > 65535 ? 65535 : nsb), 1);
        const dim3 block(32, 1, 1);

        k_get_rows_kq<float, dequantize_q4_K<float>><<<grid, block>>>(
            dsrc, didx, dref, k.ne00, 1, ne12_fdv, s1, s2, s3, nb01, nb02, nb03, 1, 1, 1);
        cudaError_t e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { printf("LAUNCH FAIL native c=%d: %s\n", c, cudaGetErrorString(e)); ++launch_fail; }

        goose_getrows<<<grid, block>>>(dsrc, didx, dgo, k.ne00, 1, 1, k.ne10,
            s1, s2, s3, nb01, nb02, nb03, 1, 1, 1);
        e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { printf("LAUNCH FAIL goose c=%d: %s\n", c, cudaGetErrorString(e)); ++launch_fail; }

        std::vector<float> a(dst_elems), b(dst_elems);
        cudaMemcpy(a.data(), dref, dst_elems * 4, cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(), dgo, dst_elems * 4, cudaMemcpyDeviceToHost);
        for (size_t i = 0; i < dst_elems; ++i) {
            ++checks;
            if (memcmp(&a[i], &b[i], 4)) { ++fails; if (fails == 1) printf("FIRST FAIL c=%d i=%zu\n", c, i); }
        }
        cudaFree(dsrc); cudaFree(didx); cudaFree(dref); cudaFree(dgo);
    }
    printf("FG435_GETROWS_Q4K values=%d failures=%d launch_fail=%d\n", checks, fails, launch_fail);
    return (fails || launch_fail) ? 1 : 0;
}
