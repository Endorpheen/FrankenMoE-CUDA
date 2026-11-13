// Differential fixture for the Goose pad_reflect_1d port.
//
// The oracle is a verbatim copy of pad_reflect_1d_kernel_f32 from
// ggml-cuda/pad_reflect_1d.cu, including fast_div_modulo. The Goose arm runs
// the generated device functions through the same launch geometry. Both arms
// receive identically padded source and destination buffers, and the whole
// destination buffer is compared bit for bit, so a wrong stride or a wrong
// reflection shows up as a value difference or as a sentinel that moved.
#include "common.cuh"
#include <cstdarg>
#include <cstdlib>

extern "C" void ggml_abort(const char * file, int line, const char * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ggml_abort at %s:%d: ", file, line);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
}
#include <cstdio>
#include <cstring>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <cstdint>

#define CUDA_PAD_REFLECT_1D_BLOCK_SIZE 256

// ---- oracle: verbatim body from ggml-cuda/pad_reflect_1d.cu -----------------
static __global__ __launch_bounds__(CUDA_PAD_REFLECT_1D_BLOCK_SIZE, 1) void
    pad_reflect_1d_kernel_f32(
        const void * __restrict__ src0,
        void * __restrict__       dst,
        const int64_t             ne0,
        const int64_t             ne00,
        const uint3               ne01,
        const int64_t             ne02,
        const int64_t             ne03,
        const int64_t             nb00,
        const int64_t             nb01,
        const int64_t             nb02,
        const int64_t             nb03,
        const int64_t             nb0,
        const int64_t             nb1,
        const int64_t             nb2,
        const int64_t             nb3,
        const int                 p0,
        const int                 p1) {
    const int64_t i3 = blockIdx.z;
    const int64_t i2 = blockIdx.y;

    const uint2   div_mod_packed = fast_div_modulo(blockIdx.x, ne01);
    const int64_t tile1          = div_mod_packed.y;  // i1
    const int64_t tile0          = div_mod_packed.x;  // nth i0 tile
    const int64_t i1             = tile1;
    const int64_t i0             = threadIdx.x + tile0 * blockDim.x;

    if (i0 >= ne0 || i1 >= ne01.z || i2 >= ne02 || i3 >= ne03) {
        return;
    }

    const char * src0_ptr = (const char *) src0 + i3 * nb03 + i2 * nb02 + i1 * nb01;
    char *       dst_ptr  = (char *) dst + i3 * nb3 + i2 * nb2 + i1 * nb1;

    const int64_t rel_i0 = i0 - p0;
    int64_t src_idx;

    if (rel_i0 < 0) {
        src_idx = -rel_i0;
    } else if (rel_i0 < ne00) {
        src_idx = rel_i0;
    } else {
        src_idx = 2 * ne00 - 2 - rel_i0;
    }
    const float value               = *(const float *) (src0_ptr + src_idx * nb00);
    *(float *) (dst_ptr + i0 * nb0) = value;

    GGML_UNUSED(p1);
}

// ---- goose arm --------------------------------------------------------------
static __device__ __forceinline__ float fg413d_ld_f32(uint64_t p, uint64_t o) {
    return *(const float *) ((const char *) (uintptr_t) p + o);
}
static __device__ __forceinline__ void fg413d_st_f32(uint64_t p, uint64_t o, float v) {
    *(float *) ((char *) (uintptr_t) p + o) = v;
}
static __device__ __forceinline__ void fg413d_fail(int64_t) { asm("trap;"); }
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_pad(const char * src, char * dst,
        int64_t ne0, int64_t ne00, int64_t ne01, int64_t ne02, int64_t ne03,
        int64_t nb00, int64_t nb01, int64_t nb02, int64_t nb03,
        int64_t nb0, int64_t nb1, int64_t nb2, int64_t nb3, int64_t p0) {
    FG413D_CB_pad_elem((uint64_t) (uintptr_t) src, (uint64_t) (uintptr_t) dst,
        ne0, ne00, ne01, ne02, ne03, nb00, nb01, nb02, nb03, nb0, nb1, nb2, nb3,
        p0, (int64_t) blockIdx.x, (int64_t) blockIdx.y, (int64_t) blockIdx.z,
        (int64_t) threadIdx.x, (int64_t) blockDim.x, 0);
}

struct Case {
    int64_t ne00, p0, p1, ne01, ne02, ne03;
    int64_t pad_src_row, pad_dst_row;
    const char * name;
};

int main(void) {
    int ndev = 0;
    if (cudaGetDeviceCount(&ndev) != cudaSuccess || ndev < 1) {
        fprintf(stderr, "no CUDA device\n");
        return 77;
    }
    cudaSetDevice(0);

    const Case cases[6] = {
        {256, 0, 0, 1, 1, 1, 0, 0, "identity"},
        {300, 2, 3, 4, 2, 3, 0, 0, "both-pads"},
        {64, 5, 0, 7, 1, 1, 3, 2, "left-tail"},
        {1, 0, 0, 1, 1, 1, 0, 0, "single"},
        {1000, 10, 7, 3, 2, 2, 5, 9, "multi-plane-padded"},
        {33, 1, 1, 2, 1, 1, 1, 1, "small-tail"},
    };

    int checks = 0, fails = 0, launch_fail = 0;
    for (int c = 0; c < 6; ++c) {
        const Case & k = cases[c];
        const int64_t ne0 = k.ne00 + k.p0 + k.p1;
        const int64_t nb00 = 4;
        const int64_t nb01 = (k.ne00 + k.pad_src_row) * 4;
        const int64_t nb02 = nb01 * k.ne01;
        const int64_t nb03 = nb02 * k.ne02;
        const int64_t nb0 = 4;
        const int64_t nb1 = (ne0 + k.pad_dst_row) * 4;
        const int64_t nb2 = nb1 * k.ne01;
        const int64_t nb3 = nb2 * k.ne02;
        const size_t src_bytes = (size_t) (nb03 * k.ne03) + 64;
        const size_t dst_bytes = (size_t) (nb3 * k.ne03) + 64;

        std::vector<unsigned char> hsrc(src_bytes, 0xA5);
        std::vector<unsigned char> hsent(dst_bytes, 0x5A);
        for (int64_t i3 = 0; i3 < k.ne03; ++i3)
        for (int64_t i2 = 0; i2 < k.ne02; ++i2)
        for (int64_t i1 = 0; i1 < k.ne01; ++i1)
        for (int64_t i0 = 0; i0 < k.ne00; ++i0) {
            const int64_t off = i3 * nb03 + i2 * nb02 + i1 * nb01 + i0 * nb00;
            const float v = (float) (i3 * 1000000 + i2 * 10000 + i1 * 1000 + i0) * 0.25f;
            memcpy(&hsrc[(size_t) off], &v, 4);
        }

        char * dsrc = nullptr;
        char * dref = nullptr;
        char * dgo = nullptr;
        cudaMalloc(&dsrc, src_bytes);
        cudaMalloc(&dref, dst_bytes);
        cudaMalloc(&dgo, dst_bytes);
        cudaMemcpy(dsrc, hsrc.data(), src_bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(dref, hsent.data(), dst_bytes, cudaMemcpyHostToDevice);
        cudaMemcpy(dgo, hsent.data(), dst_bytes, cudaMemcpyHostToDevice);

        const int64_t tiles0 = (ne0 + CUDA_PAD_REFLECT_1D_BLOCK_SIZE - 1) / CUDA_PAD_REFLECT_1D_BLOCK_SIZE;
        const dim3 grid((unsigned) (k.ne01 * tiles0), (unsigned) k.ne02, (unsigned) k.ne03);
        const dim3 block(CUDA_PAD_REFLECT_1D_BLOCK_SIZE, 1, 1);
        const uint3 ne01_packed = init_fastdiv_values(k.ne01);

        pad_reflect_1d_kernel_f32<<<grid, block>>>(dsrc, dref, ne0, k.ne00, ne01_packed,
            k.ne02, k.ne03, nb00, nb01, nb02, nb03, nb0, nb1, nb2, nb3,
            (int) k.p0, (int) k.p1);
        cudaError_t e = cudaDeviceSynchronize();
        if (e != cudaSuccess) {
            printf("LAUNCH FAIL native c=%d: %s\n", c, cudaGetErrorString(e));
            ++launch_fail;
        }

        goose_pad<<<grid, block>>>(dsrc, dgo, ne0, k.ne00, k.ne01, k.ne02, k.ne03,
            nb00, nb01, nb02, nb03, nb0, nb1, nb2, nb3, k.p0);
        e = cudaDeviceSynchronize();
        if (e != cudaSuccess) {
            printf("LAUNCH FAIL goose c=%d: %s\n", c, cudaGetErrorString(e));
            ++launch_fail;
        }

        std::vector<unsigned char> a(dst_bytes), b(dst_bytes);
        cudaMemcpy(a.data(), dref, dst_bytes, cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(), dgo, dst_bytes, cudaMemcpyDeviceToHost);
        for (size_t i = 0; i < dst_bytes; ++i) {
            ++checks;
            if (a[i] != b[i]) {
                ++fails;
                if (fails == 1) {
                    printf("FIRST FAIL c=%d byte=%zu native=%02x goose=%02x\n",
                           c, i, a[i], b[i]);
                }
            }
        }
        cudaFree(dsrc);
        cudaFree(dref);
        cudaFree(dgo);
    }
    printf("FG413D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n", checks, fails, launch_fail);
    return (fails || launch_fail) ? 1 : 0;
}
