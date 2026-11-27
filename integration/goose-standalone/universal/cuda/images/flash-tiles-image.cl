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

#include "inkling-online-device.h"

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
    const uint warp = thread >> 5, lane = thread & 31, group = lane >> 2, col = 2 * (lane & 3);
    const int64_t query = 16 * (int64_t)query_tile;
    const int64_t begin = (int64_t)part*2048;
    const int64_t end = (keys < begin + 2048 ? keys : begin + 2048);
    const int tail = (int)(queries - query < 16 ? queries - query : 16);
    const int64_t qhead = query*24 + head;
    const bool parallel_scores = (q4 & 2) != 0 && queries == 1;
    q4 &= 1;
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
        if (parallel_scores) {
            if (thread < 32) i2_online_adjust_key_g((uint64_t)scores,(uint64_t)key_pos,
                (uint64_t)relative,(uint64_t)bias,(uint64_t)sinks,missing,start,
                query_pos[query],qhead,thread,window,scale);
            sync_group();
        }
        if (thread < 16) {
            if ((int)thread < tail) {
                if (parallel_scores) i2_online_prepared_row_g((uint64_t)scores,(uint64_t)probs,
                    (uint64_t)maxval,(uint64_t)sum,(uint64_t)alpha);
                else i2_online_row_g((uint64_t)scores,(uint64_t)probs,(uint64_t)maxval,(uint64_t)sum,(uint64_t)alpha,
                    (uint64_t)key_pos,(uint64_t)relative,(uint64_t)bias,(uint64_t)sinks,missing,start,
                    query_pos[query+thread],qhead+thread*24,thread,window,scale);
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
    const uint warp = thread >> 5, lane = thread & 31, group = lane >> 2, col = 2 * (lane & 3);
    const int64_t query = 0;
    const int64_t begin = (int64_t)part*2048;
    const int64_t end = (keys < begin + 2048 ? keys : begin + 2048);
    const int tail = 1;
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
    for (uint i = thread + 32; i < 16 * 32; i += 128) probs[i] = 0;
    if (thread < 16) alpha[thread] = 0;
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
        if (thread < 32) {
            i2_online_adjust_key_g((uint64_t)scores, (uint64_t)key_pos,
                (uint64_t)relative, (uint64_t)bias, (uint64_t)sinks, missing, start,
                query_pos[query], qhead, thread, window, scale);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            const float before = maxval[0];
            const float score = scores[thread];
            float maximum = score > before ? score : before;
            for (int distance = 16; distance > 0; distance >>= 1) {
                float other;
                __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, 0xffffffff;"
                        : "=f"(other) : "f"(maximum), "r"(distance));
                if (other > maximum) maximum = other;
            }
            const float value = __nv_expf(score - maximum);
            scores[thread] = value;
            probs[thread] = half_bits(value);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            if (thread == 0) {
                const float scale_prior = __nv_expf(before - maximum);
                float total = sum[0] * scale_prior;
                for (int i = 0; i < 32; ++i) total += scores[i];
                maxval[0] = maximum;
                sum[0] = total;
                alpha[0] = scale_prior;
            }
        }
        sync_group();
        const float4 weights = (float4)(alpha[group],alpha[group],alpha[group+8],alpha[group+8]);
        #pragma unroll
        for (int i=0; i<4; ++i) acc[i] *= weights;
        #pragma unroll 1
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


static float4 relative_load4(__global const float *address) {
    float4 value;
    __asm__("ld.global.v4.f32 {%0, %1, %2, %3}, [%4];"
        : "=f"(value.x), "=f"(value.y), "=f"(value.z), "=f"(value.w)
        : "l"((ulong)address));
    return value;
}

static float relative_adjust_vector(float score, int64_t query, int64_t key,
    int64_t window, __global const float *relative, __global const float *bias,
    float sink, float scale) {
    const int64_t distance = query - key;
    if (key < 0 || distance < 0 || (window && distance >= window)) return i2o_neg_inf();
    double dot = 0.0;
    if (distance < 1024) {
        const __global float *entry = bias + distance * 16;
        #pragma unroll
        for (int i = 0; i < 16; i += 4) {
            const float4 products = relative_load4(relative + i) * relative_load4(entry + i);
            dot += (double)products.x;
            dot += (double)products.y;
            dot += (double)products.z;
            dot += (double)products.w;
        }
    }
    return scale * score + ((float)dot - sink);
}

static uint q4_pair_product(uint nibble_pair, uint scale_pair) {
    const uint encoded = 0x64006400u | (nibble_pair & 15u) | ((nibble_pair & 0xf00u) << 8);
    uint integers, values;
    __asm__("sub.rn.f16x2 %0, %1, %2;" : "=r"(integers) : "r"(encoded), "r"(0x64086408u));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(values) : "r"(integers), "r"(scale_pair));
    return values;
}

static uint load_q4_pair_columns(__global const uchar *data, int64_t element) {
    const __global uchar *at = data + (element >> 5) * 18;
    const uint channel = (uint)element & 31;
    const uint pair = *(__global const ushort *)(at + 2 + (channel & 15));
    const uint scale = *(__global const ushort *)at;
    return q4_pair_product(pair >> (channel >= 16 ? 4 : 0), scale | (scale << 16));
}

static uint load_q4_pair_rows(__global const uchar *data, int64_t a, int64_t b,
                               int64_t end, int64_t channel) {
    uint first_scale = 0, second_scale = 0, first = 0, second = 0;
    const uint offset = 2 + ((uint)channel & 15);
    const uint shift = (channel & 16) ? 4 : 0;
    const int64_t group = channel >> 5;
    if (a < end) {
        const __global uchar *block = data + (a * 32 + group) * 18;
        first_scale = *(__global const ushort *)block;
        first = ((uint)block[offset] >> shift) & 15;
    }
    if (b < end) {
        const __global uchar *block = data + (b * 32 + group) * 18;
        second_scale = *(__global const ushort *)block;
        second = ((uint)block[offset] >> shift) & 15;
    }
    const uint result = q4_pair_product(first | (second << 8), first_scale | (second_scale << 16));
    return result & ((a < end ? 0xffffu : 0u) | (b < end ? 0xffff0000u : 0u));
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
__kernel void i2_flash_prefill_vector(__global const ushort *q, __global const ushort *k,
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
            const uint lane_key = at & 31;
            const int64_t relative_head = qhead + row * 24;
            scores[at] = lane_key < (uint)missing ?
                relative_adjust_vector(scores[at], query_pos[query + row], key_pos[start + lane_key],
                    window, relative + relative_head * 16, bias, sinks[relative_head], scale) :
                i2o_neg_inf();
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
__kernel void i2_flash_prefill_prune(__global const ushort *q, __global const ushort *k,
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

    const int64_t earliest_query = query_pos[query];
    const int64_t latest_query = query_pos[query + tail - 1];
    for (int64_t start=begin; start<end; start+=32) {
        int missing = (end-start < 32 ? (int)(end-start) : 32);
        const int64_t first_key = key_pos[start];
        const int64_t last_key = key_pos[start + missing - 1];
        if (first_key > latest_query || (window && earliest_query - last_key >= window)) continue;
        const bool no_relative = !window && earliest_query - last_key >= 1024 && missing == 32;
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
        if (no_relative) {
            const float sink0 = group < (uint)tail ? sinks[qhead + group * 24] : 0;
            const float sink1 = group + 8 < (uint)tail ? sinks[qhead + (group + 8) * 24] : 0;
            qk.x = scale * qk.x + (0.0f - sink0);
            qk.y = scale * qk.y + (0.0f - sink0);
            qk.z = scale * qk.z + (0.0f - sink1);
            qk.w = scale * qk.w + (0.0f - sink1);
        }
        scores[idx] = qk.x;
        scores[idx+1] = qk.y;
        scores[idx+8*32] = qk.z;
        scores[idx+8*32+1] = qk.w;
        sync_group();
        if (!no_relative) for (uint at = thread; at < (uint)(tail * 32); at += 128) {
            const uint row = at >> 5;
            i2_online_adjust_key_g((uint64_t)(scores + row * 32), (uint64_t)key_pos,
                (uint64_t)relative, (uint64_t)bias, (uint64_t)sinks, missing, start,
                query_pos[query + row], qhead + row * 24, at & 31, window, scale);
        }
        if (!no_relative) sync_group();
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
__kernel void i2_flash_prefill_combined(__global const ushort *q, __global const ushort *k,
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

    const int64_t earliest_query = query_pos[query];
    const int64_t latest_query = query_pos[query + tail - 1];
    for (int64_t start=begin; start<end; start+=32) {
        int missing = (end-start < 32 ? (int)(end-start) : 32);
        const int64_t first_key = key_pos[start];
        const int64_t last_key = key_pos[start + missing - 1];
        if (first_key > latest_query || (window && earliest_query - last_key >= window)) continue;
        const bool no_relative = !window && earliest_query - last_key >= 1024 && missing == 32;
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
        if (no_relative) {
            const float sink0 = group < (uint)tail ? sinks[qhead + group * 24] : 0;
            const float sink1 = group + 8 < (uint)tail ? sinks[qhead + (group + 8) * 24] : 0;
            qk.x = scale * qk.x + (0.0f - sink0);
            qk.y = scale * qk.y + (0.0f - sink0);
            qk.z = scale * qk.z + (0.0f - sink1);
            qk.w = scale * qk.w + (0.0f - sink1);
        }
        scores[idx] = qk.x;
        scores[idx+1] = qk.y;
        scores[idx+8*32] = qk.z;
        scores[idx+8*32+1] = qk.w;
        sync_group();
        if (!no_relative) for (uint at = thread; at < (uint)(tail * 32); at += 128) {
            const uint row = at >> 5;
            const uint lane_key = at & 31;
            const int64_t relative_head = qhead + row * 24;
            scores[at] = lane_key < (uint)missing ?
                relative_adjust_vector(scores[at], query_pos[query + row], key_pos[start + lane_key],
                    window, relative + relative_head * 16, bias, sinks[relative_head], scale) :
                i2o_neg_inf();
        }
        if (!no_relative) sync_group();
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
__kernel void i2_flash_prefill_half(__global const ushort *q, __global const ushort *k,
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
                b = (uint2)(load_q4_pair_columns((__global const uchar *)k, offset),
                             load_q4_pair_columns((__global const uchar *)k, offset + 8));
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
                acc[i] = mma(acc[i], a, (uint2)(
                    load_q4_pair_rows((__global const uchar *)v, i0, i1, end, c),
                    load_q4_pair_rows((__global const uchar *)v, i2, i3, end, c)));
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
__kernel void i2_flash_prefill_all(__global const ushort *q, __global const ushort *k,
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

    const int64_t earliest_query = query_pos[query];
    const int64_t latest_query = query_pos[query + tail - 1];
    for (int64_t start=begin; start<end; start+=32) {
        int missing = (end-start < 32 ? (int)(end-start) : 32);
        const int64_t first_key = key_pos[start];
        const int64_t last_key = key_pos[start + missing - 1];
        if (first_key > latest_query || (window && earliest_query - last_key >= window)) continue;
        const bool no_relative = !window && earliest_query - last_key >= 1024 && missing == 32;
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
                b = (uint2)(load_q4_pair_columns((__global const uchar *)k, offset),
                             load_q4_pair_columns((__global const uchar *)k, offset + 8));
            }
            qk = mma(qk,a,b);
        }
        const uint idx = group*32 + warp*8 + col;
        if (no_relative) {
            const float sink0 = group < (uint)tail ? sinks[qhead + group * 24] : 0;
            const float sink1 = group + 8 < (uint)tail ? sinks[qhead + (group + 8) * 24] : 0;
            qk.x = scale * qk.x + (0.0f - sink0);
            qk.y = scale * qk.y + (0.0f - sink0);
            qk.z = scale * qk.z + (0.0f - sink1);
            qk.w = scale * qk.w + (0.0f - sink1);
        }
        scores[idx] = qk.x;
        scores[idx+1] = qk.y;
        scores[idx+8*32] = qk.z;
        scores[idx+8*32+1] = qk.w;
        sync_group();
        if (!no_relative) for (uint at = thread; at < (uint)(tail * 32); at += 128) {
            const uint row = at >> 5;
            const uint lane_key = at & 31;
            const int64_t relative_head = qhead + row * 24;
            scores[at] = lane_key < (uint)missing ?
                relative_adjust_vector(scores[at], query_pos[query + row], key_pos[start + lane_key],
                    window, relative + relative_head * 16, bias, sinks[relative_head], scale) :
                i2o_neg_inf();
        }
        if (!no_relative) sync_group();
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
                acc[i] = mma(acc[i], a, (uint2)(
                    load_q4_pair_rows((__global const uchar *)v, i0, i1, end, c),
                    load_q4_pair_rows((__global const uchar *)v, i2, i3, end, c)));
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
__kernel void i2_flash_prefill_softmax(__global const ushort *q, __global const ushort *k,
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
            if (thread < (uint)tail) {
                const float before = maxval[thread];
                float maximum = before;
                #pragma unroll
                for (uint i = 0; i < 32; ++i) {
                    const float score = scores[thread * 32 + i];
                    if (score > maximum) maximum = score;
                }
                alpha[thread] = i2o_exp(before - maximum);
                maxval[thread] = maximum;
                sum[thread] *= alpha[thread];
            } else alpha[thread] = 0;
        }
        sync_group();
        #pragma unroll
        for (uint index = thread; index < 16 * 32; index += 128) {
            const uint row = index >> 5;
            const float weight = row < (uint)tail ? i2o_exp(scores[index] - maxval[row]) : 0.0f;
            scores[index] = weight;
            probs[index] = half_bits(weight);
        }
        sync_group();
        if (thread < (uint)tail) {
            float total = sum[thread];
            #pragma unroll
            for (uint i = 0; i < 32; ++i) total += scores[thread * 32 + i];
            sum[thread] = total;
        }
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
__kernel void i2_flash_prefill_all_softmax(__global const ushort *q, __global const ushort *k,
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

    const int64_t earliest_query = query_pos[query];
    const int64_t latest_query = query_pos[query + tail - 1];
    for (int64_t start=begin; start<end; start+=32) {
        int missing = (end-start < 32 ? (int)(end-start) : 32);
        const int64_t first_key = key_pos[start];
        const int64_t last_key = key_pos[start + missing - 1];
        if (first_key > latest_query || (window && earliest_query - last_key >= window)) continue;
        const bool no_relative = !window && earliest_query - last_key >= 1024 && missing == 32;
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
                b = (uint2)(load_q4_pair_columns((__global const uchar *)k, offset),
                             load_q4_pair_columns((__global const uchar *)k, offset + 8));
            }
            qk = mma(qk,a,b);
        }
        const uint idx = group*32 + warp*8 + col;
        if (no_relative) {
            const float sink0 = group < (uint)tail ? sinks[qhead + group * 24] : 0;
            const float sink1 = group + 8 < (uint)tail ? sinks[qhead + (group + 8) * 24] : 0;
            qk.x = scale * qk.x + (0.0f - sink0);
            qk.y = scale * qk.y + (0.0f - sink0);
            qk.z = scale * qk.z + (0.0f - sink1);
            qk.w = scale * qk.w + (0.0f - sink1);
        }
        scores[idx] = qk.x;
        scores[idx+1] = qk.y;
        scores[idx+8*32] = qk.z;
        scores[idx+8*32+1] = qk.w;
        sync_group();
        if (!no_relative) for (uint at = thread; at < (uint)(tail * 32); at += 128) {
            const uint row = at >> 5;
            const uint lane_key = at & 31;
            const int64_t relative_head = qhead + row * 24;
            scores[at] = lane_key < (uint)missing ?
                relative_adjust_vector(scores[at], query_pos[query + row], key_pos[start + lane_key],
                    window, relative + relative_head * 16, bias, sinks[relative_head], scale) :
                i2o_neg_inf();
        }
        if (!no_relative) sync_group();
        if (thread < 16) {
            if (thread < (uint)tail) {
                const float before = maxval[thread];
                float maximum = before;
                #pragma unroll
                for (uint i = 0; i < 32; ++i) {
                    const float score = scores[thread * 32 + i];
                    if (score > maximum) maximum = score;
                }
                alpha[thread] = i2o_exp(before - maximum);
                maxval[thread] = maximum;
                sum[thread] *= alpha[thread];
            } else alpha[thread] = 0;
        }
        sync_group();
        #pragma unroll
        for (uint index = thread; index < 16 * 32; index += 128) {
            const uint row = index >> 5;
            const float weight = row < (uint)tail ? i2o_exp(scores[index] - maxval[row]) : 0.0f;
            scores[index] = weight;
            probs[index] = half_bits(weight);
        }
        sync_group();
        if (thread < (uint)tail) {
            float total = sum[thread];
            #pragma unroll
            for (uint i = 0; i < 32; ++i) total += scores[thread * 32 + i];
            sum[thread] = total;
        }
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
                acc[i] = mma(acc[i], a, (uint2)(
                    load_q4_pair_rows((__global const uchar *)v, i0, i1, end, c),
                    load_q4_pair_rows((__global const uchar *)v, i2, i3, end, c)));
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
