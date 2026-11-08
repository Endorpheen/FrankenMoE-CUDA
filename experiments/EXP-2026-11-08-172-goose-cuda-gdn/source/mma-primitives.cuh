#pragma once
#include "mma-types-generated.cuh"
static_assert(sizeof(fg174m_F4)==16&&sizeof(fg174m_U4)==16&&sizeof(fg174m_U2)==8,"Wrong emitted MMA POD ABI");
static __device__ __forceinline__ float fg174m_load(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ fg174m_F4 fg174m_load4(uint64_t p,uint64_t i){const float4 x=*(const float4*)((const float*)(uintptr_t)p+i);return{x.x,x.y,x.z,x.w};}
static __device__ __forceinline__ void fg174m_store(uint64_t p,uint64_t i,float f){((float*)(uintptr_t)p)[i]=f;}
static __device__ __forceinline__ float fg174m_expf(float f){return expf(f);}
static __device__ __forceinline__ void fg174m_barrier(){__syncthreads();}
static __device__ __forceinline__ float fg174m_shfl_up(float f,int32_t o){return __shfl_up_sync(0xffffffff,f,o,32);}
static __device__ __forceinline__ float fg174m_shfl(float f,int32_t lane){return __shfl_sync(0xffffffff,f,lane,32);}
static __device__ __forceinline__ uint32_t fg174m_sm_u32(uint64_t p,uint64_t i){return((const uint32_t*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg174m_sm_store_u32(uint64_t p,uint64_t i,uint32_t w){((uint32_t*)(uintptr_t)p)[i]=w;}
static __device__ __forceinline__ float fg174m_sm_bf16(uint64_t p,uint64_t i){return __bfloat162float(((const nv_bfloat16*)(uintptr_t)p)[i]);}
static __device__ __forceinline__ void fg174m_sm_store_bf16(uint64_t p,uint64_t i,float f){((nv_bfloat16*)(uintptr_t)p)[i]=__float2bfloat16(f);}
static __device__ __forceinline__ float fg174m_sm_f32(uint64_t p,uint64_t i){return((const float*)(uintptr_t)p)[i];}
static __device__ __forceinline__ void fg174m_sm_store_f32(uint64_t p,uint64_t i,float f){((float*)(uintptr_t)p)[i]=f;}
static __device__ __forceinline__ uint32_t fg174m_cvt2(float a,float b){auto v=__float22bfloat162_rn(make_float2(a,b));uint32_t w;memcpy(&w,&v,4);return w;}
static __device__ __forceinline__ float fg174m_wlo(uint32_t w){nv_bfloat162 v;memcpy(&v,&w,4);return __bfloat1622float2(v).x;}
static __device__ __forceinline__ float fg174m_whi(uint32_t w){nv_bfloat162 v;memcpy(&v,&w,4);return __bfloat1622float2(v).y;}
static __device__ __forceinline__ fg174m_U4 fg174m_ldm4(uint64_t a){uint32_t r0,r1,r2,r3;asm volatile("ldmatrix.sync.aligned.m8n8.x4.b16 {%0, %1, %2, %3}, [%4];":"=r"(r0),"=r"(r1),"=r"(r2),"=r"(r3):"l"(a));return{r0,r1,r2,r3};}
static __device__ __forceinline__ fg174m_U2 fg174m_ldm2(uint64_t a){uint32_t r0,r1;asm volatile("ldmatrix.sync.aligned.m8n8.x2.b16 {%0, %1}, [%2];":"=r"(r0),"=r"(r1):"l"(a));return{r0,r1};}
static __device__ __forceinline__ fg174m_U4 fg174m_ldm4t(uint64_t a){uint32_t r0,r1,r2,r3;asm volatile("ldmatrix.sync.aligned.m8n8.x4.trans.b16 {%0, %1, %2, %3}, [%4];":"=r"(r0),"=r"(r1),"=r"(r2),"=r"(r3):"l"(a));return{r0,r1,r2,r3};}
static __device__ __forceinline__ fg174m_U2 fg174m_ldm2t(uint64_t a){uint32_t r0,r1;asm volatile("ldmatrix.sync.aligned.m8n8.x2.trans.b16 {%0, %1}, [%2];":"=r"(r0),"=r"(r1):"l"(a));return{r0,r1};}
static __device__ __forceinline__ fg174m_F4 fg174m_mma(fg174m_F4 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){uint32_t d0=__float_as_uint(d.FG174M_FX),d1=__float_as_uint(d.FG174M_FY),d2=__float_as_uint(d.FG174M_FZ),d3=__float_as_uint(d.FG174M_FW);asm("mma.sync.aligned.m16n8k16.row.col.f32.bf16.bf16.f32 {%0, %1, %2, %3}, {%4, %5, %6, %7}, {%8, %9}, {%0, %1, %2, %3};":"+r"(d0),"+r"(d1),"+r"(d2),"+r"(d3):"r"(a0),"r"(a1),"r"(a2),"r"(a3),"r"(b0),"r"(b1));return{__uint_as_float(d0),__uint_as_float(d1),__uint_as_float(d2),__uint_as_float(d3)};}
