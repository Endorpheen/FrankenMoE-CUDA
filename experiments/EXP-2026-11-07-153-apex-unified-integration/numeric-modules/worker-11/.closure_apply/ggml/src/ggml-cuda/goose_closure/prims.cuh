#pragma once
// Hardware primitives for the matrix-closure Goose unit.  Arithmetic stays in
// Goose; these are only IEEE half, bit reinterpretation and round-to-int8.
#include <cuda_fp16.h>
#include <cmath>
#include <cstdint>

static __host__ __device__ __forceinline__ float fgc_half(uint16_t v) {
    __half h; __builtin_memcpy(&h, &v, 2); return __half2float(h);
}
static __host__ __device__ __forceinline__ uint16_t fgc_to_half(float v) {
    __half h = __float2half(v); uint16_t r; __builtin_memcpy(&r, &h, 2); return r;
}
static __host__ __device__ __forceinline__ uint32_t fgc_bits(float x) {
    uint32_t v; __builtin_memcpy(&v, &x, 4); return v;
}
static __host__ __device__ __forceinline__ float fgc_from_bits(uint32_t v) {
    float x; __builtin_memcpy(&x, &v, 4); return x;
}
static __host__ __device__ __forceinline__ float fgc_roundf(float x) {
    return ::roundf(x);
}
// Native quantize kernels assign roundf(x) to int8_t; reproduce exactly the
// two's-complement byte for the [-127, 127] range those kernels produce.
static __host__ __device__ __forceinline__ uint8_t fgc_f2i8(float x) {
    return (uint8_t) (int8_t) (int) ::roundf(x);
}
static __host__ __device__ __forceinline__ void fgc_witness(int64_t op, int64_t value) {
    (void) op; (void) value;
}
