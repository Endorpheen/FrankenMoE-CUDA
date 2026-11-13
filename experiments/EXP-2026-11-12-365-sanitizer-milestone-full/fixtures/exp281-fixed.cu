// Differential fixture for the Goose softmax with a mask (F32 and F16 mask).
#include "common.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg281d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float fg281d_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ void  fg281d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
extern __shared__ float fg281dyn[];
static __device__ __forceinline__ uint64_t fg281d_smem(){ return (uint64_t)(uintptr_t)fg281dyn; }
static __device__ __forceinline__ float fg281d_sload(uint64_t a){ return *(float*)(uintptr_t)a; }
static __device__ __forceinline__ void  fg281d_sstore(uint64_t a,float v){ *(float*)(uintptr_t)a = v; }
static __device__ __forceinline__ void  fg281d_sync(){ __syncthreads(); }
static __device__ __forceinline__ float fg281d_shfl_xor(float v,int32_t m){ return __shfl_xor_sync(0xffffffffu, v, m); }
static __device__ __forceinline__ float fg281d_fmaxf(float a,float b){ return fmaxf(a,b); }
static __device__ __forceinline__ float fg281d_expf(float x){ return expf(x); }
static __device__ __forceinline__ float fg281d_powf(float a,float b){ return powf(a,b); }
static __device__ __forceinline__ float fg281d_ninf(){ return -INFINITY; }
static __device__ __forceinline__ void  fg281d_fail(int64_t){ asm("trap;"); }
#include "softmax-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_f32(const float * x, const float * msk, float * dst, soft_max_params p,
        int64_t ne12,int64_t ne13,int64_t nb11,int64_t nb12,int64_t nb13,int use_shared){
    FG281D_CB_mask_f32((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)msk,(uint64_t)(uintptr_t)dst,
        p.ncols,p.scale,p.max_bias,(int64_t)p.n_head_log2,p.m0,p.m1,ne12,ne13,nb11,nb12,nb13,
        (int64_t)blockDim.x,(int64_t)threadIdx.x,(int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockIdx.z,
        (int64_t)gridDim.x,(int64_t)gridDim.y,(int64_t)use_shared,0);
}
static __global__ void goose_f16(const float * x, const half * msk, float * dst, soft_max_params p,
        int64_t ne12,int64_t ne13,int64_t nb11,int64_t nb12,int64_t nb13,int use_shared){
    FG281D_CB_mask_f16((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)msk,(uint64_t)(uintptr_t)dst,
        p.ncols,p.scale,p.max_bias,(int64_t)p.n_head_log2,p.m0,p.m1,ne12,ne13,nb11,nb12,nb13,
        (int64_t)blockDim.x,(int64_t)threadIdx.x,(int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockIdx.z,
        (int64_t)gridDim.x,(int64_t)gridDim.y,(int64_t)use_shared,0);
}

static int checks, total_fails, launch_fail;

struct Case { int64_t ncols, ne01, ne02, ne03, ne12, ne13; float max_bias; };

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(281011);
    const Case cases[5]={
        {32,  1,1,1, 1,1, 0.0f},
        {100, 2,3,2, 3,2, 0.0f},
        {128, 2,3,2, 1,1, 8.0f},
        {1024,1,1,1, 1,1, 0.0f},
        {1000,2,2,1, 2,1, 0.0f},
    };
    for (int c=0;c<5;++c){
        const Case& k=cases[c];
        const int64_t nrows=k.ne01*k.ne02*k.ne03;
        const int64_t total=nrows*k.ncols;
        const int64_t nmask=k.ncols*k.ne01*k.ne12*k.ne13;
        int nth=32; while (nth<k.ncols && nth<1024) nth*=2;
        const size_t nbs=(size_t)(((k.ncols+31)/32)*32 + 32)*4;
        const uint32_t n_head=(uint32_t)k.ne02;
        const uint32_t nhl=1u << (uint32_t)floorf(log2f((float)n_head));
        const float m0=powf(2.0f, -(k.max_bias)/(float)nhl);
        const float m1=powf(2.0f, -(k.max_bias/2.0f)/(float)nhl);
        soft_max_params p{};
        p.nheads=k.ne02; p.n_head_log2=nhl; p.ncols=k.ncols; p.nrows_x=nrows; p.nrows_y=k.ne01;
        p.ne00=k.ncols; p.ne01=k.ne01; p.ne02=k.ne02; p.ne03=k.ne03;
        p.nb11=4*k.ncols; p.nb12=p.nb11*k.ne01; p.nb13=p.nb12*k.ne12; p.ne12=k.ne12; p.ne13=k.ne13;
        p.scale=1.0f; p.max_bias=k.max_bias; p.m0=m0; p.m1=m1;

        std::vector<float> hx((size_t)total);
        std::vector<float> hmf((size_t)nmask);
        std::vector<half>  hmh((size_t)nmask);
        for (auto & v:hx) v=(float)((int64_t)(rng()%2001)-1000)*0.125f;
        for (auto & v:hmf) v=(float)((int64_t)(rng()%201)-100)*0.25f;
        for (int64_t i=0;i<nmask;++i) hmh[(size_t)i]=__float2half(hmf[(size_t)i]);

        float *ds=nullptr; float *dn=nullptr; float *dg=nullptr;
        float *dmsk=nullptr; half *dmsh=nullptr;
        cudaMalloc(&ds,(size_t)total*4); cudaMalloc(&dn,(size_t)total*4); cudaMalloc(&dg,(size_t)total*4);
        cudaMalloc(&dmsk,(size_t)nmask*4); cudaMalloc(&dmsh,(size_t)nmask*2);
        cudaMemcpy(ds,hx.data(),(size_t)total*4,cudaMemcpyHostToDevice);
        cudaMemcpy(dmsk,hmf.data(),(size_t)nmask*4,cudaMemcpyHostToDevice);
        cudaMemcpy(dmsh,hmh.data(),(size_t)nmask*2,cudaMemcpyHostToDevice);
        dim3 grid((unsigned)k.ne01,(unsigned)k.ne02,(unsigned)k.ne03);

        // F32 mask
        cudaMemset(dn,0xFF,(size_t)total*4); cudaMemset(dg,0xFF,(size_t)total*4);
        soft_max_f32_ref<true,0,0,float><<<grid,nth,nbs>>>(ds,dmsk,nullptr,dn,p);
        goose_f32<<<grid,nth,nbs>>>(ds,dmsk,dg,p,k.ne12,k.ne13,p.nb11,p.nb12,p.nb13,1);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL f32 c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        {
            std::vector<float> a((size_t)total),b((size_t)total);
            cudaMemcpy(a.data(),dn,(size_t)total*4,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dg,(size_t)total*4,cudaMemcpyDeviceToHost);
            for (int64_t i=0;i<total;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL f32 c=%d i=%lld\n",c,(long long)i); } }
        }
        // F16 mask: native takes nb from the mask tensor, so a half mask has half the strides.
        // The fixture used the F32 strides here, which made both kernels read past the allocation.
        p.nb11=2*k.ncols; p.nb12=p.nb11*k.ne01; p.nb13=p.nb12*k.ne12;
        cudaMemset(dn,0xFF,(size_t)total*4); cudaMemset(dg,0xFF,(size_t)total*4);
        soft_max_f32_ref<true,0,0,half><<<grid,nth,nbs>>>(ds,dmsh,nullptr,dn,p);
        goose_f16<<<grid,nth,nbs>>>(ds,dmsh,dg,p,k.ne12,k.ne13,p.nb11,p.nb12,p.nb13,1);
        e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL f16 c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        {
            std::vector<float> a((size_t)total),b((size_t)total);
            cudaMemcpy(a.data(),dn,(size_t)total*4,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dg,(size_t)total*4,cudaMemcpyDeviceToHost);
            for (int64_t i=0;i<total;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL f16 c=%d i=%lld\n",c,(long long)i); } }
        }
        cudaFree(ds);cudaFree(dn);cudaFree(dg);cudaFree(dmsk);cudaFree(dmsh);
    }
    printf("FG281D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
