#include <cuda-mmvq.cl>

__kernel void goose_mmvq_short_rows(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, uint64_t stride,
    int32_t extent, int32_t kind, int32_t words) {
    uint32_t block, route, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(route));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const uint64_t row = (uint64_t) block * 4 + thread / 32;
    __global const uint64_t * job = jobs + (uint64_t) route * words;
    const uint64_t plane = words == 3 ? job[2] : 0;
    fg_mmvq_short_row_g((uint64_t) weights + plane + row * stride,
        (uint64_t) packed + (uint64_t) route * extent * 288,
        (uint64_t) output + job[1] + row * 4, extent, kind, thread % 32);
}

__kernel void goose_mmvq_dense_quantize(__global const uint8_t * input,
    __global uint8_t * packed, int32_t extent) {
    uint32_t block, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const MmvqFastDiv_g divisor = {0, 0, 1};
    const int64_t width = (int64_t) extent * 256;
    fg178q_quant_g((uint64_t) input, (uint64_t) packed,
        width, 0, 0, 0, width, 1, 1, divisor, block, 0, 0, thread, 128);
}

__kernel void goose_mmvq_dense_matvec(__global const uint8_t * weights,
    __global const uint8_t * packed, __global uint8_t * output, uint64_t stride,
    int32_t extent, int32_t kind) {
    uint32_t row, thread;
    __local float shared[96];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg_mmvq_row_g((uint64_t) weights + (uint64_t) row * stride, (uint64_t) packed,
        (uint64_t) output + (uint64_t) row * 4, (uint64_t) shared, extent, kind, thread);
}
