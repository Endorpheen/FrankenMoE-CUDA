#include <cuda-rms-reference.cl>

__kernel void goose_rms_decode_2048(__global const uint8_t * x,
    __global const uint8_t * weights, __global uint8_t * out, float eps) {
    uint32_t thread;
    __local float shared[32];
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg_rms_decode_2048_g((uint64_t) x, (uint64_t) weights, (uint64_t) out,
                        (uint64_t) shared, thread, eps);
}
