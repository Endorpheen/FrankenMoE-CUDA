// Differential fixture for the Goose cumsum (fallback-kernel semantics).
#include "common.cuh"
#include "convert.cuh"
#include "cumsum.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg279d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg279d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
extern __shared__ float fg279dyn[];
static __device__ __forceinline__ uint64_t fg279d_smem(){ return (uint64_t)(uintptr_t)fg279dyn; }
static __device__ __forceinline__ float fg279d_sload(uint64_t a){ return *(float*)(uintptr_t)a; }
static __device__ __forceinline__ void  fg279d_sstore(uint64_t a,float v){ *(float*)(uintptr_t)a = v; }
static __device__ __forceinline__ void  fg279d_sync(){ __syncthreads(); }
static __device__ __forceinline__ float fg279d_shfl_up(float v,int32_t o){ return __shfl_up_sync(0xffffffffu, v, o); }
static __device__ __forceinline__ void  fg279d_fail(int64_t){ asm("trap;"); }
#include "cumsum-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * src, float * dst,
        int64_t ne00,int64_t ne01,int64_t ne02,int64_t ne03,
        int64_t s01,int64_t s02,int64_t s03,int64_t s1,int64_t s2,int64_t s3){
    FG279D_CB_cumsum((uint64_t)(uintptr_t)src,(uint64_t)(uintptr_t)dst,
        ne00,ne01,ne02,ne03,s01,s02,s03,s1,s2,s3,
        (int64_t)blockDim.x,(int64_t)threadIdx.x,
        (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockIdx.z,(int64_t)WARP_SIZE,0);
}

static int checks, total_fails, launch_fail;

struct Case { int64_t ne00, ne01, ne02, ne03; };

int main(void){
    int ndev=0;
    cudaDeviceProp props{};
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0); cudaGetDeviceProperties(&props,0);
    const int ws=props.warpSize;
    std::mt19937_64 rng(279011);
    const Case cases[9]={
        {1,1,1,1},{31,1,1,1},{32,1,1,1},{33,1,1,1},{127,1,1,1},
        {128,1,1,1},{129,1,1,1},{1000,1,1,1},{4096,2,1,1},
    };
    for (int c=0;c<9;++c){
        const Case& k=cases[c];
        const int64_t total=k.ne00*k.ne01*k.ne02*k.ne03;
        const int64_t s01=k.ne00, s02=s01*k.ne01, s03=s02*k.ne02;
        const int64_t s1=s01, s2=s02, s3=s03;
        const int nw=(int)((k.ne00+ws-1)/ws);
        int blk=nw*ws; if (blk>256) blk=256;
        const int warps=blk/ws;
        const size_t shm=(size_t)(blk+warps+2)*4;
        std::vector<float> hx((size_t)total);
        for (auto & v:hx){ v=(float)((int64_t)(rng()%2001)-1000)*0.125f; }
        float *ds=nullptr,*dn=nullptr,*dg=nullptr;
        cudaMalloc(&ds,(size_t)total*4); cudaMalloc(&dn,(size_t)total*4); cudaMalloc(&dg,(size_t)total*4);
        cudaMemcpy(ds,hx.data(),(size_t)total*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,(size_t)total*4); cudaMemset(dg,0xFF,(size_t)total*4);
        dim3 grid((unsigned)k.ne01,(unsigned)k.ne02,(unsigned)k.ne03);
        cumsum_kernel_ref<float><<<grid,blk,shm>>>(ds,dn,k.ne00,k.ne01,k.ne02,k.ne03,1,s01,s02,s03,1,s1,s2,s3);
        goose_kernel<<<grid,blk,shm>>>(ds,dg,k.ne00,k.ne01,k.ne02,k.ne03,s01,s02,s03,s1,s2,s3);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL c=%d: %s\n",c,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> a((size_t)total),b((size_t)total);
        cudaMemcpy(a.data(),dn,(size_t)total*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,(size_t)total*4,cudaMemcpyDeviceToHost);
        for (int64_t i=0;i<total;++i){ ++checks; if (memcmp(&a[(size_t)i],&b[(size_t)i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL c=%d i=%lld native=%a goose=%a\n",c,(long long)i,a[(size_t)i],b[(size_t)i]); } }
        cudaFree(ds);cudaFree(dn);cudaFree(dg);
    }
    printf("FG279D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
