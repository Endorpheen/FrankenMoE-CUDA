// Differential fixture for the Goose diag device index mapping.
#include "common.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float    fg275d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void     fg275d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ uint32_t fg275d_ld_u16(uint64_t p,uint64_t o){ return *(const uint16_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void     fg275d_st_u16(uint64_t p,uint64_t o,uint32_t v){ *(uint16_t*)((char*)(uintptr_t)p+o) = (uint16_t)v; }
static __device__ __forceinline__ void     fg275d_fail(int64_t){ asm("trap;"); }
#include "diag-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_f32(float * dst, const float * src, int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,int64_t total){
    FG275D_CB_diag_f32((uint64_t)(uintptr_t)dst,(uint64_t)(uintptr_t)src,ne0,ne1,ne2,ne3,
        (int64_t)(blockIdx.x*blockDim.x + threadIdx.x),total,0);
}
static __global__ void goose_f16(half * dst, const half * src, int64_t ne0,int64_t ne1,int64_t ne2,int64_t ne3,int64_t total){
    FG275D_CB_diag_f16((uint64_t)(uintptr_t)dst,(uint64_t)(uintptr_t)src,ne0,ne1,ne2,ne3,
        (int64_t)(blockIdx.x*blockDim.x + threadIdx.x),total,0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(275011);
    const int64_t shapes[7][4]={{4,4,1,1},{8,8,2,2},{7,7,1,3},{256,256,1,1},{3,3,4,5},{4,6,1,1},{3,5,2,3}};
    for (int s=0;s<7;++s){
        const int64_t ne0=shapes[s][0],ne1=shapes[s][1],ne2=shapes[s][2],ne3=shapes[s][3];
        const int64_t nd=ne0*ne1*ne2*ne3, nsrc=ne0*ne2*ne3;
        const int nb=(int)((nd+255)/256);
        // F32
        {
            std::vector<float> src((size_t)nsrc);
            for (auto & v:src){ uint32_t u=(uint32_t)rng(); memcpy(&v,&u,4); }
            float *ds=nullptr,*dn=nullptr,*dg=nullptr;
            cudaMalloc(&ds,(size_t)nsrc*4); cudaMalloc(&dn,(size_t)nd*4); cudaMalloc(&dg,(size_t)nd*4);
            cudaMemcpy(ds,src.data(),(size_t)nsrc*4,cudaMemcpyHostToDevice);
            cudaMemset(dn,0xFF,(size_t)nd*4); cudaMemset(dg,0xFF,(size_t)nd*4);
            diag_kernel_ref<float><<<nb,256>>>(dn,ds,ne0,ne1,ne2,ne3,nd);
            goose_f32<<<nb,256>>>(dg,ds,ne0,ne1,ne2,ne3,nd);
            if (cudaDeviceSynchronize()!=cudaSuccess){ printf("LAUNCH FAIL f32 s=%d\n",s); ++launch_fail; }
            std::vector<float> a((size_t)nd),b((size_t)nd);
            cudaMemcpy(a.data(),dn,(size_t)nd*4,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dg,(size_t)nd*4,cudaMemcpyDeviceToHost);
            for (int64_t i=0;i<nd;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)) { ++total_fails; if(total_fails==1) printf("FIRST FAIL f32 s=%d i=%lld\n",s,(long long)i); } }
            cudaFree(ds);cudaFree(dn);cudaFree(dg);
        }
        // F16 (bit patterns, so NaN payloads and signs are exercised)
        {
            std::vector<uint16_t> src((size_t)nsrc);
            for (auto & v:src) v=(uint16_t)rng();
            uint16_t *ds=nullptr,*dn=nullptr,*dg=nullptr;
            cudaMalloc(&ds,(size_t)nsrc*2); cudaMalloc(&dn,(size_t)nd*2); cudaMalloc(&dg,(size_t)nd*2);
            cudaMemcpy(ds,src.data(),(size_t)nsrc*2,cudaMemcpyHostToDevice);
            cudaMemset(dn,0xFF,(size_t)nd*2); cudaMemset(dg,0xFF,(size_t)nd*2);
            diag_kernel_ref<half><<<nb,256>>>((half*)dn,(const half*)ds,ne0,ne1,ne2,ne3,nd);
            goose_f16<<<nb,256>>>((half*)dg,(const half*)ds,ne0,ne1,ne2,ne3,nd);
            if (cudaDeviceSynchronize()!=cudaSuccess){ printf("LAUNCH FAIL f16 s=%d\n",s); ++launch_fail; }
            std::vector<uint16_t> a((size_t)nd),b((size_t)nd);
            cudaMemcpy(a.data(),dn,(size_t)nd*2,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dg,(size_t)nd*2,cudaMemcpyDeviceToHost);
            for (int64_t i=0;i<nd;++i){ ++checks; if (a[(size_t)i]!=b[(size_t)i]) { ++total_fails; if(total_fails==1) printf("FIRST FAIL f16 s=%d i=%lld\n",s,(long long)i); } }
            cudaFree(ds);cudaFree(dn);cudaFree(dg);
        }
    }
    printf("FG275D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
