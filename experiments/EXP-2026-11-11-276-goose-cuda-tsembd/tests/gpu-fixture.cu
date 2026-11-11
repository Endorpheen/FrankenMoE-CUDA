// Differential fixture for the Goose timestep embedding.
#include "common.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float fg276d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void  fg276d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ float fg276d_expf(float x){ return expf(x); }
static __device__ __forceinline__ float fg276d_logf(float x){ return logf(x); }
static __device__ __forceinline__ float fg276d_cosf(float x){ return cosf(x); }
static __device__ __forceinline__ float fg276d_sinf(float x){ return sinf(x); }
static __device__ __forceinline__ void  fg276d_fail(int64_t){ asm("trap;"); }
#include "tsembd-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * ts, float * dst, int nb1, int dim, int maxp){
    FG276D_CB_tsembd((uint64_t)(uintptr_t)dst,(uint64_t)(uintptr_t)ts,(int64_t)nb1,(int64_t)dim,(int64_t)maxp,
        (int64_t)blockIdx.y,(int64_t)(threadIdx.x + blockIdx.x*blockDim.x),0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(276011);
    const int dims[6]={8,9,64,100,256,257};
    const int ne00s[3]={1,3,7};
    const int maxps[2]={10000,1000};
    for (int d=0;d<6;++d) for (int n=0;n<3;++n) for (int mp=0;mp<2;++mp){
        const int dim=dims[d], ne00=ne00s[n], maxp=maxps[mp], nb1=dim*4;
        const int nb=(int)((((dim+1)/2)+255)/256);
        std::vector<float> ts((size_t)ne00);
        for (auto & v:ts){ uint32_t u=(uint32_t)rng() & 0x7fffffff; float f; memcpy(&f,&u,4); v=f*0.01f; }
        float *ds=nullptr,*dn=nullptr,*dg=nullptr;
        cudaMalloc(&ds,(size_t)ne00*4); cudaMalloc(&dn,(size_t)ne00*nb1); cudaMalloc(&dg,(size_t)ne00*nb1);
        cudaMemcpy(ds,ts.data(),(size_t)ne00*4,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,(size_t)ne00*nb1); cudaMemset(dg,0xFF,(size_t)ne00*nb1);
        dim3 grid((unsigned)nb,(unsigned)ne00,1);
        timestep_embedding_f32_ref<<<grid,256>>>(ds,dn,nb1,dim,maxp);
        goose_kernel<<<grid,256>>>(ds,dg,nb1,dim,maxp);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL dim=%d ne00=%d mp=%d: %s\n",dim,ne00,maxp,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> a((size_t)ne00*dim),b((size_t)ne00*dim);
        cudaMemcpy(a.data(),dn,(size_t)ne00*nb1,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,(size_t)ne00*nb1,cudaMemcpyDeviceToHost);
        for (size_t i=0;i<a.size();++i){ ++checks; if (memcmp(&a[i],&b[i],4)) { ++total_fails; if(total_fails==1) printf("FIRST FAIL dim=%d ne00=%d mp=%d i=%zu native=%a goose=%a\n",dim,ne00,maxp,i,a[i],b[i]); } }
        cudaFree(ds);cudaFree(dn);cudaFree(dg);
    }
    printf("FG276D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
