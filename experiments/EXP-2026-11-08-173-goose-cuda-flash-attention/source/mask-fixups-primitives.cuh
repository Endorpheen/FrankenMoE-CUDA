#pragma once
static __device__ __forceinline__ uint32_t fg168f_ld_u32(uint64_t p,uint64_t b){return*(const uint32_t*)((const char*)(uintptr_t)p+b);}
static __device__ __forceinline__ float fg168f_ld_f32(uint64_t p,uint64_t b){return*(const float*)((const char*)(uintptr_t)p+b);}
static __device__ __forceinline__ void fg168f_st_f32(uint64_t p,uint64_t b,float v){*(float*)((char*)(uintptr_t)p+b)=v;}
static __device__ __forceinline__ void fg168f_st_i32(uint64_t p,uint64_t b,int32_t v){*(int32_t*)((char*)(uintptr_t)p+b)=v;}
static __device__ __forceinline__ int32_t fg168f_sm_i32(uint64_t p,uint64_t w){return((const int32_t*)(uintptr_t)p)[w];}
static __device__ __forceinline__ void fg168f_sm_st_i32(uint64_t p,uint64_t w,int32_t v){((int32_t*)(uintptr_t)p)[w]=v;}
static __device__ __forceinline__ int32_t fg168f_shfl_xor_i32(int32_t v,int32_t o){return __shfl_xor_sync(0xffffffff,v,o,32);}
static __device__ __forceinline__ void fg168f_barrier(){__syncthreads();}
static __device__ __forceinline__ void fg168f_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ void fg168f_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ float fg168f_expf(float x){return expf(x);}
static __device__ __forceinline__ float fg168f_fmaxf(float a,float b){return fmaxf(a,b);}
