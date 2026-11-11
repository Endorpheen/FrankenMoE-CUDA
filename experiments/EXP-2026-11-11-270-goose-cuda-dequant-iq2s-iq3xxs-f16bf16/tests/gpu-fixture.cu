// Differential fixture for IQ2_S and IQ3_XXS to half/bf16.
#include "common.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ uint32_t fg270d_ld_u8(uint64_t p,uint64_t o){ return *(const uint8_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ uint32_t fg270d_ld_u16(uint64_t p,uint64_t o){ return *(const uint16_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float    fg270d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ uint64_t fg270d_grid_s(uint32_t i){ return iq2s_grid[i]; }
static __device__ __forceinline__ uint32_t fg270d_grid3(uint32_t i){ return iq3xxs_grid[i]; }
static __device__ __forceinline__ uint32_t fg270d_ksign(uint32_t i){ return ksigns_iq2xs[i]; }
static __device__ __forceinline__ void     fg270d_st_half(uint64_t p,uint64_t o,float v){ *(half*)((char*)(uintptr_t)p+o) = __float2half(v); }
static __device__ __forceinline__ void     fg270d_st_bf16(uint64_t p,uint64_t o,float v){ *(nv_bfloat16*)((char*)(uintptr_t)p+o) = __float2bfloat16(v); }
static __device__ __forceinline__ void     fg270d_fail(int64_t){ asm("trap;"); }
#include "device-functions-generated.cuh"
#include "entries.h"

#define S_K(SUF,T) \
static __global__ void native_s_##SUF(const void * vx, T * yy){ \
    const int64_t i=blockIdx.x; dequantize_iq2_s<T>(vx,i,yy+i*QK_K,threadIdx.x); } \
static __global__ void goose_s_##SUF(const void * vx, T * yy){ \
    FG270D_CB_q2_s_##SUF((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy, \
        (int64_t)blockIdx.x,(int64_t)threadIdx.x,0); }
S_K(f16,half)
S_K(bf16,nv_bfloat16)

#define G3_K(SUF,T) \
static __global__ void native_g3_##SUF(const void * vx, T * yy){ \
    const int64_t i=blockIdx.x; dequantize_iq3_xxs<T>(vx,i,yy+i*QK_K,threadIdx.x); } \
static __global__ void goose_g3_##SUF(const void * vx, T * yy){ \
    FG270D_CB_q3_xxs_##SUF((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy, \
        (int64_t)blockIdx.x,(int64_t)threadIdx.x,0); }
G3_K(f16,half)
G3_K(bf16,nv_bfloat16)

static int checks, total_fails, launch_fail;
static unsigned first_info[8];

static void cmp_buf(const uint16_t* a,const uint16_t* b,size_t n,int kind){
    for (size_t i=0;i<n;++i) if (a[i]!=b[i]) {
        ++total_fails;
        if (total_fails==1){ first_info[0]=kind; first_info[1]=(unsigned)i; first_info[2]=a[i]; first_info[3]=b[i]; }
        return;
    }
}

template<typename T, typename NF, typename GF>
static void run(const char* name,int kind,NF nk,GF gk,size_t bytes,std::mt19937_64& rng){
    int64_t nbs[4]={1,4,64,256};
    for (int c=0;c<4;++c){
        const int64_t nb=nbs[c];
        std::vector<uint8_t> raw((size_t)nb*bytes);
        for (auto&v:raw) v=(uint8_t)rng();
        void* d=nullptr; T* dn=nullptr; T* dg=nullptr;
        cudaMalloc(&d,(size_t)nb*bytes); cudaMalloc(&dn,(size_t)nb*QK_K*sizeof(T)); cudaMalloc(&dg,(size_t)nb*QK_K*sizeof(T));
        cudaMemcpy(d,raw.data(),(size_t)nb*bytes,cudaMemcpyHostToDevice);
        nk<<<(unsigned)nb,32>>>(d,dn);
        gk<<<(unsigned)nb,32>>>(d,dg);
        cudaError_t e=cudaDeviceSynchronize();
        if(e!=cudaSuccess){ printf("LAUNCH FAIL %s: %s\n",name,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<T> a((size_t)nb*QK_K),b((size_t)nb*QK_K);
        cudaMemcpy(a.data(),dn,(size_t)nb*QK_K*sizeof(T),cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,(size_t)nb*QK_K*sizeof(T),cudaMemcpyDeviceToHost);
        cmp_buf((const uint16_t*)a.data(),(const uint16_t*)b.data(),(size_t)nb*QK_K,kind);
        checks+=(int)(nb*QK_K);
        cudaFree(d);cudaFree(dn);cudaFree(dg);
    }
    printf("case %-10s values=%d\n",name,checks);
}

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(270011);
    run<half>        ("s_f16",  0,native_s_f16, goose_s_f16, 82,rng);
    run<nv_bfloat16> ("s_bf16", 1,native_s_bf16,goose_s_bf16,82,rng);
    run<half>        ("g3_f16", 2,native_g3_f16,goose_g3_f16,98,rng);
    run<nv_bfloat16> ("g3_bf16",3,native_g3_bf16,goose_g3_bf16,98,rng);
    if (total_fails) printf("FIRST FAIL kind=%u idx=%u native=%04x goose=%04x\n",first_info[0],first_info[1],first_info[2],first_info[3]);
    printf("FG270D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
