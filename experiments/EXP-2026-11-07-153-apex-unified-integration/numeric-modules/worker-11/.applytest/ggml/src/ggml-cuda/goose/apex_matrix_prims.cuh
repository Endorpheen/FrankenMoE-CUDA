#pragma once
// Device-callable primitives the generated Goose unit expects.  These are the
// only hardware operations the Goose code is not allowed to spell itself:
// IEEE half <-> float, float bit reinterpretation, and a witness counter.
#include <cuda_fp16.h>
#include <cstdint>

static __host__ __device__ __forceinline__ float fgm_half(uint16_t v) {
    __half h; __builtin_memcpy(&h, &v, 2); return __half2float(h);
}
static __host__ __device__ __forceinline__ uint16_t fgm_to_half(float v) {
    __half h = __float2half(v); uint16_t r; __builtin_memcpy(&r, &h, 2); return r;
}
static __host__ __device__ __forceinline__ uint32_t fgm_bits(float x) {
    uint32_t v; __builtin_memcpy(&v, &x, 4); return v;
}
static __host__ __device__ __forceinline__ float fgm_from_bits(uint32_t v) {
    float x; __builtin_memcpy(&x, &v, 4); return x;
}
static __host__ __device__ __forceinline__ void fgm_witness(int64_t op, int64_t value) {
    (void) op; (void) value;
}
