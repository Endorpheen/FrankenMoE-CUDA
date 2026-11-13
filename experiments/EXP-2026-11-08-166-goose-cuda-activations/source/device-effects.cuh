#pragma once
#include "plan.h"
static __device__ __forceinline__ int64_t fg166_di(int64_t p,int64_t k){return((const fg166_plan*)(uintptr_t)p)->iv[k];}
static __device__ __forceinline__ float fg166_df(int64_t p,int64_t k){return((const fg166_plan*)(uintptr_t)p)->fv[k];}
static __device__ __forceinline__ float fg166_load(int64_t kind,int64_t p,int64_t off){const char*q=(const char*)(uintptr_t)p+off;if(kind==0)return*(const float*)q;if(kind==1)return ggml_cuda_cast<float>(*(const half*)q);return ggml_cuda_cast<float>(*(const nv_bfloat16*)q);}
static __device__ __forceinline__ void fg166_store(int64_t kind,int64_t p,int64_t off,float v){char*q=(char*)(uintptr_t)p+off;if(kind==0)*(float*)q=v;else if(kind==1)*(half*)q=ggml_cuda_cast<half>(v);else*(nv_bfloat16*)q=ggml_cuda_cast<nv_bfloat16>(v);}
static __device__ __forceinline__ void fg166_pdl_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg166_pdl_sync(){ggml_cuda_pdl_sync();}
