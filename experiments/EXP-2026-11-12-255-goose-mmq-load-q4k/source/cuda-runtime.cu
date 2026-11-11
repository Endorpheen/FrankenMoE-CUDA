// EXP255 A/B: one thread computes the native and the Goose Q4_K tile staging for the same block.
#include "common.cuh"
#include "mmq.cuh"
#include "ggml-common.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <algorithm>

static __device__ __forceinline__ void  fg255q_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg255q_ld_i32(uint64_t b,int64_t i){return (int64_t)((const int32_t*)(uintptr_t)b)[i];}
static __device__ __forceinline__ void  fg255q_st_i32(uint64_t b,int64_t i,int64_t v){((int32_t*)(uintptr_t)b)[i]=(int32_t)v;}
static __device__ __forceinline__ int64_t fg255q_unpack_scales(uint64_t s,int64_t k){return (int64_t) unpack_scales_q45_K((const int*)(uintptr_t)s,(int)k);}
static __device__ __forceinline__ int64_t fg255q_dm_scale(int64_t dm_bits,int64_t a,int64_t b){
    half2 dm = *(const half2*)&dm_bits;
    half2 v = dm * make_half2((float)a,(float)b);
    int32_t out; memcpy(&out,&v,4); return (int64_t)out;
}
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"

static __device__ __forceinline__ void native_load(const char * x, int * qs_stage, half2 * dm_stage,
                                                   int kbx0, int stride, int i, int txi, int sram_stride) {
    const block_q4_K * bxi = (const block_q4_K *) x + kbx0 + i*stride;
    const int qs0 = get_int_b4(bxi->qs, txi);
    qs_stage[i*sram_stride + 16*(txi/8) + txi % 8 + 0] = (qs0 >> 0) & 0x0F0F0F0F;
    qs_stage[i*sram_stride + 16*(txi/8) + txi % 8 + 8] = (qs0 >> 4) & 0x0F0F0F0F;

    const int * scales = (const int *) bxi->scales;
    const int ksc = txi % 2;
    const int sc32 = unpack_scales_q45_K(scales, ksc + 0);
    const int  m32 = unpack_scales_q45_K(scales, ksc + 2);
    const uint8_t * sc8 = (const uint8_t *) &sc32;
    const uint8_t *  m8 = (const uint8_t *)  &m32;
    const half2 dm = bxi->dm * make_half2(1.0f, -1.0f);
    #pragma unroll
    for (int l = 0; l < 4; ++l) {
        dm_stage[i*sram_stride + 4*ksc + l] = dm*make_half2(sc8[l], m8[l]);
    }
}

__global__ static void fg255q_check(const char * x, int kbx0, int stride, int I, int sram_stride, int * bad) {
    extern __shared__ int sh[];
    int * qs_n = sh;               // 64*32 ints
    int * qs_g = qs_n + 64*32;
    half2 * dm_n = (half2*)(qs_g + 64*32);
    half2 * dm_g = dm_n + 64*16;
    const int txi = threadIdx.x;
    for (int i = 0; i < I; ++i) {
        for (int k = 0; k < 64*32; ++k) { qs_n[k] = 0x7f7f7f7f; qs_g[k] = 0x7f7f7f7f; }
        for (int k = 0; k < 64*16; ++k) { dm_n[k] = make_half2(1.5f,1.5f); dm_g[k] = make_half2(1.5f,1.5f); }
        __syncthreads();
        native_load(x, qs_n, dm_n, kbx0, stride, i, txi, sram_stride);
        (void) fg255qd_gg_function_1((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) qs_g, (uint64_t)(uintptr_t) dm_g,
                                     kbx0, stride, i, txi, sram_stride, txi % 2, 0);
        __syncthreads();
        for (int k = 0; k < 64*32; ++k) if (qs_n[k] != qs_g[k]) atomicAdd(bad, 1);
        for (int k = 0; k < 64*16; ++k) { int a,b; memcpy(&a,&dm_n[k],4); memcpy(&b,&dm_g[k],4); if (a != b) atomicAdd(bad, 1); }
        __syncthreads();
    }
}

int main() {
    const int I = 8, sram_stride = 64;
    std::vector<block_q4_K> x(I);
    std::mt19937 rng(255);
    for (auto & b : x) {
        for (auto & q : b.qs) q = (uint8_t) rng();
        for (auto & s2 : b.scales) s2 = (uint8_t) rng();
        b.dm = make_half2(0.5f, 0.25f);
    }
    int * bad = nullptr; cudaMalloc(&bad, sizeof(int)); cudaMemset(bad, 0, sizeof(int));
    block_q4_K * dx = nullptr;
    cudaMalloc(&dx, x.size() * sizeof(block_q4_K));
    cudaMemcpy(dx, x.data(), x.size() * sizeof(block_q4_K), cudaMemcpyHostToDevice);
    size_t shmem = (64*32*2 + 64*16*2) * sizeof(int);
    fg255q_check<<<1, 32, shmem>>>((const char *) dx, 0, 1, I, sram_stride, bad);
    cudaError_t e = cudaDeviceSynchronize();
    int hb = 0; cudaMemcpy(&hb, bad, sizeof(int), cudaMemcpyDeviceToHost);
    printf("FG255Q_AB mismatches=%d cuda=%d\n", hb, (int) e);
    return hb == 0 ? 0 : 1;
}
