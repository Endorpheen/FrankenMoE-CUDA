// Differential fixture for the Goose diag_mask_inf port.
//
// Both arms run the same shapes: the verbatim native kernel from diagmask.cu is
// the oracle, the Goose module is the other arm, and the outputs are compared
// bit for bit. The native kernel is copied rather than linked because diagmask.cu
// is a .cu translation unit, exactly as the other ported-kernel fixtures do.
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cfloat>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <cstdint>

#define CUDA_DIAG_MASK_INF_BLOCK_SIZE 256

// ---- oracle: verbatim body from ggml-cuda/diagmask.cu -----------------------
static __global__ void diag_mask_inf_f32(const float * x, float * dst, const int ncols,
                                         const int rows_per_channel, const int n_past) {
    const int col = blockDim.y*blockIdx.y + threadIdx.y;
    const int row = blockDim.x*blockIdx.x + threadIdx.x;

    if (col >= ncols) {
        return;
    }

    const int i = row*ncols + col;
    dst[i] = x[i] - (col > n_past + row % rows_per_channel) * FLT_MAX;
}

// ---- goose arm --------------------------------------------------------------
static __device__ __forceinline__ float fg367d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg367d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o)=v; }
static __device__ __forceinline__ void  fg367d_fail(int64_t){ asm("trap;"); }
#if defined(__cplusplus) && !defined(_Alignas)
#define _Alignas(x) alignas(x)
#endif
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_mask(const float * x, float * dst,
        int64_t ncols,int64_t rows,int64_t rows_per_channel,int64_t n_past){
    FG367D_CB_mask_rows((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)dst,
        ncols,rows,rows_per_channel,n_past,
        (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)threadIdx.x,(int64_t)threadIdx.y,
        (int64_t)blockDim.x,(int64_t)blockDim.y,0);
}

struct Case { int64_t ncols, ne01, ne02, ne03, n_past; };

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(367011);
    const Case cases[6]={
        {256, 1,1,1, 0},
        {512, 3,1,1, 0},
        {300, 2,1,1, 5},
        {1000,2,2,1, 0},
        {128, 4,2,2, 7},
        {4096,1,1,1, 4096},
    };
    int checks=0, fails=0, launch_fail=0;

    for (int c=0;c<6;++c){
        const Case& k=cases[c];
        const int64_t nrows=(k.ne01*k.ne02*k.ne03);
        const int64_t total=nrows*k.ncols;
        std::vector<float> hx((size_t)total);
        for (auto & v:hx) v=(float)((int64_t)(rng()%20001)-10000)*0.125f;

        float *dx=nullptr; float *dref=nullptr; float *dgo=nullptr;
        cudaMalloc(&dx,(size_t)total*4); cudaMalloc(&dref,(size_t)total*4); cudaMalloc(&dgo,(size_t)total*4);
        cudaMemcpy(dx,hx.data(),(size_t)total*4,cudaMemcpyHostToDevice);
        cudaMemset(dref,0x7F,(size_t)total*4); cudaMemset(dgo,0x7F,(size_t)total*4);

        const int block_y=CUDA_DIAG_MASK_INF_BLOCK_SIZE;
        const int block_num_x=(int)((k.ncols+block_y-1)/block_y);
        const dim3 blocks((unsigned)nrows,(unsigned)block_num_x,1);
        const dim3 threads(1,(unsigned)block_y,1);

        diag_mask_inf_f32<<<blocks,threads>>>(dx,dref,(int)k.ncols,(int)k.ne01,(int)k.n_past);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL native c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }

        goose_mask<<<blocks,threads>>>(dx,dgo,k.ncols,nrows,k.ne01,k.n_past);
        e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL goose c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }

        std::vector<float> a((size_t)total),b((size_t)total);
        cudaMemcpy(a.data(),dref,(size_t)total*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dgo,(size_t)total*4,cudaMemcpyDeviceToHost);
        for (int64_t i=0;i<total;++i){
            ++checks;
            if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){
                ++fails;
                if (fails==1) printf("FIRST FAIL c=%d i=%lld native=%08x goose=%08x\n",c,(long long)i,
                                     *(unsigned*)&a[(size_t)i],*(unsigned*)&b[(size_t)i]);
            }
        }
        cudaFree(dx);cudaFree(dref);cudaFree(dgo);
    }
    printf("FG367D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,fails,launch_fail);
    return (fails||launch_fail)?1:0;
}
