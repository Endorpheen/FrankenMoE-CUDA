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

static uint64_t i2m_job_word(uint64_t jobs, int64_t index) { return ((__global const uint64_t *)jobs)[index]; }

#include "inkling-matrix-device.h"

#define I2M_KERNEL(ENTRY, KIND) \
__kernel void ENTRY(__global const uint8_t *weights, __global const uint8_t *input, \
    __global uint8_t *output, __global const uint64_t *jobs, uint64_t row_stride, \
    int32_t width, int32_t rows, int32_t count, int32_t job_base, int32_t words, int32_t job_tile) { \
    uint32_t x, y, thread; \
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x)); \
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(y)); \
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread)); \
    matrix_launch_g((uint64_t)weights, (uint64_t)input, (uint64_t)output, (uint64_t)jobs, \
        row_stride, width, rows, count, job_base, words, \
        KIND, 4 * x + (thread >> 5), (uint32_t)job_tile * y, thread & 31, (uint32_t)job_tile); \
}

I2M_KERNEL(goose_i2_matrix_q8_0, 8)
I2M_KERNEL(goose_i2_matrix_iq4_nl, 20)
I2M_KERNEL(goose_i2_matrix_iq4_xs, 23)

#ifndef GOOSE_INKLING_DECODE_MATRIX
__kernel void goose_i2_matrix_iq4_xs_blocks(__global const uint8_t *weights, __global const uint8_t *input,
    __global uint8_t *output, __global const uint64_t *jobs, uint64_t row_stride,
    int32_t width, int32_t rows, int32_t count, int32_t job_base, int32_t words, int32_t tile) {
    uint32_t x, y, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(y));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    matrix_launch_xs_g((uint64_t)weights, (uint64_t)input, (uint64_t)output, (uint64_t)jobs,
        row_stride, width, rows, count, job_base, words, x, y, thread);
}

#else
__kernel void goose_i2_matrix_q8_attention_projections(
    uint64_t input_q, uint64_t input_k, uint64_t input_v, uint64_t output,
    uint64_t weight_q, uint64_t weight_k, uint64_t weight_v, uint64_t weight_r,
    uint64_t weight_sink, uint64_t weight_gate, uint64_t weight_key_taps, uint64_t weight_value_taps) {
    uint32_t block, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    matrix_launch_projections_g(input_q,input_k,input_v,output,weight_q,weight_k,weight_v,weight_r,
        weight_sink,weight_gate,weight_key_taps,weight_value_taps,block,thread);
}
#endif
