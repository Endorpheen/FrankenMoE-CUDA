#pragma once
#include "plan.h"
static __host__ __device__ __forceinline__ uint64_t fg168_project(const fg168_plan*p,int64_t k,int64_t i){switch(k){case 0:return p->ptrs[i];case 1:return p->dims[i/4][i%4];case 2:return p->strides[i/4][i%4];case 3:return p->iv[i];case 4:return p->magic[i/3][i%3];default:
#ifdef __CUDA_ARCH__
 asm("trap;");return 0;
#else
 GGML_ABORT("Invalid normalization field");
#endif
}}
static __device__ __forceinline__ uint64_t fg168_di(int64_t p,int64_t k,int64_t i){return fg168_project((const fg168_plan*)(uintptr_t)p,k,i);}
static __device__ __forceinline__ float fg168_df(int64_t p,int64_t i){return((const fg168_plan*)(uintptr_t)p)->fv[i];}
static __device__ __forceinline__ uint32_t fg168_dm(int64_t p,int64_t i,int64_t k){return((const fg168_plan*)(uintptr_t)p)->magic[i][k];}
static __device__ __forceinline__ uint32_t fg168_umulhi(uint32_t a,uint32_t b){return __umulhi(a,b);}
static __device__ __forceinline__ float fg168_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg168_store(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg168_shuffle(float v,int32_t off){return __shfl_xor_sync(0xffffffff,v,off,32);}
static __device__ __forceinline__ void fg168_barrier(){__syncthreads();}
static __device__ __forceinline__ float fg168_rsqrt(float v){return rsqrtf(v);}
static __device__ __forceinline__ float fg168_fma(float a,float b,float c){return fmaf(a,b,c);}
static __device__ __forceinline__ float fg168_max(float a,float b){return fmaxf(a,b);}
static __device__ __forceinline__ void fg168_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg168_sync(){ggml_cuda_pdl_sync();}
