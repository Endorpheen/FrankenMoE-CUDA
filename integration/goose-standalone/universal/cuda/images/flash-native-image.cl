#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_expf(float);

static ushort half_bits(float value) {
    ushort result;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(result) : "f"(value));
    return result;
}
static uint pair_bits(ushort a, ushort b) {
    return (uint)a | ((uint)b << 16);
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

static uint4 load_q(__global const ushort *q, int64_t head0, int64_t head1,
                   int tail, uint group, uint col, uint chunk) {
    const uint at = 128 * chunk + col;
    const uint2 zero = (uint2)(0);
    const uint2 x0 = group < (uint)tail ? (uint2)(*(__global uint *)(q + head0 + at),
                                    *(__global uint *)(q + head0 + at + 8)) : zero;
    const uint2 x1 = group + 8 < (uint)tail ? (uint2)(*(__global uint *)(q + head1 + at),
                                         *(__global uint *)(q + head1 + at + 8)) : zero;
    return (uint4)(x0.x,x1.x,x0.y,x1.y);
}

static void save_fragment(__global float *out, int64_t slot, int64_t row_stride, int tail,
                          uint group, uint col, float4 value) {
    if (group < (uint)tail) {
        out[slot + col] = value.x;
        out[slot + col + 1] = value.y;
    }
    if (group + 8 < (uint)tail) {
        out[slot + 8 * row_stride + col] = value.z;
        out[slot + 8 * row_stride + col + 1] = value.w;
    }
}

__kernel void goose_i2_pack_f16(__global const float *source, __global ushort *dest, int count) {
    uint x, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(x));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    uint i = 256*x + thread;
    if (i < (uint)count) dest[i] = half_bits(source[i]);
}

// Q4_0 uses one half scale followed by 16 bytes for two nibbles each.
static ushort load_q4(__global const uchar *data, int64_t element) {
    const __global uchar *block = data + (element >> 5) * 18;
    const ushort scale_bits = *(__global const ushort *)block;
    const uint lane = (uint)element & 31;
    const uint packed = block[2 + (lane & 15)];
    const int quant = (int)((packed >> (lane >= 16 ? 4 : 0)) & 15) - 8;
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(scale_bits));
    return half_bits(scale * (float)quant);
}

static ushort load_kv(__global const ushort *data, int64_t element, int q4) {
    return q4 ? load_q4((__global const uchar *)data, element) : data[element];
}

static uint i2_native_pair(uint64_t address, int64_t offset) {
    return *(__global const uint *)((__global const ushort *)address + offset);
}
static uint i2_native_load_half(uint64_t address, int64_t offset, int q4) {
    return load_kv((__global const ushort *)address, offset, q4);
}
static uint i2_native_q4_pair(uint64_t address, int64_t first, int64_t second, int q4) {
    return pair_bits(load_kv((__global const ushort *)address, first, q4),
                     load_kv((__global const ushort *)address, second, q4));
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
#include "inkling-native-flash-device.h"

// One CTA consumes up to 16 queries and one key partition. The host
// keeps this workspace bounded by chunking the attention query axis.
__kernel void goose_i2_flash_mma(__global const ushort *q, __global const ushort *k,
    __global const ushort *v, __global const float *relative, __global const float *bias,
    __global const float *sinks, __global const int64_t *key_pos, __global const int64_t *query_pos,
    __global float *partials, __global float *metadata, int64_t keys, int64_t queries,
    int64_t parts, int64_t window, float scale, int q4) {
    uint query_tile, head, part, thread;
    __local float scores[16*32], maxval[16], sum[16], alpha[16];
    __local ushort probs[16*32];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(query_tile));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(part));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    i2_native_flash_g((uint64_t)q,(uint64_t)k,(uint64_t)v,(uint64_t)relative,(uint64_t)bias,
        (uint64_t)sinks,(uint64_t)key_pos,(uint64_t)query_pos,(uint64_t)partials,(uint64_t)metadata,
        (uint64_t)scores,(uint64_t)maxval,(uint64_t)sum,(uint64_t)alpha,(uint64_t)probs,
        keys,queries,parts,window,scale,q4,query_tile,head,part,thread,false);
}

__kernel void goose_i2_flash_mma_merge(__global const float *partial,
     __global const float *meta, __global float *out, int64_t parts) {
    uint head, channel;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(channel));
    const int64_t first = (int64_t)head*parts;
    float maxval = 0.0f;
    for (int64_t i=0; i<parts; ++i) {
        const float m = meta[(first+i)*2];
        if (m > maxval) maxval = m;
    }
    double total = __nv_expf(-maxval), sum = 0;
    for (int64_t i=0; i<parts; ++i) {
        const int64_t slot = first+i;
        const double w = __nv_expf(meta[slot*2]-maxval);
        total += w*meta[slot*2+1];
        sum += w*partial[slot*128+channel];
    }
    out[(int64_t)head*128+channel] = (float)(sum/total);
}

__kernel void goose_i2_flash_mma_decode(__global const ushort *q, __global const ushort *k,
    __global const ushort *v, __global const float *relative, __global const float *bias,
    __global const float *sinks, __global const int64_t *key_pos, __global const int64_t *query_pos,
    __global float *partials, __global float *metadata, int64_t keys, int64_t queries,
    int64_t parts, int64_t window, float scale, int q4) {
    uint head, part, thread;
    __local float scores[16*32], maxval[16], sum[16], alpha[16];
    __local ushort probs[16*32];
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(part));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    i2_native_flash_g((uint64_t)q,(uint64_t)k,(uint64_t)v,(uint64_t)relative,(uint64_t)bias,
        (uint64_t)sinks,(uint64_t)key_pos,(uint64_t)query_pos,(uint64_t)partials,(uint64_t)metadata,
        (uint64_t)scores,(uint64_t)maxval,(uint64_t)sum,(uint64_t)alpha,(uint64_t)probs,
        keys,queries,parts,window,scale,q4,0,head,part,thread,true);
}

__kernel void goose_i2_flash_mma_prefill(__global const ushort *q, __global const ushort *k,
    __global const ushort *v, __global const float *relative, __global const float *bias,
    __global const float *sinks, __global const int64_t *key_pos, __global const int64_t *query_pos,
    __global float *partials, __global float *metadata, int64_t keys, int64_t queries,
    int64_t parts, int64_t window, float scale, int q4) {
    uint query_tile, head, part, thread;
    __local float scores[16*32], maxval[16], sum[16], alpha[16];
    __local ushort probs[16*32];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(query_tile));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(part));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const uint warp = thread >> 5, lane = thread & 31, group = lane >> 2, col = 2 * (lane & 3);
    const int64_t query = 16 * (int64_t)query_tile;
    const int64_t begin = (int64_t)part*2048;
    const int64_t end = (keys < begin + 2048 ? keys : begin + 2048);
    const int tail = (int)(queries - query < 16 ? queries - query : 16);
    const int64_t qhead = query*24 + head;
    q4 = 1;
    const int64_t g = (head/3)*128;
    float4 acc[4];
    #pragma unroll
    for (int i=0; i<4; ++i) acc[i] = (float4)(0);
    if (thread < 16) {
        maxval[thread] = 0;
        sum[thread] = 0;
    }
    sync_group();

    for (int64_t start=begin; start<end; start+=32) {
        int missing = (end-start < 32 ? (int)(end-start) : 32);
        const int64_t krow = start + warp*8 + group;
        const int64_t qa0 = (qhead + group*24)*128;
        const int64_t qa1 = qa0 + 8*3072;
        float4 qk = (float4)(0);
        #pragma unroll
        for (int dim=0; dim<128; dim+=16) {
            const uint4 a = load_q(q,qa0+dim,qa1+dim,tail,group,col,0);
            const int64_t offset = krow*1024 + g + dim + col;
            uint2 b = (uint2)(0);
            if (krow < end) {
                if (q4) {
                    b = (uint2)(pair_bits(load_q4((__global const uchar *)k, offset),
                                          load_q4((__global const uchar *)k, offset + 1)),
                                pair_bits(load_q4((__global const uchar *)k, offset + 8),
                                          load_q4((__global const uchar *)k, offset + 9)));
                } else {
                    b = (uint2)(*(__global const uint *)(k + offset),
                                *(__global const uint *)(k + offset + 8));
                }
            }
            qk = mma(qk,a,b);
        }
        const uint idx = group*32 + warp*8 + col;
        scores[idx] = qk.x;
        scores[idx+1] = qk.y;
        scores[idx+8*32] = qk.z;
        scores[idx+8*32+1] = qk.w;
        sync_group();
        for (uint at = thread; at < (uint)(tail * 32); at += 128) {
            const uint row = at >> 5;
            i2_online_adjust_key_g((uint64_t)(scores + row * 32), (uint64_t)key_pos,
                (uint64_t)relative, (uint64_t)bias, (uint64_t)sinks, missing, start,
                query_pos[query + row], qhead + row * 24, at & 31, window, scale);
        }
        sync_group();
        if (thread < 16) {
            if ((int)thread < tail) {
                i2_online_prepared_row_at_g((uint64_t)scores, (uint64_t)probs,
                    (uint64_t)maxval, (uint64_t)sum, (uint64_t)alpha, thread);
            } else {
                alpha[thread] = 0;
                for (int j=0; j<32; ++j) probs[thread*32+j] = 0;
            }
        }
        sync_group();
        const float4 weights = (float4)(alpha[group],alpha[group],alpha[group+8],alpha[group+8]);
        #pragma unroll
        for (int i=0; i<4; ++i) acc[i] *= weights;
        #pragma unroll
        for (int j=0; j<32; j+=16) {
            const __local uint *r0 = (__local const uint *)(probs+group*32+j+col);
            const __local uint *r1 = r0 + 8*16;
            uint4 a = (uint4)(r0[0],r1[0],r0[4],r1[4]);
            const int64_t i0=start+j+col, i1=i0+1, i2=i0+8, i3=i0+9;
            #pragma unroll
            for (int i=0; i<4; ++i) {
                int64_t c = g+32*warp+8*i+group;
                ushort v0=i0<end ? load_kv(v, i0*1024+c, q4) : 0, v1=i1<end ? load_kv(v, i1*1024+c, q4) : 0;
                ushort v2=i2<end ? load_kv(v, i2*1024+c, q4) : 0, v3=i3<end ? load_kv(v, i3*1024+c, q4) : 0;
                acc[i] = mma(acc[i],a,(uint2)(pair_bits(v0,v1),pair_bits(v2,v3)));
            }
        }
        sync_group();
    }
    const int64_t slot = ((qhead+group*24)*parts+part)*128;
    #pragma unroll
    for (int i=0; i<4; ++i) {
        save_fragment(partials,slot,24*parts*128,tail,group,warp*32+i*8+col,acc[i]);
    }
    if (thread < (uint)tail) {
        const int64_t idx = ((qhead+thread*24)*parts+part)*2;
        metadata[idx]=maxval[thread]; metadata[idx+1]=sum[thread];
    }
}
