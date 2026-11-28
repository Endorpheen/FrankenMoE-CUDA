static uint lane_index(void) {
    uint v;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(v));
    return v;
}

static uint block_x(void) {
    uint v;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(v));
    return v;
}

static uint block_y(void) {
    uint v;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(v));
    return v;
}

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
__kernel void i2_pack_q8_input8(__global const uchar *input, __global const ulong *jobs,
                                __global float4 *packed, uint width) {
    i2_q8_pack8_g((uint64_t)input, (uint64_t)jobs, (uint64_t)packed, width, block_x(), block_y(), lane_index());
}

#define Q8_PACKED_KERNEL(ENTRY, WARPS) \
__kernel void ENTRY(__global const uchar *weights, __global const float4 *packed, \
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width) { \
    i2_q8_rows16_g((uint64_t)weights,(uint64_t)packed,(uint64_t)output,(uint64_t)jobs, \
        stride,width,WARPS,block_x(),block_y(),lane_index(),false,false); \
}
Q8_PACKED_KERNEL(i2_q8_packed4, 4)
Q8_PACKED_KERNEL(i2_q8_packed8, 8)



__kernel void i2_q8_packed16w2(__global const uchar *weights, __global const float4 *packed,
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width) {
    i2_q8_rows16_g((uint64_t)weights,(uint64_t)packed,(uint64_t)output,(uint64_t)jobs,
        stride,width,2,block_x(),block_y(),lane_index(),false,true);
}

__kernel void i2_q8_packed16w4(__global const uchar *weights, __global const float4 *packed,
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width) {
    i2_q8_rows16_g((uint64_t)weights,(uint64_t)packed,(uint64_t)output,(uint64_t)jobs,
        stride,width,4,block_x(),block_y(),lane_index(),false,true);
}

__kernel void i2_q8_expand_f32(__global const uchar *weight,
    __global float *output, ulong stride, uint width) {
    i2_q8_expand_g((uint64_t)weight, (uint64_t)output, stride, width, block_x(), block_y(), lane_index());
}
__kernel void i2_q8_fp32w2(__global const float *weights, __global const float4 *packed,
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width) {
    i2_q8_rows16_g((uint64_t)weights,(uint64_t)packed,(uint64_t)output,(uint64_t)jobs,
        stride,width,2,block_x(),block_y(),lane_index(),true,true);
}
__kernel void i2_q8_fp32w4(__global const float *weights, __global const float4 *packed,
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width) {
    i2_q8_rows16_g((uint64_t)weights,(uint64_t)packed,(uint64_t)output,(uint64_t)jobs,
        stride,width,4,block_x(),block_y(),lane_index(),true,true);
}
