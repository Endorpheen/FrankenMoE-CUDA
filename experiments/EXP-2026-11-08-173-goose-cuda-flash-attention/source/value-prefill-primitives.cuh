#pragma once
#include "value-prefill-types-generated.cuh"
static __device__ __forceinline__ uint32_t fg175c_fg176i_bits(float v){return fg176i_bits(v);}
static __device__ __forceinline__ float fg175c_fg176i_frombits(uint32_t v){return fg176i_frombits(v);}
static __device__ __forceinline__ uint32_t fg175c_fg176i_ldu(uint64_t a){return fg176i_ldu(a);}
static __device__ __forceinline__ void fg175c_fg176i_stu(uint64_t a,uint32_t v){fg176i_stu(a,v);}
static __device__ __forceinline__ fg175c_U4 fg175c_fg176i_ldm4(uint64_t a){auto z=fg176i_ldm4(a);return{z.f0,z.f1,z.f2,z.f3};}
static __device__ __forceinline__ fg175c_U4 fg175c_fg176i_ldm4t(uint64_t a){auto z=fg176i_ldm4t(a);return{z.f0,z.f1,z.f2,z.f3};}
static __device__ __forceinline__ fg175c_F4 fg175c_fg176i_mma_f32(fg175c_F4 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){auto z=fg176i_mma_f32(fg175i_F4{d.f136,d.f137,d.f138,d.f139},a0,a1,a2,a3,b0,b1);return{z.f4,z.f5,z.f6,z.f7};}
static __device__ __forceinline__ fg175c_U2 fg175c_fg176i_mma_f16(fg175c_U2 d,uint32_t a0,uint32_t a1,uint32_t a2,uint32_t a3,uint32_t b0,uint32_t b1){auto z=fg176i_mma_f16(fg175i_U2{d.f140,d.f141},a0,a1,a2,a3,b0,b1);return{z.f8,z.f9};}
static __device__ __forceinline__ float fg175c_fg176i_hlo(uint32_t w){return fg176i_hlo(w);}
static __device__ __forceinline__ float fg175c_fg176i_hhi(uint32_t w){return fg176i_hhi(w);}
static __device__ __forceinline__ uint32_t fg175c_fg176i_h2(float a,float b){return fg176i_h2(a,b);}
static __device__ __forceinline__ uint32_t fg175c_fg176i_mulh2(uint32_t w,uint32_t s){return fg176i_mulh2(w,s);}
static __device__ __forceinline__ float fg175c_fg176i_expf(float x){return fg176i_expf(x);}
static __device__ __forceinline__ float fg175c_fg176i_fmaf(float a,float b,float c){return fg176i_fmaf(a,b,c);}
static __device__ __forceinline__ float fg175c_fg176i_shflx(float x,int32_t off){return fg176i_shflx(x,off);}
static __device__ __forceinline__ void fg175c_fg176i_cpwait(){fg176i_cpwait();}
static __device__ __forceinline__ void fg175c_fg176i_barrier(){fg176i_barrier();}
static __device__ __forceinline__ float fg175c_fg175p_gload(uint64_t p,uint64_t i){return fg175p_gload(p,i);}
static __device__ __forceinline__ void fg175c_fg175p_gstore(uint64_t p,uint64_t i,float v){fg175p_gstore(p,i,v);}
static __device__ __forceinline__ uint32_t fg175c_fg175p_ldu32(uint64_t p,uint64_t i){return fg175p_ldu32(p,i);}
static __device__ __forceinline__ void fg175c_fg175p_stu32(uint64_t p,uint64_t i,uint32_t v){fg175p_stu32(p,i,v);}
static __device__ __forceinline__ uint32_t fg175c_fg175p_smld(uint64_t p,uint64_t i){return fg175p_smld(p,i);}
static __device__ __forceinline__ void fg175c_fg175p_smst(uint64_t p,uint64_t i,uint32_t v){fg175p_smst(p,i,v);}
static __device__ __forceinline__ float fg175c_fg175p_smldf(uint64_t p,uint64_t i){return fg175p_smldf(p,i);}
static __device__ __forceinline__ void fg175c_fg175p_smstf(uint64_t p,uint64_t i,float v){fg175p_smstf(p,i,v);}
static __device__ __forceinline__ uint32_t fg175c_fg175p_h2mul(uint32_t a,uint32_t b){return fg175p_h2mul(a,b);}
static __device__ __forceinline__ uint32_t fg175c_fg175p_h2pack(float x,float y){return fg175p_h2pack(x,y);}
