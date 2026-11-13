// EXP259 A/B: Q5_K MMA tile loader -- qs staging and dm rows, native vs Goose, same blocks.
#include "common.cuh"
#include "mmq.cuh"
#include "ggml-common.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

static __device__ __forceinline__ void    fg259q_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg259q_ld_i32(uint64_t b,int64_t i){int32_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)i),4); return (int64_t)v;}
static __device__ __forceinline__ void    fg259q_st_i32(uint64_t b,int64_t i,int64_t v){int32_t t=(int32_t)v; memcpy((void*)(uintptr_t)(b+(uint64_t)i),&t,4);}
static __device__ __forceinline__ void    fg259m_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg259m_ld_i32(uint64_t b,int64_t i){int32_t v; memcpy(&v,(const void*)(uintptr_t)(b+(uint64_t)i),4); return (int64_t)v;}
static __device__ __forceinline__ void    fg259m_st_i32(uint64_t b,int64_t i,int64_t v){int32_t t=(int32_t)v; memcpy((void*)(uintptr_t)(b+(uint64_t)i),&t,4);}
static __device__ __forceinline__ int64_t fg259m_unpack_scales(uint64_t s,int64_t k){return (int64_t) unpack_scales_q45_K((const int*)(uintptr_t)s,(int)k);}
static __device__ __forceinline__ int64_t fg259m_dm_scale(int64_t dm_bits,int64_t a,int64_t b){
    half2 dm = *(const half2*)&dm_bits;
    dm = dm * make_half2(1.0f, -1.0f);
    half2 v = dm * make_half2((float)a,(float)b);
    int32_t out; memcpy(&out,&v,4); return (int64_t)out;
}
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "qs5-functions-generated.cuh"
#include "dm5-functions-generated.cuh"

#define NQ 4096
#define ND 512

__global__ static void fg259_check(const char * x, int kbx0, int stride, int I, int sram_stride, int * bad) {
    extern __shared__ int sh[];
    int * qs_n = sh;
    int * qs_g = qs_n + NQ;
    int * dm_n = qs_g + NQ;
    int * dm_g = dm_n + ND;

    for (int k = 0; k < NQ; ++k) { qs_n[k] = 0x7f7f7f7f; qs_g[k] = 0x7f7f7f7f; }
    for (int k = 0; k < ND; ++k) { dm_n[k] = 0x7f7f7f7f; dm_g[k] = 0x7f7f7f7f; }
    __syncthreads();

    const int txi = threadIdx.x;
    for (int i = 0; i < I; ++i) {
        {
            const block_q5_K * bxi = (const block_q5_K *) x + kbx0 + i*stride;
            const int ky = QR5_K*txi;
            const int ql = get_int_b4(bxi->qs, txi);
            const int ql0 = (ql >> 0) & 0x0F0F0F0F;
            const int ql1 = (ql >> 4) & 0x0F0F0F0F;
            const int qh = get_int_b4(bxi->qh, txi % (QI5_K/4));
            const int qh0 = ((qh >> (2 * (txi / (QI5_K/4)) + 0)) << 4) & 0x10101010;
            const int qh1 = ((qh >> (2 * (txi / (QI5_K/4)) + 1)) << 4) & 0x10101010;
            const int kq0 = ky - ky % (QI5_K/2) + txi % (QI5_K/4) + 0;
            const int kq1 = ky - ky % (QI5_K/2) + txi % (QI5_K/4) + QI5_K/4;
            qs_n[i*sram_stride + kq0] = ql0 | qh0;
            qs_n[i*sram_stride + kq1] = ql1 | qh1;
        }
        fg259q_run((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) qs_g,
                   kbx0, stride, i, txi, sram_stride, QR5_K, QI5_K);

        {
            const block_q5_K * bxi = (const block_q5_K *) x + kbx0 + i*stride;
            const int * scales = (const int *) bxi->scales;
            const int ksc = threadIdx.x % 2;
            const int sc32 = unpack_scales_q45_K(scales, ksc + 0);
            const int  m32 = unpack_scales_q45_K(scales, ksc + 2);
            const uint8_t * sc8 = (const uint8_t *) &sc32;
            const uint8_t *  m8 = (const uint8_t *)  &m32;
            const half2 dm = bxi->dm * make_half2(1.0f, -1.0f);
            #pragma unroll
            for (int l = 0; l < int(sizeof(int)); ++l) {
                half2 v = dm*make_half2(sc8[l], m8[l]);
                int vi; memcpy(&vi, &v, 4);
                dm_n[i*sram_stride + sizeof(int)*ksc + l] = vi;
            }
        }
        fg259m_run((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) dm_g,
                   kbx0, stride, i, threadIdx.x, sram_stride);
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        for (int k = 0; k < NQ; ++k) if (qs_n[k] != qs_g[k]) atomicAdd(bad, 1);
        for (int k = 0; k < ND; ++k) if (dm_n[k] != dm_g[k]) atomicAdd(bad, 1);
    }
}


int main() {
    const int I = 8, sram_stride = 64;
    std::vector<block_q5_K> x(I);
    std::mt19937 rng(259);
    for (auto & b : x) {
        uint16_t d[2] = { (uint16_t) rng(), (uint16_t) rng() }; memcpy(&b.dm, d, 4);
        for (auto & s2 : b.scales) s2 = (uint8_t) rng();
        for (auto & q : b.qh) q = (uint8_t) rng();
        for (auto & q : b.qs) q = (uint8_t) rng();
    }
    block_q5_K * dx = nullptr; cudaMalloc(&dx, x.size()*sizeof(block_q5_K));
    cudaMemcpy(dx, x.data(), x.size()*sizeof(block_q5_K), cudaMemcpyHostToDevice);
    int * bad = nullptr; cudaMalloc(&bad, sizeof(int)); cudaMemset(bad, 0, sizeof(int));
    size_t shmem = (NQ*2 + ND*2) * sizeof(int);
    fg259_check<<<1, 32, shmem>>>((const char *) dx, 0, 1, I, sram_stride, bad);
    cudaError_t e = cudaDeviceSynchronize();
    int hb = 0; cudaMemcpy(&hb, bad, sizeof(int), cudaMemcpyDeviceToHost);
    printf("FG259_AB mismatches=%d cuda=%d shmem=%zu\n", hb, (int) e, shmem);
    return hb == 0 && e == cudaSuccess ? 0 : 1;
}
