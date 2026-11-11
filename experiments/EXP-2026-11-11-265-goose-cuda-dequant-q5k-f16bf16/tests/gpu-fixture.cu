// Differential fixture for Q6_K dequantization to half and bfloat16.
#include "common.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <string>

static __device__ __forceinline__ uint32_t fg265d_ld_u8(uint64_t p,uint64_t o){ return *(const uint8_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ int32_t  fg265d_ld_i8(uint64_t p,uint64_t o){ return (int32_t)*(const int8_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float    fg265d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ void     fg265d_st_half(uint64_t p,uint64_t o,float v){ *(half*)((char*)(uintptr_t)p+o) = __float2half(v); }
static __device__ __forceinline__ void     fg265d_st_bf16(uint64_t p,uint64_t o,float v){ *(nv_bfloat16*)((char*)(uintptr_t)p+o) = __float2bfloat16(v); }
static __device__ __forceinline__ void     fg265d_fail(int64_t){ asm("trap;"); }
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void native_f16(const void * vx, half * yy){
    const int64_t i = blockIdx.x;
    dequantize_q5_K<half>(vx, i, yy + i*QK_K, threadIdx.x);
}
static __global__ void goose_f16(const void * vx, half * yy){
    FG265D_CB_f16((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy,
                  (int64_t)blockIdx.x,(int64_t)threadIdx.x,0);
}
static __global__ void native_bf16(const void * vx, nv_bfloat16 * yy){
    const int64_t i = blockIdx.x;
    dequantize_q5_K<nv_bfloat16>(vx, i, yy + i*QK_K, threadIdx.x);
}
static __global__ void goose_bf16(const void * vx, nv_bfloat16 * yy){
    FG265D_CB_bf16((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy,
                   (int64_t)blockIdx.x,(int64_t)threadIdx.x,0);
}

static const size_t Q5K_BYTES = 176;
static int checks, total_fails, launch_fail;
static unsigned first_info[8];

static void cmp_buf(const uint16_t * a, const uint16_t * b, size_t n, int kind){
    for (size_t i=0;i<n;++i){
        if (a[i]!=b[i]) {
            ++total_fails;
            if (total_fails==1) { first_info[0]=kind; first_info[1]=(unsigned)i; first_info[2]=a[i]; first_info[3]=b[i]; }
            return;
        }
    }
}

template<typename T, typename NF, typename GF>
static void run(const char * name, int kind, NF nk, GF gk, std::mt19937_64 & rng){
    int64_t nb_list[4]={1,4,64,256};
    for (int c=0;c<4;++c){
        const int64_t nb=nb_list[c];
        std::vector<uint8_t> raw((size_t)nb*Q5K_BYTES);
        for (auto & v: raw) v=(uint8_t)rng();
        T * dnat=nullptr; T * dgoo=nullptr; void * dsrc=nullptr;
        cudaMalloc(&dsrc,(size_t)nb*Q5K_BYTES);
        cudaMalloc(&dnat,(size_t)nb*QK_K*sizeof(T));
        cudaMalloc(&dgoo,(size_t)nb*QK_K*sizeof(T));
        cudaMemcpy(dsrc,raw.data(),(size_t)nb*Q5K_BYTES,cudaMemcpyHostToDevice);
        cudaMemset(dnat,0xFF,(size_t)nb*QK_K*sizeof(T));
        cudaMemset(dgoo,0xFF,(size_t)nb*QK_K*sizeof(T));
        nk<<<(unsigned)nb,64>>>(dsrc,dnat);
        gk<<<(unsigned)nb,64>>>(dsrc,dgoo);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL %s: %s\n",name,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<T> nat((size_t)nb*QK_K), goo((size_t)nb*QK_K);
        cudaMemcpy(nat.data(),dnat,(size_t)nb*QK_K*sizeof(T),cudaMemcpyDeviceToHost);
        cudaMemcpy(goo.data(),dgoo,(size_t)nb*QK_K*sizeof(T),cudaMemcpyDeviceToHost);
        cmp_buf((const uint16_t*)nat.data(),(const uint16_t*)goo.data(),(size_t)nb*QK_K,kind);
        checks += (int)(nb*QK_K);
        cudaFree(dsrc); cudaFree(dnat); cudaFree(dgoo);
    }
    printf("case %-4s values=%d\n",name,checks);
}

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(265011);
    run<half>("f16",0,native_f16,goose_f16,rng);
    int c1=checks; checks=0;
    run<nv_bfloat16>("bf16",1,native_bf16,goose_bf16,rng);
    checks += c1;
    if (total_fails) {
        printf("FIRST FAIL kind=%u idx=%u native=%04x goose=%04x\n",first_info[0],first_info[1],first_info[2],first_info[3]);
    }
    printf("FG265D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
