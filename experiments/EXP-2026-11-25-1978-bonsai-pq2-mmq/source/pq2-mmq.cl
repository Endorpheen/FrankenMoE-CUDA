// Token-major F32 activations are rounded once per projection input. A
// separate launch lets sibling projections reuse the same packed rows.
static uint bonsai_half_pair(float lo, float hi) {
    uint packed;
    __asm__("cvt.rn.f16x2.f32 %0, %1, %2;" : "=r"(packed) : "f"(hi), "f"(lo));
    return packed;
}

__kernel void bonsai_pack_half(__global const float *input, __global uint *packed, uint pairs) {
    const uint index = 256 * work_group() + local_id();
    if (index < pairs) packed[index] = bonsai_half_pair(input[2 * index], input[2 * index + 1]);
}

static float4 bonsai_mma16x8(float4 acc, uint a0, uint a1, uint a2, uint a3, uint b0, uint b1) {
    float d0 = acc.x, d1 = acc.y, d2 = acc.z, d3 = acc.w;
    __asm__("mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
            : "+f"(d0), "+f"(d1), "+f"(d2), "+f"(d3)
            : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(b0), "r"(b1));
    return (float4)(d0, d1, d2, d3);
}

static void bonsai_store_mma(__global float *output, float4 acc, uint rows, uint tokens,
                             uint row, uint token) {
    if (token < tokens) {
        if (row < rows) output[(ulong)token * rows + row] = acc.x;
        if (row + 8 < rows) output[(ulong)token * rows + row + 8] = acc.z;
    }
    if (token + 1 < tokens) {
        if (row < rows) output[(ulong)(token + 1) * rows + row] = acc.y;
        if (row + 8 < rows) output[(ulong)(token + 1) * rows + row + 8] = acc.w;
    }
}

// Grid (ceil(rows/32), ceil(tokens/32)), block 128. All active projections
// use K divisible by 128, so each stage consumes one complete PQ2 block.
__kernel void bonsai_pq2_mmq(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local uint w[32][68];
    __local uint x[32][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 32 * work_group(), token_base = 32 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 2) + lane / 4;
    const uint t = 16 * (warp % 2) + lane / 4;
    const uint pair = lane % 4;
    const uint blocks = width / 128;
    float4 left = (float4)(0.0f), right = (float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        for (uint index = thread; index < 32 * 32; index += 128) {
            const uint local_row = index / 32, quad = index % 32;
            const uint row = row_base + local_row, token = token_base + local_row;
            uint a0 = 0, a1 = 0, b0 = 0, b1 = 0;
            if (row < rows) {
                const __global uchar *q = weights + ((ulong)row * blocks + block) * 34;
                const uint codes = q[2 + quad];
                const ushort scale_bits = *(__global const ushort *)q;
                float scale;
                __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(scale_bits));
                a0 = bonsai_half_pair(scale * (float)((int)(codes & 3) - 1),
                                      scale * (float)((int)((codes >> 2) & 3) - 1));
                a1 = bonsai_half_pair(scale * (float)((int)((codes >> 4) & 3) - 1),
                                      scale * (float)((int)(codes >> 6) - 1));
            }
            if (token < tokens) {
                const ulong col = (ulong)token * (width / 2) + block * 64 + quad * 2;
                b0 = input[col];
                b1 = input[col + 1];
            }
            w[local_row][quad * 2] = a0;
            w[local_row][quad * 2 + 1] = a1;
            x[local_row][quad * 2] = b0;
            x[local_row][quad * 2 + 1] = b1;
        }
        sync_threads();

        #pragma unroll
        for (uint k = 0; k < 64; k += 8) {
            const uint p = k + pair;
            const uint a0 = w[r][p], a1 = w[r + 8][p], a2 = w[r][p + 4], a3 = w[r + 8][p + 4];
            left = bonsai_mma16x8(left, a0, a1, a2, a3, x[t][p], x[t][p + 4]);
            right = bonsai_mma16x8(right, a0, a1, a2, a3, x[t + 8][p], x[t + 8][p + 4]);
        }
        sync_threads();
    }

    const uint out_token = token_base + 16 * (warp % 2) + 2 * pair;
    bonsai_store_mma(output, left, rows, tokens, row_base + r, out_token);
    bonsai_store_mma(output, right, rows, tokens, row_base + r, out_token + 8);
}
