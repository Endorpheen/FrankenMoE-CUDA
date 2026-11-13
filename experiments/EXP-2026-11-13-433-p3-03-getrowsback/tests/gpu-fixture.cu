// Differential fixture for the Goose GET_ROWS_BACK port.
#include "common.cuh"
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <cstdint>

// ---- oracle: verbatim body from ggml-cuda/getrows.cu -----------------------
template<typename grad_t, typename dst_t>
static __global__ void k_get_rows_back_float(
        const grad_t * __restrict__ grad, const int32_t * __restrict__ rows, dst_t * __restrict__ dst,
        const int64_t ncols, const int64_t nrows_grad, const int64_t nrows_dst) {
    const int col = blockIdx.x*blockDim.x + threadIdx.x;
    if (col >= ncols) return;
    ggml_cuda_pdl_sync();
    for (int64_t dst_row = blockIdx.y; dst_row < nrows_dst; dst_row += gridDim.y) {
        float sum = 0.0f;
        for (int64_t i = 0; i < nrows_grad; ++i) {
            if (rows[i] != dst_row) continue;
            sum += grad[i*ncols + col];
        }
        dst[dst_row*ncols + col] = sum;
    }
}

// ---- goose arm --------------------------------------------------------------
static __device__ __forceinline__ float fg433b_ld_f32(uint64_t p, uint64_t o) {
    return *(const float *) ((const char *) (uintptr_t) p + o);
}
static __device__ __forceinline__ int64_t fg433b_ld_i32(uint64_t p, uint64_t o) {
    return (int64_t) *(const int32_t *) ((const char *) (uintptr_t) p + o);
}
static __device__ __forceinline__ void fg433b_st_f32(uint64_t p, uint64_t o, float v) {
    *(float *) ((char *) (uintptr_t) p + o) = v;
}
static __device__ __forceinline__ void fg433b_fail(int64_t) { asm("trap;"); }
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_rows(const float * grad, const int32_t * rows, float * dst,
        int64_t ncols, int64_t nrows_grad, int64_t nrows_dst) {
    FG433B_CB_rows_back((uint64_t) (uintptr_t) grad, (uint64_t) (uintptr_t) rows,
        (uint64_t) (uintptr_t) dst, ncols, nrows_grad, nrows_dst,
        (int64_t) blockIdx.x * blockDim.x + threadIdx.x,
        (int64_t) blockIdx.y, (int64_t) gridDim.y, 0);
}

struct Case { int64_t ncols, nrows_grad, nrows_dst; const char * name; };

int main(void) {
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) { fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(433001);
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    const Case cases[10] = {
        {1, 0, 1, "empty-grad"}, {1, 1, 1, "one"}, {31, 5, 4, "short"},
        {256, 17, 32, "block"}, {257, 100, 1000, "tail"}, {1000, 100, 17, "wide"},
        {64, 1024, 64, "many-grad"}, {32, 3, 70000, "grid-y-clamp"},
        {513, 33, 129, "tail2"}, {128, 1, 1, "single-row"},
    };
    int checks = 0, fails = 0, launch_fail = 0;
    for (int c = 0; c < 10; ++c) {
        const Case & k = cases[c];
        const int64_t n = k.nrows_grad * k.ncols;
        std::vector<float> hg((size_t) n);
        for (auto & v : hg) v = dist(rng);
        std::vector<int32_t> hr((size_t) k.nrows_grad);
        for (auto & v : hr) v = (int32_t) (rng() % (uint64_t) k.nrows_dst);
        float * dg = nullptr; int32_t * dr = nullptr; float * dref = nullptr; float * dgo = nullptr;
        cudaMalloc(&dg, (size_t) n * 4);
        cudaMalloc(&dr, (size_t) k.nrows_grad * 4);
        cudaMalloc(&dref, (size_t) k.nrows_dst * k.ncols * 4);
        cudaMalloc(&dgo, (size_t) k.nrows_dst * k.ncols * 4);
        cudaMemcpy(dg, hg.data(), (size_t) n * 4, cudaMemcpyHostToDevice);
        cudaMemcpy(dr, hr.data(), (size_t) k.nrows_grad * 4, cudaMemcpyHostToDevice);
        cudaMemset(dref, 0x7F, (size_t) k.nrows_dst * k.ncols * 4);
        cudaMemset(dgo, 0x7F, (size_t) k.nrows_dst * k.ncols * 4);

        const int bs = 256;
        const int gx = (int) ((k.ncols + bs - 1) / bs);
        const int gy = (int) (k.nrows_dst > 65535 ? 65535 : k.nrows_dst);
        const dim3 grid((unsigned) gx, (unsigned) gy, 1);
        const dim3 block((unsigned) bs, 1, 1);

        k_get_rows_back_float<float, float><<<grid, block>>>(dg, dr, dref, k.ncols, k.nrows_grad, k.nrows_dst);
        cudaError_t e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { printf("LAUNCH FAIL native c=%d: %s\n", c, cudaGetErrorString(e)); ++launch_fail; }

        goose_rows<<<grid, block>>>(dg, dr, dgo, k.ncols, k.nrows_grad, k.nrows_dst);
        e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { printf("LAUNCH FAIL goose c=%d: %s\n", c, cudaGetErrorString(e)); ++launch_fail; }

        std::vector<float> a((size_t) k.nrows_dst * k.ncols), b((size_t) k.nrows_dst * k.ncols);
        cudaMemcpy(a.data(), dref, a.size() * 4, cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(), dgo, b.size() * 4, cudaMemcpyDeviceToHost);
        for (size_t i = 0; i < a.size(); ++i) {
            ++checks;
            if (memcmp(&a[i], &b[i], 4)) {
                ++fails;
                if (fails == 1) printf("FIRST FAIL c=%d i=%zu native=%08x goose=%08x\n", c, i,
                    *(unsigned *) &a[i], *(unsigned *) &b[i]);
            }
        }
        cudaFree(dg); cudaFree(dr); cudaFree(dref); cudaFree(dgo);
    }
    printf("FG433B_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n", checks, fails, launch_fail);
    return (fails || launch_fail) ? 1 : 0;
}
