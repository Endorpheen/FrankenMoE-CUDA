typedef struct { uint32_t mp_g, shift_g, divisor_g; } MmvqFastDiv_g;

static int32_t mmvq_dp4a(int32_t a, int32_t b, int32_t c) {
    int32_t dot;
    __asm__("dp4a.s32.s32 %0, %1, %2, %3;" : "=r"(dot) : "r"(a), "r"(b), "r"(c));
    return dot;
}

static int32_t fg185q_ld_i32(uint64_t p, uint64_t off) { return *(__global int32_t *) (p + off); }
static uint32_t fg185q_ld_u16(uint64_t p, uint64_t off) { return *(__global uint16_t *) (p + off); }
static float fg185q_ld_h2(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off); }
static float fg185q_ld_h2b(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off + 2); }
static int32_t fg185q_dp4a(int32_t a, int32_t b, int32_t c) { return mmvq_dp4a(a, b, c); }
static float fg185q_shfl_xor(float v, int32_t delta, int32_t width) {
    return fg_device_shfl(v, delta);
}

static int32_t fg186q_ld_i32(uint64_t p, uint64_t off) { return fg185q_ld_i32(p, off); }
static uint32_t fg186q_ld_i16(uint64_t p, uint64_t off) { return fg185q_ld_u16(p, off); }
static int32_t fg186q_ld_i8(uint64_t p, uint64_t off) { return fg248d_ld_i8(p + off); }
static float fg186q_ld_half(uint64_t p, uint64_t off) { return fg248d_ld_half(p + off); }
static int32_t fg186q_dp4a(int32_t a, int32_t b, int32_t c) { return mmvq_dp4a(a, b, c); }

static float fg178q_ld_f32(uint64_t p, uint64_t off) { return fg_device_global_load(p + off); }
static void fg178q_st_i8(uint64_t p, uint64_t off, int32_t v) { *(__global uint8_t *) (p + off) = (uint8_t) v; }
static void fg178q_st_f16(uint64_t p, uint64_t off, float v) { *(__global uint16_t *) (p + off) = mmq_half_bits(v); }
static float fg178q_shfl_xor(float v, uint32_t mask, int32_t delta) { return fg180q_shfl_xor(v, mask, delta); }
static float fg178q_round(float v) { return fg180q_round(v); }
static int32_t fg178q_trunc_i32(float v) { return fg180q_i32(v); }
static float fg178q_fabs(float v) { return fg180q_fabs(v); }
static uint32_t fg178q_mulhi(uint32_t a, uint32_t b) {
    uint32_t high;
    __asm__("mul.hi.u32 %0, %1, %2;" : "=r"(high) : "r"(a), "r"(b));
    return high;
}
static void fg178q_pdl_sync(void) {}
static void fg178q_pdl_lc(void) {}
static void fg178q_fail(int64_t code) { fg248d_fail(code); }

#include "mmvq-q4-device.h"
#include "mmvq-q6-device.h"
#define gs_add_u32(a, b) ((uint32_t) ((a) + (b)))
#include "mmvq-pack-device.h"
#undef gs_add_u32
#include "mmvq-device.h"

__kernel void goose_mmvq_quantize(__global const uint8_t * input, __global uint8_t * packed,
    __global const uint64_t * jobs, int32_t extent, int32_t words) {
    uint32_t block, route, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(route));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const MmvqFastDiv_g divisor = {0, 0, 1};
    const int64_t width = (int64_t) extent * 256;
    fg178q_quant_g((uint64_t) input + jobs[(uint64_t) route * words],
        (uint64_t) packed + (uint64_t) route * extent * 288,
        width, 0, 0, 0, width, 1, 1, divisor, block, 0, 0, thread, 128);
}

__kernel void goose_mmvq_matvec(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, uint64_t stride,
    int32_t extent, int32_t kind, int32_t words) {
    uint32_t row, route, thread;
    __local float shared[96];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(route));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __global const uint64_t * job = jobs + (uint64_t) route * words;
    const uint64_t plane = words == 3 ? job[2] : 0;
    fg_mmvq_row_g((uint64_t) weights + plane + (uint64_t) row * stride,
        (uint64_t) packed + (uint64_t) route * extent * 288,
        (uint64_t) output + job[1] + (uint64_t) row * 4, (uint64_t) shared, extent, kind, thread);
}

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
