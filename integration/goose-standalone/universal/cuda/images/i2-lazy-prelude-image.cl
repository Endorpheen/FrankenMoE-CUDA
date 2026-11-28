static double depth_fma(double a, double b, double c) {
    double result;
    __asm__("fma.rn.f64 %0, %1, %2, %3;" : "=d"(result) : "d"(a), "d"(b), "d"(c));
    return result;
}
static uint64_t depth_bits(double value) { return as_ulong(value); }
static double depth_value(uint64_t bits) { return as_double(bits); }

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

#include "i2-lazy-prelude-image-entries.h"
