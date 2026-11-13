#pragma once
#include "mma-process-types-generated.cuh"
static __device__ __forceinline__ float fg175p_gload(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg175p_gstore(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ uint32_t fg175p_ldu32(uint64_t p,uint64_t i){return((const uint32_t*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg175p_stu32(uint64_t p,uint64_t i,uint32_t v){((uint32_t*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ uint32_t fg175p_smld(uint64_t p,uint64_t i){return((const uint32_t*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg175p_smst(uint64_t p,uint64_t i,uint32_t v){((uint32_t*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ float fg175p_smldf(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg175p_smstf(uint64_t p,uint64_t i,float v){((float*)(uintptr_t)p)[i]=v;}
static __device__ __forceinline__ uint32_t fg175p_h2mul(uint32_t a,uint32_t b){return fg176i_mulh2(a,b);}
static __device__ __forceinline__ uint32_t fg175p_h2pack(float a,float b){return fg176i_h2(a,b);}
static __device__ __forceinline__ float fg175p_hlo(uint32_t a){return fg176i_hlo(a);}
static __device__ __forceinline__ float fg175p_hhi(uint32_t a){return fg176i_hhi(a);}
static __device__ __forceinline__ fg175p_U4 fg175p_ldm4(uint64_t a){auto x=fg176i_ldm4(a);return{x.f0,x.f1,x.f2,x.f3};}
static __device__ __forceinline__ float fg175p_shflx(float v,int32_t o){return fg176i_shflx(v,o);}
static __device__ __forceinline__ float fg175p_expf(float v){return fg176i_expf(v);}
static __device__ __forceinline__ void fg175p_barrier(){__syncthreads();}
static __device__ __forceinline__ void fg175m_iter(uint64_t k,uint64_t v,uint64_t m,int64_t sk,int64_t sv,int64_t sm,float slope,int32_t jt,int32_t kb,int64_t nq,int32_t nc,uint64_t q,uint64_t va,uint64_t mx,uint64_t rs,uint64_t shared,int32_t tx,int32_t ty,int32_t np,uint8_t last){FG175I_ITER(k,v,m,sk,sv,sm,slope,jt,kb,nq,nc,q,va,mx,rs,shared,tx,ty,np,last,1);}
