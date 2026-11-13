// Differential fixture for the Goose transposed CPY tile (F32).
#include "common.cuh"
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>
#include <random>

#define CUDA_CPY_TILE_DIM_2D 32
#define CUDA_CPY_BLOCK_NM 8
#define CUDA_CPY_BLOCK_ROWS 8

// ---- oracle: verbatim cpy_scalar_transpose<float> from cpy.cu --------------
template <typename T>
static __global__ void cpy_scalar_transpose(const char * cx, char * cdst, const int64_t ne,
        const int64_t ne00, const int64_t ne01, const int64_t ne02, const int64_t nb00,
        const int64_t nb01, const int64_t nb02, const int64_t nb03, const int64_t ne10,
        const int64_t ne11, const int64_t ne12, const int64_t nb10, const int64_t nb11,
        const int64_t nb12, const int64_t nb13) {
    const T* src = reinterpret_cast<const T*>(cx);
    T* dst = reinterpret_cast<T*>(cdst);
    const int64_t nmat = ne / (ne00 * ne01);
    const int64_t n = ne00 * ne01;
    const int64_t x  = (int64_t) blockIdx.x * CUDA_CPY_TILE_DIM_2D + threadIdx.x;
    const int64_t y  = (int64_t) blockIdx.y * CUDA_CPY_TILE_DIM_2D + threadIdx.y;
    const int64_t tx = (int64_t) blockIdx.y * CUDA_CPY_TILE_DIM_2D + threadIdx.x;
    const int64_t ty = (int64_t) blockIdx.x * CUDA_CPY_TILE_DIM_2D + threadIdx.y;
    __shared__ float tile[2][CUDA_CPY_TILE_DIM_2D][CUDA_CPY_TILE_DIM_2D+1];
    int cur_tile_buf = 0;
    ggml_cuda_pdl_sync();
#pragma unroll
    for (int i = 0; i < CUDA_CPY_BLOCK_NM; ++i) {
        const unsigned int imat = blockIdx.z * CUDA_CPY_BLOCK_NM + i;
        if (imat >= (unsigned) nmat) break;
#pragma unroll
        for (int j = 0; j < CUDA_CPY_TILE_DIM_2D; j += CUDA_CPY_BLOCK_ROWS) {
            if(x < ne01 && y + j < ne00){
                const int row = threadIdx.y+j;
                const int col = threadIdx.x * sizeof(float)/sizeof(T);
                T *tile2 = reinterpret_cast<T*>(tile[cur_tile_buf][row]);
                tile2[col] = src[imat*n + (y+j)*ne01 + x];
            }
        }
        __syncthreads();
#pragma unroll
        for (int j = 0; j < CUDA_CPY_TILE_DIM_2D; j += CUDA_CPY_BLOCK_ROWS) {
            if (ty + j < ne01 && tx < ne00) {
                const int col = (threadIdx.y+j)*sizeof(float)/sizeof(T);
                const T *tile2 = reinterpret_cast<const T*>(tile[cur_tile_buf][threadIdx.x]);
                dst[imat*n + (ty+j)*ne00 + tx] = tile2[col];
            }
        }
        cur_tile_buf = (cur_tile_buf + 1) % 2;
    }
    GGML_UNUSED_VARS(ne02, nb00, nb01, nb02, nb03, ne10, ne11, ne12, nb10, nb11, nb12, nb13);
}

// ---- goose arm --------------------------------------------------------------
static __device__ __forceinline__ float fg434c_ld_f32(uint64_t p, uint64_t o) {
    return *(const float *) ((const char *) (uintptr_t) p + o);
}
static __device__ __forceinline__ void fg434c_st_f32(uint64_t p, uint64_t o, float v) {
    *(float *) ((char *) (uintptr_t) p + o) = v;
}
static __device__ __forceinline__ void fg434c_sync(void) { __syncthreads(); }
static __device__ __forceinline__ void fg434c_fail(int64_t) { asm("trap;"); }
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_transpose(const char * cx, char * cdst,
        int64_t ne00, int64_t ne01, int64_t ne) {
    __shared__ float tile[2][32][33];
    FG434C_CB_transpose((uint64_t) (uintptr_t) cx, (uint64_t) (uintptr_t) cdst,
        ne00, ne01, ne, (int64_t) blockIdx.x, (int64_t) blockIdx.y, (int64_t) blockIdx.z,
        (int64_t) threadIdx.x, (int64_t) threadIdx.y, (uint64_t) (uintptr_t) tile, 0);
}

struct Case { int64_t ne00, ne01, nmat; const char * name; };

int main(void) {
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) { fprintf(stderr, "no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937 rng(434001);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);
    const Case cases[10] = {
        {32, 32, 1, "tile"}, {33, 32, 1, "tail-x"}, {32, 33, 1, "tail-y"},
        {100, 7, 1, "wide"}, {7, 100, 1, "tall"}, {1, 1, 1, "single"},
        {32, 32, 8, "mat8"}, {32, 32, 9, "mat9"}, {65, 65, 3, "multi-tail"},
        {17, 19, 5, "odd"},
    };
    int checks = 0, fails = 0, launch_fail = 0;
    for (int c = 0; c < 10; ++c) {
        const Case & k = cases[c];
        const int64_t ne = k.ne00 * k.ne01 * k.nmat;
        std::vector<float> h((size_t) ne);
        for (auto & v : h) v = dist(rng);
        float * dsrc = nullptr; float * dref = nullptr; float * dgo = nullptr;
        cudaMalloc(&dsrc, (size_t) ne * 4);
        cudaMalloc(&dref, (size_t) ne * 4);
        cudaMalloc(&dgo, (size_t) ne * 4);
        cudaMemcpy(dsrc, h.data(), (size_t) ne * 4, cudaMemcpyHostToDevice);
        cudaMemset(dref, 0x7F, (size_t) ne * 4);
        cudaMemset(dgo, 0x7F, (size_t) ne * 4);

        const int bs = 32;
        const dim3 grid((unsigned) ((k.ne01 + bs - 1) / bs), (unsigned) ((k.ne00 + bs - 1) / bs),
                        (unsigned) ((k.nmat + 7) / 8));
        const dim3 block(32, 8, 1);

        cpy_scalar_transpose<float><<<grid, block>>>((const char *) dsrc, (char *) dref, ne,
            k.ne00, k.ne01, 1, 4, k.ne01 * 4, k.ne00 * k.ne01 * 4, ne * 4, 1, 1, 1, 4,
            k.ne00 * 4, k.ne01 * k.ne00 * 4, ne * 4);
        cudaError_t e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { printf("LAUNCH FAIL native c=%d: %s\n", c, cudaGetErrorString(e)); ++launch_fail; }

        goose_transpose<<<grid, block>>>((const char *) dsrc, (char *) dgo, k.ne00, k.ne01, ne);
        e = cudaDeviceSynchronize();
        if (e != cudaSuccess) { printf("LAUNCH FAIL goose c=%d: %s\n", c, cudaGetErrorString(e)); ++launch_fail; }

        std::vector<float> a((size_t) ne), b((size_t) ne);
        cudaMemcpy(a.data(), dref, (size_t) ne * 4, cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(), dgo, (size_t) ne * 4, cudaMemcpyDeviceToHost);
        for (size_t i = 0; i < a.size(); ++i) {
            ++checks;
            if (memcmp(&a[i], &b[i], 4)) {
                ++fails;
                if (fails == 1) printf("FIRST FAIL c=%d i=%zu native=%08x goose=%08x\n", c, i,
                    *(unsigned *) &a[i], *(unsigned *) &b[i]);
            }
        }
        cudaFree(dsrc); cudaFree(dref); cudaFree(dgo);
    }
    printf("FG434C_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n", checks, fails, launch_fail);
    return (fails || launch_fail) ? 1 : 0;
}
