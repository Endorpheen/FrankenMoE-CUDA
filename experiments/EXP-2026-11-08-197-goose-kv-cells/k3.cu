#include <cstdint>
extern "C" __global__ void ldg(const uint64_t base, uint64_t * o,
        int64_t ne00, int64_t s1, int64_t s2, int i01, int i02) {
    int64_t i0 = (int64_t)blockIdx.x*blockDim.x + threadIdx.x;
    if (i0 >= ne00) return;
    int64_t p = (int64_t)base + (i02*s2 + i01*s1)*4;
    float s = 0.f;
    #pragma unroll 4
    for (int k = 0; k < 8; ++k) {
        s += __ldg((const float *)((const char *)(uintptr_t)(p + (i0 + k)*4)));
    }
    o[i0] = (uint64_t)__float_as_uint(s);
}
