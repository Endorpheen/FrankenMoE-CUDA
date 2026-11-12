#pragma once
static __device__ __forceinline__ uint32_t fg175q_ld_u8(uint64_t p,uint64_t b){return*((const uint8_t*)(uintptr_t)p+b);}
static __device__ __forceinline__ uint32_t fg175q_ld_u16(uint64_t p,uint64_t b){return*(const uint16_t*)((const char*)(uintptr_t)p+b);}
static __device__ __forceinline__ float fg175q_h2f(uint32_t b){return __half2float(__ushort_as_half((uint16_t)b));}
static __device__ __forceinline__ void fg175q_st_f16(uint64_t p,uint64_t b,float v){*(half*)((char*)(uintptr_t)p+b)=__float2half_rn(v);}
