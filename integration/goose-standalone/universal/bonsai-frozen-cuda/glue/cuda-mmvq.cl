#include <cuda-mmvq.cl>

__kernel void goose_mmvq_short_rows(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, uint64_t stride,
    int32_t extent, int32_t kind, int32_t words) {
    uint32_t block, route, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(route));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    bonsai_mmvq_short_g((uint64_t) weights, (uint64_t) packed, (uint64_t) output,
        (uint64_t) jobs, stride, extent, kind, words, block, route, thread);
}

__kernel void goose_mmvq_dense_quantize(__global const uint8_t * input,
    __global uint8_t * packed, int32_t extent) {
    uint32_t block, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    bonsai_mmvq_quantize_g((uint64_t) input, (uint64_t) packed, extent, block, thread);
}

__kernel void goose_mmvq_dense_matvec(__global const uint8_t * weights,
    __global const uint8_t * packed, __global uint8_t * output, uint64_t stride,
    int32_t extent, int32_t kind) {
    uint32_t row, thread;
    __local float shared[96];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    bonsai_mmvq_dense_g((uint64_t) weights, (uint64_t) packed, (uint64_t) output,
        stride, extent, kind, row, thread, (uint64_t) shared);
}
