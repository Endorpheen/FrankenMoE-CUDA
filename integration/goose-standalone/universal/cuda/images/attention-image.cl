#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_expf(float);

static float i2d_load_f32(uint64_t address, int64_t index) {
    return ((__global const float *) address)[index];
}

static int64_t i2d_load_i64(uint64_t address, int64_t index) {
    return ((__global const int64_t *) address)[index];
}

static void i2d_store_f32(uint64_t address, int64_t index, float value) {
    ((__global float *) address)[index] = value;
}

static float i2d_expf(float value) {
    return __nv_expf(value);
}

static float i2d_negative_infinity(void) {
    return as_float(0xff800000u);
}


static float i2d_max_shared(uint64_t address, float value, int64_t lane) {
    __local float *slots = (__local float *) address;
    slots[lane] = value;
    __asm__ volatile("bar.sync 0;" ::: "memory");
    for (int offset = 64; offset > 0; offset /= 2) {
        if (lane < offset) {
            const float other = slots[lane + offset];
            if (other > slots[lane]) {
                slots[lane] = other;
            }
        }
        __asm__ volatile("bar.sync 0;" ::: "memory");
    }
    return slots[0];
}

static double i2d_sum_shared(uint64_t address, double value, int64_t lane) {
    __local double *slots = (__local double *) address;
    slots[lane] = value;
    __asm__ volatile("bar.sync 0;" ::: "memory");
    for (int offset = 64; offset > 0; offset /= 2) {
        if (lane < offset) {
            slots[lane] += slots[lane + offset];
        }
        __asm__ volatile("bar.sync 0;" ::: "memory");
    }
    return slots[0];
}

static void i2d_store_f64(uint64_t address, int64_t index, double value) {
    ((__global double *) address)[index] = value;
}

static double i2d_load_f64(uint64_t address, int64_t index) {
    return ((__global const double *) address)[index];
}

#include "inkling-attention-device.h"

__kernel void goose_i2_attention_score(__global const float *query, __global const float *keys,
    __global const float *relative, __global const float *bias, __global const float *sink,
    __global const int64_t *positions, __global float *scores, int64_t rows, int64_t absolute,
    int64_t window, float scale) {
    uint32_t x, y, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(y));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    i2_device_score_g((uint64_t)query, (uint64_t)keys, (uint64_t)relative, (uint64_t)bias,
        (uint64_t)sink, (uint64_t)positions, (uint64_t)scores, rows, absolute, window, y,
        (int64_t)x * 128 + lane, scale);
}


__kernel void goose_i2_attention_softmax(__global float *scores, __global float *inverse, int64_t rows) {
    uint32_t head, lane;
    __local float maximum[128];
    __local double total[128];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    i2_device_softmax_g((uint64_t)scores, (uint64_t)inverse, head, rows, lane,
                        (uint64_t)maximum, (uint64_t)total);
}

__kernel void goose_i2_attention_value(__global const float *values, __global const float *scores,
    __global const float *inverse, __global double *partials, int64_t rows) {
    uint32_t x, y, tile, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(y));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(tile));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    i2_device_value_g((uint64_t)values, (uint64_t)scores, (uint64_t)inverse,
        (uint64_t)partials, rows, y, 32 * x + lane, tile);
}

__kernel void goose_i2_attention_merge(__global const double *partials, __global float *output,
    int64_t tiles) {
    uint32_t x, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    i2_device_merge_g((uint64_t)partials, (uint64_t)output, tiles, 128 * x + lane);
}
