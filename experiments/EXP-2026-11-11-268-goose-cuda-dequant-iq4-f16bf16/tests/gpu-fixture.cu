// Differential fixture for IQ4_NL and IQ4_XS dequantization to half/bf16.
#include "common.cuh"
#include "dequantize.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ uint32_t fg268d_ld_u8(uint64_t p,uint64_t o){ return *(const uint8_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ uint32_t fg268d_ld_u16(uint64_t p,uint64_t o){ return *(const uint16_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float    fg268d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ void     fg268d_st_half(uint64_t p,uint64_t o,float v){ *(half*)((char*)(uintptr_t)p+o) = __float2half(v); }
static __device__ __forceinline__ void     fg268d_st_bf16(uint64_t p,uint64_t o,float v){ *(nv_bfloat16*)((char*)(uintptr_t)p+o) = __float2bfloat16(v); }
static __device__ __forceinline__ void     fg268d_fail(int64_t){ asm("trap;"); }
#include "device-functions-generated.cuh"
#include "entries.h"

#define NL_K(SUF,T) \
static __global__ void native_nl_##SUF(const void * vx, T * yy, int nb32){ \
    const int64_t i=blockIdx.x; const int64_t ib=8*i+threadIdx.x%8; if(ib>=nb32)return; \
    dequantize_iq4_nl<T>(vx,i,yy+i*QK_K,threadIdx.x); } \
static __global__ void goose_nl_##SUF(const void * vx, T * yy, int nb32){ \
    FG268D_CB_q4_nl_##SUF((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy, \
        (int64_t)blockIdx.x,(int64_t)nb32,(int64_t)threadIdx.x,0); }
NL_K(f16,half)
NL_K(bf16,nv_bfloat16)

#define XS_K(SUF,T) \
static __global__ void native_xs_##SUF(const void * vx, T * yy){ \
    const int64_t i=blockIdx.x; dequantize_iq4_xs<T>(vx,i,yy+i*QK_K,threadIdx.x); } \
static __global__ void goose_xs_##SUF(const void * vx, T * yy){ \
    FG268D_CB_q4_xs_##SUF((uint64_t)(uintptr_t)vx,(uint64_t)(uintptr_t)yy, \
        (int64_t)blockIdx.x,(int64_t)threadIdx.x,0); }
XS_K(f16,half)
XS_K(bf16,nv_bfloat16)

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
static void run_nl(const char* name,int kind,NF nk,GF gk,std::mt19937_64& rng){
    int64_t nb32s[5]={8,32,512,2048,7};
    for (int c=0;c<5;++c){
        const int64_t nb32=nb32s[c]; const int64_t nb=(nb32+7)/8;
        std::vector<uint8_t> raw((size_t)nb32*18);
        for (auto&v:raw) v=(uint8_t)rng();
        void* d=nullptr; T* dn=nullptr; T* dg=nullptr;
        cudaMalloc(&d,(size_t)nb32*18); cudaMalloc(&dn,(size_t)nb32*32*sizeof(T)); cudaMalloc(&dg,(size_t)nb32*32*sizeof(T));
        cudaMemcpy(d,raw.data(),(size_t)nb32*18,cudaMemcpyHostToDevice);
        nk<<<(unsigned)nb,32>>>(d,dn,(int)nb32);
        gk<<<(unsigned)nb,32>>>(d,dg,(int)nb32);
        cudaError_t e=cudaDeviceSynchronize();
        if(e!=cudaSuccess){ printf("LAUNCH FAIL %s: %s\n",name,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<T> a((size_t)nb32*32),b((size_t)nb32*32);
        cudaMemcpy(a.data(),dn,(size_t)nb32*32*sizeof(T),cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,(size_t)nb32*32*sizeof(T),cudaMemcpyDeviceToHost);
        cmp_buf((const uint16_t*)a.data(),(const uint16_t*)b.data(),(size_t)nb32*32,kind);
        checks+=(int)(nb32*32);
        cudaFree(d);cudaFree(dn);cudaFree(dg);
    }
    printf("case %-8s values=%d\n",name,checks);
}

template<typename T, typename NF, typename GF>
static void run_xs(const char* name,int kind,NF nk,GF gk,std::mt19937_64& rng){
    int64_t nbs[4]={1,4,64,256};
    for (int c=0;c<4;++c){
        const int64_t nb=nbs[c];
        std::vector<uint8_t> raw((size_t)nb*136);
        for (auto&v:raw) v=(uint8_t)rng();
        void* d=nullptr; T* dn=nullptr; T* dg=nullptr;
        cudaMalloc(&d,(size_t)nb*136); cudaMalloc(&dn,(size_t)nb*QK_K*sizeof(T)); cudaMalloc(&dg,(size_t)nb*QK_K*sizeof(T));
        cudaMemcpy(d,raw.data(),(size_t)nb*136,cudaMemcpyHostToDevice);
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
    printf("case %-8s values=%d\n",name,checks);
}

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(268011);
    run_nl<half>("nl_f16",0,native_nl_f16,goose_nl_f16,rng);
    run_nl<nv_bfloat16>("nl_bf16",1,native_nl_bf16,goose_nl_bf16,rng);
    run_xs<half>("xs_f16",2,native_xs_f16,goose_xs_f16,rng);
    run_xs<nv_bfloat16>("xs_bf16",3,native_xs_bf16,goose_xs_bf16,rng);
    if (total_fails) printf("FIRST FAIL kind=%u idx=%u native=%04x goose=%04x\n",first_info[0],first_info[1],first_info[2],first_info[3]);
    printf("FG268D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
