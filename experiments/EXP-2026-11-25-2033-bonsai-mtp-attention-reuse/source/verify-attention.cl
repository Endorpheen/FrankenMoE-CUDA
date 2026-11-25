static inline __attribute__((always_inline)) void verify_grouped_tiles(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 2 ? first + 256 : first_keys + 2;
    float query[3][2][8], sum[3][2][8], maximum[3][2], mass[3][2];
    #pragma unroll
    for (uint t = 0; t < 3; ++t) {
        #pragma unroll
        for (uint h = 0; h < heads; ++h) {
            maximum[t][h] = as_float(0xff800000u);
            mass[t][h] = 0.0f;
            #pragma unroll
            for (uint b = 0; b < 8; ++b) {
                query[t][h][b] = q[t * 6144 + (first_head + h) * 256 + b * 32 + lane];
                sum[t][h][b] = 0.0f;
            }
        }
    }

    for (uint row = first; row < last; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[3][2] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 3; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[3][2], weight[3][2];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            if (row >= first_keys + t) continue;
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const float score = bonsai_decode_warp_sum(dot[t][h]) * scale + 0.0f;
                const float next = score > maximum[t][h] ? score : maximum[t][h];
                previous[t][h] = attention_exp(maximum[t][h] - next);
                weight[t][h] = attention_exp(score - next);
                mass[t][h] = mass[t][h] * previous[t][h] + weight[t][h];
                maximum[t][h] = next;
            }
        }
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float value = q4_value(vr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 3; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 3; ++t) {
        const uint tiles = (first_keys + t + 255) / 256;
        if (tile >= tiles) continue;
        #pragma unroll
        for (uint h = 0; h < heads; ++h) {
            __global float *out = scratch + ((size_t)t * 24 * max_tiles +
                (first_head + h) * tiles + tile) * 258;
            if (lane == 0) {
                out[0] = maximum[t][h];
                out[1] = mass[t][h];
            }
            #pragma unroll
            for (uint b = 0; b < 8; ++b) out[2 + b * 32 + lane] = sum[t][h][b];
        }
    }
}

#define VERIFY_ATTN(H) \
__kernel void bonsai_verify_q4_gqa##H(__global const float *q, __global const uchar *k, \
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) { \
    verify_grouped_tiles(q, k, v, scratch, keys, max_tiles, scale, H); \
}
VERIFY_ATTN(1)
VERIFY_ATTN(2)
