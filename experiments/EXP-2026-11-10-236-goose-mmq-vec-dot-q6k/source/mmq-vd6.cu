// GPU differential + timing for the Goose Q6_K inner product (EXP236).
#include "common.cuh"
#include "vecdotq.cuh"
#include "ggml-common.h"
#include <cstdio>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

static __device__ __forceinline__ void    fg236k_fail(int64_t){ asm("trap;"); }
static __device__ __forceinline__ int64_t fg236k_ld_i32(uint64_t b,int64_t i){ return (int64_t)((const int32_t*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ float   fg236k_ld_f32(uint64_t b,int64_t i){ return ((const float*)(uintptr_t)b)[i]; }
static __device__ __forceinline__ float   fg236k_to_f32(int64_t v){ return (float)v; }
static __device__ __forceinline__ int64_t gg_div(int64_t a,int64_t b){ if(!b) fg236k_fail(1); return a/b; }
static __device__ __forceinline__ int64_t gg_mod(int64_t a,int64_t b){ if(!b) fg236k_fail(1); return a%b; }
static __device__ __forceinline__ int64_t fg236k_dp4a(int64_t a,int64_t b,int64_t c){ return (int64_t)__dp4a((int)a,(int)b,(int)c); }

#include "vd6-device-generated.cuh"

static __device__ __forceinline__ float fg236_goose(const int*v,const int*u,const int8_t*sc,
                                                    const float&d6,const float*d8,int64_t n){
    return (float) gg_function_1((uint64_t)(uintptr_t)v,(uint64_t)(uintptr_t)u,(uint64_t)(uintptr_t)sc,
                                 d6,(uint64_t)(uintptr_t)d8,n,0);
}
static __global__ void check_k(const int*v,const int*u,const int8_t*sc,const float*d6,
                               const float*d8,float*on,float*og,int n,int nsteps){
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=n) return;
    on[i]=vec_dot_q6_K_q8_1_impl_mmq(v+i*16,u+i*16,sc+i*8,d6[i],d8+i*4);
    og[i]=fg236_goose(v+i*16,u+i*16,sc+i*8,d6[i],d8+i*4,nsteps);
}
static __global__ void time_k(const int*v,const int*u,const int8_t*sc,const float*d6,
                              const float*d8,float*sink,int n,int nsteps,int rep,int arm){
    const int i=blockIdx.x*blockDim.x+threadIdx.x;
    if(i>=n) return;
    float acc=0;
    for(int r=0;r<rep;++r)
        acc += arm ? fg236_goose(v+i*16,u+i*16,sc+i*8,d6[i],d8+i*4,nsteps)
                   : vec_dot_q6_K_q8_1_impl_mmq(v+i*16,u+i*16,sc+i*8,d6[i],d8+i*4);
    sink[i]=acc;
}
int main(){
    { int nd=0; if(cudaGetDeviceCount(&nd)!=cudaSuccess||nd<1){fprintf(stderr,"no CUDA device\n");return 77;} }
    cudaSetDevice(0);
    const int n=1<<16;
    std::vector<int32_t> v((size_t)n*16), u((size_t)n*16);
    std::vector<int8_t> sc((size_t)n*8);
    std::vector<float> d6((size_t)n), d8((size_t)n*4);
    std::mt19937 rng(236777);
    for(auto&x:v) x=(int32_t)rng();
    for(auto&x:u) x=(int32_t)rng();
    for(auto&x:sc) x=(int8_t)(rng()%256);
    for(auto&x:d6) x=(float)((int)(rng()%2001)-1000)/512.f;
    for(auto&x:d8) x=(float)((int)(rng()%2001)-1000)/64.f;
    int32_t *dv,*du; int8_t *dsc; float *dd6,*dd8,*on,*og,*sink;
    cudaMalloc(&dv,v.size()*4); cudaMalloc(&du,u.size()*4); cudaMalloc(&dsc,sc.size());
    cudaMalloc(&dd6,d6.size()*4); cudaMalloc(&dd8,d8.size()*4);
    cudaMalloc(&on,(size_t)n*4); cudaMalloc(&og,(size_t)n*4); cudaMalloc(&sink,(size_t)n*4);
    cudaMemcpy(dv,v.data(),v.size()*4,cudaMemcpyHostToDevice);
    cudaMemcpy(du,u.data(),u.size()*4,cudaMemcpyHostToDevice);
    cudaMemcpy(dsc,sc.data(),sc.size(),cudaMemcpyHostToDevice);
    cudaMemcpy(dd6,d6.data(),d6.size()*4,cudaMemcpyHostToDevice);
    cudaMemcpy(dd8,d8.data(),d8.size()*4,cudaMemcpyHostToDevice);
    const int tpb=256, blocks=(n+tpb-1)/tpb;
    for (int nsteps : {1,2}) {
        check_k<<<blocks,tpb>>>(dv,du,dsc,dd6,dd8,on,og,n,nsteps);
        auto e=cudaDeviceSynchronize();
        if(e!=cudaSuccess){fprintf(stderr,"launch: %s\n",cudaGetErrorString(e));return 1;}
        std::vector<float> hn(n),hg(n);
        cudaMemcpy(hn.data(),on,(size_t)n*4,cudaMemcpyDeviceToHost);
        cudaMemcpy(hg.data(),og,(size_t)n*4,cudaMemcpyDeviceToHost);
        int bad=0; double worst=0;
        for(int i=0;i<n;++i){
            const double d=std::fabs((double)hn[i]-(double)hg[i]);
            const double tol=1e-3*(1.0+std::fabs((double)hn[i]));
            if(d>tol) ++bad;
            worst=std::max(worst,d/(1.0+std::fabs((double)hn[i])));
        }
        printf("FG236K_KERNEL_MATCH nsteps=%d n=%d bad=%d worst_rel=%.3e\n",nsteps,n,bad,worst);
    }
    const int rep=64, nsteps=2;
    for (int arm=0; arm<2; ++arm) {
        std::vector<double> s;
        for(int it=0;it<20;++it){
            cudaEvent_t a,b; cudaEventCreate(&a); cudaEventCreate(&b);
            cudaEventRecord(a);
            time_k<<<blocks,tpb>>>(dv,du,dsc,dd6,dd8,sink,n,nsteps,rep,arm);
            cudaEventRecord(b); cudaEventSynchronize(b);
            float ms=0; cudaEventElapsedTime(&ms,a,b);
            s.push_back(ms*1e6/rep); cudaEventDestroy(a); cudaEventDestroy(b);
        }
        std::sort(s.begin(),s.end());
        printf("FG236K_TIMING arm=%s median_ns=%.1f min=%.1f max=%.1f\n",arm?"goose":"native",
               s[s.size()/2], s.front(), s.back());
    }
    return 0;
}
