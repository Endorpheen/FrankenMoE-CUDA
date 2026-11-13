// EXP258 A/B: Q6_K MMA tile loader -- qs, df and sc staging, native vs Goose, same blocks.
#include "common.cuh"
#include "mmq.cuh"
#include "ggml-common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

static __device__ __forceinline__ void    fg258qs_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg258qs_ld_i32(uint64_t b,int64_t i){int32_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)i),4); return (int64_t)v;}
static __device__ __forceinline__ void    fg258qs_st_i32(uint64_t b,int64_t i,int64_t v){int32_t t=(int32_t)v; memcpy((void*)(uintptr_t)(b+(uint64_t)i),&t,4);}
static __device__ __forceinline__ void    fg258df_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ float   fg258df_ld_f16_f32(uint64_t b,int64_t i){ return (float) *(const __half *)(const void*)(uintptr_t)(b+(uint64_t)i); }
static __device__ __forceinline__ void    fg258df_st_f32(uint64_t b,int64_t i,float v){ memcpy((void*)(uintptr_t)(b+(uint64_t)i),&v,4); }
static __device__ __forceinline__ void    fg258sc_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg258sc_ld_i32(uint64_t b,int64_t i){int32_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)i),4); return (int64_t)v;}
static __device__ __forceinline__ void    fg258sc_st_i32(uint64_t b,int64_t i,int64_t v){int32_t t=(int32_t)v; memcpy((void*)(uintptr_t)(b+(uint64_t)i),&t,4);}
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "qs-functions-generated.cuh"
#include "df-functions-generated.cuh"
#include "sc-functions-generated.cuh"

#define NQ 4096
#define ND 512
#define NS 512

__global__ static void fg258_check(const char * x, int kbx0, int stride, int I, int sram_stride, int qi6k, int * bad) {
    extern __shared__ int sh[];
    int   * qs_n = sh;
    int   * qs_g = qs_n + NQ;
    float * df_n = (float *) (qs_g + NQ);
    float * df_g = df_n + ND;
    int   * sc_n = (int   *) (df_g + ND);
    int   * sc_g = sc_n + NS;

    for (int k = 0; k < NQ; ++k) { qs_n[k] = 0x7f7f7f7f; qs_g[k] = 0x7f7f7f7f; }
    for (int k = 0; k < ND; ++k) { int b = 0x7f7f7f7f; float f; memcpy(&f,&b,4); df_n[k] = f; df_g[k] = f; }
    for (int k = 0; k < NS; ++k) { sc_n[k] = 0x7f7f7f7f; sc_g[k] = 0x7f7f7f7f; }
    __syncthreads();

    const int tx = threadIdx.x;
    for (int i = 0; i < I; ++i) {
        {
            const block_q6_K * bxi = (const block_q6_K *) x + kbx0 + i*stride;
            const int ql = get_int_b2(bxi->ql, tx);
            const int ql0 = (ql >> 0) & 0x0F0F0F0F;
            const int ql1 = (ql >> 4) & 0x0F0F0F0F;
            const int qh = get_int_b2(bxi->qh, (qi6k/4) * (tx / (qi6k/2)) + tx % (qi6k/4));
            const int qh0 = ((qh >> ((tx & 0x08) >> 2)) << 4) & 0x30303030;
            const int qh1 =  (qh >> ((tx & 0x08) >> 2))       & 0x30303030;
            const int kq0 = 2*tx - tx % (qi6k/2) + 0;
            const int kq1 = 2*tx - tx % (qi6k/2) + qi6k/2;
            qs_n[i*sram_stride + kq0] = __vsub4(ql0 | qh0, 0x20202020);
            qs_n[i*sram_stride + kq1] = __vsub4(ql1 | qh1, 0x20202020);
        }
        fg258qs_run((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) qs_g,
                    kbx0, stride, i, tx, sram_stride, qi6k);

        {
            const block_q6_K * bxi = (const block_q6_K *) x + kbx0 + i*stride;
            df_n[i*sram_stride] = bxi->d;
        }
        fg258df_run((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) df_g,
                    kbx0, stride, i, sram_stride);

        {
            const block_q6_K * bxi = (const block_q6_K *) x + kbx0 + i*stride + (tx % (MMQ_TILE_NE_K/8)) / 4;
            sc_n[i*sram_stride + tx%4] = get_int_b2(bxi->scales, tx % (MMQ_TILE_NE_K/8));
        }
        fg258sc_run((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) sc_g,
                    kbx0, stride, i, sram_stride, tx, 32);
    }
    __syncthreads();

    if (tx == 0) {
        for (int k = 0; k < NQ; ++k) if (qs_n[k] != qs_g[k]) atomicAdd(bad, 1);
        for (int k = 0; k < ND; ++k) { int a, b; memcpy(&a,&df_n[k],4); memcpy(&b,&df_g[k],4); if (a != b) atomicAdd(bad, 1); }
        for (int k = 0; k < NS; ++k) if (sc_n[k] != sc_g[k]) atomicAdd(bad, 1);
    }
}

int main() {
    const int I = 8, sram_stride = 64, qi6k = 32;
    std::vector<block_q6_K> x(I);
    std::mt19937 rng(258);
    for (auto & b : x) { for (auto & q : b.ql) q = (uint8_t) rng(); for (auto & q : b.qh) q = (uint8_t) rng();
                         for (auto & s2 : b.scales) s2 = (int8_t) rng();
                         uint16_t d = (uint16_t) rng(); memcpy(&b.d, &d, 2); }
    block_q6_K * dx = nullptr; cudaMalloc(&dx, x.size()*sizeof(block_q6_K));
    cudaMemcpy(dx, x.data(), x.size()*sizeof(block_q6_K), cudaMemcpyHostToDevice);
    int * bad = nullptr; cudaMalloc(&bad, sizeof(int)); cudaMemset(bad, 0, sizeof(int));
    size_t shmem = (NQ*2 + ND*2 + NS*2) * sizeof(int);
    fg258_check<<<1, 32, shmem>>>((const char *) dx, 0, 1, I, sram_stride, qi6k, bad);
    cudaError_t e = cudaDeviceSynchronize();
    int hb = 0; cudaMemcpy(&hb, bad, sizeof(int), cudaMemcpyDeviceToHost);
    printf("FG258_AB mismatches=%d cuda=%d shmem=%zu\n", hb, (int) e, shmem);
    return hb == 0 && e == cudaSuccess ? 0 : 1;
}
