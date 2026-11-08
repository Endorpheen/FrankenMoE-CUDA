#ifndef APEX_DEVICE_RUNTIME_CUH
#define APEX_DEVICE_RUNTIME_CUH
// Device-side replacement for the subset of the frozen Goose C runtime that
// the Apex kernels use: slices, bounds-checked indexing and float literals.
// This file contains no compute logic.
#include <cstdint>
#include <cstddef>
#include <math.h>

typedef struct { void *data; int64_t len; } gg_slice;

__device__ __forceinline__ void apex_device_trap() {
    asm volatile("trap;");
}

__device__ __forceinline__ void gg_fail(const char * message) {
    (void) message;
    apex_device_trap();
}

__device__ __forceinline__ int64_t gg_index(int64_t i, int64_t len) {
    if ((uint64_t) i >= (uint64_t) len) {
        apex_device_trap();
    }
    return i;
}

__device__ __forceinline__ gg_slice gg_view(void * p, int64_t n) {
    gg_slice v;
    v.data = p;
    v.len  = n;
    return v;
}


__device__ __forceinline__ int64_t gg_div(int64_t a, int64_t b) {
    if (!b) { gg_fail("division by zero"); }
    if (a == INT64_MIN && b == -1) { gg_fail("division overflow"); }
    return a / b;
}
__device__ __forceinline__ int64_t gg_mod(int64_t a, int64_t b) {
    int64_t r;
    if (!b) { gg_fail("division by zero"); }
    if (b == -1) { return 0; }
    r = a % b;
    if (r < 0) { return (int64_t)((uint64_t)r + (b < 0 ? (uint64_t)0 - (uint64_t)b : (uint64_t)b)); }
    return r;
}
__device__ __forceinline__ uint64_t gg_udiv(uint64_t a, uint64_t b) {
    if (!b) { gg_fail("division by zero"); }
    return a / b;
}
__device__ __forceinline__ uint64_t gg_umod(uint64_t a, uint64_t b) {
    if (!b) { gg_fail("division by zero"); }
    return a % b;
}

__device__ __forceinline__ double gg_double_bits(uint64_t u) {
    union { uint64_t u; double d; } x;
    x.u = u;
    return x.d;
}

#endif
