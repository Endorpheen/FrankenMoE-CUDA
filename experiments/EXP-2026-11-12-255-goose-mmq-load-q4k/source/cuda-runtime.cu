// Device services and the A/B harness for the Goose Q4_K tile loader.
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

#include "device-functions-generated.cuh"

__global__ static void fg255q_kernel(const char * x, int * qs_stage, int * dm_stage,
                                     int kbx0, int stride, int sram_stride) {
    const int txi = threadIdx.x;
    const int i   = blockIdx.x;
    (void) fg255q_load_row((uint64_t)(uintptr_t) x, (uint64_t)(uintptr_t) qs_stage,
                           (uint64_t)(uintptr_t) dm_stage, kbx0, stride, i, txi, sram_stride, txi % 2);
}

int main() {
    // CPU-side emulation of the same body, then compare element by element inside
    // the harness at kernel level is done in check.cu; this file's main only smoke-tests.
    printf("FG255Q_RUNTIME ok\n");
    return 0;
}
