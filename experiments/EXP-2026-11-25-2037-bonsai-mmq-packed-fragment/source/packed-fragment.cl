static inline uint bonsai_pq2_fragment_pair(float scale, uint codes, uint nibble) {
    const uint pair = codes >> (4 * nibble);
    return bonsai_half_pair(scale * (float)((int)(pair & 3) - 1),
                            scale * (float)((int)((pair >> 2) & 3) - 1));
}

static inline float bonsai_pq2_fragment_scale(ushort stored) {
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(stored));
    return scale;
}

static inline __attribute__((always_inline)) void bonsai_pq2_compressed_fragment(
    __global const uchar *weights, __global const uint *input, __global float *output,
    uint width, uint rows, uint tokens, uint stage_words,
    __local ushort *packed, __local uint *activation) {
    const uint row_base = 64 * work_group(), token_base = 128 * flash_grid_y();
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint warp_row = 16 * (warp / 2), warp_token = 64 * (warp % 2);
    const uint blocks = width / 128, activation_stride = stage_words + 4;
    const uint r0 = warp_row + lane / 4, r1 = r0 + 8;
    const uint pair = lane % 4;
    float4 acc[8];
    #pragma unroll
    for (uint i = 0; i < 8; ++i) acc[i] = (float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        #pragma unroll
        for (uint part = 0; part < 4; ++part) {
            const uint index = thread + part * 256, r = index / 16, column = index % 16;
            ushort codes = 0, scale = 0;
            if (row_base + r < rows) {
                const __global ushort *quant = (__global const ushort *)(weights +
                    ((size_t)(row_base + r) * blocks + block) * 34);
                codes = quant[1 + column];
                if (column == 0) scale = quant[0];
            }
            packed[r * 18 + 1 + column] = codes;
            if (column == 0) packed[r * 18] = scale;
        }

        #pragma unroll
        for (uint stage = 0; stage < 64; stage += stage_words) {
            for (uint i = thread * 4; i < 128 * stage_words; i += 1024) {
                const uint token = token_base + i / stage_words, column = i % stage_words;
                const uint valid = token < tokens ? 16 : 0;
                const size_t source = valid ? (size_t)token * (width / 2) + block * 64 + stage + column : 0;
                bonsai_copy16(&activation[(i / stage_words) * activation_stride + column], input + source, valid);
            }
            bonsai_copy_complete();
            sync_threads();

            const float scale0 = bonsai_pq2_fragment_scale(packed[r0 * 18]);
            const float scale1 = bonsai_pq2_fragment_scale(packed[r1 * 18]);
            #pragma unroll
            for (uint k = 0; k < stage_words; k += 8) {
                const uint col = 1 + (stage + k) / 4;
                const ushort lo0 = packed[r0 * 18 + col], hi0 = packed[r0 * 18 + col + 1];
                const ushort lo1 = packed[r1 * 18 + col], hi1 = packed[r1 * 18 + col + 1];
                const uint a0 = bonsai_pq2_fragment_pair(scale0, lo0, pair);
                const uint a1 = bonsai_pq2_fragment_pair(scale1, lo1, pair);
                const uint a2 = bonsai_pq2_fragment_pair(scale0, hi0, pair);
                const uint a3 = bonsai_pq2_fragment_pair(scale1, hi1, pair);
                #pragma unroll
                for (uint i = 0; i < 8; ++i) {
                    const uint2 b = bonsai_load_b(&activation[(warp_token + 8*i) * activation_stride],
                                                  activation_stride, k, lane);
                    acc[i] = bonsai_mma16x8(acc[i], a0, a1, a2, a3, b.x, b.y);
                }
            }
            sync_threads();
        }
    }

    const uint token = token_base + warp_token + 2 * pair;
    #pragma unroll
    for (uint i = 0; i < 8; ++i) bonsai_store_mma(output, acc[i], rows, tokens, row_base + r0, token + 8*i);
}

#define PQ2_COMPRESSED(K, WORDS) \
__kernel void bonsai_pq2_fragment_##K(__global const uchar *weights, \
    __global const uint *input, __global float *output, uint width, uint rows, uint tokens) { \
    __local __attribute__((aligned(16))) ushort w[64][18]; \
    __local __attribute__((aligned(16))) uint x[128][WORDS + 4]; \
    bonsai_pq2_compressed_fragment(weights, input, output, width, rows, tokens, WORDS, &w[0][0], &x[0][0]); \
}
PQ2_COMPRESSED(k64, 32)
PQ2_COMPRESSED(k128, 64)
