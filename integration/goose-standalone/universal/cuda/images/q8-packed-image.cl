#include "q8-packed-image-ptx-0.h"
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

static float q8_load_quant(uint64_t base, uint lane) { return q8_value((__global const uchar *)base, lane); }
#define q8_vector(base,index) ({ \
    float4 vector = read4((__global const float4 *)(base) + (index)); \
    (I2Q8Vec_g){vector.x,vector.y,vector.z,vector.w}; \
})
#define q8_store_vector(base,index,v) (((__global float4 *)(base))[index] = (float4)((v).x_g,(v).y_g,(v).z_g,(v).w_g))
#include "i2-q8-packed-device.h"

// Each tile stores two contiguous planes with four input rows per channel.

#include "q8-packed-image-entries.h"
