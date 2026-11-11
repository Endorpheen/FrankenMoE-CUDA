// Differential fixture for the Goose count_equal device computation.
// The oracle is the verbatim native kernel from count-equal.cu (see
// reference/count-equal-kernel.cuh). The host launch plan is computed here with
// the native formula so both arms receive the same grid and chunk size; the
// plan itself is shared launch geometry and not part of this module.
#include "common.cuh"
#include "count-equal.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ int32_t fg273d_ld_i32(uint64_t p,uint64_t o){ return *(const int32_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ int32_t fg273d_shfl_xor(int32_t v,int32_t m){ return __shfl_xor_sync(0xffffffff, v, m); }
static __device__ __forceinline__ void    fg273d_atomic_add(uint64_t p,int32_t v){ atomicAdd((int*)(uintptr_t)p, v); }
static __device__ __forceinline__ void    fg273d_fail(int64_t){ asm("trap;"); }
#include "count-equal-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const int * x, const int * y, int64_t * dst, int64_t dk, int64_t k){
    FG273D_CB_count_equal((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)y,(uint64_t)(uintptr_t)dst,
                          dk,k,(int64_t)blockIdx.x,(int64_t)threadIdx.x,0);
}

static int checks, total_fails, launch_fail;
static unsigned first_info[8];

static void plan(int64_t ne,int nsm,int64_t&dne,int64_t&blocks){
    const int64_t q=(ne+4*nsm-1)/(4*nsm);
    dne=((q+127)/128)*128;
    const int64_t b=(ne+127)/128;
    blocks = b < 4*nsm ? b : 4*nsm;
}

int main(void){
    int ndev=0;
    cudaDeviceProp props{};
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0); cudaGetDeviceProperties(&props,0);
    const int nsm=props.multiProcessorCount;
    std::mt19937_64 rng(273011);
    const int64_t nes[9]={1,31,32,127,128,129,1000,8192,100003};
    const int patterns=4;
    for (int c=0;c<9;++c){
        const int64_t ne=nes[c];
        int64_t dne=0,blocks=0; plan(ne,nsm,dne,blocks);
        std::vector<int> hx((size_t)ne),hy((size_t)ne);
        for (int64_t i=0;i<ne;++i) hx[(size_t)i]=(int)(rng() & 0xffff);
        for (int pat=0;pat<patterns;++pat){
            for (int64_t i=0;i<ne;++i){
                int v=(int)(rng() & 0xffff);
                switch(pat){
                    case 0: hy[(size_t)i]=hx[(size_t)i]; break;
                    case 1: hy[(size_t)i]=v; break;
                    case 2: hy[(size_t)i]=(i%3==0)?hx[(size_t)i]:v; break;
                    default: hy[(size_t)i]=hx[(size_t)i]+(int)(i%2); break;
                }
            }
            int *dx=nullptr; int *dy=nullptr; int64_t *dn=nullptr; int64_t *dg=nullptr;
            cudaMalloc(&dx,(size_t)ne*4); cudaMalloc(&dy,(size_t)ne*4);
            cudaMalloc(&dn,8); cudaMalloc(&dg,8);
            cudaMemcpy(dx,hx.data(),(size_t)ne*4,cudaMemcpyHostToDevice);
            cudaMemcpy(dy,hy.data(),(size_t)ne*4,cudaMemcpyHostToDevice);
            cudaMemset(dn,0,8); cudaMemset(dg,0,8);
            if (blocks>0){
                count_equal_ref<int><<<(unsigned)blocks,32>>>(dx,dy,dn,dne,ne);
                goose_kernel<<<(unsigned)blocks,32>>>(dx,dy,dg,dne,ne);
            }
            cudaError_t e=cudaDeviceSynchronize();
            if (e!=cudaSuccess){ printf("LAUNCH FAIL ne=%lld pat=%d: %s\n",(long long)ne,pat,cudaGetErrorString(e)); ++launch_fail; }
            int64_t a=0,b=0;
            cudaMemcpy(&a,dn,8,cudaMemcpyDeviceToHost); cudaMemcpy(&b,dg,8,cudaMemcpyDeviceToHost);
            ++checks;
            if (a!=b){
                ++total_fails;
                if (total_fails==1){ first_info[0]=(unsigned)c; first_info[1]=(unsigned)pat; first_info[2]=(unsigned)a; first_info[3]=(unsigned)b; }
            }
            cudaFree(dx);cudaFree(dy);cudaFree(dn);cudaFree(dg);
        }
    }
    if (total_fails) printf("FIRST FAIL ne[%u] pat[%u] native=%u goose=%u\n",first_info[0],first_info[1],first_info[2],first_info[3]);
    printf("FG273D_GPU_FIXTURE checks=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
