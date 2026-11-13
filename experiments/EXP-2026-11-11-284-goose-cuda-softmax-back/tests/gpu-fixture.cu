// Differential fixture for the Goose softmax backward.
#include "common.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg284d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg284d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ float fg284d_shfl_xor(float v,int32_t m){ return __shfl_xor_sync(0xffffffffu, v, m); }
static __device__ __forceinline__ void  fg284d_fail(int64_t){ asm("trap;"); }
#include "softmax-back.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * grad, const float * dstf, float * dst, int ncols, float scale){
    FG284D_CB_softback((uint64_t)(uintptr_t)grad,(uint64_t)(uintptr_t)dstf,(uint64_t)(uintptr_t)dst,
        (int64_t)ncols,scale,(int64_t)threadIdx.x,(int64_t)blockIdx.x,0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(284011);
    const int64_t ncs[6]={1,31,32,33,100,1000};
    const int64_t nrs[2]={1,3};
    const float scales[3]={1.0f,0.5f,-2.0f};
    for (int a=0;a<6;++a) for (int b=0;b<2;++b) for (int c=0;c<3;++c){
        const int64_t ncols=ncs[a], nrows=nrs[b];
        const float scale=scales[c];
        const size_t total=(size_t)(ncols*nrows);
        std::vector<float> hg(total), hf(total);
        for (auto & v:hg) v=(float)((int64_t)(rng()%2001)-1000)*0.001f;
        for (auto & v:hf) v=(float)((int64_t)(rng()%2001)-1000)*0.001f;
        float *dg=nullptr,*df=nullptr,*dn=nullptr,*do_=nullptr;
        cudaMalloc(&dg,total*4); cudaMalloc(&df,total*4); cudaMalloc(&dn,total*4); cudaMalloc(&do_,total*4);
        cudaMemcpy(dg,hg.data(),total*4,cudaMemcpyHostToDevice);
        cudaMemcpy(df,hf.data(),total*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,total*4); cudaMemset(do_,0xFF,total*4);
        soft_max_back_f32_ref<<<(unsigned)nrows,32>>>(dg,df,dn,(int)ncols,scale);
        goose_kernel<<<(unsigned)nrows,32>>>(dg,df,do_,(int)ncols,scale);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL a=%d b=%d c=%d: %s\n",a,b,c,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> x(total),y(total);
        cudaMemcpy(x.data(),dn,total*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(y.data(),do_,total*4,cudaMemcpyDeviceToHost);
        for (size_t i=0;i<total;++i){ ++checks; if (memcmp(&x[i],&y[i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL cols=%lld rows=%lld scale=%g i=%zu native=%a goose=%a\n",(long long)ncols,(long long)nrows,scale,i,x[i],y[i]); } }
        cudaFree(dg);cudaFree(df);cudaFree(dn);cudaFree(do_);
    }
    printf("FG284D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
