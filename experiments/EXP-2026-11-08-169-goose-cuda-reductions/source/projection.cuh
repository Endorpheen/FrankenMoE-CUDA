#pragma once
#include "plan.h"
static __host__ __device__ __forceinline__ uint64_t fg169_project(const fg169_plan*p,int64_t k,int64_t i){switch(k){case 0:return p->ptrs[i];case 1:return p->dims[i/4][i%4];case 2:return p->strides[i/4][i%4];case 3:return p->iv[i];default:
#ifdef __CUDA_ARCH__
 asm("trap;");return 0;
#else
 GGML_ABORT("Invalid reduction field");
#endif
}}
static __device__ __forceinline__ float fg169r_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg169r_store(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg169r_shuffle(float v,int32_t off){return __shfl_xor_sync(0xffffffff,v,off,32);}
static __device__ __forceinline__ void fg169r_barrier(){__syncthreads();}
static __device__ __forceinline__ void fg169r_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ float fg169a_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg169a_store(uint64_t p,uint64_t i,int32_t v){((int32_t*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg169a_sloadf(uint64_t i){extern __shared__ unsigned char fg169a_shared[];return((float*)fg169a_shared)[i];}
static __device__ __forceinline__ void fg169a_sstoref(uint64_t i,float v){extern __shared__ unsigned char fg169a_shared[];((float*)fg169a_shared)[i]=v;}
static __device__ __forceinline__ int32_t fg169a_sloadi(uint64_t i){extern __shared__ unsigned char fg169a_shared[];return((int32_t*)(fg169a_shared+128))[i];}
static __device__ __forceinline__ void fg169a_sstorei(uint64_t i,int32_t v){extern __shared__ unsigned char fg169a_shared[];((int32_t*)(fg169a_shared+128))[i]=v;}
static __device__ __forceinline__ float fg169a_shuffle(float v,int32_t off){return __shfl_xor_sync(0xffffffff,v,off,32);}
static __device__ __forceinline__ int32_t fg169a_ishuffle(int32_t v,int32_t off){return __shfl_xor_sync(0xffffffff,v,off,32);}
static __device__ __forceinline__ void fg169a_barrier(){__syncthreads();}
