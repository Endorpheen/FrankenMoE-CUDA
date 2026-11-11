// Differential fixture for the Goose acc device computation.
#include "common.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg278d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg278d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ void  fg278d_fail(int64_t){ asm("trap;"); }
#include "acc-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * x,const float * y,float * dst,int64_t ne,
        int64_t ne10,int64_t ne11,int64_t ne12,int64_t ne13,
        int64_t s11,int64_t s12,int64_t s13,int64_t offset){
    FG278D_CB_acc((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)y,(uint64_t)(uintptr_t)dst,
        ne,ne10,ne11,ne12,ne13,s11,s12,s13,offset,
        (int64_t)(blockDim.x*blockIdx.x + threadIdx.x),0);
}

static int checks, total_fails, launch_fail;

struct Case { int64_t ne, ne10,ne11,ne12,ne13, s11,s12,s13, offset; };

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(278011);
    const Case cases[6]={
        {1000, 4,3,2,2, 4,12,24, 0},
        {1000, 4,3,2,2, 4,12,24, 5},
        {1000, 4,3,2,2, 4,12,24, -3},
        {1000, 4,3,2,2, 4,12,24, 2000},
        {300,  3,2,2,2, 5,12,20, 0},
        {1,    1,1,1,1, 1,1,1,  0},
    };
    for (int c=0;c<6;++c){
        const Case& k=cases[c];
        std::vector<float> hx((size_t)k.ne), hy(64);
        for (auto & v:hx){ uint32_t u=(uint32_t)rng(); memcpy(&v,&u,4); }
        for (auto & v:hy){ uint32_t u=(uint32_t)rng(); memcpy(&v,&u,4); }
        float *dx=nullptr,*dy=nullptr,*dn=nullptr,*dg=nullptr;
        cudaMalloc(&dx,(size_t)k.ne*4); cudaMalloc(&dy,64*4);
        cudaMalloc(&dn,(size_t)k.ne*4); cudaMalloc(&dg,(size_t)k.ne*4);
        cudaMemcpy(dx,hx.data(),(size_t)k.ne*4,cudaMemcpyHostToDevice);
        cudaMemcpy(dy,hy.data(),64*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,(size_t)k.ne*4); cudaMemset(dg,0xFF,(size_t)k.ne*4);
        const int nb=(int)((k.ne+255)/256);
        acc_f32_ref<<<nb,256>>>(dx,dy,dn,k.ne,k.ne10,k.ne11,k.ne12,k.ne13,k.s11,k.s12,k.s13,k.offset);
        goose_kernel<<<nb,256>>>(dx,dy,dg,k.ne,k.ne10,k.ne11,k.ne12,k.ne13,k.s11,k.s12,k.s13,k.offset);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> a((size_t)k.ne),b((size_t)k.ne);
        cudaMemcpy(a.data(),dn,(size_t)k.ne*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,(size_t)k.ne*4,cudaMemcpyDeviceToHost);
        for (int64_t i=0;i<k.ne;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL c=%d i=%lld native=%a goose=%a\n",c,(long long)i,a[(size_t)i],b[(size_t)i]); } }
        cudaFree(dx);cudaFree(dy);cudaFree(dn);cudaFree(dg);
    }
    printf("FG278D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
