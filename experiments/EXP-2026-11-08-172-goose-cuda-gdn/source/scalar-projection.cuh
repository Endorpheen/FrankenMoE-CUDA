#pragma once
#include "plan.h"
static __host__ __device__ __forceinline__ uint64_t fg174s_project(const fg174s_plan*p,int64_t k,int64_t i){switch(k){case 0:return p->ptrs[i];case 1:return p->dims[i/4][i%4];case 2:return p->strides[i/4][i%4];case 3:return p->iv[i];case 4:return p->magic[i/3][i%3];default:
#ifdef __CUDA_ARCH__
 asm("trap;");return 0;
#else
 GGML_ABORT("Invalid GDN plan read");
#endif
}}
static __device__ __forceinline__ uint64_t fg174s_dget(int64_t p,int64_t k,int64_t i){return fg174s_project((const fg174s_plan*)(uintptr_t)p,k,i);}
static __device__ __forceinline__ float fg174s_df(int64_t p,int64_t i){return((const fg174s_plan*)(uintptr_t)p)->fv[i];}
static __device__ __forceinline__ uint32_t fg174s_dm(int64_t p,int64_t i,int64_t k){return((const fg174s_plan*)(uintptr_t)p)->magic[i][k];}
static __device__ __forceinline__ float fg174s_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg174s_store(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg174s_fma(float a,float b,float c){return __fmaf_rn(a,b,c);}
static __device__ __forceinline__ float fg174s_mul_rn(float a,float b){return __fmul_rn(a,b);}
static __device__ __forceinline__ float fg174s_expf(float f){return expf(f);}
static __device__ __forceinline__ float fg174s_shfl_xor(float f,int32_t off,int32_t width){return __shfl_xor_sync(0xffffffff,f,off,width);}
static __device__ __forceinline__ uint32_t fg174s_umulhi(uint32_t a,uint32_t b){return __umulhi(a,b);}
static __device__ __forceinline__ void fg174s_sync(){ggml_cuda_pdl_sync();}
