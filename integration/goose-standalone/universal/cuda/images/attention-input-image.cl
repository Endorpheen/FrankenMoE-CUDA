#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_sqrtf(float);
extern float __nv_logf(float);

static float input_load(uint64_t base, int64_t i) { return ((__global const float *)base)[i]; }
static void input_store(uint64_t base, int64_t i, float value) { ((__global float *)base)[i] = value; }
static double input_sum(uint64_t base, int64_t i) { return ((__local const double *)base)[i]; }
static void input_set_sum(uint64_t base, int64_t i, double value) { ((__local double *)base)[i] = value; }
static void input_sync(void) { __asm__ volatile("bar.sync 0;" ::: "memory"); }
static void input_half(uint64_t base, int64_t i, float value) {
    ushort bits;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(bits) : "f"(value));
    ((__global ushort *)base)[i] = bits;
}
#include "i2-attention-input-device.h"

__kernel void goose_i2_prepare_attention(__global const float *raw, __global const float *coeff,
    __global const float *history, __global ushort *q, __global float *r, __global float *k,
    __global float *v, int count, int first, int sliding) {
    uint head, row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __local double sum[128];
    attention_input_g((uint64_t)raw,(uint64_t)coeff,(uint64_t)history,(uint64_t)q,(uint64_t)r,(uint64_t)k,
        (uint64_t)v,count,first,sliding,(uint64_t)sum,head,row,lane);
}

__kernel void goose_i2_update_history(__global const float *raw, __global float *history, int count) {
    uint block, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    attention_update_history_g((uint64_t)raw,(uint64_t)history,count,block,lane);
}
