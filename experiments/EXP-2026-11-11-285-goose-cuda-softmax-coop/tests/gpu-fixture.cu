// Differential fixture for the Goose cooperative softmax.
#include "common.cuh"
#include "convert.cuh"
#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg285d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg285d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
extern __shared__ float fg285dyn[];
static __device__ __forceinline__ uint64_t fg285d_smem(){ return (uint64_t)(uintptr_t)fg285dyn; }
static __device__ __forceinline__ float fg285d_sload(uint64_t a){ return *(float*)(uintptr_t)a; }
static __device__ __forceinline__ void  fg285d_sstore(uint64_t a,float v){ *(float*)(uintptr_t)a = v; }
static __device__ __forceinline__ void  fg285d_sync(){ __syncthreads(); }
static __device__ __forceinline__ void  fg285d_gridsync(){ cooperative_groups::this_grid().sync(); }
static __device__ __forceinline__ float fg285d_shfl_xor(float v,int32_t m){ return __shfl_xor_sync(0xffffffffu, v, m); }
static __device__ __forceinline__ float fg285d_fmaxf(float a,float b){ return fmaxf(a,b); }
static __device__ __forceinline__ float fg285d_expf(float x){ return expf(x); }
static __device__ __forceinline__ float fg285d_ninf(){ return -INFINITY; }
static __device__ __forceinline__ void  fg285d_fail(int64_t){ asm("trap;"); }
#include "softmax-coop.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * x, float * dst, float * tmpmax, float * tmpsum,
        int64_t ncols, int64_t nrows, float scale){
    FG285D_CB_coop((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)dst,
        (uint64_t)(uintptr_t)tmpmax,(uint64_t)(uintptr_t)tmpsum,
        ncols,nrows,scale,(int64_t)blockDim.x,(int64_t)threadIdx.x,
        (int64_t)blockIdx.x,(int64_t)gridDim.x,0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    cudaDeviceProp props{};
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0); cudaGetDeviceProperties(&props,0);
    if (!props.cooperativeLaunch){ fprintf(stderr,"no cooperative launch\n"); return 77; }
    const int nsm=props.multiProcessorCount;
    std::mt19937_64 rng(285011);
    const int64_t cases[3][2]={{20000,1},{40000,2},{100000,1}};
    for (int c=0;c<3;++c){
        const int64_t ncols=cases[c][0], nrows=cases[c][1];
        const size_t total=(size_t)(ncols*nrows);
        soft_max_params p{};
        p.ncols=ncols; p.ne01=nrows; p.ne02=1; p.ne03=1; p.nrows_x=nrows;
        p.scale=1.0f; p.max_bias=0.0f; p.m0=1.0f; p.m1=1.0f;
        std::vector<float> hx(total);
        for (auto & v:hx) v=(float)((int64_t)(rng()%2001)-1000)*0.001f;
        // Put the row maximum inside the LAST CTA so a skipped CTA is observable.
        for (int64_t r=0;r<nrows;++r) hx[(size_t)(r*ncols + (int64_t)(nsm-1)*256)] = 2.0f;
        float *ds=nullptr,*dn=nullptr,*dg=nullptr,*dmn=nullptr,*dmg=nullptr,*dsn=nullptr,*dsg=nullptr;
        cudaMalloc(&ds,total*4); cudaMalloc(&dn,total*4); cudaMalloc(&dg,total*4);
        cudaMalloc(&dmn,nsm*4); cudaMalloc(&dmg,nsm*4); cudaMalloc(&dsn,nsm*4); cudaMalloc(&dsg,nsm*4);
        cudaMemcpy(ds,hx.data(),total*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,total*4); cudaMemset(dg,0xFF,total*4);
        {
            void * args[] = {(void*)&ds,(void*)&dn,(void*)&dmn,(void*)&dsn,(void*)&p};
            cudaError_t e=cudaLaunchCooperativeKernel((void*)soft_max_f32_parallelize_cols_ref,
                dim3(nsm),dim3(8*WARP_SIZE),args,0,0);
            if (e!=cudaSuccess){ printf("LAUNCH FAIL native c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        }
        {
            float scale=1.0f;
            void * args[] = {(void*)&ds,(void*)&dg,(void*)&dmg,(void*)&dsg,(void*)&ncols,(void*)&nrows,(void*)&scale};
            cudaError_t e=cudaLaunchCooperativeKernel((void*)goose_kernel,
                dim3(nsm),dim3(8*WARP_SIZE),args,256,0);
            if (e!=cudaSuccess){ printf("LAUNCH FAIL goose c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        }
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("SYNC FAIL c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> a(total),b(total);
        cudaMemcpy(a.data(),dn,total*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,total*4,cudaMemcpyDeviceToHost);
        for (size_t i=0;i<total;++i){ ++checks; if (memcmp(&a[i],&b[i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL c=%d i=%zu native=%a goose=%a\n",c,i,a[i],b[i]); } }
        cudaFree(ds);cudaFree(dn);cudaFree(dg);cudaFree(dmn);cudaFree(dmg);cudaFree(dsn);cudaFree(dsg);
    }
    printf("FG285D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
