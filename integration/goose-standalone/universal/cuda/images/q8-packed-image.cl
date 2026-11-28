static float madd(float a, float b, float c) {
    float v;
    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(v) : "f"(a), "f"(b), "f"(c));
    return v;
}

static float butterfly(float value, int distance) {
    float v;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(v) : "f"(value), "r"(distance));
    return v;
}

static float reduce(float v) {
    v += butterfly(v, 16);
    v += butterfly(v, 8);
    v += butterfly(v, 4);
    v += butterfly(v, 2);
    v += butterfly(v, 1);
    return v;
}

static float q8_value(__global const uchar *block, uint lane) {
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*(__global const ushort *)block));
    return scale * (float)((__global const char *)block)[2 + lane];
}

static float4 read4(__global const float4 *p) {
    float4 value;
    __asm__("ld.global.v4.f32 {%0, %1, %2, %3}, [%4];"
            : "=f"(value.x), "=f"(value.y), "=f"(value.z), "=f"(value.w) : "l"((ulong)p));
    return value;
}

static float q8_load_f32(uint64_t base, int64_t index) { return ((__global const float *)base)[index]; }
static float q8_load_quant(uint64_t base, uint lane) { return q8_value((__global const uchar *)base, lane); }
static uint64_t q8_job_word(uint64_t base, int64_t index) { return ((__global const ulong *)base)[index]; }
static void q8_store_f32(uint64_t base, float value) { *(__global float *)base = value; }
#define q8_vector(base,index) ({ \
    float4 vector = read4((__global const float4 *)(base) + (index)); \
    (I2Q8Vec_g){vector.x,vector.y,vector.z,vector.w}; \
})
#define q8_store_vector(base,index,v) (((__global float4 *)(base))[index] = (float4)((v).x_g,(v).y_g,(v).z_g,(v).w_g))
#include "i2-q8-packed-device.h"

// Each tile stores two contiguous planes with four input rows per channel.

#include "q8-packed-image-entries.h"
