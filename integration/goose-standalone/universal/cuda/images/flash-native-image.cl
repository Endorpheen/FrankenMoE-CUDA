#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_expf(float);

#include "flash-native-image-ptx-0.h"
static void i2o_probability_write(uint64_t address, int64_t index, float value) { ((__local ushort *)address)[index] = half_bits(value); }
static float i2o_exp(float value) { return __nv_expf(value); }
static float i2o_neg_inf(void) { return as_float(0xff800000u); }

static float4 mma(float4 d, uint4 a, uint2 b) {
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+f"(d.x), "+f"(d.y), "+f"(d.z), "+f"(d.w)
        : "r"(a.x), "r"(a.y), "r"(a.z), "r"(a.w), "r"(b.x), "r"(b.y));
    return d;
}

static void i2_flash_store_half(uint64_t address, float value) {
    *(__global ushort *)address = half_bits(value);
}

static float i2_flash_half_value(uint32_t bits) {
    float value;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(value) : "h"((ushort)bits));
    return value;
}
static uint i2_flash_to_half_bits(float value) { return half_bits(value); }
static uint i2_native_pair(uint64_t address, int64_t offset) {
    return *(__global const uint *)((__global const ushort *)address + offset);
}
static uint i2_native_shared_pair(uint64_t address, int64_t offset) {
    return *(__local const uint *)((__local const ushort *)address + offset);
}
#include "flash-native-image-ptx-1.h"
#define i2_native_mma(acc,a0,a1,a2,a3,b0,b1) ({ \
    float4 value = mma((float4)((acc).f0,(acc).f1,(acc).f2,(acc).f3),(uint4)(a0,a1,a2,a3),(uint2)(b0,b1)); \
    (__typeof__(acc)){value.x,value.y,value.z,value.w}; \
})
static float4 relative_load4(__global const float *address) {
    float4 value;
    __asm__("ld.global.v4.f32 {%0, %1, %2, %3}, [%4];"
        : "=f"(value.x), "=f"(value.y), "=f"(value.z), "=f"(value.w)
        : "l"((ulong)address));
    return value;
}

#define i2_relative_products(acc,a,b) ({ \
    float4 product = relative_load4((__global const float *)(a)) * relative_load4((__global const float *)(b)); \
    (__typeof__(acc)){product.x, product.y, product.z, product.w}; \
})

static uint i2_flash_q4_product(uint encoded, uint scale_pair) {
    uint integers, values;
    __asm__("sub.rn.f16x2 %0, %1, %2;" : "=r"(integers) : "r"(encoded), "r"(0x64086408u));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(values) : "r"(integers), "r"(scale_pair));
    return values;
}

#include "inkling-native-flash-device.h"

#include "flash-native-image-entries.h"
