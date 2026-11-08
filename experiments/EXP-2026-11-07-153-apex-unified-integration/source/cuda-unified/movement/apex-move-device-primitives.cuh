#pragma once
/* worker-10: raw CUDA device primitives for the generated Goose bodies.
 * These are typed load/store, thread/block indices and IEEE conversions only.
 */
#include <cuda_fp16.h>
#include <cstdint>

static __device__ __forceinline__ void gg_fail(const char * msg) {
    (void) msg;
    __trap();
}
static __device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b) {
    if (b == 0 || (a == INT64_MIN && b == -1)) gg_fail("integer division");
    return a / b;
}
static __device__ __forceinline__ uint64_t gg_udiv(uint64_t a, uint64_t b) {
    if (b == 0) gg_fail("unsigned division");
    return a / b;
}
static __device__ __forceinline__ uint64_t gg_umod(uint64_t a, uint64_t b) {
    if (b == 0) gg_fail("unsigned modulo");
    return a % b;
}
static __device__ __forceinline__ double gg_double_bits(uint64_t bits) {
    double d;
    __builtin_memcpy(&d, &bits, sizeof d);
    return d;
}

static __device__ __forceinline__ uint8_t * am_ptr(int64_t h) {
    return (uint8_t *) (uintptr_t) h;
}
static __device__ __forceinline__ int64_t fg153_am_param(int64_t p, int64_t index) {
    return ((const int64_t *) (uintptr_t) p)[index];
}
static __device__ __forceinline__ int64_t fg153_am_load_u8(int64_t base, int64_t off) {
    return am_ptr(base)[off];
}
static __device__ __forceinline__ int64_t fg153_am_load_u16(int64_t base, int64_t off) {
    return *reinterpret_cast<const uint16_t *>(am_ptr(base) + off);
}
static __device__ __forceinline__ int64_t fg153_am_load_i32(int64_t base, int64_t off) {
    return *reinterpret_cast<const int32_t *>(am_ptr(base) + off);
}
static __device__ __forceinline__ int64_t fg153_am_load_i64(int64_t base, int64_t off) {
    return *reinterpret_cast<const int64_t *>(am_ptr(base) + off);
}
static __device__ __forceinline__ void fg153_am_store_u8(int64_t base, int64_t off, int64_t v) {
    am_ptr(base)[off] = (uint8_t) v;
}
static __device__ __forceinline__ void fg153_am_store_u16(int64_t base, int64_t off, int64_t v) {
    *reinterpret_cast<uint16_t *>(am_ptr(base) + off) = static_cast<uint16_t>(v);
}
static __device__ __forceinline__ void fg153_am_store_i32(int64_t base, int64_t off, int64_t v) {
    *reinterpret_cast<int32_t *>(am_ptr(base) + off) = static_cast<int32_t>(v);
}
static __device__ __forceinline__ void fg153_am_store_f32(int64_t base, int64_t off, float v) {
    *reinterpret_cast<float *>(am_ptr(base) + off) = static_cast<float>(v);
}
static __device__ __forceinline__ float fg153_am_load_f32(int64_t base, int64_t off) {
    return *reinterpret_cast<const float *>(am_ptr(base) + off);
}
static __device__ __forceinline__ int64_t fg153_am_f16_bits(float v) {
    return (int64_t) __half_as_ushort(__float2half_rn(v));
}
static __device__ __forceinline__ float fg153_am_f16_value(int64_t bits) {
    return __half2float(__ushort_as_half((unsigned short) bits));
}
static __device__ __forceinline__ float fg153_am_fabs(float v) { return fabsf(v); }
static __device__ __forceinline__ int64_t fg153_am_trunc_i64(float v) { return (int64_t) v; }

static __device__ __forceinline__ int64_t fg153_am_gid_x(void) {
    return (int64_t) blockIdx.x * blockDim.x + threadIdx.x;
}
static __device__ __forceinline__ int64_t fg153_am_bid_x(void) { return blockIdx.x; }
static __device__ __forceinline__ int64_t fg153_am_bid_y(void) { return blockIdx.y; }
static __device__ __forceinline__ int64_t fg153_am_bid_z(void) { return blockIdx.z; }
static __device__ __forceinline__ int64_t fg153_am_tid_x(void) { return threadIdx.x; }
static __device__ __forceinline__ int64_t fg153_am_tid_y(void) { return threadIdx.y; }
static __device__ __forceinline__ int64_t fg153_am_bdim_x(void) { return blockDim.x; }
static __device__ __forceinline__ int64_t fg153_am_gdim_y(void) { return gridDim.y; }
static __device__ __forceinline__ int64_t fg153_am_gdim_z(void) { return gridDim.z; }

static __device__ __forceinline__ void fg153_am_enter_copy_scalar(void) {}
static __device__ __forceinline__ void fg153_am_enter_copy_contig(void) {}
static __device__ __forceinline__ void fg153_am_enter_copy_blocks(void) {}
static __device__ __forceinline__ void fg153_am_enter_transpose(void) {}
static __device__ __forceinline__ void fg153_am_enter_getrows(void) {}
static __device__ __forceinline__ void fg153_am_enter_setrows(void) {}
static __device__ __forceinline__ void fg153_am_enter_q4_dequant(void) {}
