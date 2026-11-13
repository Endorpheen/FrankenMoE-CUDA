// Differential fixture for the Goose softmax (mask-free, sink-free path).
#include "common.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg280d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg280d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
extern __shared__ float fg280dyn[];
static __device__ __forceinline__ uint64_t fg280d_smem(){ return (uint64_t)(uintptr_t)fg280dyn; }
static __device__ __forceinline__ float fg280d_sload(uint64_t a){ return *(float*)(uintptr_t)a; }
static __device__ __forceinline__ void  fg280d_sstore(uint64_t a,float v){ *(float*)(uintptr_t)a = v; }
static __device__ __forceinline__ void  fg280d_sync(){ __syncthreads(); }
static __device__ __forceinline__ float fg280d_shfl_xor(float v,int32_t m){ return __shfl_xor_sync(0xffffffffu, v, m); }
static __device__ __forceinline__ float fg280d_fmaxf(float a,float b){ return fmaxf(a,b); }
static __device__ __forceinline__ float fg280d_expf(float x){ return expf(x); }
static __device__ __forceinline__ float fg280d_powf(float a,float b){ return powf(a,b); }
static __device__ __forceinline__ float fg280d_ninf(){ return -INFINITY; }
static __device__ __forceinline__ void  fg280d_fail(int64_t){ asm("trap;"); }
#include "softmax-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * x, float * dst, soft_max_params p, int use_shared){
    FG280D_CB_softmax((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)dst,
        p.ncols,p.scale,p.max_bias,(int64_t)p.n_head_log2,p.m0,p.m1,
        (int64_t)blockDim.x,(int64_t)threadIdx.x,
        (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockIdx.z,
        (int64_t)gridDim.x,(int64_t)gridDim.y,(int64_t)use_shared,0);
}

static int checks, total_fails, launch_fail;

struct Case { int64_t ncols, ne01, ne02, ne03; float scale, max_bias; int use_shared; };

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(280011);
    const Case cases[8]={
        {1,    1,1,1, 1.0f, 0.0f, 1},
        {32,   1,1,1, 1.0f, 0.0f, 1},
        {33,   1,1,1, 1.0f, 0.0f, 1},
        {100,  1,1,1, 0.5f, 0.0f, 1},
        {128,  1,1,1, 1.0f, 0.0f, 1},
        {1024, 1,1,1, 1.0f, 0.0f, 1},
        {4096, 2,3,1, 1.0f, 8.0f, 1},
        {1000, 2,1,1, 1.0f, 0.0f, 0},
    };
    for (int c=0;c<8;++c){
        const Case& k=cases[c];
        const int64_t nrows=k.ne01*k.ne02*k.ne03;
        const int64_t total=nrows*k.ncols;
        int nth=32; while (nth<k.ncols && nth<1024) nth*=2;
        const size_t nbs=(size_t)(((k.ncols+31)/32)*32 + 32)*4;
        const size_t shm = k.use_shared ? nbs : (size_t)32*4;
        const uint32_t n_head=(uint32_t)k.ne02;
        const uint32_t nhl=1u << (uint32_t)floorf(log2f((float)n_head));
        const float m0=powf(2.0f, -(k.max_bias)/(float)nhl);
        const float m1=powf(2.0f, -(k.max_bias/2.0f)/(float)nhl);
        soft_max_params p{};
        p.nheads=k.ne02; p.n_head_log2=nhl; p.ncols=k.ncols; p.nrows_x=nrows; p.nrows_y=k.ne01;
        p.ne00=k.ncols; p.ne01=k.ne01; p.ne02=k.ne02; p.ne03=k.ne03;
        p.nb11=1;p.nb12=1;p.nb13=1;p.ne12=1;p.ne13=1;
        p.scale=k.scale; p.max_bias=k.max_bias; p.m0=m0; p.m1=m1;

        std::vector<float> hx((size_t)total);
        for (auto & v:hx){ v=(float)((int64_t)(rng()%2001)-1000)*0.125f; }
        float *ds=nullptr,*dn=nullptr,*dg=nullptr;
        cudaMalloc(&ds,(size_t)total*4); cudaMalloc(&dn,(size_t)total*4); cudaMalloc(&dg,(size_t)total*4);
        cudaMemcpy(ds,hx.data(),(size_t)total*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,(size_t)total*4); cudaMemset(dg,0xFF,(size_t)total*4);
        dim3 grid((unsigned)k.ne01,(unsigned)k.ne02,(unsigned)k.ne03);
        if (k.use_shared) soft_max_f32_ref<true,0,0,float><<<grid,nth,shm>>>(ds,(const float*)nullptr,(const float*)nullptr,dn,p);
        else             soft_max_f32_ref<false,0,0,float><<<grid,nth,shm>>>(ds,(const float*)nullptr,(const float*)nullptr,dn,p);
        goose_kernel<<<grid,nth,shm>>>(ds,dg,p,k.use_shared);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> a((size_t)total),b((size_t)total);
        cudaMemcpy(a.data(),dn,(size_t)total*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,(size_t)total*4,cudaMemcpyDeviceToHost);
        for (int64_t i=0;i<total;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL c=%d i=%lld native=%a goose=%a\n",c,(long long)i,a[(size_t)i],b[(size_t)i]); } }
        cudaFree(ds);cudaFree(dn);cudaFree(dg);
    }
    printf("FG280D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
