#pragma once
#include "plan.h"
static __host__ __device__ __forceinline__ uint64_t fg171r_project(const fg171r_plan*p,int64_t k,int64_t i){switch(k){case 0:return p->ptrs[i];case 1:return p->dims[i/4][i%4];case 2:return p->strides[i/4][i%4];case 3:return p->iv[i];default:
#ifdef __CUDA_ARCH__
 asm("trap;");return 0;
#else
 GGML_ABORT("Invalid RoPE plan read");
#endif
}}
static __device__ __forceinline__ uint64_t fg171r_dget(int64_t p,int64_t k,int64_t i){return fg171r_project((const fg171r_plan*)(uintptr_t)p,k,i);}
static __device__ __forceinline__ float fg171r_dfl(int64_t p,int64_t i){return((const fg171r_plan*)(uintptr_t)p)->fv[i];}
static __device__ __forceinline__ float fg171r_f32_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg171r_f32_store(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg171r_f16_load(uint64_t p,uint64_t i){return __half2float(((const half*)(uintptr_t)p)[i]);}
static __device__ __forceinline__ void fg171r_f16_store(uint64_t p,uint64_t i,float v){((half*)(uintptr_t)p)[i]=__float2half_rn(v);}
static __device__ __forceinline__ int32_t fg171r_i32_load(uint64_t p,uint64_t i){return((const int32_t*)(uintptr_t)p)[i];}
static __device__ __forceinline__ float fg171r_sin(float v){return sinf(v);}
static __device__ __forceinline__ float fg171r_cos(float v){return cosf(v);}
static __device__ __forceinline__ float fg171r_pow(float a,float b){return powf(a,b);}
static __device__ __forceinline__ float fg171r_log(float v){return logf(v);}
static __device__ __forceinline__ void fg171r_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg171r_psync(){ggml_cuda_pdl_sync();}
