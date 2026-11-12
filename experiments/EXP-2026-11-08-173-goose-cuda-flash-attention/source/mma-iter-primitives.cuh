#pragma once
#include "mma-iter-types-generated.cuh"
static __device__ __forceinline__ uint32_t fg176i_bits(float v){return __float_as_uint(v);}
static __device__ __forceinline__ float fg176i_frombits(uint32_t v){return __uint_as_float(v);}
static_assert(sizeof(fg175i_U2)==8&&sizeof(fg175i_U4)==16&&sizeof(fg175i_F4)==16,"Goose MMA POD widths");
static __device__ __forceinline__ void fg175l_cp16(uint64_t d,uint64_t s){unsigned p=__cvta_generic_to_shared((void*)(uintptr_t)d);asm volatile("cp.async.cg.shared.global.L2::64B [%0], [%1], 16;"::"r"(p),"l"(s));}
#include "loaders-functions-generated.cuh"
static __device__ __forceinline__ void fg175l_load_kv(uint64_t s,uint64_t d,int64_t stride,int64_t row,int32_t x,int32_t y){FG175L_KV(s,d,stride,row,x,y,1);}
static __device__ __forceinline__ void fg175l_load_mask(uint64_t s,uint64_t d,int64_t stride,int64_t row,int64_t query,int64_t nq,int32_t n,int32_t x,int32_t y){FG175L_MASK(s,d,stride,row,query,nq,n,x,y,1);}
static __device__ __forceinline__ uint32_t fg176i_ldu(uint64_t a){return*(const uint32_t*)(uintptr_t)a;}
static __device__ __forceinline__ void fg176i_stu(uint64_t a,uint32_t v){*(uint32_t*)(uintptr_t)a=v;}
static __device__ __forceinline__ fg175i_U4 fg176i_ldm4(uint64_t a){fg175i_U4 o;asm volatile("ldmatrix.sync.aligned.m8n8.x4.b16 {%0,%1,%2,%3},[%4];":"=r"(o.f0),"=r"(o.f1),"=r"(o.f2),"=r"(o.f3):"l"(a));return o;}
static __device__ __forceinline__ fg175i_U4 fg176i_ldm4t(uint64_t a){fg175i_U4 o;asm volatile("ldmatrix.sync.aligned.m8n8.x4.trans.b16 {%0,%1,%2,%3},[%4];":"=r"(o.f0),"=r"(o.f1),"=r"(o.f2),"=r"(o.f3):"l"(a));return o;}
static __device__ __forceinline__ fg175i_F4 fg176i_mma_f32(fg175i_F4 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){asm("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 {%0,%1,%2,%3},{%4,%5,%6,%7},{%8,%9},{%0,%1,%2,%3};":"+f"(d.f4),"+f"(d.f5),"+f"(d.f6),"+f"(d.f7):"r"(a0),"r"(a1),"r"(a2),"r"(a3),"r"(b0),"r"(b1));return d;}
static __device__ __forceinline__ fg175i_U2 fg176i_mma_f16(fg175i_U2 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){asm("mma.sync.aligned.m16n8k16.row.col.f16.f16.f16.f16 {%0,%1},{%2,%3,%4,%5},{%6,%7},{%0,%1};":"+r"(d.f8),"+r"(d.f9):"r"(a0),"r"(a1),"r"(a2),"r"(a3),"r"(b0),"r"(b1));return d;}
static __device__ __forceinline__ float fg176i_hlo(uint32_t w){return __half2float(__ushort_as_half((uint16_t)w));}
static __device__ __forceinline__ float fg176i_hhi(uint32_t w){return __half2float(__ushort_as_half(w>>16));}
static __device__ __forceinline__ uint32_t fg176i_h2(float a,float b){half2 z=__floats2half2_rn(a,b);uint32_t w;memcpy(&w,&z,4);return w;}
static __device__ __forceinline__ uint32_t fg176i_mulh2(uint32_t w,uint32_t s){half2 a,b;memcpy(&a,&w,4);memcpy(&b,&s,4);half2 z=__hmul2(a,b);uint32_t o;memcpy(&o,&z,4);return o;}
static __device__ __forceinline__ float fg176i_expf(float x){return expf(x);}
static __device__ __forceinline__ float fg176i_fmaf(float a,float b,float c){return __fmaf_rn(a,b,c);}
static __device__ __forceinline__ float fg176i_shflx(float x,int32_t d){return __shfl_xor_sync(0xffffffff,x,d,32);}
static __device__ __forceinline__ void fg176i_cpwait(){cp_async_wait_all();}
static __device__ __forceinline__ void fg176i_barrier(){__syncthreads();}
