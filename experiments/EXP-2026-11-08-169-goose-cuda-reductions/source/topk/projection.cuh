#pragma once
#include "plan.h"
static __host__ __device__ __forceinline__ uint64_t fg169t_project(const fg169t_plan*p,int64_t k,int64_t i){if(k==0)return p->ptrs[i];if(k==3)return p->iv[i];
#ifdef __CUDA_ARCH__
 asm("trap;");return 0;
#else
 GGML_ABORT("Invalid topk field");
#endif
}
static __device__ __forceinline__ uint64_t fg169t_di(int64_t p,int64_t k,int64_t i){return fg169t_project((const fg169t_plan*)(uintptr_t)p,k,i);}
static __device__ __forceinline__ float fg169t_df(int64_t p,int64_t i){return((const fg169t_plan*)(uintptr_t)p)->fv[i];}
static __device__ __forceinline__ float fg169t_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg169t_store(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ void fg169t_storei(uint64_t p,uint64_t i,int32_t v){((int32_t*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg169t_exp(float v){return expf(v);}
static __device__ __forceinline__ float fg169t_sqrt(float v){return sqrtf(v);}
static __device__ __forceinline__ float fg169t_log(float v){return logf(v);}
static __device__ __forceinline__ float fg169t_fmax(float a,float b){return fmaxf(a,b);}
static __device__ __forceinline__ float fg169t_fmin(float a,float b){return fminf(a,b);}
static __device__ __forceinline__ float fg169t_max(float a,float b){return max(a,b);}
static __device__ __forceinline__ float fg169t_shuffle(float v,int32_t off){return __shfl_xor_sync(0xffffffff,v,off,32);}
static __device__ __forceinline__ float fg169t_shufflei(float v,int32_t lane){return __shfl_sync(0xffffffff,v,lane,32);}
static __device__ __forceinline__ int32_t fg169t_ishuffle(int32_t v,int32_t off){return __shfl_xor_sync(0xffffffff,v,off,32);}
static __device__ __forceinline__ uint64_t fg169t_ballot(uint8_t v){return __ballot_sync(0xffffffff,v!=0);}
static __device__ __forceinline__ void fg169t_barrier(){__syncthreads();}
static __device__ __forceinline__ void fg169t_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void fg169t_lc(){ggml_cuda_pdl_lc();}
