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

// Each tile stores two contiguous planes with four input rows per channel.
__kernel void i2_pack_q8_input8(__global const uchar *input, __global const ulong *jobs,
                                __global float4 *packed, uint width) {
    const uint channel = block_x() * 128 + lane_index();
    if (channel >= width) return;
    const uint tile = block_y();
    jobs += tile * 16;
    const float4 lo = (float4)(
        ((__global const float *)(input + jobs[0]))[channel],
        ((__global const float *)(input + jobs[2]))[channel],
        ((__global const float *)(input + jobs[4]))[channel],
        ((__global const float *)(input + jobs[6]))[channel]);
    const float4 hi = (float4)(
        ((__global const float *)(input + jobs[8]))[channel],
        ((__global const float *)(input + jobs[10]))[channel],
        ((__global const float *)(input + jobs[12]))[channel],
        ((__global const float *)(input + jobs[14]))[channel]);
    packed[2 * tile * width + channel] = lo;
    packed[(2 * tile + 1) * width + channel] = hi;
}

static void i2_q8_packed_rows(__global const uchar *weights, __global const float4 *packed,
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width,
    uint warps) {
    const uint thread = lane_index(), lane = thread & 31;
    const uint row = block_x() * warps + (thread >> 5), tile = block_y();
    const __global uchar *weight = weights + row * stride;
    const __global float4 *lo = packed + 2 * tile * width + lane;
    const __global float4 *hi = lo + width;
    float4 a = (float4)(0.0f), b = (float4)(0.0f);
    for (uint at = 0; at < width; at += 32, weight += 34) {
        const float value = q8_value(weight, lane);
        const float4 l = read4(lo + at);
        const float4 h = read4(hi + at);
        a.x = madd(value, l.x, a.x);
        a.y = madd(value, l.y, a.y);
        a.z = madd(value, l.z, a.z);
        a.w = madd(value, l.w, a.w);
        b.x = madd(value, h.x, b.x);
        b.y = madd(value, h.y, b.y);
        b.z = madd(value, h.z, b.z);
        b.w = madd(value, h.w, b.w);
    }
    a = (float4)(reduce(a.x), reduce(a.y), reduce(a.z), reduce(a.w));
    b = (float4)(reduce(b.x), reduce(b.y), reduce(b.z), reduce(b.w));
    if (lane != 0) return;
    jobs += tile * 16 + 1;
    *(__global float *)(output + jobs[0] + 4 * row) = a.x;
    *(__global float *)(output + jobs[2] + 4 * row) = a.y;
    *(__global float *)(output + jobs[4] + 4 * row) = a.z;
    *(__global float *)(output + jobs[6] + 4 * row) = a.w;
    *(__global float *)(output + jobs[8] + 4 * row) = b.x;
    *(__global float *)(output + jobs[10] + 4 * row) = b.y;
    *(__global float *)(output + jobs[12] + 4 * row) = b.z;
    *(__global float *)(output + jobs[14] + 4 * row) = b.w;
}

#define Q8_PACKED_KERNEL(ENTRY, WARPS) \
__kernel void ENTRY(__global const uchar *weights, __global const float4 *packed, \
    __global uchar *output, __global const ulong *jobs, ulong stride, uint width) { \
    i2_q8_packed_rows(weights, packed, output, jobs, stride, width, WARPS); \
}

Q8_PACKED_KERNEL(i2_q8_packed4, 4)
Q8_PACKED_KERNEL(i2_q8_packed8, 8)
