#pragma once
#include "vector-types-generated.cuh"
static_assert(sizeof(fg175v_V4)==16,"Wrong emitted V4 ABI");
static __device__ __forceinline__ float fg175v_ld_f32(uint64_t p,uint64_t a){return*(const float*)((const char*)(uintptr_t)p+a);}
static __device__ __forceinline__ void fg175v_st_f32(uint64_t p,uint64_t a,float v){*(float*)((char*)(uintptr_t)p+a)=v;}
static __device__ __forceinline__ uint32_t fg175v_ld_u16(uint64_t p,uint64_t a){return*(const uint16_t*)((const char*)(uintptr_t)p+a);}
static __device__ __forceinline__ int32_t fg175v_ld_i32(uint64_t p,uint64_t a){return*(const int32_t*)((const char*)(uintptr_t)p+a);}
static __device__ __forceinline__ float fg175v_h2f(uint32_t w){uint16_t bits=(uint16_t)w;half h;memcpy(&h,&bits,2);return __half2float(h);}
static __device__ __forceinline__ int32_t fg175v_dp4a(int32_t a,int32_t b,int32_t c){return __dp4a(a,b,c);}
static __device__ __forceinline__ float fg175v_expf(float x){return expf(x);}
static __device__ __forceinline__ float fg175v_powf(float x,int32_t e){return powf(x,(float)e);}
static __device__ __forceinline__ float fg175v_fmaxf(float a,float b){return fmaxf(a,b);}
static __device__ __forceinline__ int32_t fg175v_round_i8(float x){return(int8_t)roundf(x);}
static __device__ __forceinline__ float fg175v_shfl_xor(float x,int32_t o){return __shfl_xor_sync(0xffffffff,x,o,32);}
static __device__ __forceinline__ void fg175v_syncwarp(){ggml_cuda_syncwarp();}
static __device__ __forceinline__ void fg175v_barrier(){__syncthreads();}
static __device__ __forceinline__ void fg175v_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg175v_sync(){ggml_cuda_pdl_sync();}
static __device__ __forceinline__ float fg175v_sm_f32(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg175v_sm_st_f32(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ int32_t fg175v_sm_i32(uint64_t p,uint64_t i){return((const int32_t*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg175v_sm_st_i32(uint64_t p,uint64_t i,int32_t v){((int32_t*)(uintptr_t)p)[i]=v;}

static __device__ __forceinline__ float fg175v_mul_rn(float a,float b){return __fmul_rn(a,b);}
static __device__ __forceinline__ float fg175v_fma_rn(float a,float b,float c){return __fmaf_rn(a,b,c);}

static __device__ __forceinline__ void fg175v_trace(int32_t slot,float v){fg175v_trace_ptr[40960+(blockIdx.z*128+threadIdx.y*32+threadIdx.x)*20+slot]=v;}
