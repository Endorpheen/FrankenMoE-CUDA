// Differential fixture for the Goose add_id device computation.
#include "common.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>

static __device__ __forceinline__ float   fg277d_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void    fg277d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ int32_t fg277d_ld_i32(uint64_t p,uint64_t o){ return *(const int32_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void    fg277d_fail(int64_t){ asm("trap;"); }
#include "add-id-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const float * s0,const float * s1,const int32_t * s2,float * dst,
        int64_t ne0,int64_t ne1,size_t nb01,size_t nb02,size_t nb11,size_t nb21){
    FG277D_CB_add_id((uint64_t)(uintptr_t)s0,(uint64_t)(uintptr_t)s1,(uint64_t)(uintptr_t)s2,
        (uint64_t)(uintptr_t)dst,ne0,ne1,(int64_t)nb01,(int64_t)nb02,(int64_t)nb11,(int64_t)nb21,
        (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockDim.x,(int64_t)threadIdx.x,0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    std::mt19937_64 rng(277011);
    const int64_t shapes[5][3]={{4,2,2},{768,3,1},{1000,2,4},{1,1,1},{16,3,2}};
    for (int s=0;s<5;++s){
        const int64_t ne0=shapes[s][0],ne1=shapes[s][1],ne2=shapes[s][2];
        // The last shape pads src1 rows so nb11 != nb01; otherwise a wrong stride
        // is invisible.
        const int64_t pad = (s==4) ? 2 : 0;
        const int64_t nb01=4*ne0, nb02=nb01*ne1, nb11=4*(ne0+pad), nb21=4*ne1;
        const size_t n0=(size_t)(ne0*ne1*ne2), n1=(size_t)(ne1*(ne0+pad)), n2=(size_t)(ne1*ne2);
        std::vector<float> h0(n0),h1(n1);
        for (auto & v:h0){ uint32_t u=(uint32_t)rng(); memcpy(&v,&u,4); }
        for (auto & v:h1){ uint32_t u=(uint32_t)rng(); memcpy(&v,&u,4); }
        std::vector<int32_t> h2(n2);
        for (auto & v:h2) v=(int32_t)(rng()% (uint64_t)ne1);
        float *d0=nullptr,*d1=nullptr,*dd=nullptr,*dg=nullptr; int32_t *d2=nullptr;
        cudaMalloc(&d0,n0*4); cudaMalloc(&d1,n1*4); cudaMalloc(&d2,n2*4);
        cudaMalloc(&dd,n0*4); cudaMalloc(&dg,n0*4);
        cudaMemcpy(d0,h0.data(),n0*4,cudaMemcpyHostToDevice);
        cudaMemcpy(d1,h1.data(),n1*4,cudaMemcpyHostToDevice);
        cudaMemcpy(d2,h2.data(),n2*4,cudaMemcpyHostToDevice);
        cudaMemset(dd,0xFF,n0*4); cudaMemset(dg,0xFF,n0*4);
        int threads=(int)std::min<int64_t>(ne0,768);
        dim3 blocks((unsigned)ne1,(unsigned)ne2);
        add_id_kernel_ref<<<blocks,threads>>>(d0,d1,d2,dd,ne0,ne1,(size_t)nb01,(size_t)nb02,(size_t)nb11,(size_t)nb21);
        goose_kernel<<<blocks,threads>>>(d0,d1,d2,dg,ne0,ne1,(size_t)nb01,(size_t)nb02,(size_t)nb11,(size_t)nb21);
        cudaError_t e=cudaDeviceSynchronize();
        if (e!=cudaSuccess){ printf("LAUNCH FAIL s=%d: %s\n",s,cudaGetErrorString(e)); ++launch_fail; }
        std::vector<float> a(n0),b(n0);
        cudaMemcpy(a.data(),dd,n0*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(b.data(),dg,n0*4,cudaMemcpyDeviceToHost);
        for (size_t i=0;i<n0;++i){ ++checks; if (memcmp(&a[i],&b[i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL s=%d i=%zu native=%a goose=%a\n",s,i,a[i],b[i]); } }
        cudaFree(d0);cudaFree(d1);cudaFree(d2);cudaFree(dd);cudaFree(dg);
    }
    printf("FG277D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
