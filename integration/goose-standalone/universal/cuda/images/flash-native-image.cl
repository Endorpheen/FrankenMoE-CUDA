#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_expf(float);

static ushort half_bits(float value) {
    ushort result;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(result) : "f"(value));
    return result;
}
static void sync_group(void) {
    __asm__ volatile("bar.sync 0;" ::: "memory");
}
static float i2o_load(uint64_t address, int64_t index) { return ((__global float *)address)[index]; }
static int64_t i2o_position(uint64_t address, int64_t index) { return ((__global int64_t *)address)[index]; }
static float i2o_shared_read(uint64_t address, int64_t index) { return ((__local float *)address)[index]; }
static void i2o_shared_write(uint64_t address, int64_t index, float value) { ((__local float *)address)[index] = value; }
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

static uint i2_flash_load_byte(uint64_t address) { return *(__global const uchar *)address; }
static uint i2_flash_load_half_bits(uint64_t address) { return *(__global const ushort *)address; }
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
static float i2_native_shuffle(float value, int32_t distance) {
    float result;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, 0xffffffff;" : "=f"(result) : "f"(value), "r"(distance));
    return result;
}
static void i2_native_warp_sync(void) { __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory"); }
static void i2_native_store(uint64_t address, int64_t index, float value) {
    ((__global float *)address)[index] = value;
}
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

__kernel void goose_i2_pack_f16(__global const float *source, __global ushort *dest, int count) {
    uint x, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    native_pack_f16_g((uint64_t)source, (uint64_t)dest, count, x, thread);
}

// One CTA consumes up to 16 queries and one key partition. The host
// keeps this workspace bounded by chunking the attention query axis.
#define INKLING_FLASH_ENTRY(name, decode, prefill, all_softmax) \
__kernel void name(__global const ushort *q, __global const ushort *k, \
    __global const ushort *v, __global const float *relative, __global const float *bias, \
    __global const float *sinks, __global const int64_t *key_pos, __global const int64_t *query_pos, \
    __global float *partials, __global float *metadata, int64_t keys, int64_t queries, \
    int64_t parts, int64_t window, float scale, int q4) { \
    uint query_tile, head, part, thread; \
    __local float scores[16*32], maxval[16], sum[16], alpha[16]; \
    __local ushort probs[16*32]; \
    if (decode) query_tile = 0; \
    else __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(query_tile)); \
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(head)); \
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(part)); \
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread)); \
    i2_native_flash_g((uint64_t)q,(uint64_t)k,(uint64_t)v,(uint64_t)relative,(uint64_t)bias, \
        (uint64_t)sinks,(uint64_t)key_pos,(uint64_t)query_pos,(uint64_t)partials,(uint64_t)metadata, \
        (uint64_t)scores,(uint64_t)maxval,(uint64_t)sum,(uint64_t)alpha,(uint64_t)probs, \
        keys,queries,parts,window,scale,q4,query_tile,head,part,thread,decode,prefill,all_softmax); \
}

INKLING_FLASH_ENTRY(goose_i2_flash_mma, false, false, false)
INKLING_FLASH_ENTRY(goose_i2_flash_mma_decode, true, false, false)
INKLING_FLASH_ENTRY(goose_i2_flash_mma_prefill, false, true, false)
#ifdef GOOSE_INKLING_FLASH_TILES_IMAGE
INKLING_FLASH_ENTRY(i2_flash_prefill_all_softmax, false, true, true)
#endif
#undef INKLING_FLASH_ENTRY

__kernel void goose_i2_flash_mma_merge(__global const float *partial,
     __global const float *meta, __global float *out, int64_t parts) {
    uint head, channel;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(channel));
    native_flash_merge_g((uint64_t)partial, (uint64_t)meta, (uint64_t)out, parts, head, channel);
}
