#include <cstdint>
__device__ __forceinline__ float ld_u(uint64_t p, uint64_t o) {
    return *(const float *)((const char *)(uintptr_t)p + o);
}
// Kernel parameter is typed+restrict (the wrapper can do this), but the body converts to
// uint64_t immediately, exactly as the generated Goose body does.
extern "C" __global__ void paramtyped(const float * __restrict__ a, uint64_t * o,
        int64_t ne00, int64_t s1, int64_t s2, int i01, int i02) {
    const uint64_t base = (uint64_t)(uintptr_t)a;
    int64_t i0 = (int64_t)blockIdx.x*blockDim.x + threadIdx.x;
    if (i0 >= ne00) return;
    uint64_t p = base + (uint64_t)((i02*s2 + i01*s1)*4);
    float s = 0.f;
    #pragma unroll 4
    for (int k = 0; k < 8; ++k) s += ld_u(p, (uint64_t)((i0 + k)*4));
    o[i0] = (uint64_t)__float_as_uint(s);
}
