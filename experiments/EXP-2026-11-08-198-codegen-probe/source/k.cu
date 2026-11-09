#include <cstdint>
extern "C" __global__ void typed(const float * __restrict__ a, float * __restrict__ o, int n) {
    int i = blockIdx.x*blockDim.x + threadIdx.x;
    float s = 0.f;
    #pragma unroll
    for (int k = 0; k < 8; ++k) s += a[i*8 + k];
    o[i] = s;
}
extern "C" __global__ void integer(const uint64_t base, uint64_t * o, int n) {
    int i = blockIdx.x*blockDim.x + threadIdx.x;
    float s = 0.f;
    #pragma unroll
    for (int k = 0; k < 8; ++k) {
        uint64_t off = (uint64_t)(i*8 + k) * 4ULL;
        s += *(const float *)((const char *)(uintptr_t)base + off);
    }
    o[i] = (uint64_t)__float_as_uint(s);
}
