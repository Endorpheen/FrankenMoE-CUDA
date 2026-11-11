// EXP257 A/B: the Q6_K qs staging, native vs Goose, for the same blocks.
#include "common.cuh"
#include "mmq.cuh"
#include "ggml-common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

static __device__ __forceinline__ void     fg257d_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t  fg257d_ld_i32(uint64_t b,int64_t i){int32_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)i),4); return (int64_t)v;}
static __device__ __forceinline__ void     fg257d_st_i32(uint64_t b,int64_t i,int64_t v){int32_t t=(int32_t)v; memcpy((void*)(uintptr_t)(b+(uint64_t)i),&t,4);}
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"

__global__ static void fg257d_check(const char * x, int kbx0, int stride, int I, int sram_stride, int qi6k, int * bad) {
    extern __shared__ int sh[];
    int * qs_n = sh;
    int * qs_g = qs_n + 64*64;
    const int txi = threadIdx.x;
    for (int i = 0; i < I; ++i) {
        for (int k = 0; k < 64*64; ++k) { qs_n[k] = 0x7f7f7f7f; qs_g[k] = 0x7f7f7f7f; }
        __syncthreads();
        {
            const block_q6_K * bxi = (const block_q6_K *) x + kbx0 + i*stride;
            const int ql = get_int_b2(bxi->ql, txi);
            const int ql0 = (ql >> 0) & 0x0F0F0F0F;
            const int ql1 = (ql >> 4) & 0x0F0F0F0F;
            const int qh = get_int_b2(bxi->qh, (qi6k/4) * (txi / (qi6k/2)) + txi % (qi6k/4));
            const int qh0 = ((qh >> ((txi & 0x08) >> 2)) << 4) & 0x30303030;
            const int qh1 =  (qh >> ((txi & 0x08) >> 2))       & 0x30303030;
            const int kq0 = 2*txi - txi % (qi6k/2) + 0;
            const int kq1 = 2*txi - txi % (qi6k/2) + qi6k/2;
            qs_n[i*sram_stride + kq0] = __vsub4(ql0 | qh0, 0x20202020);
            qs_n[i*sram_stride + kq1] = __vsub4(ql1 | qh1, 0x20202020);
        }
        (void) fg257d_gg_function_1((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) qs_g,
                                    kbx0, stride, i, txi, sram_stride, qi6k, 0);
        __syncthreads();
        for (int k = 0; k < 64*64; ++k) if (qs_n[k] != qs_g[k]) atomicAdd(bad, 1);
        __syncthreads();
    }
}

int main() {
    const int I = 8, sram_stride = 64, qi6k = 32;
    std::vector<block_q6_K> x(I);
    std::mt19937 rng(257);
    for (auto & b : x) { for (auto & q : b.ql) q = (uint8_t) rng(); for (auto & q : b.qh) q = (uint8_t) rng();
                         for (auto & s2 : b.scales) s2 = (int8_t) rng(); }
    block_q6_K * dx = nullptr; cudaMalloc(&dx, x.size()*sizeof(block_q6_K));
    cudaMemcpy(dx, x.data(), x.size()*sizeof(block_q6_K), cudaMemcpyHostToDevice);
    int * bad = nullptr; cudaMalloc(&bad, sizeof(int)); cudaMemset(bad, 0, sizeof(int));
    size_t shmem = (64*64*2) * sizeof(int);
    fg257d_check<<<1, 32, shmem>>>((const char *) dx, 0, 1, I, sram_stride, qi6k, bad);
    cudaError_t e = cudaDeviceSynchronize();
    int hb = 0; cudaMemcpy(&hb, bad, sizeof(int), cudaMemcpyDeviceToHost);
    printf("FG257D_AB mismatches=%d cuda=%d\n", hb, (int) e);
    return hb == 0 && e == cudaSuccess ? 0 : 1;
}
