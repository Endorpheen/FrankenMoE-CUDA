#define UINT64_C(x) x##UL
#define INT64_C(x) x##L

typedef uchar uint8_t;
typedef ushort uint16_t;
typedef uint uint32_t;
typedef ulong uint64_t;
typedef char int8_t;
typedef int int32_t;
typedef long int64_t;
static double gg_double_bits(uint64_t bits) { return as_double(bits); }

static uint32_t i2m_load_u8(uint64_t address) {
    return *(__global const uint8_t *)address;
}

static int32_t i2m_load_i8(uint64_t address) {
    return *(__global const int8_t *)address;
}

static uint32_t i2m_load_u16(uint64_t address) {
    return *(__global const uint16_t *)address;
}

static float i2m_load_half(uint64_t address) {
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"((uint16_t)i2m_load_u16(address)));
    return result;
}

static float i2m_load(uint64_t address) {
    return *(__global const float *)address;
}

static void i2m_store(uint64_t address, float value) {
    *(__global float *)address = value;
}

static float i2m_fma(float a, float b, float sum) {
    float result;
    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(result) : "f"(a), "f"(b), "f"(sum));
    return result;
}

static float i2m_shuffle(float value, int32_t distance) {
    float result;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(result) : "f"(value), "r"(distance));
    return result;
}

#include "inkling-matrix-device.h"

static uint64_t plane_at(__global const uint64_t *jobs, int words) {
    return words == 3 ? jobs[2] : 0;
}

static __attribute__((always_inline)) void matrix_group(__global const uint8_t *weight,
    __global const uint8_t *input, __global uint8_t *output, __global const uint64_t *jobs,
    uint64_t stride, int32_t width, int32_t rows, int32_t count, int32_t base, int32_t words,
    int32_t kind, uint32_t row, uint32_t first, uint32_t lane, uint32_t job_tile) {
    if (row >= (uint32_t)rows) {
        return;
    }
    const int32_t groups = width / 32;
    __global const uint64_t *job = jobs + (uint64_t)words * (base + first);
    const uint64_t plane = plane_at(job, words);
    const uint64_t w = (uint64_t)weight + row * stride;
    const uint64_t input_address = (uint64_t)input;
    const uint64_t output_address = (uint64_t)output + 4 * (uint64_t)row;
    if (kind == 8 && words == 2 && job_tile == 8 && first + 7 < (uint32_t)count) {
        i2_matrix_eight_g(w,
            input_address + job[0 * words + 0], input_address + job[1 * words + 0],
            input_address + job[2 * words + 0], input_address + job[3 * words + 0],
            input_address + job[4 * words + 0], input_address + job[5 * words + 0],
            input_address + job[6 * words + 0], input_address + job[7 * words + 0],
            output_address + job[0 * words + 1], output_address + job[1 * words + 1],
            output_address + job[2 * words + 1], output_address + job[3 * words + 1],
            output_address + job[4 * words + 1], output_address + job[5 * words + 1],
            output_address + job[6 * words + 1], output_address + job[7 * words + 1],
            groups, kind, lane);
        return;
    }
    if (job_tile == 4 && first + 3 < (uint32_t)count &&
        plane_at(job + words, words) == plane &&
        plane_at(job + 2 * words, words) == plane &&
        plane_at(job + 3 * words, words) == plane) {
        i2_matrix_four_g(w + plane,
            input_address + job[0], input_address + job[words],
            input_address + job[2 * words], input_address + job[3 * words],
            output_address + job[1], output_address + job[words + 1],
            output_address + job[2 * words + 1], output_address + job[3 * words + 1],
            groups, kind, lane);
        return;
    }
    for (uint32_t i = first; i < (uint32_t)count && i < first + job_tile; ++i, job += words) {
        i2_matrix_one_g(w + plane_at(job, words), input_address + job[0], output_address + job[1],
            groups, kind, lane);
    }
}

#define I2M_KERNEL(ENTRY, KIND) \
__kernel void ENTRY(__global const uint8_t *weights, __global const uint8_t *input, \
    __global uint8_t *output, __global const uint64_t *jobs, uint64_t row_stride, \
    int32_t width, int32_t rows, int32_t count, int32_t job_base, int32_t words, int32_t job_tile) { \
    uint32_t x, y, thread; \
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x)); \
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(y)); \
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread)); \
    matrix_group(weights, input, output, jobs, row_stride, width, rows, count, job_base, words, \
        KIND, 4 * x + (thread >> 5), (uint32_t)job_tile * y, thread & 31, (uint32_t)job_tile); \
}

I2M_KERNEL(goose_i2_matrix_q8_0, 8)
I2M_KERNEL(goose_i2_matrix_iq4_nl, 20)
I2M_KERNEL(goose_i2_matrix_iq4_xs, 23)

__kernel void goose_i2_matrix_iq4_xs_blocks(__global const uint8_t *weights, __global const uint8_t *input,
    __global uint8_t *output, __global const uint64_t *jobs, uint64_t row_stride,
    int32_t width, int32_t rows, int32_t count, int32_t job_base, int32_t words, int32_t tile) {
    uint32_t x, y, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(y));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const uint32_t row = 4 * x + (thread >> 5);
    if (row >= (uint32_t)rows || y >= (uint32_t)count) return;
    __global const uint64_t *job = jobs + (uint64_t)words * ((uint64_t)job_base + y);
    const uint64_t w = (uint64_t)weights + row * row_stride + plane_at(job, words);
    i2_matrix_xs_blocks_g(w, (uint64_t)input + job[0], (uint64_t)output + job[1] + 4 * (uint64_t)row,
                         width / 256, thread & 31);
}
