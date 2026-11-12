#include <cstdint>
// What the emitter produces, but with a locally restrict-qualified cast at the load.
__device__ __forceinline__ float ld_r(uint64_t p, uint64_t o) {
    const float * __restrict__ q = (const float * __restrict__)(const char *)(uintptr_t)p;
    return q[o/4];
}
extern "C" __global__ void localrestrict(const uint64_t base, uint64_t * o,
        int64_t ne00, int64_t s1, int64_t s2, int i01, int i02) {
    int64_t i0 = (int64_t)blockIdx.x*blockDim.x + threadIdx.x;
    if (i0 >= ne00) return;
    uint64_t p = base + (uint64_t)((i02*s2 + i01*s1)*4);
    float s = 0.f;
    #pragma unroll 4
    for (int k = 0; k < 8; ++k) s += ld_r(p, (uint64_t)((i0 + k)*4));
    o[i0] = (uint64_t)__float_as_uint(s);
}
