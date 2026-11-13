static __device__ __forceinline__ fg168_F4 fg168_load4(uint64_t p,int32_t i){float4 v=((const float4*)(uintptr_t)p)[i];return fg168_F4{v.x,v.y,v.z,v.w};}
static __device__ __forceinline__ void fg168_store4(uint64_t p,int32_t i,fg168_F4 v){((float4*)(uintptr_t)p)[i]=make_float4(v.f2,v.f3,v.f4,v.f5);}
