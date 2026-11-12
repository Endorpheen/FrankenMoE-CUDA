// Differential fixture for the Goose reduce_rows port.
//
// The oracle is the actual native template from reduce_rows.cuh, included
// directly. Both arms get the same launch geometry and the same data, and the
// destination floats are compared bit for bit. The cases span ncols 1..4096,
// both block-size branches of the occupancy heuristic, sum and mean, and both
// the contiguous and the strided kernel.
#include "reduce_rows.cuh"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>

extern "C" void ggml_abort(const char * file, int line, const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ggml_abort at %s:%d: ", file, line);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}

// ---- goose arm --------------------------------------------------------------
static __device__ __forceinline__ float fg414d_ld_f32(uint64_t p, uint64_t o) {
    return *(const float *) ((const char *) (uintptr_t) p + o);
}
static __device__ __forceinline__ void fg414d_st_f32(uint64_t p, uint64_t o, float v) {
    *(float *) ((char *) (uintptr_t) p + o) = v;
}
static __device__ __forceinline__ float fg414d_shfl_xor_f32(float v, int64_t m) {
    return __shfl_xor_sync(0xffffffffu, v, (int) m, 32);
}
static __device__ __forceinline__ void fg414d_sync(void) { __syncthreads(); }
static __device__ __forceinline__ void fg414d_fail(int64_t) { asm("trap;"); }
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_reduce(const char * base, float * dst,
        int64_t ncols, int64_t norm, int64_t strided,
        int64_t ne1, int64_t ne2, int64_t nb1, int64_t nb2, int64_t nb3) {
    __shared__ float shared[32];
    FG414D_CB_reduce_entry((uint64_t) (uintptr_t) base, (uint64_t) (uintptr_t) dst,
        (int64_t) blockIdx.x, ncols, norm, strided, ne1, ne2, nb1, nb2, nb3,
        (int64_t) threadIdx.x, (int64_t) blockDim.x,
        (uint64_t) (uintptr_t) shared, 0);
}

struct Case {
    int64_t ncols, nrows, nsm;
    int64_t ne1, ne2, ne3, pad_row, pad_plane;
    int norm, strided;
    const char * name;
};

int main(void) {
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) {
        fprintf(stderr, "no CUDA device\n");
        return 77;
    }
    cudaSetDevice(0);

    const Case cases[12] = {
        {1,    1,   108, 1, 1, 1, 0, 0, 0, 0, "one-element"},
        {31,   4,   108, 1, 1, 1, 0, 0, 0, 0, "short-row"},
        {127,  17,  108, 1, 1, 1, 0, 0, 0, 0, "sub-warp-row"},
        {1024, 500, 100, 1, 1, 1, 0, 0, 0, 0, "many-rows-wide"},
        {1000, 500, 100, 1, 1, 1, 0, 0, 0, 0, "many-rows-narrow"},
        {4096, 8,   108, 1, 1, 1, 0, 0, 0, 0, "wide-few-rows"},
        {777,  300, 100, 1, 1, 1, 0, 0, 1, 0, "mean-many-rows"},
        {513,  12,  108, 1, 1, 1, 0, 0, 1, 0, "mean-wide-block"},
        {300,  12,  108, 3, 2, 2, 5, 2, 0, 1, "strided-sum"},
        {1024, 15,  100, 5, 3, 1, 3, 1, 1, 1, "strided-mean"},
        {512,  400, 100, 40, 10, 1, 1, 0, 0, 1, "strided-small-block"},
        {2048, 500, 100, 50, 10, 1, 2, 0, 1, 1, "strided-medium-block"},
    };

    int checks = 0, fails = 0, launch_fail = 0;
    for (int c = 0; c < 12; ++c) {
        const Case & k = cases[c];
        const int64_t nrows = k.strided ? (k.ne1 * k.ne2 * k.ne3) : k.nrows;
        const int64_t nb1 = (k.ncols + k.pad_row) * 4;
        const int64_t nb2 = k.strided ? nb1 * k.ne1 + k.pad_plane * 4 : k.ncols * 4 * k.ne1;
        const int64_t nb3 = k.strided ? nb2 * k.ne2 : nrows * k.ncols * 4;
        const size_t src_bytes = k.strided ? (size_t) (nb3 * k.ne3) + 64
                                           : (size_t) (nrows * k.ncols * 4) + 64;

        std::vector<unsigned char> hsrc(src_bytes, 0xA5);
        uint32_t rng = 0x9e3779b9u + (uint32_t) c;
        for (int64_t i3 = 0; i3 < (k.strided ? k.ne3 : 1); ++i3)
        for (int64_t i2 = 0; i2 < (k.strided ? k.ne2 : 1); ++i2)
        for (int64_t i1 = 0; i1 < (k.strided ? k.ne1 : nrows); ++i1)
        for (int64_t i0 = 0; i0 < k.ncols; ++i0) {
            const int64_t off = k.strided
                ? i3 * nb3 + i2 * nb2 + i1 * nb1 + i0 * 4
                : (i1 * k.ncols + i0) * 4;
            float v;
            if (c % 3 == 0) {
                v = 1.0f;
            } else if (c % 3 == 1) {
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                v = (float) ((int32_t) (rng >> 8)) * 1.0e-3f;
            } else {
                v = (float) (((uint64_t) i1 * 1315423911u + (uint64_t) i0 * 2654435761u) >> 9) * 1.0e-4f - 900.0f;
            }
            memcpy(&hsrc[(size_t) off], &v, 4);
        }

        char * dsrc = nullptr;
        float * dref = nullptr;
        float * dgo = nullptr;
        cudaMalloc(&dsrc, src_bytes);
        cudaMalloc(&dref, (size_t) nrows * 4);
        cudaMalloc(&dgo, (size_t) nrows * 4);
        cudaMemcpy(dsrc, hsrc.data(), src_bytes, cudaMemcpyHostToDevice);
        cudaMemset(dref, 0x7F, (size_t) nrows * 4);
        cudaMemset(dgo, 0x7F, (size_t) nrows * 4);

        int64_t bdx;
        if ((nrows / k.nsm) < 2) {
            bdx = 512;
        } else {
            bdx = k.ncols < 1024 ? 32 : 128;
        }
        const dim3 grid((unsigned) nrows, 1, 1);
        const dim3 block((unsigned) bdx, 1, 1);

        if (k.strided) {
            const char * base = (const char *) dsrc;
            if (k.norm) {
                reduce_rows_f32_strided<true><<<grid, block>>>(base, dref, (int) k.ncols,
                    k.ne1, k.ne2, nb1, nb2, nb3);
            } else {
                reduce_rows_f32_strided<false><<<grid, block>>>(base, dref, (int) k.ncols,
                    k.ne1, k.ne2, nb1, nb2, nb3);
            }
        } else {
            const float * base = (const float *) dsrc;
            if (k.norm) {
                reduce_rows_f32<true><<<grid, block>>>(base, dref, (int) k.ncols);
            } else {
                reduce_rows_f32<false><<<grid, block>>>(base, dref, (int) k.ncols);
            }
        }
        cudaError_t e = cudaDeviceSynchronize();
        if (e != cudaSuccess) {
            printf("LAUNCH FAIL native c=%d: %s\n", c, cudaGetErrorString(e));
            ++launch_fail;
        }

        goose_reduce<<<grid, block>>>((const char *) dsrc, dgo, k.ncols,
            k.norm, k.strided, k.ne1, k.ne2, nb1, nb2, nb3);
        e = cudaDeviceSynchronize();
        if (e != cudaSuccess) {
            printf("LAUNCH FAIL goose c=%d: %s\n", c, cudaGetErrorString(e));
            ++launch_fail;
        }

        std::vector<float> a((size_t) nrows), b((size_t) nrows);
        cudaMemcpy(a.data(), dref, (size_t) nrows * 4, cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(), dgo, (size_t) nrows * 4, cudaMemcpyDeviceToHost);
        for (int64_t i = 0; i < nrows; ++i) {
            ++checks;
            if (memcmp(&a[(size_t) i], &b[(size_t) i], 4)) {
                ++fails;
                if (fails == 1) {
                    printf("FIRST FAIL c=%d row=%lld native=%08x goose=%08x (%g vs %g)\n",
                           c, (long long) i, *(unsigned *) &a[(size_t) i], *(unsigned *) &b[(size_t) i],
                           a[(size_t) i], b[(size_t) i]);
                }
            }
        }
        cudaFree(dsrc);
        cudaFree(dref);
        cudaFree(dgo);
    }
    printf("FG414D_GPU_FIXTURE rows=%d failures=%d launch_fail=%d\n", checks, fails, launch_fail);
    return (fails || launch_fail) ? 1 : 0;
}
