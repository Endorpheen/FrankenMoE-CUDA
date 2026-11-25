static inline float q4_value(__global const uchar *block, uint lane) {
    float scale;
    const ushort bits = *(__global const ushort *)block;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(bits));
    const uint pair = block[2 + (lane & 15)];
    const uint nibble = lane < 16 ? pair & 15 : pair >> 4;
    return ((float)nibble - 8.0f) * scale;
}

static inline float warp_sum(float value) {
    for (uint delta = 16; delta != 0; delta >>= 1) {
        uint other;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;"
            : "=r"(other) : "r"(as_uint(value)), "r"(delta));
        value += as_float(other);
    }
    return value;
}

static inline float attention_exp(float value) {
    float result;
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result)
        : "f"(value * 1.4426950408889634f));
    return result;
}

static inline __attribute__((always_inline)) void grouped_tiles(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint keys, uint tiles, float scale, uint heads) {
    uint lane, job, tile;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(job));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile));
    const uint jobs_per_kv = 6 / heads;
    const uint kv = job / jobs_per_kv;
    const uint first_head = job * heads;
    const uint first = tile * 256;
    const uint last = first + 256 < keys ? first + 256 : keys;
    float query[3][8], sum[3][8];
    float maximum[3], mass[3];

    #pragma unroll
    for (uint h = 0; h < heads; ++h) {
        maximum[h] = as_float(0xff800000u);
        mass[h] = 0.0f;
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            query[h][b] = q[(first_head + h) * 256 + b * 32 + lane];
            sum[h][b] = 0.0f;
        }
    }

    for (uint row = first; row < last; ++row) {
        __global const uchar *kr = k + (size_t)row * 576 + kv * 144;
        __global const uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[3] = {0.0f, 0.0f, 0.0f};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                float next;
                __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                    : "f"(query[h][b]), "f"(key), "f"(dot[h]));
                dot[h] = next;
            }
        }
        float previous[3], weight[3];
        #pragma unroll
        for (uint h = 0; h < heads; ++h) {
            const float score = warp_sum(dot[h]) * scale + 0.0f;
            const float next = score > maximum[h] ? score : maximum[h];
            previous[h] = attention_exp(maximum[h] - next);
            weight[h] = attention_exp(score - next);
            mass[h] = mass[h] * previous[h] + weight[h];
            maximum[h] = next;
        }
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float value = q4_value(vr + 18 * b, lane);
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                sum[h][b] = sum[h][b] * previous[h] + weight[h] * value;
            }
        }
    }

    #pragma unroll
    for (uint h = 0; h < heads; ++h) {
        __global float *out = scratch + ((first_head + h) * tiles + tile) * 258;
        if (lane == 0) {
            out[0] = maximum[h];
            out[1] = mass[h];
        }
        #pragma unroll
        for (uint b = 0; b < 8; ++b) out[2 + b * 32 + lane] = sum[h][b];
    }
}

#define GROUPED(H) \
__kernel void bonsai_decode_gqa##H(__global const float *q, __global const uchar *k, \
    __global const uchar *v, __global float *scratch, uint keys, uint tiles, float scale) { \
    grouped_tiles(q, k, v, scratch, keys, tiles, scale, H); \
}
GROUPED(1)
GROUPED(2)
GROUPED(3)
