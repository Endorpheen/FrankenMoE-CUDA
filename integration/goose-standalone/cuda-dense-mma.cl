typedef struct { float a_g, b_g, c_g, d_g; } DenseDot_g;
typedef struct { uint32_t hi_g, lo_g; } DenseParts_g;

static float fg_dense_load(uint64_t address) { return fg_device_global_load(address); }
static uint64_t fg_dense_job(uint64_t address) { return *(__global const uint64_t *) address; }
static float fg_dense_f32(uint32_t bits) { return as_float(bits); }
static void fg_dense_store(uint64_t address, float value) { fg_device_global_store(address, value); }

static uint32_t fg_dense_tf32(float value) {
    uint32_t bits;
    __asm__("cvt.rna.tf32.f32 %0, %1;" : "=r"(bits) : "f"(value));
    return bits;
}

static DenseDot_g fg_dense_mma8(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                              uint32_t b0, uint32_t b1, DenseDot_g c) {
    DenseDot_g d;
    __asm__ volatile("mma.sync.aligned.m16n8k8.row.col.f32.tf32.tf32.f32 "
                     "{%0, %1, %2, %3}, {%4, %5, %6, %7}, {%8, %9}, {%10, %11, %12, %13};"
                     : "=f"(d.a_g), "=f"(d.b_g), "=f"(d.c_g), "=f"(d.d_g)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1),
                       "f"(c.a_g), "f"(c.b_g), "f"(c.c_g), "f"(c.d_g));
    return d;
}

#include "dense-mma-device.h"

__kernel void goose_dense_mma(__global const uint8_t * weights, __global const uint8_t * input,
    __global uint8_t * output, __global const uint64_t * jobs, int64_t stride,
    int32_t extent, int32_t rows, int32_t count) {
    uint32_t row_block, token_block, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row_block));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(token_block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const int64_t row = 64l * row_block + 16 * (thread / 32);
    const int64_t first = 8l * token_block;
    const int64_t remaining = count - first;
    if (row < rows) {
        fg_device_dense_mma_g((uint64_t) weights, (uint64_t) input, (uint64_t) output,
            (uint64_t) (jobs + 2 * first), stride, extent, row, rows, remaining < 8 ? remaining : 8, thread % 32);
    }
}
