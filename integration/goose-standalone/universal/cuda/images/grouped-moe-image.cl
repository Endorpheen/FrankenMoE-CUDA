static ushort half_bits(float f) {
    ushort h;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(h) : "f"(f));
    return h;
}

static uint half_pair(float lo, float hi) {
    return (uint)half_bits(lo) | ((uint)half_bits(hi) << 16);
}

static float iq4_code(uint index) {
    const ulong codes = index < 8 ? 0xf6eaddcfbfad9881UL : 0x7159453526190d01UL;
    return (float)(char)(codes >> (8 * (index & 7)));
}

static uint2 iq4_pairs(__global const uchar *row, int k, uint col, int kind) {
    const __global uchar *block = row + (kind == 23 ? (k >> 8) * 136 : (k >> 5) * 18);
    const ushort d = *(__global const ushort *)block;
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(d));
    const __global uchar *qs = block + 2 + col;
    if (kind == 23) {
        const uint sub = ((uint)k >> 5) & 7;
        const uint low = (block[4 + (sub >> 1)] >> (4 * (sub & 1))) & 15;
        const uint high = (*(__global const ushort *)(block + 2) >> (2 * sub)) & 3;
        scale *= (float)((int)(low + 16 * high) - 32);
        qs += 6 + 16 * sub;
    }
    const uint shift = ((uint)k & 16) >> 2;
    return (uint2)(
        half_pair(scale * iq4_code((qs[0] >> shift) & 15), scale * iq4_code((qs[1] >> shift) & 15)),
        half_pair(scale * iq4_code((qs[8] >> shift) & 15), scale * iq4_code((qs[9] >> shift) & 15)));
}

static uint64_t moe_word(uint64_t base, int64_t index) { return ((__global const ulong *)base)[index]; }
static uint moe_pair(uint64_t base, int64_t index) { return *(__global const uint *)((__global const ushort *)base + index); }
static void moe_store(uint64_t base, int64_t index, float value) { ((__global float *)base)[index] = value; }
static void moe_pack(uint64_t src, uint64_t dst, int64_t i, int64_t j) {
    ((__global ushort *)dst)[j] = half_bits(((__global const float *)src)[i]);
}
static float4 moe_mma(float4 acc, uint2 a, uint2 b, uint lo, uint hi) {
    __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+f"(acc.x), "+f"(acc.y), "+f"(acc.z), "+f"(acc.w)
        : "r"(a.x), "r"(b.x), "r"(a.y), "r"(b.y), "r"(lo), "r"(hi));
    return acc;
}
#define moe_iq_pairs(base,k,column,kind) ({ \
    uint2 pair = iq4_pairs((__global const uchar *)(base),k,column,kind); \
    (I2MoePair_g){pair.x,pair.y}; \
})
#define moe_dot(acc,a,b,lo,hi) ({ \
    float4 result = moe_mma((float4)((acc).x_g,(acc).y_g,(acc).z_g,(acc).w_g), \
        (uint2)((a).x_g,(a).y_g),(uint2)((b).x_g,(b).y_g),lo,hi); \
    (__typeof__(acc)){result.x,result.y,result.z,result.w}; \
})
#include "i2-grouped-moe-device.h"

#include "grouped-moe-image-entries.h"
