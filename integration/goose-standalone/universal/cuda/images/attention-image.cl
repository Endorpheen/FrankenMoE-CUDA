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


static float i2d_shared_f32(uint64_t base, int64_t index) { return ((__local float *)base)[index]; }
static void i2d_set_shared_f32(uint64_t base, int64_t index, float value) { ((__local float *)base)[index] = value; }
static double i2d_shared_f64(uint64_t base, int64_t index) { return ((__local double *)base)[index]; }
static void i2d_set_shared_f64(uint64_t base, int64_t index, double value) { ((__local double *)base)[index] = value; }
static void i2d_sync(void) { __asm__ volatile("bar.sync 0;" ::: "memory"); }

static void i2d_store_f64(uint64_t address, int64_t index, double value) {
    ((__global double *) address)[index] = value;
}

static double i2d_load_f64(uint64_t address, int64_t index) {
    return ((__global const double *) address)[index];
}

#include "inkling-attention-device.h"

#include "attention-image-entries.h"
