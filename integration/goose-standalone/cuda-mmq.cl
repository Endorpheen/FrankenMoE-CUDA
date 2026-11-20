typedef struct { int32_t a_g, b_g, c_g, d_g; } MmqDot_g;
typedef struct { float a_g, b_g, c_g, d_g; } MmqSum_g;
#define gs_shl_i32(a, b) as_int((uint32_t) (a) << ((b) & 31))

static uint32_t fg_mmq_word(uint64_t address) {
    if ((address & 3u) == 0) return *(__global uint32_t *) address;
    __global uint16_t * halfwords = (__global uint16_t *) address;
    return (uint32_t) halfwords[0] | ((uint32_t) halfwords[1] << 16);
}

static uint64_t fg_mmq_index(uint64_t address, int64_t index) {
    return ((__global const uint64_t *) address)[index];
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

static MmqDot_g fg_mmq_mma32_shared(uint64_t address, uint32_t b0, uint32_t b1) {
    uint32_t a0, a1, a2, a3;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x4.shared.b16 {%0, %1, %2, %3}, [%4];"
        : "=r"(a0), "=r"(a1), "=r"(a2), "=r"(a3) : "r"((uint32_t) address) : "memory");
    return fg_mmq_mma32(a0, a1, a2, a3, b0, b1);
}

static MmqDot_g fg_mmq_mma16_shared(uint64_t address, uint32_t b0) {
    uint32_t a0, a1;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x2.shared.b16 {%0, %1}, [%2];"
        : "=r"(a0), "=r"(a1) : "r"((uint32_t) address) : "memory");
    return fg_mmq_mma16(a0, a1, b0);
}

static void fg_mmq_shared_store(uint64_t address, uint32_t word) {
    *(__local uint32_t *) address = word;
}

static float fg_mmq_shared_float(uint64_t address) { return *(__local float *) address; }
static int32_t fg_mmq_shared_i8(uint64_t address) { return *(__local int8_t *) address; }
static void fg_mmq_shared_store_float(uint64_t address, float value) { *(__local float *) address = value; }

static void fg_mmq_barrier(void) { fg_device_barrier(); }

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

static void pack_mmq_row(uint64_t input, uint64_t packed, int32_t extent, int32_t layout,
                         uint32_t group, uint32_t lane) {
    fg180q_mmq_g(input, 0, packed, 256l * extent, 0, 0, 0, 256l * extent,
        1, 1, 1, layout, 0, 0, group, 0, lane, 32, 1, 2 * extent);
}

__kernel void goose_mmq_pack_q8(__global const uint8_t * input, __global const uint64_t * jobs,
    __global uint8_t * packed, int32_t extent, int32_t layout, int32_t first, int32_t job_words) {
    uint32_t group, job, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(job));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint64_t route = (uint64_t) first + job;
    pack_mmq_row((uint64_t) input + jobs[route * job_words],
        (uint64_t) packed + (uint64_t) job * 288 * extent, extent, layout, group, lane);
}

__kernel void goose_mmq_pack_shared(__global const uint8_t * input, __global uint8_t * packed,
    int32_t extent, int32_t layout, int64_t input_stride) {
    uint32_t group, row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    pack_mmq_row((uint64_t) input + (uint64_t) row * input_stride,
        (uint64_t) packed + (uint64_t) row * 288 * extent, extent, layout, group, lane);
}

static __attribute__((always_inline)) void mmq_matmul(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles,
    uint64_t stride, int32_t extent, int32_t kind, int32_t rows, int32_t first, int32_t first_route,
    int32_t job_words, int64_t input_stride, int32_t reference_columns, int32_t reference_sms) {
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
    const uint64_t q = (uint64_t) packed + (input_stride == 0 ? (route - first_route) * 288 * extent : 0);
    fg_device_mmq_g((uint64_t) weights + tiles[at + 2], q, (uint64_t) output,
        stride, extent, kind, row, rows, count, tid % 32, out0, out1, 0, extent, 0,
        (uint64_t) jobs, route, job_words, input_stride, reference_columns, reference_sms);
}

__kernel void goose_mmq_matmul(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles,
    uint64_t stride, int32_t extent, int32_t kind, int32_t rows, int32_t first, int32_t first_route,
    int32_t job_words, int64_t input_stride) {
    mmq_matmul(weights, packed, output, jobs, tiles, stride, extent, kind, rows, first, first_route,
        job_words, input_stride, 0, 0);
}

__kernel void goose_mmq_q6_reference(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles,
    uint64_t stride, int32_t extent, int32_t kind, int32_t rows, int32_t first, int32_t first_route,
    int32_t job_words, int64_t input_stride, int32_t columns, int32_t sms) {
    mmq_matmul(weights, packed, output, jobs, tiles, stride, extent, 14, rows, first, first_route,
        job_words, input_stride, columns, sms);
}

__kernel void goose_mmq_q4_reference(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles,
    uint64_t stride, int32_t extent, int32_t kind, int32_t rows, int32_t first, int32_t first_route,
    int32_t job_words, int64_t input_stride, int32_t columns, int32_t sms) {
    mmq_matmul(weights, packed, output, jobs, tiles, stride, extent, 12, rows, first, first_route,
        job_words, input_stride, columns, sms);
}

#define MMQ_DENSE_SHARED(name, kind, words) \
__kernel void name(__global const uint8_t * weights, __global const uint8_t * packed, \
    __global uint8_t * output, __global const uint64_t * jobs, uint64_t stride, int32_t extent, \
    int32_t rows, int32_t first_route, int32_t sms) { \
    uint32_t row, column, thread; \
    __local uint32_t shared[words] __attribute__((aligned(16))); \
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row)); \
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(column)); \
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread)); \
    fg_device_mmq_dense_shared_g((uint64_t) weights, (uint64_t) packed, (uint64_t) output, \
        (uint64_t) jobs, stride, extent, kind, 16u * row, rows, first_route + 32u * column, \
        first_route, sms, thread, (uint64_t) shared); \
}

#define MMQ_ROUTED_SHARED(name, kind, words) \
__kernel void name(__global const uint8_t * weights, __global const uint8_t * packed, \
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles, \
    uint64_t stride, int32_t extent, int32_t rows, int32_t first, int32_t first_route, int64_t input_stride) { \
    uint32_t row, tile, thread; \
    __local uint32_t shared[words] __attribute__((aligned(16))); \
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row)); \
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile)); \
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread)); \
    const uint64_t at = ((uint64_t) first + tile) * 3; \
    fg_device_mmq_routed_shared_g((uint64_t) weights + tiles[at + 2], (uint64_t) packed, \
        (uint64_t) output, (uint64_t) jobs, stride, extent, kind, 16u * row, rows, \
        tiles[at], first_route, tiles[at + 1], input_stride, thread, (uint64_t) shared); \
}

MMQ_DENSE_SHARED(goose_mmq_q4_dense_shared, 12, 1280)
MMQ_DENSE_SHARED(goose_mmq_q6_dense_shared, 14, 1104)
MMQ_ROUTED_SHARED(goose_mmq_q4_routed_shared, 12, 1280)
MMQ_ROUTED_SHARED(goose_mmq_q6_routed_shared, 14, 1104)
#undef MMQ_DENSE_SHARED
#undef MMQ_ROUTED_SHARED

__kernel void goose_mmq_stream(__global const uint8_t * weights, __global const uint8_t * packed,
    __global uint8_t * output, __global const uint64_t * jobs, __global const uint64_t * tiles,
    __global float * fixup, uint64_t stride, int32_t extent, int32_t kind, int32_t rows,
    int32_t first, int32_t first_route, int32_t job_words, int32_t count, int64_t input_stride) {
    uint32_t worker, workers, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(worker));
    __asm__("mov.u32 %0, %%nctaid.x;" : "=r"(workers));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg_device_mmq_stream_g((uint64_t) weights, (uint64_t) packed, (uint64_t) output,
        (uint64_t) jobs, (uint64_t) tiles, (uint64_t) fixup, stride, extent, kind, rows, first,
        first_route, job_words, count, input_stride, worker, workers, thread);
}

__kernel void goose_mmq_stream_fixup(__global float * output, __global const uint64_t * jobs,
    __global const uint64_t * tiles, __global const float * fixup, int32_t extent, int32_t rows,
    int32_t first, int32_t job_words, int32_t count) {
    uint32_t worker, workers, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(worker));
    __asm__("mov.u32 %0, %%nctaid.x;" : "=r"(workers));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg_device_mmq_fixup_g((uint64_t) output, (uint64_t) jobs, (uint64_t) tiles, (uint64_t) fixup,
        extent, rows, first, job_words, count, worker, workers, thread);
}
