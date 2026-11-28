#include "depth-expf.h"

#define WIDTH 2304
#define LANES 128
#define MAX_HISTORY 11

static uint lane_id(void) {
    uint id;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(id));
    return id;
}

static uint block_id(void) {
    uint id;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(id));
    return id;
}

static void block_barrier(void) {
    __asm__ volatile("bar.sync 0;" ::: "memory");
}

static float divide_rn(float a, float b) {
    float result;
    __asm__("div.rn.f32 %0, %1, %2;" : "=f"(result) : "f"(a), "f"(b));
    return result;
}

static float sqrt_rn(float x) {
    float result;
    __asm__("sqrt.rn.f32 %0, %1;" : "=f"(result) : "f"(x));
    return result;
}

static float lazy_load(uint64_t base, int64_t index) { return ((__global const float *)base)[index]; }
static void lazy_store(uint64_t base, int64_t index, float value) { ((__global float *)base)[index] = value; }
static uint64_t lazy_plane(uint64_t base, int64_t index) { return ((__global const ulong *)base)[index]; }
static float lazy_local_read(uint64_t base, int64_t index) { return ((__local const float *)base)[index]; }
static void lazy_local_write(uint64_t base, int64_t index, float value) { ((__local float *)base)[index] = value; }
static double lazy_sum_read(uint64_t base, int64_t index) { return ((__local const double *)base)[index]; }
static void lazy_sum_write(uint64_t base, int64_t index, double value) { ((__local double *)base)[index] = value; }
static uint lazy_float_bits(float value) { return as_uint(value); }

#include "i2-lazy-prelude-device.h"

__kernel __attribute__((reqd_work_group_size(LANES, 1, 1)))
void i2_lazy_prepare(__global const float *attention, __global const float *residual,
    __global float *next, __global float *normalized, __global float *scales, int add_residual) {
    __local float squares[WIDTH], inverse;
    __local double sums[LANES], magnitudes[LANES];
    lazy_prepare_g((uint64_t)attention, (uint64_t)residual, (uint64_t)next, (uint64_t)normalized,
        (uint64_t)scales, add_residual != 0, (uint64_t)squares, (uint64_t)sums, (uint64_t)magnitudes,
        (uint64_t)&inverse, block_id(), lane_id());
}

__kernel __attribute__((reqd_work_group_size(LANES, 1, 1)))
void i2_plane_score(__global const ulong *planes, __global const float *current,
    __global const float *query, __global float *scores, int rows, int lazy_index, __global const float *scales) {
    __local float products[WIDTH];
    __local double sums[LANES], magnitudes[LANES];
    uint token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(token));
    lazy_score_g((uint64_t)planes, (uint64_t)current, (uint64_t)query, (uint64_t)scores, rows, lazy_index,
        (uint64_t)scales, (uint64_t)products, (uint64_t)sums, (uint64_t)magnitudes, token, block_id(), lane_id());
}

__kernel __attribute__((reqd_work_group_size(LANES, 1, 1)))
void i2_plane_mix(__global const ulong *planes, __global const float *current,
    __global const float *scores, __global float *output, int rows, int lazy_index, __global const float *scales) {
    __local float weights[MAX_HISTORY + 1];
    uint token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(token));
    lazy_mix_g((uint64_t)planes, (uint64_t)current, (uint64_t)scores, (uint64_t)output,
        rows, lazy_index, (uint64_t)scales, (uint64_t)weights, token, block_id(), lane_id());
}
