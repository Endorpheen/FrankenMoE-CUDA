#pragma once
#include "plan.h"
static __host__ __device__ __forceinline__ uint64_t fg167_project(const fg167_plan*p,int64_t k,int64_t i){if(k==0)return p->ptrs[i];if(k>=1&&k<=3)return p->dims[k-1][i];if(k>=4&&k<=6)return p->strides[k-4][i];switch(k){case 7:return p->magic[i/3][i%3];case 8:return p->grid[i];case 9:return p->threads[i];case 10:return p->kind_a;case 11:return p->kind_b;case 12:return p->kind_d;case 13:return p->op;case 14:return p->fuse;case 15:return p->path;case 16:return p->mix;default:
#ifdef __CUDA_ARCH__
 asm("trap;");return 0;
#else
 GGML_ABORT("Invalid broadcast field");
#endif
}}
static __device__ __forceinline__ uint64_t fg167_dget(int64_t p,int64_t k,int64_t i){return fg167_project((const fg167_plan*)(uintptr_t)p,k,i);}
static __device__ __forceinline__ uint32_t fg167_dmagic(int64_t p,int64_t i,int64_t k){return((const fg167_plan*)(uintptr_t)p)->magic[i][k];}
static __device__ __forceinline__ uint32_t fg167_umulhi(uint32_t a,uint32_t b){return __umulhi(a,b);}
static __device__ __forceinline__ float fg167_load(int64_t kind,uint64_t p,uint64_t off){const char*q=(const char*)(uintptr_t)p+off;if(kind==0)return*(const float*)q;if(kind==1)return ggml_cuda_cast<float>(*(const half*)q);return ggml_cuda_cast<float>(*(const nv_bfloat16*)q);}
static __device__ __forceinline__ void fg167_store(int64_t kind,uint64_t p,uint64_t off,float v){char*q=(char*)(uintptr_t)p+off;if(kind==0)*(float*)q=v;else if(kind==1)*(half*)q=ggml_cuda_cast<half>(v);else*(nv_bfloat16*)q=ggml_cuda_cast<nv_bfloat16>(v);}
static __device__ __forceinline__ void fg167_lc(){ggml_cuda_pdl_lc();}
static __device__ __forceinline__ void fg167_sync(){ggml_cuda_pdl_sync();}
