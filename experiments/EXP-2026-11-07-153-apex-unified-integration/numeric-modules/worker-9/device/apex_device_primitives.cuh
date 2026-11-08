#ifndef APEX_DEVICE_PRIMITIVES_CUH
#define APEX_DEVICE_PRIMITIVES_CUH
// Requirement-permitted device primitives: thread/block indices, warp
// shuffle, block synchronization, transcendental math and F16 conversion.
// All kernel structure, indexing, bounds and numerical order stay in Goose.
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include "apex_device_runtime.cuh"

__device__ __forceinline__ int64_t apex_tid_x()       { return threadIdx.x; }
__device__ __forceinline__ int64_t apex_tid_y()       { return threadIdx.y; }
__device__ __forceinline__ int64_t apex_tid_z()       { return threadIdx.z; }
__device__ __forceinline__ int64_t apex_bid_x()       { return blockIdx.x; }
__device__ __forceinline__ int64_t apex_bid_y()       { return blockIdx.y; }
__device__ __forceinline__ int64_t apex_bid_z()       { return blockIdx.z; }
__device__ __forceinline__ int64_t apex_bdim_x()      { return blockDim.x; }
__device__ __forceinline__ int64_t apex_bdim_y()      { return blockDim.y; }
__device__ __forceinline__ int64_t apex_bdim_z()      { return blockDim.z; }
__device__ __forceinline__ int64_t apex_gdim_x()      { return gridDim.x; }
__device__ __forceinline__ int64_t apex_gdim_y()      { return gridDim.y; }
__device__ __forceinline__ int64_t apex_gdim_z()      { return gridDim.z; }

__device__ __forceinline__ float apex_shfl_xor_f32(float v, int64_t off) {
    return __shfl_xor_sync(0xffffffffu, v, (int) off, 32);
}
__device__ __forceinline__ float apex_shfl_down_f32(float v, int64_t off) {
    return __shfl_down_sync(0xffffffffu, v, (int) off, 32);
}
__device__ __forceinline__ void apex_sync() { __syncthreads(); }

__device__ __forceinline__ float apex_expf(float x)   { return expf(x); }
__device__ __forceinline__ float apex_logf(float x)   { return logf(x); }
__device__ __forceinline__ float apex_rsqrtf(float x) { return rsqrtf(x); }
__device__ __forceinline__ float apex_sqrtf(float x)  { return sqrtf(x); }
__device__ __forceinline__ float apex_powf(float a, float b) { return powf(a, b); }
__device__ __forceinline__ float apex_floorf(float x) { return floorf(x); }
__device__ __forceinline__ float apex_ceilf(float x)  { return ceilf(x); }
__device__ __forceinline__ float apex_cosf(float x)   { return cosf(x); }
__device__ __forceinline__ float apex_sinf(float x)   { return sinf(x); }
__device__ __forceinline__ float apex_tanhf(float x)  { return tanhf(x); }
__device__ __forceinline__ float apex_fabsf(float x)  { return fabsf(x); }
__device__ __forceinline__ float apex_fmaxf(float a, float b) { return fmaxf(a, b); }
__device__ __forceinline__ float apex_fminf(float a, float b) { return fminf(a, b); }
__device__ __forceinline__ float apex_neg_inf()       { return -INFINITY; }
__device__ __forceinline__ float apex_from_bits(uint32_t u) {
    union { uint32_t u; float f; } x; x.u = u; return x.f;
}

__device__ __forceinline__ uint16_t apex_f32_to_f16(float x) {
    return __half_as_ushort(__float2half_rn(x));
}
__device__ __forceinline__ float apex_f16_to_f32(uint16_t b) {
    return __half2float(__ushort_as_half(b));
}

#endif
