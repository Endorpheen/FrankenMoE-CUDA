typedef struct { int32_t a_g, b_g, c_g, d_g; } MmqDot_g;
#define gs_shl_i32(a, b) as_int((uint32_t) (a) << ((b) & 31))

static uint32_t fg_mmq_word(uint64_t address) {
    if ((address & 3u) == 0) return *(__global uint32_t *) address;
    __global uint16_t * halfwords = (__global uint16_t *) address;
    return (uint32_t) halfwords[0] | ((uint32_t) halfwords[1] << 16);
}

static uint16_t mmq_half_bits(float value) {
    uint16_t bits;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(bits) : "f"(value));
    return bits;
}

static float fg_mmq_half_round(float value) {
    const uint16_t bits = mmq_half_bits(value);
    float rounded;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(rounded) : "h"(bits));
    return rounded;
}

static MmqDot_g fg_mmq_mma32(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3,
                            uint32_t b0, uint32_t b1) {
    MmqDot_g r;
    __asm__ volatile("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
                     "{%0, %1, %2, %3}, {%4, %5, %6, %7}, {%8, %9}, {%10, %10, %10, %10};"
                     : "=r"(r.a_g), "=r"(r.b_g), "=r"(r.c_g), "=r"(r.d_g)
                     : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1), "r"(0u));
    return r;
}

static MmqDot_g fg_mmq_mma16(uint32_t a0, uint32_t a1, uint32_t b0) {
    MmqDot_g r;
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.s32.s8.s8.s32 "
                     "{%0, %1, %2, %3}, {%4, %5}, {%6}, {%7, %7, %7, %7};"
                     : "=r"(r.a_g), "=r"(r.b_g), "=r"(r.c_g), "=r"(r.d_g)
                     : "r"(a0), "r"(a1), "r"(b0), "r"(0u));
    return r;
}

static float fg180q_ld_f4x(uint64_t src, uint64_t off) { return fg_device_global_load(src + off); }
static float fg180q_ld_f4y(uint64_t src, uint64_t off) { return fg_device_global_load(src + off + 4); }
static float fg180q_ld_f4z(uint64_t src, uint64_t off) { return fg_device_global_load(src + off + 8); }
static float fg180q_ld_f4w(uint64_t src, uint64_t off) { return fg_device_global_load(src + off + 12); }
static int64_t fg180q_ld_i32(uint64_t src, uint64_t off) { return *(__global int32_t *) (src + off); }
static void fg180q_st_i32(uint64_t dst, uint64_t off, int32_t value) { *(__global int32_t *) (dst + off) = value; }
static void fg180q_st_f32(uint64_t dst, uint64_t off, float value) { fg_device_global_store(dst + off, value); }
static void fg180q_st_half2(uint64_t dst, uint64_t off, float d, float sum) {
    __global uint16_t * output = (__global uint16_t *) (dst + off);
    output[0] = mmq_half_bits(d);
    output[1] = mmq_half_bits(sum);
}
static float fg180q_shfl_xor(float value, uint32_t mask, int32_t delta) {
    uint32_t result;
    __asm__ volatile("shfl.sync.bfly.b32 %0, %1, %2, 31, %3;"
                     : "=r"(result) : "r"(as_uint(value)), "r"(delta), "r"(mask));
    return as_float(result);
}
static int32_t fg180q_i32(float value) {
    int32_t result;
    __asm__("cvt.rzi.s32.f32 %0, %1;" : "=r"(result) : "f"(value));
    return result;
}
static float fg180q_round(float value) {
    if ((as_uint(value) & 0x7f800000u) == 0x7f800000u) return 0.0f;
    const int32_t integral = fg180q_i32(value);
    const float rest = value - (float) integral;
    return (float) (integral + (rest >= 0.5f) - (rest <= -0.5f));
}
static float fg180q_fabs(float value) { return as_float(as_uint(value) & 0x7fffffffu); }
static void fg180q_pdl_sync(void) {}
static void fg180q_fail(int64_t code) { fg248d_fail(code); }

#include "mmq-quant-device.h"
#include "mmq-device.h"

__kernel void goose_mmq_pack_q8(__global const uint8_t * input, __global const uint64_t * jobs,
    __global uint8_t * packed, int32_t extent, int32_t kind, int32_t first, int32_t job_words) {
    uint32_t group, job, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(job));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint64_t route = (uint64_t) first + job;
    fg180q_mmq_g((uint64_t) input + jobs[route * job_words], 0,
        (uint64_t) packed + (uint64_t) job * 288 * extent, 256l * extent, 0, 0, 0, 256l * extent,
        1, 1, 1, kind == 12, 0, 0, group, 0, lane, 32, 1, 2 * extent);
}

__kernel void goose_mmq_matmul(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles,
    uint64_t stride, int32_t extent, int32_t kind, int32_t rows, int32_t first, int32_t first_route,
    int32_t job_words) {
    uint32_t block, tile, tid;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(tid));
    const int64_t row = 64l * block + 16 * (tid / 32);
    if (row >= rows) return;
    const uint64_t at = ((uint64_t) first + tile) * 3;
    const uint64_t route = tiles[at];
    const uint64_t count = tiles[at + 1];
    const uint32_t col = 2 * ((tid % 32) % 4);
    const uint64_t out0 = col < count ? jobs[(route + col) * job_words + 1] : 0;
    const uint64_t out1 = col + 1 < count ? jobs[(route + col + 1) * job_words + 1] : 0;
    fg_device_mmq_g((uint64_t) weights + tiles[at + 2], (uint64_t) packed + (route - first_route) * 288 * extent,
        (uint64_t) output, stride, extent, kind, row, rows, count, tid % 32, out0, out1);
}
