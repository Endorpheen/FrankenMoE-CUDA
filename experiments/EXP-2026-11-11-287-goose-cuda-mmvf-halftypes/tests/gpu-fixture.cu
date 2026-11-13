// Differential fixture for mul_mat_vec_f with half/bfloat16 sources.
#include "ggml.h"
#include "common.cuh"
#include "unary.cuh"
#include "mmvf.cuh"
#include "mmf.cuh"
#include "convert.cuh"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cuda_runtime.h>
#include <vector>
#include <random>

static __device__ __forceinline__ float    fg287m_ld_f32(uint64_t p,uint64_t o){ return *(const float*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ float    fg287m_ld_half(uint64_t p,uint64_t o){ return __half2float(*(const half*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ float    fg287m_ld_bf16(uint64_t p,uint64_t o){ return __bfloat162float(*(const nv_bfloat16*)((const char*)(uintptr_t)p+o)); }
static __device__ __forceinline__ int32_t  fg287m_ld_i32(uint64_t p,uint64_t o){ return *(const int32_t*)((const char*)(uintptr_t)p+o); }
static __device__ __forceinline__ void     fg287m_st_f32(uint64_t p,uint64_t o,float v){ *(float*)((char*)(uintptr_t)p+o)=v; }
static __device__ __forceinline__ float    fg287m_mad(float acc,float v,float u){ return __fmaf_rn(v,u,acc); }
static __device__ __forceinline__ float    fg287m_shfl_xor(float v,int32_t o,int32_t w){ return __shfl_xor_sync(0xffffffffu,v,o,w); }
extern __shared__ float fg287shm[];
static __device__ __forceinline__ void fg287m_shm_put(int32_t s,float v){ fg287shm[s]=v; }
static __device__ __forceinline__ float fg287m_shm_get(int32_t s){ return fg287shm[s]; }
static __device__ __forceinline__ void fg287m_barrier(){ __syncthreads(); }
static __device__ __forceinline__ void fg287m_fail(int64_t){ asm("trap;"); }
extern "C" void ggml_abort(const char * file, int line, const char * fmt, ...) {
    fprintf(stderr, "ggml_abort %s:%d %s\n", file, line, fmt);
    abort();
}
#include "mmvf-kernel.cuh"
#include "device-functions-generated.cuh"
#include "entries.h"

static __global__ void goose_kernel(const void * x, const float * y, float * dst, int dtype,
        int64_t ncols2,int64_t stride_row,int64_t scy2,int64_t scdst,
        int64_t scx,int64_t scy,int64_t scd,int64_t ssx,int64_t ssy,int64_t ssd,
        int64_t nchy,int64_t cr,int64_t sr,float* dbg){
    FG287D_CB_mmvf((uint64_t)(uintptr_t)x,(uint64_t)(uintptr_t)y,(uint64_t)(uintptr_t)dst,(uint64_t)(uintptr_t)dbg,
        (int64_t)dtype,ncols2,stride_row,scy2,scdst,scx,scy,scd,ssx,ssy,ssd,nchy,cr,sr,false,0,
        (int64_t)blockIdx.x,(int64_t)blockIdx.y,(int64_t)blockIdx.z,
        (int64_t)blockDim.x,(int64_t)threadIdx.x,0);
}

static int checks, total_fails, launch_fail;

struct Case { int64_t ncols, nrows, nch, ns; };

int main(void){
    int ndev=0; cudaDeviceProp props{};
    if (cudaGetDeviceCount(&ndev)!=cudaSuccess || ndev<1){ fprintf(stderr,"no CUDA device\n"); return 77; }
    cudaSetDevice(0); cudaGetDeviceProperties(&props,0);
    std::mt19937_64 rng(287011);
    const Case cases[8]={
        {256,8,1,1},{512,4,2,2},{2048,2,1,1},{4096,2,2,1},{4,8,2,1},{8192,2,1,2},{4096,257,3,2},{8192,64,4,1},
    };
    for (int c=0;c<8;++c){
        const Case& k=cases[c];
        const int64_t ncols2=k.ncols/2;
        const int64_t stride_row=k.ncols;
        const int64_t scx=k.nrows*k.ncols, ssx=k.nch*k.nrows*k.ncols;
        const int64_t scy=k.ncols, ssy=k.nch*k.ncols;
        const int64_t scd=k.nrows, ssd=k.nch*k.nrows;
        const size_t nx=(size_t)(k.nch*k.nrows*k.ncols*k.ns);
        const size_t ny=(size_t)(k.nch*k.ncols*k.ns);
        const size_t nd=(size_t)(k.nrows*k.nch*k.ns);
        std::vector<float> hy(ny); for (auto&v:hy) v=(float)((int64_t)(rng()%2001)-1000)*0.002f;
        dim3 grid((unsigned)k.nrows,(unsigned)k.nch,(unsigned)k.ns);
        for (int dtype=1; dtype<=2; ++dtype){
            std::vector<half> hxh(nx); std::vector<nv_bfloat16> hxb(nx);
            std::vector<float> hx(nx); for (auto&v:hx) v=(float)((int64_t)(rng()%2001)-1000)*0.002f;
            for (size_t i=0;i<nx;++i){ hxh[i]=__float2half(hx[i]); hxb[i]=__float2bfloat16(hx[i]); }
            void* dx=nullptr; float* dy=nullptr; float* dn=nullptr; float* dg=nullptr;
            cudaMalloc(&dx,nx*2); cudaMalloc(&dy,ny*4); cudaMalloc(&dn,nd*4); cudaMalloc(&dg,nd*4);
            cudaMemcpy(dx, dtype==1?(void*)hxh.data():(void*)hxb.data(), nx*2, cudaMemcpyHostToDevice);
            cudaMemcpy(dy,hy.data(),ny*4,cudaMemcpyHostToDevice);
            cudaMemset(dn,0xFF,nd*4); cudaMemset(dg,0xFF,nd*4);
            const uint3 nchy=make_uint3((unsigned)k.nch,0,0);
            const uint3 cr=make_uint3(1,0,0), sr=make_uint3(1,0,0);
            ggml_cuda_mm_fusion_args_device fusion{};
            if (dtype==1){
                mul_mat_vec_f_ref<half,float,1,256,false,false><<<grid,256,128>>>(
                    (const half*)dx,dy,nullptr,fusion,dn,(int)ncols2,nchy,(int)stride_row,(int)(k.ncols/2),(int)scd,
                    cr,(int)scx,(int)scy,(int)scd,sr,(int)ssx,(int)ssy,(int)ssd,0);
            }else{
                mul_mat_vec_f_ref<nv_bfloat16,float,1,256,false,false><<<grid,256,128>>>(
                    (const nv_bfloat16*)dx,dy,nullptr,fusion,dn,(int)ncols2,nchy,(int)stride_row,(int)(k.ncols/2),(int)scd,
                    cr,(int)scx,(int)scy,(int)scd,sr,(int)ssx,(int)ssy,(int)ssd,0);
            }
            goose_kernel<<<grid,256,128>>>(dx,dy,dg,dtype,(int64_t)ncols2,(int64_t)stride_row,(int64_t)(k.ncols/2),(int64_t)scd,
                (int64_t)scx,(int64_t)scy,(int64_t)scd,(int64_t)ssx,(int64_t)ssy,(int64_t)ssd,(int64_t)k.nch,1,1,nullptr);
            cudaError_t e=cudaDeviceSynchronize();
            if (e!=cudaSuccess){ printf("LAUNCH FAIL c=%d dtype=%d: %s\n",c,dtype,cudaGetErrorString(e)); ++launch_fail; }
            std::vector<float> a(nd),b(nd);
            cudaMemcpy(a.data(),dn,nd*4,cudaMemcpyDeviceToHost); cudaMemcpy(b.data(),dg,nd*4,cudaMemcpyDeviceToHost);
            for (size_t i=0;i<nd;++i){ ++checks; if (memcmp(&a[i],&b[i],4)){ ++total_fails; if(total_fails==1) printf("FIRST FAIL c=%d dtype=%d i=%zu nat=%a goo=%a\n",c,dtype,i,a[i],b[i]); } }
            cudaFree(dx);cudaFree(dy);cudaFree(dn);cudaFree(dg);
        }
    }
    {
        const int64_t k_ncols=2,k_nrows=1,k_nch=2,k_ns=1;
        std::vector<float> x{1,2, 3,4};
        std::vector<float> y{5,6, 7,8};
        std::vector<float> outn(2,-1), outg(2,-1);
        float *dx=nullptr,*dy=nullptr,*dn=nullptr,*dg=nullptr,*db=nullptr;
        cudaMalloc(&dx,16);cudaMalloc(&dy,16);cudaMalloc(&dn,8);cudaMalloc(&dg,32);cudaMalloc(&db,32);
        cudaMemcpy(dx,x.data(),16,cudaMemcpyHostToDevice);cudaMemcpy(dy,y.data(),16,cudaMemcpyHostToDevice);
        cudaMemset(dn,0xFF,8);cudaMemset(dg,0xFF,32);cudaMemset(db,0,32);
        dim3 g(1,2,1);
        ggml_cuda_mm_fusion_args_device fu{};
        mul_mat_vec_f_ref<float,float,1,256,false,false><<<g,256,128>>>((const float*)dx,dy,nullptr,fu,dn,1,make_uint3(2,0,0),2,1,1,make_uint3(1,0,0),2,2,1,make_uint3(1,0,0),4,2,1,0);
        goose_kernel<<<g,256,128>>>(dx,dy,dg,0,1,2,1,1,2,2,1,4,2,1,2,1,1,db);
        cudaDeviceSynchronize();
        cudaMemcpy(outn.data(),dn,8,cudaMemcpyDeviceToHost);cudaMemcpy(outg.data(),dg,32,cudaMemcpyDeviceToHost);
        unsigned dbits[8]={0}; cudaMemcpy(dbits,db,32,cudaMemcpyDeviceToHost);
        printf("MICRO nat=[%g %g] goo=[%g %g] expect=[17 53] dbg=%08x %08x %08x %08x\n",outn[0],outn[1],outg[0],outg[1],dbits[0],dbits[1],dbits[2],dbits[3]);
        cudaFree(dx);cudaFree(dy);cudaFree(dn);cudaFree(dg);cudaFree(db);
    }
    for (int bs=32; bs<=256; bs*=2){
        std::vector<float> x{1,2, 3,4};
        std::vector<float> y{5,6, 7,8};
        std::vector<float> outg(8,-1);
        float *dx=nullptr,*dy=nullptr,*dg=nullptr;
        cudaMalloc(&dx,16);cudaMalloc(&dy,16);cudaMalloc(&dg,32);
        cudaMemcpy(dx,x.data(),16,cudaMemcpyHostToDevice);cudaMemcpy(dy,y.data(),16,cudaMemcpyHostToDevice);
        cudaMemset(dg,0xFF,32);
        dim3 g(1,2,1);
        goose_kernel<<<g,bs,128>>>(dx,dy,dg,0,1,2,1,1,2,2,1,4,2,1,2,1,1,nullptr);
        cudaError_t e=cudaDeviceSynchronize();
        cudaMemcpy(outg.data(),dg,32,cudaMemcpyDeviceToHost);
        printf("MICRO3 bs=%d goo=[%g %g] sum=%g xa=%g ya=%g xc=%g yc=%g expect=[17 53] err=%s\n",bs,outg[0],outg[1],outg[2],outg[3],outg[4],outg[5],outg[6],cudaGetErrorString(e));
        cudaFree(dx);cudaFree(dy);cudaFree(dg);
    }
    printf("FG287D_GPU_FIXTURE values=%d failures=%d launch_fail=%d\n",checks,total_fails,launch_fail);
    return (total_fails||launch_fail)?1:0;
}
