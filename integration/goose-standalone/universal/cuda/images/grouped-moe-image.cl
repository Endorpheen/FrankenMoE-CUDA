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

__kernel void goose_i2_moe_pack(__global const uchar *input, __global const ulong *jobs,
    __global ushort *packed, int width, int count) {
    uint group, job, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(job));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint k = 256 * group + lane;
    if (job >= (uint)count || k >= (uint)width) return;
    const __global float *source = (__global const float *)(input + jobs[3 * (ulong)job]);
    packed[(ulong)job * width + k] = half_bits(source[k]);
}

static __attribute__((always_inline)) void grouped_mmq(__global const uchar *weight, __global const ushort *packed,
    __global uchar *output, __global const ulong *jobs, __global const ulong *tiles,
    ulong stride, int width, int rows, int kind, bool wide) {
    uint group, tile, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint warp = lane >> 5;
    lane &= 31;
    const uint row = 64 * group + 16 * warp + (lane >> 2);
    const uint col = 2 * (lane & 3);
    const ulong first = tiles[3 * (ulong)tile], count = tiles[3 * (ulong)tile + 1];
    const __global uchar *plane = weight + tiles[3 * (ulong)tile + 2];
    const __global ushort *input = packed + (first + (lane >> 2)) * width;
    const __global uchar *w0 = plane + row * stride;
    const __global uchar *w1 = w0 + 8 * stride;
    float4 acc = (float4)(0);
    float4 acc_hi = (float4)(0);
    const bool has_hi = wide && count > 8;
    for (int k = 0; k < width; k += 16) {
        const uint2 a0 = row < (uint)rows ? iq4_pairs(w0, k, col, kind) : (uint2)(0);
        const uint2 a1 = row + 8 < (uint)rows ? iq4_pairs(w1, k, col, kind) : (uint2)(0);
        uint2 b = (uint2)(0);
        if ((lane >> 2) < count) {
            b.x = *(__global const uint *)(input + k + col);
            b.y = *(__global const uint *)(input + k + col + 8);
        }
        __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
            : "+f"(acc.x), "+f"(acc.y), "+f"(acc.z), "+f"(acc.w)
            : "r"(a0.x), "r"(a1.x), "r"(a0.y), "r"(a1.y), "r"(b.x), "r"(b.y));
        if (has_hi) {
            b = (uint2)(0);
            if ((lane >> 2) + 8 < count) {
                const __global ushort *hi = input + 8 * (ulong)width;
                b.x = *(__global const uint *)(hi + k + col);
                b.y = *(__global const uint *)(hi + k + col + 8);
            }
            __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
                "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
                : "+f"(acc_hi.x), "+f"(acc_hi.y), "+f"(acc_hi.z), "+f"(acc_hi.w)
                : "r"(a0.x), "r"(a1.x), "r"(a0.y), "r"(a1.y), "r"(b.x), "r"(b.y));
        }
    }
    if (col < count) {
        __global float *target = (__global float *)(output + jobs[3 * (first + col) + 1]);
        if (row < (uint)rows) target[row] = acc.x;
        if (row + 8 < (uint)rows) target[row + 8] = acc.z;
    }
    if (col + 1 < count) {
        __global float *target = (__global float *)(output + jobs[3 * (first + col + 1) + 1]);
        if (row < (uint)rows) target[row] = acc.y;
        if (row + 8 < (uint)rows) target[row + 8] = acc.w;
    }
    if (has_hi) {
        const uint hi_col = col + 8;
        if (hi_col < count) {
            __global float *target = (__global float *)(output + jobs[3 * (first + hi_col) + 1]);
            if (row < (uint)rows) target[row] = acc_hi.x;
            if (row + 8 < (uint)rows) target[row + 8] = acc_hi.z;
        }
        if (hi_col + 1 < count) {
            __global float *target = (__global float *)(output + jobs[3 * (first + hi_col + 1) + 1]);
            if (row < (uint)rows) target[row] = acc_hi.y;
            if (row + 8 < (uint)rows) target[row + 8] = acc_hi.w;
        }
    }
}


#define MOE_MMQ(ENTRY, TYPE, WIDE) \
__kernel void ENTRY(__global const uchar *weight, __global const ushort *packed, \
    __global uchar *output, __global const ulong *jobs, __global const ulong *tiles, \
    ulong stride, int width, int rows) { \
    grouped_mmq(weight, packed, output, jobs, tiles, stride, width, rows, TYPE, WIDE); \
}

MOE_MMQ(goose_i2_xs_mmq, 23, false)
MOE_MMQ(goose_i2_nl_mmq, 20, false)
MOE_MMQ(goose_i2_xs_mmq16, 23, true)
MOE_MMQ(goose_i2_nl_mmq16, 20, true)

static __attribute__((always_inline)) void grouped_mmq_many(
    __global const uchar *weight, __global const ushort *packed, __global uchar *output,
    __global const ulong *jobs, __global const ulong *tiles,
    ulong stride, int width, int rows, int kind, uint groups) {
    uint x, tile, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const uint lane = thread & 31;
    const uint row = 64 * x + 16 * (thread >> 5) + (lane >> 2);
    const uint col = 2 * (lane & 3);
    const ulong first = tiles[3 * (ulong)tile], count = tiles[3 * (ulong)tile + 1];
    const __global uchar *w0 = weight + tiles[3 * (ulong)tile + 2] + row * stride;
    const __global uchar *w1 = w0 + 8 * stride;
    float4 accum[4];
    #pragma unroll
    for (uint g = 0; g < groups; ++g) accum[g] = (float4)(0);
    for (int k = 0; k < width; k += 16) {
        const uint2 a0 = row < (uint)rows ? iq4_pairs(w0, k, col, kind) : (uint2)(0);
        const uint2 a1 = row + 8 < (uint)rows ? iq4_pairs(w1, k, col, kind) : (uint2)(0);
        #pragma unroll
        for (uint g = 0; g < groups; ++g) {
            if (8 * g >= count) continue;
            const uint index = 8 * g + (lane >> 2);
            uint2 b = (uint2)(0);
            if (index < count) {
                const __global ushort *p = packed + (first + index) * width + k + col;
                b.x = *(__global const uint *)p;
                b.y = *(__global const uint *)(p + 8);
            }
            float4 acc = accum[g];
            __asm__ volatile("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
                "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
                : "+f"(acc.x), "+f"(acc.y), "+f"(acc.z), "+f"(acc.w)
                : "r"(a0.x), "r"(a1.x), "r"(a0.y), "r"(a1.y), "r"(b.x), "r"(b.y));
            accum[g] = acc;
        }
    }
    #pragma unroll
    for (uint g = 0; g < groups; ++g) {
        const uint column = col + 8 * g;
        if (column < count) {
            __global float *dst = (__global float *)(output + jobs[3 * (first + column) + 1]);
            if (row < (uint)rows) dst[row] = accum[g].x;
            if (row + 8 < (uint)rows) dst[row + 8] = accum[g].z;
        }
        if (column + 1 < count) {
            __global float *dst = (__global float *)(output + jobs[3 * (first + column + 1) + 1]);
            if (row < (uint)rows) dst[row] = accum[g].y;
            if (row + 8 < (uint)rows) dst[row + 8] = accum[g].w;
        }
    }
}

#define MOE_MMQ_MANY(ENTRY, TYPE, GROUPS) \
__kernel void ENTRY(__global const uchar *weight, __global const ushort *packed, \
    __global uchar *output, __global const ulong *jobs, __global const ulong *tiles, \
    ulong stride, int width, int rows) { \
    grouped_mmq_many(weight, packed, output, jobs, tiles, stride, width, rows, TYPE, GROUPS); \
}

MOE_MMQ_MANY(goose_i2_xs_mmq32, 23, 4)
MOE_MMQ_MANY(goose_i2_nl_mmq32, 20, 4)
