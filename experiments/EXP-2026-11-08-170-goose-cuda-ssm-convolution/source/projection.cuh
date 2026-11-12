#pragma once
#include "plan.h"
static uint64_t fg170s_project(const fg170s_plan*p,int64_t k,int64_t i){switch(k){case 0:GGML_ASSERT(i>=0&&i<4);return p->ptrs[i];case 1:GGML_ASSERT(i>=0&&i<16);return p->dims[i/4][i%4];case 2:GGML_ASSERT(i>=0&&i<16);return p->strides[i/4][i%4];case 3:GGML_ASSERT(i>=0&&i<16);return p->iv[i];default:GGML_ABORT("Invalid SSM plan read");}}
static __device__ __forceinline__ float fg170s_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg170s_store(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg170s_expf(float x){return expf(x);}
static __device__ __forceinline__ void fg170s_barrier(){__syncthreads();}
static __device__ __forceinline__ void fg170s_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg170s_sync(){ggml_cuda_pdl_sync();}
