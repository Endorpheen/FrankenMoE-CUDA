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

#include "attention-input-image-entries.h"
