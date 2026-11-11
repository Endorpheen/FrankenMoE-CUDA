// Differential fixture for the Goose arange device value generation.
#include "common.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <string>

static __device__ __forceinline__ void fg274d_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o) = v; }
static __device__ __forceinline__ void fg274d_fail(int64_t){ asm("trap;"); }
#include "arange-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(float * dst, int ne0, float start, float step){
    FG274D_CB_arange((uint64_t)(uintptr_t)dst,(int64_t)ne0,start,step,
                     (int64_t)(threadIdx.x + blockIdx.x*blockDim.x),0);
}

static int checks, total_fails, launch_fail;

int main(void){
    int ndev=0;
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0);
    const int nes[7]={1,100,255,256,257,4096,100000};
    const float starts[4]={0.0f,-3.5f,1.0e6f,0.1f};
    const float steps[4]={1.0f,0.25f,-0.5f,0.1f};
    for (int c=0;c<7;++c){
        const int ne0=nes[c];
        for (int k=0;k<4;++k){
            const int nb=(ne0+255)/256;
            float *dn=nullptr; float *dg=nullptr;
            cudaMalloc(&dn,(size_t)ne0*4); cudaMalloc(&dg,(size_t)ne0*4);
            cudaMemset(dn,0xFF,(size_t)ne0*4); cudaMemset(dg,0xFF,(size_t)ne0*4);
            arange_f32_ref<<<nb,256>>>(dn,ne0,starts[k],steps[k]);
            goose_kernel<<<nb,256>>>(dg,ne0,starts[k],steps[k]);
            cudaError_t e=cudaDeviceSynchronize();
            if (e!=cudaSuccess){ printf("LAUNCH FAIL ne0=%d k=%d: %s\n",ne0,k,cudaGetErrorString(e)); ++launch_fail; }
            std::vector<float> a((size_t)ne0),b((size_t)ne0);
            cudaMemcpy(a.data(),dn,(size_t)ne0*4,cudaMemcpyDeviceToHost);
            cudaMemcpy(b.data(),dg,(size_t)ne0*4,cudaMemcpyDeviceToHost);
            for (int i=0;i<ne0;++i){
                ++checks;
                if (std::memcmp(&a[(size_t)i],&b[(size_t)i],4)!=0){
                    ++total_fails;
                    if (total_fails==1) printf("FIRST FAIL ne0=%d k=%d idx=%d native=%a goose=%a\n",ne0,k,i,a[(size_t)i],b[(size_t)i]);
                }
            }
            cudaFree(dn); cudaFree(dg);
        }
    }
    printf("FG274D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
