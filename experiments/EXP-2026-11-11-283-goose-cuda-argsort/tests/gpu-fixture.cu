// Differential fixture for the Goose bitonic argsort.
#include "common.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float   fg283d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void    fg283d_st_i32(uint64_t p,uint64_t o,int32_t v){ *(int32_t*)((char*)(uintptr_t)p+o) = v; }
extern __shared__ int fg283dyn[];
static __device__ __forceinline__ uint64_t fg283d_smem(){ return (uint64_t)(uintptr_t)fg283dyn; }
static __device__ __forceinline__ int32_t fg283d_sload_i32(uint64_t a){ return *(int32_t*)(uintptr_t)a; }
static __device__ __forceinline__ void    fg283d_sstore_i32(uint64_t a,int32_t v){ *(int32_t*)(uintptr_t)a = v; }
static __device__ __forceinline__ void    fg283d_sync(){ __syncthreads(); }
static __device__ __forceinline__ void    fg283d_fail(int64_t){ asm("trap;"); }
#include "argsort-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * x, int * dst, int ncols, int ncols_pad, int order){
    FG283D_CB_argsort((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)dst,
        (int64_t)ncols,(int64_t)ncols_pad,(int64_t)order,
        (int64_t)blockDim.x,(int64_t)threadIdx.x,(int64_t)blockIdx.x,0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(283011);
    const int64_t cases[9][2]={{1,1},{2,3},{3,2},{5,4},{8,2},{16,1},{33,2},{100,3},{1024,1}};
    for (int c=0;c<9;++c){
        const int64_t ncols=cases[c][0], nrows=cases[c][1];
        const size_t total=(size_t)(ncols*nrows);
        int pad=1; while (pad<ncols) pad*=2;
        const int blk = pad<1024?pad:1024;
        const size_t shm=(size_t)pad*4;
        std::vector<float> hx(total);
        for (auto & v:hx) v=(float)((int64_t)(rng()%7)-3);   // heavy ties
        float *ds=nullptr; int *dn=nullptr; int *dg=nullptr;
        cudaMalloc(&ds,total*4); cudaMalloc(&dn,total*4); cudaMalloc(&dg,total*4);
        cudaMemcpy(ds,hx.data(),total*4,cudaMemcpyHostToDevice);
        for (int ord=0; ord<2; ++ord){
            cudaMemset(dn,0xFF,total*4); cudaMemset(dg,0xFF,total*4);
            if (ord==0) k_argsort_f32_i32_ref<GGML_SORT_ORDER_ASC><<<(unsigned)nrows,blk,shm>>>(ds,dn,(int)ncols,pad);
            else        k_argsort_f32_i32_ref<GGML_SORT_ORDER_DESC><<<(unsigned)nrows,blk,shm>>>(ds,dn,(int)ncols,pad);
            goose_kernel<<<(unsigned)nrows,blk,shm>>>(ds,dg,(int)ncols,pad,ord);
            cudaError_t e=cudaDeviceSynchronize();
            if (e!=cudaSuccess){ printf("LAUNCH FAIL c=%d ord=%d: %s\n",c,ord,cudaGetErrorString(e)); ++launch_fail; }
            std::vector<int> a(total),b(total);
            cudaMemcpy(a.data(),dn,total*4,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dg,total*4,cudaMemcpyDeviceToHost);
            for (size_t i=0;i<total;++i){ ++checks; if (a[i]!=b[i]){ ++total_fails; if(total_fails==1) printf("FIRST FAIL c=%d ord=%d i=%zu native=%d goose=%d\n",c,ord,i,a[i],b[i]); } }
        }
        cudaFree(ds);cudaFree(dn);cudaFree(dg);
    }
    printf("FG283D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
