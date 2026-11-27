static uint bonsai_grid_y(void);
static uint local_id(void) {
    uint n;
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(n));
    return n;
}

static uint work_group(void) {
    uint n;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(n));
    return n;
}

static void sync_threads(void) {
    __asm__ volatile("bar.sync 0;" ::: "memory");
}

static float pq2_element(__global const uchar *row, uint column) {
    const __global uchar *quant = row + (column / 128) * 34;
    const uint index = column % 128;
    const uint code = (quant[2 + index / 4] >> (2 * (index % 4))) & 3;
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*(__global const ushort *)quant));
    return scale * (float)((int)code - 1);
}

__kernel void bonsai_pq2_row(__global const uchar *weights, __global float *output,
                            uint width, uint row) {
    const uint column = work_group() * 256 + local_id();
    if (column < width) output[column] = pq2_element(weights + (ulong)row * (width / 128) * 34, column);
}

__kernel void bonsai_fwht1024(__global const float *input, __global const int *signs,
                             __global float *output, uint inverse,
                             uint head_width, uint key_heads, uint repeat, uint width) {
    const ulong offset = (ulong)bonsai_grid_y() * width;
    input += offset;
    output += offset;
    __local float values[1024];
    const uint lane = local_id(), base = work_group() * 1024;
    for (uint i = lane; i < 1024; i += 256) {
        const uint column = base + i;
        uint source = column;
        if (repeat > 1) {
            const uint head = column / head_width;
            source = (head % repeat * key_heads + head / repeat) * head_width + column % head_width;
        }
        const float value = input[source];
        values[i] = inverse ? value : value * (float)signs[column];
    }
    sync_threads();
    for (uint span = 1; span < 1024; span *= 2) {
        for (uint i = lane; i < 512; i += 256) {
            const uint a = (i / span) * (2 * span) + i % span;
            const float x = values[a], y = values[a + span];
            values[a] = x + y;
            values[a + span] = x - y;
        }
        sync_threads();
    }
    for (uint i = lane; i < 1024; i += 256) {
        const uint column = base + i;
        const float value = values[i] * 0.03125f;
        output[column] = inverse ? value * (float)signs[column] : value;
    }
}

__kernel void bonsai_pq2_matvec(__global const uchar *matrix, __global const float *input,
                               __global float *output, uint width, uint rows) {
    const uint thread = local_id(), lane = thread & 31, row = work_group() * 4 + thread / 32;
    if (row >= rows) return;
    const __global uchar *weights = matrix + (ulong)row * (width / 128) * 34;
    float sum = 0.0f;
    for (uint column = lane; column < width; column += 32) {
        const float w = pq2_element(weights, column);
        __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum) : "f"(w), "f"(input[column]));
    }
    for (int bit = 16; bit > 0; bit /= 2) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(peer) : "f"(sum), "r"(bit));
        sum += peer;
    }
    if (lane == 0) output[row] = sum;
}

extern float __nv_expf(float);

__kernel void bonsai_swiglu(__global const float *gate, __global const float *up,
                            __global float *output, uint width) {
    const uint i = work_group() * 256 + local_id();
    if (i < width) {
        const float x = gate[i];
        output[i] = (x / (1.0f + __nv_expf(-x))) * up[i];
    }
}

extern float __nv_expf(float);
extern float __nv_log1pf(float);
extern float __nv_sqrtf(float);

static float warp_sum(float sum) {
    for (int bit = 16; bit > 0; bit /= 2) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(peer) : "f"(sum), "r"(bit));
        sum += peer;
    }
    return sum;
}

static float silu(float x) {
    return x / (1.0f + __nv_expf(-x));
}

__kernel void bonsai_norm(__global const float *x, __global const float *weight,
                          __global const float *gate, __global float *out,
                          uint width, float eps, uint mode, uint heads, uint stride) {
    const uint lane = local_id(), row = work_group();
    const ulong source = (ulong)(row / heads) * stride + (row % heads) * width;
    float sum = 0.0f;
    for (uint i = lane; i < width; i += 32) {
        const float value = x[source + i];
        sum += value * value;
    }
    sum = warp_sum(sum);
    const float l2 = __nv_sqrtf(sum);
    const float d = mode == 1 ? (l2 > eps ? l2 : eps)
                              : __nv_sqrtf(sum / (float)width + eps);
    for (uint i = lane; i < width; i += 32) {
        const uint index = row * width + i;
        float value = x[source + i] / d;
        if (mode != 1) value *= weight[i];
        if (mode == 2) value *= silu(gate[index]);
        out[index] = value;
    }
}

__kernel void bonsai_decay(__global const float *x, __global const float *alpha,
                           __global const float *beta, __global const float *dt,
                           __global const float *a, __global float *gate,
                           __global float *mix) {
    const uint lane = local_id(), head = work_group() % 48, token = work_group() / 48;
    x += (ulong)token * 5120;
    gate += token * 48;
    mix += token * 48;
    float al = 0.0f, be = 0.0f;
    for (uint i = lane; i < 5120; i += 32) {
        al += x[i] * alpha[head * 5120 + i];
        be += x[i] * beta[head * 5120 + i];
    }
    al = warp_sum(al);
    be = warp_sum(be);
    if (lane == 0) {
        const float p = al + dt[head];
        gate[head] = a[head] * (p > 0.0f ? p + __nv_log1pf(__nv_expf(-p)) : __nv_log1pf(__nv_expf(p)));
        mix[head] = 1.0f / (1.0f + __nv_expf(-be));
    }
}

__kernel void bonsai_conv_step(__global const float *input, __global const float *weight,
                               __global float *history, __global float *out) {
    const uint col = work_group() * 256 + local_id();
    const uint width = 10240;
    const float a = history[col], b = history[width + col], c = history[2 * width + col];
    const float d = input[col];
    const __global float *w = weight + 4 * col;
    out[col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * d);
    history[col] = b;
    history[width + col] = c;
    history[2 * width + col] = d;
}

__kernel void bonsai_residual(__global const float *a, __global const float *b,
                              __global float *out) {
    const uint col = work_group() * 256 + local_id();
    out[col] = a[col] + b[col];
}

__kernel void bonsai_split_q_gate(__global const float *x, __global float *q,
                                  __global float *gate) {
    const uint col = work_group() * 256 + local_id();
    const uint base = work_group() * 512 + local_id();
    q[col] = x[base];
    gate[col] = x[base + 256];
}

__kernel void bonsai_attention_gate(__global const float *x, __global const float *gate,
                                   __global float *out) {
    const uint col = work_group() * 256 + local_id();
    out[col] = x[col] / (1.0f + __nv_expf(-gate[col]));
}

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

static uint bonsai_grid_y(void) {
    uint n;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(n));
    return n;
}

__kernel void bonsai_embed_batch(__global const uchar *weights, __global float *output,
                                __global const uint *token_ids) {
    const uint token = bonsai_grid_y();
    const uint col = work_group() * 256 + local_id();
    const __global uchar *row = weights + (ulong)token_ids[token] * 1360;
    output[(ulong)token * 5120 + col] = pq2_element(row, col);
}

__kernel void bonsai_conv_batch(__global const float *input, __global const float *weight,
                               __global float *history, __global float *out, uint count) {
    const uint col = work_group() * 256 + local_id();
    const uint width = 10240;
    float a = history[col], b = history[width + col], c = history[2 * width + col];
    const __global float *w = weight + 4 * col;
    for (uint token = 0; token < count; ++token) {
        const uint at = token * width + col;
        const float d = input[at];
        out[at] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * d);
        a = b;
        b = c;
        c = d;
    }
    history[col] = a;
    history[width + col] = b;
    history[2 * width + col] = c;
}

static float bonsai_ordered_fma(uint byte, uint shift, float scale, float value, float sum) {
    const float weight = scale * (float)((int)((byte >> shift) & 3u) - 1);
    __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum) : "f"(weight), "f"(value));
    return sum;
}

// Each lane accumulates columns lane + 32*j in the original order. A
// 128-weight block shares one scale across four successive lane updates.
static void bonsai_ordered_matvec(__global const uchar *matrix, __global const float *input,
                                  __global float *output, uint width, uint rows, uint exchange) {
    const uint thread = local_id(), lane = thread & 31;
    const uint row = work_group() * 4 + thread / 32;
    if (row >= rows) return;

    const uint blocks = width / 128;
    const uint shift = (lane & 3u) * 2u;
    const __global uchar *q = matrix + (ulong)row * blocks * 34;
    const __global float *x = input + lane;
    float sum = 0.0f;
    for (uint block = 0; block < blocks; ++block, q += 34, x += 128) {
        float scale;
        const ushort bits = *(__global const ushort *)q;
        __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(bits));
        uint codes[4];
        if (exchange) {
            const uint packed = q[2 + lane];
            for (uint j = 0; j < 4; ++j) {
                const uint peer = lane / 4 + 8*j;
                __asm__("shfl.sync.idx.b32 %0, %1, %2, 31, -1;"
                        : "=r"(codes[j]) : "r"(packed), "r"(peer));
            }
        } else {
            const __global uchar *packed = q + 2 + lane / 4;
            codes[0] = packed[0];
            codes[1] = packed[8];
            codes[2] = packed[16];
            codes[3] = packed[24];
        }
        sum = bonsai_ordered_fma(codes[0], shift, scale, x[0], sum);
        sum = bonsai_ordered_fma(codes[1], shift, scale, x[32], sum);
        sum = bonsai_ordered_fma(codes[2], shift, scale, x[64], sum);
        sum = bonsai_ordered_fma(codes[3], shift, scale, x[96], sum);
    }
    for (int bit = 16; bit > 0; bit /= 2) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(peer) : "f"(sum), "r"(bit));
        sum += peer;
    }
    if (lane == 0) output[row] = sum;
}

__kernel void bonsai_pq2_matvec_ordered(__global const uchar *matrix, __global const float *input,
                                       __global float *output, uint width, uint rows) {
    bonsai_ordered_matvec(matrix, input, output, width, rows, 0);
}

__kernel void bonsai_pq2_matvec_exchange(__global const uchar *matrix, __global const float *input,
                                        __global float *output, uint width, uint rows) {
    bonsai_ordered_matvec(matrix, input, output, width, rows, 1);
}
#define FLASH_KEYS 128
#define FLASH_ROWS 16
#define FLASH_VALUE 256
#define FLASH_PARTIAL (FLASH_VALUE + 2)

static uint flash_grid_y(void) {
    uint n;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(n));
    return n;
}

static float flash_exp(float x) {
    float result;
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(x * 1.4426950408889634f));
    return result;
}

static uint flash_min(uint a, uint b) { return a < b ? a : b; }

static float flash_max(float a, float b) { return a > b ? a : b; }

static float flash_shuffle(float x, uint bit) {
    float other;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(other) : "f"(x), "r"(bit));
    return other;
}

static float2 flash_q4_pair(__global const uchar *row, uint pair) {
    const uint block = pair / 16, index = pair % 16;
    const __global uchar *x = row + 18 * block;
    const ushort bits = *(__global const ushort *)x;
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(bits));
    const uint lo = x[2 + (2*index)%16], hi = x[3 + (2*index)%16];
    const uint shift = index >= 8 ? 4 : 0;
    return (float2)(scale * (float)((int)((lo >> shift) & 15) - 8),
                    scale * (float)((int)((hi >> shift) & 15) - 8));
}

static uint flash_pack_q4_pair(__global const uchar *row, uint pair) {
    const float2 x = flash_q4_pair(row, pair);
    return bonsai_half_pair(x.x, x.y);
}

// One CTA evaluates 2x6 query heads of a KV group against a 128-key tile.
// QK and PV use tensor cores with FP32 sums. FP32 partials are normalized
// and combined in increasing key-tile order by the second kernel.
__kernel void bonsai_flash_f32_tile(__global const float *query, __global const uchar *keys,
                                   __global const uchar *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start) {
    __local uint query_half[FLASH_ROWS][128];
    __local uint staging[32 * 128];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint kv_head = work % 4, first = (work / 4) * 2;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + first + (first+1 < queries)) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint token = first + r/8, h = r % 8;
        uint packed = 0;
        if (token < queries && h < 6) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint key = key_start + kb + index / 128;
            staging[index] = key < key_count
                ? flash_pack_q4_pair(keys + (ulong)key*576 + kv_head*144, index%128)
                : 0;
        }
        sync_threads();
        const uint key_row = warp*8 + lane_row;
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint p = k + lane_pair;
            acc = bonsai_mma16x8(acc, query_half[lane_row][p], query_half[lane_row+8][p],
                query_half[lane_row][p+4], query_half[lane_row+8][p+4],
                staging[key_row*128+p], staging[key_row*128+p+4]);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = first + r/8;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        __local ushort *transpose = (__local ushort *)staging;
        for (uint index = thread; index < 32*128; index += 128) {
            const uint row = index / 128, pair = index % 128, key = key_start + vb + row;
            const uint w = key < key_count
                ? flash_pack_q4_pair(values + (ulong)key*576 + kv_head*144,pair)
                : 0;
            transpose[(2*pair)*32+row] = (ushort)w;
            transpose[(2*pair+1)*32+row] = (ushort)(w >> 16);
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint p = ksub + lane_pair, k = vb/2 + p;
            const uint a0 = probability[lane_row][k], a1 = probability[lane_row+8][k];
            const uint a2 = probability[lane_row][k+4], a3 = probability[lane_row+8][k+4];
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature = 32*result + 8*warp + lane_row;
                acc[result] = bonsai_mma16x8(acc[result], a0,a1,a2,a3,
                    staging[feature*16+p],staging[feature*16+p+4]);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}

__kernel void bonsai_flash_f32_merge(__global const float *partial, __global float *output,
                                    uint queries, uint first_visible, uint tile_count, uint group_start) {
    const uint group = work_group()/12;
    const uint pair = group+group_start, r = work_group()%12, lane = local_id();
    const uint token = (pair/4)*2 + r/6, head = (pair%4)*6 + r%6;
    if (token >= queries) return;
    const uint rows = (first_visible+token+FLASH_KEYS-1)/FLASH_KEYS;
    const uint slot = (r/6)*8+r%6;
    const __global float *part = partial + ((ulong)group*tile_count*FLASH_ROWS+slot)*FLASH_PARTIAL;
    float max_seen = as_float(0xff800000u), sum = 0.0f;
    float v[8] = {0.0f};
    for (uint tile = 0; tile < rows; ++tile, part += FLASH_ROWS*FLASH_PARTIAL) {
        const float next = flash_max(max_seen,part[0]);
        const float old = flash_exp(max_seen-next), next_weight = flash_exp(part[0]-next);
        sum = sum*old + part[1]*next_weight;
        max_seen = next;
        #pragma unroll
        for (uint k = 0; k < 8; ++k) {
            v[k] = v[k]*old + part[2+lane+32*k]*next_weight;
        }
    }
    float inv;
    __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(sum));
    __global float *dst = output + ((ulong)token*24+head)*256 + lane;
    #pragma unroll
    for (uint k = 0; k < 8; ++k) dst[32*k] = v[k]*inv;
}
__kernel void bonsai_flash_skewed_tile(__global const float *query, __global const uchar *keys,
                                   __global const uchar *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start) {
    __local uint query_half[FLASH_ROWS][132];
    __local uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint kv_head = work % 4, first = (work / 4) * 2;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + first + (first+1 < queries)) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint token = first + r/8, h = r % 8;
        uint packed = 0;
        if (token < queries && h < 6) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint key = key_start + kb + index / 128;
            staging[(index/128)*132+index%128] = key < key_count
                ? flash_pack_q4_pair(keys + (ulong)key*576 + kv_head*144, index%128)
                : 0;
        }
        sync_threads();
        const uint key_row = warp*8 + lane_row;
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint p = k + lane_pair;
            acc = bonsai_mma16x8(acc, query_half[lane_row][p], query_half[lane_row+8][p],
                query_half[lane_row][p+4], query_half[lane_row+8][p+4],
                staging[key_row*132+p], staging[key_row*132+p+4]);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = first + r/8;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        __local ushort *transpose = (__local ushort *)staging;
        for (uint index = thread; index < 32*128; index += 128) {
            const uint row = index / 128, pair = index % 128, key = key_start + vb + row;
            const uint w = key < key_count
                ? flash_pack_q4_pair(values + (ulong)key*576 + kv_head*144,pair)
                : 0;
            transpose[(2*pair)*40+row] = (ushort)w;
            transpose[(2*pair+1)*40+row] = (ushort)(w >> 16);
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint p = ksub + lane_pair, k = vb/2 + p;
            const uint a0 = probability[lane_row][k], a1 = probability[lane_row+8][k];
            const uint a2 = probability[lane_row][k+4], a3 = probability[lane_row+8][k+4];
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature = 32*result + 8*warp + lane_row;
                acc[result] = bonsai_mma16x8(acc[result], a0,a1,a2,a3,
                    staging[feature*20+p],staging[feature*20+p+4]);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}


__kernel void bonsai_flash_staged_tile(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local uint query_half[FLASH_ROWS][132];
    __local uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint kv_head = work % 4, first = (work / 4) * 2;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + first + (first+1 < queries)) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint token = first + r/8, h = r % 8;
        uint packed = 0;
        if (token < queries && h < 6) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start + kb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/128)*132+index%128] = chunk < chunks ? keys[offset] : 0;
        }
        sync_threads();
        const uint key_row = warp*8 + lane_row;
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint p = k + lane_pair;
            acc = bonsai_mma16x8(acc, query_half[lane_row][p], query_half[lane_row+8][p],
                query_half[lane_row][p+4], query_half[lane_row+8][p+4],
                staging[key_row*132+p], staging[key_row*132+p+4]);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = first + r/8;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start+vb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/16)*20+index%16] = chunk < chunks ? values[offset] : 0;
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint p = ksub + lane_pair, k = vb/2 + p;
            const uint a0 = probability[lane_row][k], a1 = probability[lane_row+8][k];
            const uint a2 = probability[lane_row][k+4], a3 = probability[lane_row+8][k+4];
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature = 32*result + 8*warp + lane_row;
                acc[result] = bonsai_mma16x8(acc[result], a0,a1,a2,a3,
                    staging[feature*20+p],staging[feature*20+p+4]);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}


// Expand each Q4 block once for all subsequent query groups. K is key-major,
// and each 32-key tile of V is feature-major in packed half pairs.
__kernel void bonsai_flash_stage_kv(__global const uchar *k, __global const uchar *v,
                                    __global uint *packed_k, __global uint *packed_v,
                                    uint keys, uint chunks) {
    __local uint tile[32][132];
    const uint block = work_group(), head = flash_grid_y();
    const uint thread = local_id();
    const ulong base = ((ulong)head * chunks + block) * 4096;
    for (uint i = thread; i < 4096; i += 128) {
        const uint key = block*32 + i/128, pair = i%128;
        uint kk = 0, vv = 0;
        if (key < keys) {
            const ulong at = (ulong)key*576 + head*144;
            kk = flash_pack_q4_pair(k+at,pair);
            vv = flash_pack_q4_pair(v+at,pair);
        }
        packed_k[base+i] = kk;
        tile[i/128][pair] = vv;
    }
    sync_threads();
    for (uint i = thread; i < 4096; i += 128) {
        const uint feature = i/16, key = 2*(i%16), s = 16*(feature%2);
        const uint lo = (tile[key][feature/2] >> s) & 0xffff;
        const uint hi = (tile[key+1][feature/2] >> s) & 0xffff;
        packed_v[base+i] = lo | (hi<<16);
    }
}

static float bonsai_shuffle_up(float value, uint delta) {
    float peer;
    __asm__("shfl.sync.up.b32 %0, %1, %2, 0, -1;" : "=f"(peer) : "f"(value), "r"(delta));
    return peer;
}

static float bonsai_last_lane(float value) {
    float peer;
    __asm__("shfl.sync.idx.b32 %0, %1, 31, 31, -1;" : "=f"(peer) : "f"(value));
    return peer;
}

// Warp zero prepares the same keep/add coefficients as the scalar recurrence.
// Four warps consume them in increasing tile order with the original two
// multiplies and one addition per accumulator. Requires 128 threads.
__kernel void bonsai_flash_ordered_merge(__global const float *partial, __global float *output,
                                        uint queries, uint first_visible, uint tile_count, uint group_start) {
    __local float keep[32], add[32], inverse;
    const uint group = work_group()/12;
    const uint pair = group+group_start, r = work_group()%12, thread = local_id();
    const uint token = (pair/4)*2 + r/6, head = (pair%4)*6 + r%6;
    if (token >= queries) return;
    const uint rows = (first_visible+token+FLASH_KEYS-1)/FLASH_KEYS;
    const uint slot = (r/6)*8+r%6;
    const ulong stride = FLASH_ROWS*FLASH_PARTIAL;
    const __global float *part = partial + ((ulong)group*tile_count*FLASH_ROWS+slot)*FLASH_PARTIAL;
    float2 values = (float2)(0.0f);
    float maximum = as_float(0xff800000u);
    float sum = 0.0f;

    for (uint base = 0; base < rows; base += 32) {
        const uint active = flash_min(rows-base,32);
        if (thread < 32) {
            const float mx = thread < active ? part[(ulong)thread*stride] : as_float(0xff800000u);
            float seen = flash_max(maximum,mx);
            #pragma unroll
            for (uint bit = 1; bit < 32; bit *= 2) {
                const float prior = bonsai_shuffle_up(seen,bit);
                if (thread >= bit) seen = flash_max(prior,seen);
            }
            const float prior = bonsai_shuffle_up(seen,1);
            if (thread < active) {
                keep[thread] = flash_exp((thread == 0 ? maximum : prior)-seen);
                add[thread] = flash_exp(mx-seen);
            }
            maximum = bonsai_last_lane(seen);
        }
        sync_threads();
        for (uint i = 0; i < active; ++i) {
            const __global float *tile = part+(ulong)i*stride;
            const float2 input = *(__global const float2 *)(tile+2+2*thread);
            const float old = keep[i], weight = add[i];
            values = values*old + input*weight;
            if (thread == 0) sum = sum*old + tile[1]*weight;
        }
        part += 32*stride;
        sync_threads();
    }
    if (thread == 0) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(sum));
        inverse = inv;
    }
    sync_threads();
    __global float2 *dst = (__global float2 *)(output+((ulong)token*24+head)*256);
    dst[thread] = values*inverse;
}
__kernel void bonsai_flash_compact_tile(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local uint query_half[FLASH_ROWS][132];
    __local uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start + kb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/128)*132+index%128] = chunk < chunks ? keys[offset] : 0;
        }
        sync_threads();
        const uint key_row = warp*8 + lane_row;
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint p = k + lane_pair;
            acc = bonsai_mma16x8(acc, query_half[lane_row][p], query_half[lane_row+8][p],
                query_half[lane_row][p+4], query_half[lane_row+8][p+4],
                staging[key_row*132+p], staging[key_row*132+p+4]);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start+vb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/16)*20+index%16] = chunk < chunks ? values[offset] : 0;
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint p = ksub + lane_pair, k = vb/2 + p;
            const uint a0 = probability[lane_row][k], a1 = probability[lane_row+8][k];
            const uint a2 = probability[lane_row][k+4], a3 = probability[lane_row+8][k+4];
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature = 32*result + 8*warp + lane_row;
                acc[result] = bonsai_mma16x8(acc[result], a0,a1,a2,a3,
                    staging[feature*20+p],staging[feature*20+p+4]);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}
__kernel void bonsai_flash_compact_merge(__global const float *partial, __global float *output,
                                    uint queries, uint first_visible, uint tile_count, uint group_start) {
    const uint group = work_group()/FLASH_ROWS;
    const uint work = group+group_start, slot = work_group()%FLASH_ROWS;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint original_row = (work % groups_per_head)*FLASH_ROWS+slot;
    const uint token = original_row/6;
    const uint head = (work / groups_per_head)*6+original_row%6;
    const uint lane = local_id();
    if (token >= queries) return;
    const uint rows = (first_visible+token+FLASH_KEYS-1)/FLASH_KEYS;
    const __global float *part = partial + ((ulong)group*tile_count*FLASH_ROWS+slot)*FLASH_PARTIAL;
    float max_seen = as_float(0xff800000u), sum = 0.0f;
    float v[8] = {0.0f};
    for (uint tile = 0; tile < rows; ++tile, part += FLASH_ROWS*FLASH_PARTIAL) {
        const float next = flash_max(max_seen,part[0]);
        const float old = flash_exp(max_seen-next), next_weight = flash_exp(part[0]-next);
        sum = sum*old + part[1]*next_weight;
        max_seen = next;
        #pragma unroll
        for (uint k = 0; k < 8; ++k) {
            v[k] = v[k]*old + part[2+lane+32*k]*next_weight;
        }
    }
    float inv;
    __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(sum));
    __global float *dst = output + ((ulong)token*24+head)*256 + lane;
    #pragma unroll
    for (uint k = 0; k < 8; ++k) dst[32*k] = v[k]*inv;
}
__kernel void bonsai_flash_compact_ordered_merge(__global const float *partial, __global float *output,
                                        uint queries, uint first_visible, uint tile_count, uint group_start) {
    __local float keep[32], add[32], inverse;
    const uint group = work_group()/FLASH_ROWS;
    const uint work = group+group_start, slot = work_group()%FLASH_ROWS;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint original_row = (work % groups_per_head)*FLASH_ROWS+slot;
    const uint token = original_row/6;
    const uint head = (work / groups_per_head)*6+original_row%6;
    const uint thread = local_id();
    if (token >= queries) return;
    const uint rows = (first_visible+token+FLASH_KEYS-1)/FLASH_KEYS;
    const ulong stride = FLASH_ROWS*FLASH_PARTIAL;
    const __global float *part = partial + ((ulong)group*tile_count*FLASH_ROWS+slot)*FLASH_PARTIAL;
    float2 values = (float2)(0.0f);
    float maximum = as_float(0xff800000u);
    float sum = 0.0f;

    for (uint base = 0; base < rows; base += 32) {
        const uint active = flash_min(rows-base,32);
        if (thread < 32) {
            const float mx = thread < active ? part[(ulong)thread*stride] : as_float(0xff800000u);
            float seen = flash_max(maximum,mx);
            #pragma unroll
            for (uint bit = 1; bit < 32; bit *= 2) {
                const float prior = bonsai_shuffle_up(seen,bit);
                if (thread >= bit) seen = flash_max(prior,seen);
            }
            const float prior = bonsai_shuffle_up(seen,1);
            if (thread < active) {
                keep[thread] = flash_exp((thread == 0 ? maximum : prior)-seen);
                add[thread] = flash_exp(mx-seen);
            }
            maximum = bonsai_last_lane(seen);
        }
        sync_threads();
        for (uint i = 0; i < active; ++i) {
            const __global float *tile = part+(ulong)i*stride;
            const float2 input = *(__global const float2 *)(tile+2+2*thread);
            const float old = keep[i], weight = add[i];
            values = values*old + input*weight;
            if (thread == 0) sum = sum*old + tile[1]*weight;
        }
        part += 32*stride;
        sync_threads();
    }
    if (thread == 0) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(sum));
        inverse = inv;
    }
    sync_threads();
    __global float2 *dst = (__global float2 *)(output+((ulong)token*24+head)*256);
    dst[thread] = values*inverse;
}
static uint4 bonsai_load_a(__local const uint *base, uint stride, uint column, uint lane) {
    // Four row-block addresses: top-left, bottom-left, top-right, bottom-right.
    const uint address = (uint)(ulong)(base + (lane&15)*stride + column + (lane/16)*4);
    uint a0, a1, a2, a3;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x4.shared.b16 {%0,%1,%2,%3}, [%4];"
                     : "=r"(a0), "=r"(a1), "=r"(a2), "=r"(a3) : "r"(address) : "memory");
    return (uint4)(a0,a1,a2,a3);
}

static uint2 bonsai_load_b(__local const uint *base, uint stride, uint column, uint lane) {
    // K and V staging both store each output column's K elements together.
    const uint address = (uint)(ulong)(base + (lane&7)*stride + column + ((lane/8)&1)*4);
    uint b0, b1;
    __asm__ volatile("ldmatrix.sync.aligned.m8n8.x2.shared.b16 {%0,%1}, [%2];"
                     : "=r"(b0), "=r"(b1) : "r"(address) : "memory");
    return (uint2)(b0,b1);
}

static void bonsai_copy16(__local uint *dst, __global const uint *src, uint available) {
    const uint address = (uint)(ulong)dst;
    __asm__ volatile("cp.async.ca.shared.global [%0], [%1], 16, %2;"
                     :: "r"(address), "l"((ulong)src), "r"(available) : "memory");
}

static void bonsai_copy_complete(void) {
    __asm__ volatile("cp.async.commit_group; cp.async.wait_group 0;" ::: "memory");
}

__kernel void bonsai_flash_ldmatrix_tile(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start + kb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/128)*132+index%128] = chunk < chunks ? keys[offset] : 0;
        }
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        for (uint index = thread; index < 32*128; index += 128) {
            const uint chunk = (key_start+vb)/32;
            const ulong offset = ((ulong)kv_head*chunks+chunk)*4096 + index;
            staging[(index/16)*20+index%16] = chunk < chunks ? values[offset] : 0;
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}
__kernel void bonsai_flash_async_tile(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}
__kernel void bonsai_pq2_mmq_ldmatrix(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local __attribute__((aligned(16))) uint w[32][68];
    __local __attribute__((aligned(16))) uint x[32][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 32 * work_group(), token_base = 32 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 2) + lane / 4;
    const uint t = 16 * (warp % 2);
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
            const uint4 a = bonsai_load_a(&w[16*(warp/2)][0],68,k,lane);
            const uint2 b0 = bonsai_load_b(&x[t][0],68,k,lane);
            const uint2 b1 = bonsai_load_b(&x[t+8][0],68,k,lane);
            left = bonsai_mma16x8(left,a.x,a.y,a.z,a.w,b0.x,b0.y);
            right = bonsai_mma16x8(right,a.x,a.y,a.z,a.w,b1.x,b1.y);
        }
        sync_threads();
    }

    const uint out_token = token_base + 16 * (warp % 2) + 2 * pair;
    bonsai_store_mma(output, left, rows, tokens, row_base + r, out_token);
    bonsai_store_mma(output, right, rows, tokens, row_base + r, out_token + 8);
}
__kernel void bonsai_pq2_mmq_async(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local __attribute__((aligned(16))) uint w[32][68];
    __local __attribute__((aligned(16))) uint x[32][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 32 * work_group(), token_base = 32 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 2) + lane / 4;
    const uint t = 16 * (warp % 2);
    const uint pair = lane % 4;
    const uint blocks = width / 128;
    float4 left = (float4)(0.0f), right = (float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        for (uint packed = thread*4; packed < 32*64; packed += 512) {
            const uint token = token_base+packed/64, col = packed%64;
            const uint valid = token < tokens ? 16 : 0;
            const ulong source = valid ? (ulong)token*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+source,valid);
        }
        for (uint index = thread; index < 32 * 32; index += 128) {
            const uint local_row = index / 32, quad = index % 32;
            const uint row = row_base + local_row;
            uint a0 = 0, a1 = 0;
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
            w[local_row][quad * 2] = a0;
            w[local_row][quad * 2 + 1] = a1;
        }
        bonsai_copy_complete();
        sync_threads();

        #pragma unroll
        for (uint k = 0; k < 64; k += 8) {
            const uint4 a = bonsai_load_a(&w[16*(warp/2)][0],68,k,lane);
            const uint2 b0 = bonsai_load_b(&x[t][0],68,k,lane);
            const uint2 b1 = bonsai_load_b(&x[t+8][0],68,k,lane);
            left = bonsai_mma16x8(left,a.x,a.y,a.z,a.w,b0.x,b0.y);
            right = bonsai_mma16x8(right,a.x,a.y,a.z,a.w,b1.x,b1.y);
        }
        sync_threads();
    }

    const uint out_token = token_base + 16 * (warp % 2) + 2 * pair;
    bonsai_store_mma(output, left, rows, tokens, row_base + r, out_token);
    bonsai_store_mma(output, right, rows, tokens, row_base + r, out_token + 8);
}
// All four PQ2 codes are exact multiples of the stored F16 scale.
// Byte-permute directly into two F16 pairs, retaining signed zero.
static uint bonsai_pq2_pair(uint low, uint high, uint codes) {
    const uint selectors = 0x1010u | (codes&3u)*0x22u | ((codes>>2)&3u)*0x2200u;
    uint packed;
    __asm__("prmt.b32 %0, %1, %2, %3;"
            : "=r"(packed) : "r"(low), "r"(high), "r"(selectors));
    return packed;
}

static uint2 bonsai_pq2_quad(ushort scale, uint codes) {
    ushort twice;
    __asm__("add.rn.f16 %0, %1, %1;" : "=h"(twice) : "h"(scale));
    const uint low = (scale^0x8000u) | ((uint)(scale&0x8000u)<<16);
    const uint high = (uint)scale | ((uint)twice<<16);
    return (uint2)(bonsai_pq2_pair(low,high,codes), bonsai_pq2_pair(low,high,codes>>4));
}

__kernel void bonsai_pq2_mmq_lookup(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local __attribute__((aligned(16))) uint w[32][68];
    __local __attribute__((aligned(16))) uint x[32][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 32 * work_group(), token_base = 32 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 2) + lane / 4;
    const uint t = 16 * (warp % 2);
    const uint pair = lane % 4;
    const uint blocks = width / 128;
    float4 left = (float4)(0.0f), right = (float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        for (uint packed = thread*4; packed < 32*64; packed += 512) {
            const uint token = token_base+packed/64, col = packed%64;
            const uint valid = token < tokens ? 16 : 0;
            const ulong source = valid ? (ulong)token*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+source,valid);
        }
        for (uint index = thread; index < 32 * 32; index += 128) {
            const uint local_row = index / 32, quad = index % 32;
            const uint row = row_base + local_row;
            uint a0 = 0, a1 = 0;
            if (row < rows) {
                const __global uchar *q = weights + ((ulong)row * blocks + block) * 34;
                const uint codes = q[2 + quad];
                const ushort scale_bits = *(__global const ushort *)q;
                const uint2 operands = bonsai_pq2_quad(scale_bits,codes);
                a0 = operands.x;
                a1 = operands.y;
            }
            w[local_row][quad * 2] = a0;
            w[local_row][quad * 2 + 1] = a1;
        }
        bonsai_copy_complete();
        sync_threads();

        #pragma unroll
        for (uint k = 0; k < 64; k += 8) {
            const uint4 a = bonsai_load_a(&w[16*(warp/2)][0],68,k,lane);
            const uint2 b0 = bonsai_load_b(&x[t][0],68,k,lane);
            const uint2 b1 = bonsai_load_b(&x[t+8][0],68,k,lane);
            left = bonsai_mma16x8(left,a.x,a.y,a.z,a.w,b0.x,b0.y);
            right = bonsai_mma16x8(right,a.x,a.y,a.z,a.w,b1.x,b1.y);
        }
        sync_threads();
    }

    const uint out_token = token_base + 16 * (warp % 2) + 2 * pair;
    bonsai_store_mma(output, left, rows, tokens, row_base + r, out_token);
    bonsai_store_mma(output, right, rows, tokens, row_base + r, out_token + 8);
}
__kernel void bonsai_pq2_mmq_32x64(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local __attribute__((aligned(16))) uint w[32][68];
    __local __attribute__((aligned(16))) uint x[64][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 32 * work_group(), token_base = 64 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 4) + lane / 4;
    const uint t = 16 * (warp % 4);
    const uint pair = lane % 4;
    const uint blocks = width / 128;
    float4 acc[2];
    #pragma unroll
    for (uint i=0; i<2; ++i) acc[i]=(float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        for (uint packed = thread*4; packed < 64*64; packed += 1024) {
            const uint token = token_base+packed/64, col = packed%64;
            const uint valid = token < tokens ? 16 : 0;
            const ulong source = valid ? (ulong)token*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+source,valid);
        }
        for (uint index = thread; index < 32 * 32; index += 256) {
            const uint local_row = index / 32, quad = index % 32;
            const uint row = row_base + local_row;
            uint a0 = 0, a1 = 0;
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
            w[local_row][quad * 2] = a0;
            w[local_row][quad * 2 + 1] = a1;
        }
        bonsai_copy_complete();
        sync_threads();

        #pragma unroll
        for (uint k = 0; k < 64; k += 8) {
            const uint4 a = bonsai_load_a(&w[16*(warp/4)][0],68,k,lane);
            #pragma unroll
            for (uint i=0; i<2; ++i) {
                const uint2 b = bonsai_load_b(&x[t+8*i][0],68,k,lane);
                acc[i] = bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }

    const uint out_token = token_base + t + 2*pair;
    #pragma unroll
    for (uint i=0; i<2; ++i) {
        bonsai_store_mma(output,acc[i],rows,tokens,row_base+r,out_token+8*i);
    }

}
__kernel void bonsai_pq2_mmq_64x32(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][68];
    __local __attribute__((aligned(16))) uint x[32][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 64 * work_group(), token_base = 32 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 2) + lane / 4;
    const uint t = 16 * (warp % 2);
    const uint pair = lane % 4;
    const uint blocks = width / 128;
    float4 acc[2];
    #pragma unroll
    for (uint i=0; i<2; ++i) acc[i]=(float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        for (uint packed = thread*4; packed < 32*64; packed += 1024) {
            const uint token = token_base+packed/64, col = packed%64;
            const uint valid = token < tokens ? 16 : 0;
            const ulong source = valid ? (ulong)token*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+source,valid);
        }
        for (uint index = thread; index < 64 * 32; index += 256) {
            const uint local_row = index / 32, quad = index % 32;
            const uint row = row_base + local_row;
            uint a0 = 0, a1 = 0;
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
            w[local_row][quad * 2] = a0;
            w[local_row][quad * 2 + 1] = a1;
        }
        bonsai_copy_complete();
        sync_threads();

        #pragma unroll
        for (uint k = 0; k < 64; k += 8) {
            const uint4 a = bonsai_load_a(&w[16*(warp/2)][0],68,k,lane);
            #pragma unroll
            for (uint i=0; i<2; ++i) {
                const uint2 b = bonsai_load_b(&x[t+8*i][0],68,k,lane);
                acc[i] = bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }

    const uint out_token = token_base + t + 2*pair;
    #pragma unroll
    for (uint i=0; i<2; ++i) {
        bonsai_store_mma(output,acc[i],rows,tokens,row_base+r,out_token+8*i);
    }

}
__kernel void bonsai_pq2_mmq_64x64(__global const uchar *weights, __global const uint *input,
                             __global float *output, uint width, uint rows, uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][68];
    __local __attribute__((aligned(16))) uint x[64][68];
    uint tile_token;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(tile_token));
    const uint row_base = 64 * work_group(), token_base = 64 * tile_token;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint r = 16 * (warp / 2) + lane / 4;
    const uint t = 32 * (warp % 2);
    const uint pair = lane % 4;
    const uint blocks = width / 128;
    float4 acc[4];
    #pragma unroll
    for (uint i=0; i<4; ++i) acc[i]=(float4)(0.0f);

    for (uint block = 0; block < blocks; ++block) {
        for (uint packed = thread*4; packed < 64*64; packed += 1024) {
            const uint token = token_base+packed/64, col = packed%64;
            const uint valid = token < tokens ? 16 : 0;
            const ulong source = valid ? (ulong)token*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+source,valid);
        }
        for (uint index = thread; index < 64 * 32; index += 256) {
            const uint local_row = index / 32, quad = index % 32;
            const uint row = row_base + local_row;
            uint a0 = 0, a1 = 0;
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
            w[local_row][quad * 2] = a0;
            w[local_row][quad * 2 + 1] = a1;
        }
        bonsai_copy_complete();
        sync_threads();

        #pragma unroll
        for (uint k = 0; k < 64; k += 8) {
            const uint4 a = bonsai_load_a(&w[16*(warp/2)][0],68,k,lane);
            #pragma unroll
            for (uint i=0; i<4; ++i) {
                const uint2 b = bonsai_load_b(&x[t+8*i][0],68,k,lane);
                acc[i] = bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }

    const uint out_token = token_base + t + 2*pair;
    #pragma unroll
    for (uint i=0; i<4; ++i) {
        bonsai_store_mma(output,acc[i],rows,tokens,row_base+r,out_token+8*i);
    }

}
// Each warp preserves its lane-local accumulation and reduction order.
// The same load of alpha/beta weights feeds several independent tokens.
#define BONSAI_DECAY_REUSE(NAME, TILE) \
__kernel void NAME(__global const float *x,__global const float *alpha, \
                   __global const float *beta,__global const float *dt, \
                   __global const float *a,__global float *gate, \
                   __global float *mix,uint tokens) { \
    const uint thread=local_id(), lane=thread%32; \
    const uint work=work_group()*4+thread/32, head=work%48; \
    const uint first=(work/48)*(TILE); \
    if (first>=tokens) return; \
    const uint active=flash_min(tokens-first,(TILE)); \
    float al[TILE],be[TILE]; \
    for (uint t=0;t<(TILE);++t) {al[t]=0.0f;be[t]=0.0f;} \
    alpha+=head*5120; beta+=head*5120; \
    x+=(ulong)first*5120; \
    for (uint i=lane;i<5120;i+=32) { \
        const float wa=alpha[i], wb=beta[i]; \
        _Pragma("unroll") \
        for (uint t=0;t<(TILE);++t) { \
            if (t<active) { \
                const float value=x[t*5120+i]; \
                al[t]+=value*wa; be[t]+=value*wb; \
            } \
        } \
    } \
    _Pragma("unroll") \
    for (uint t=0;t<(TILE);++t) { \
        if (t<active) { \
            const float left=warp_sum(al[t]), right=warp_sum(be[t]); \
            if (lane==0) { \
                const float p=left+dt[head]; \
                const uint at=(first+t)*48+head; \
                gate[at]=a[head]*(p>0.0f ? p+__nv_log1pf(__nv_expf(-p)) : __nv_log1pf(__nv_expf(p))); \
                mix[at]=1.0f/(1.0f+__nv_expf(-right)); \
            } \
        } \
    } \
}
BONSAI_DECAY_REUSE(bonsai_decay_warp_pack,1)
BONSAI_DECAY_REUSE(bonsai_decay_reuse2,2)
BONSAI_DECAY_REUSE(bonsai_decay_reuse4,4)
BONSAI_DECAY_REUSE(bonsai_decay_reuse8,8)
static uint2 bonsai_ffn_unpack(__global const uchar *row,uint quad) {
    const uint codes=row[2+quad];
    float scale;
    __asm__("cvt.f32.f16 %0,%1;" : "=f"(scale) : "h"(*(__global const ushort *)row));
    return (uint2)(bonsai_half_pair(scale*(float)((int)(codes&3)-1),
                                   scale*(float)((int)((codes>>2)&3)-1)),
                  bonsai_half_pair(scale*(float)((int)((codes>>4)&3)-1),
                                   scale*(float)((int)(codes>>6)-1)));
}

static float bonsai_ffn_swiglu(float gate,float up) {
    return (gate/(1.0f+__nv_expf(-gate)))*up;
}

static float4 bonsai_ffn_result(float4 gate,float4 up) {
    return (float4)(bonsai_ffn_swiglu(gate.x,up.x),bonsai_ffn_swiglu(gate.y,up.y),
                    bonsai_ffn_swiglu(gate.z,up.z),bonsai_ffn_swiglu(gate.w,up.w));
}


__kernel void bonsai_ffn_fused_32x64(__global const uchar *weight_gate,
    __global const uchar *weight_up,__global const uint *input,__global float *output,
    uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[32][68];
    __local __attribute__((aligned(16))) uint x[64][68];
    __local __attribute__((aligned(16))) uint u[32][68];
    const uint blocks=width/128,row_base=32*work_group(),token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint r0=16*(warp/4),t0=16*(warp%4);
    float4 acc_gate[2],acc_up[2];
    #pragma unroll
    for (uint f=0;f<2;++f) { acc_gate[f]=(float4)(0); acc_up[f]=(float4)(0); }
    for (uint block=0;block<blocks;++block) {
        for (uint packed=thread*4;packed<64*64;packed+=1024) {
            const uint t=token_base+packed/64,col=packed%64;
            const uint valid=t<tokens ? 16 : 0;
            const ulong from=valid ? (ulong)t*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+from,valid);
        }
        const __global uchar *matrix=weight_gate;
        for (uint i=thread;i<32*32;i+=256) {
            const uint r=row_base+i/32,quad=i%32;
            const ulong from=((ulong)r*blocks+block)*34;
            const uint2 g=r<rows ? bonsai_ffn_unpack(matrix+from,quad) : (uint2)(0);
            w[i/32][quad*2]=g.x; w[i/32][quad*2+1]=g.y;
            const uint2 up=r<rows ? bonsai_ffn_unpack(weight_up+from,quad) : (uint2)(0);
            u[i/32][quad*2]=up.x; u[i/32][quad*2+1]=up.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<64;k+=8) {
            const uint4 a=bonsai_load_a(&w[r0][0],68,k,lane);
            #pragma unroll
            for (uint f=0;f<2;++f) {
                const uint2 b=bonsai_load_b(&x[t0+8*f][0],68,k,lane);
                const uint4 g=bonsai_load_a(&u[r0][0],68,k,lane);
                acc_gate[f]=bonsai_mma16x8(acc_gate[f],a.x,a.y,a.z,a.w,b.x,b.y);
                acc_up[f]=bonsai_mma16x8(acc_up[f],g.x,g.y,g.z,g.w,b.x,b.y);
            }
        }
        sync_threads();

    }
    const uint row=row_base+r0+lane/4,token=token_base+t0+2*(lane%4);
    #pragma unroll
    for (uint f=0;f<2;++f) {
        bonsai_store_mma(output,bonsai_ffn_result(acc_gate[f],acc_up[f]),rows,tokens,row,token+8*f);
    }
}

__kernel void bonsai_ffn_fused_64x64(__global const uchar *weight_gate,
    __global const uchar *weight_up,__global const uint *input,__global float *output,
    uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][68];
    __local __attribute__((aligned(16))) uint x[64][68];

    const uint blocks=width/128,row_base=64*work_group(),token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint r0=16*(warp/2),t0=32*(warp%2);
    float4 acc_gate[4],acc_up[4];
    #pragma unroll
    for (uint f=0;f<4;++f) { acc_gate[f]=(float4)(0); acc_up[f]=(float4)(0); }
    for (uint block=0;block<blocks;++block) {
        for (uint packed=thread*4;packed<64*64;packed+=1024) {
            const uint t=token_base+packed/64,col=packed%64;
            const uint valid=t<tokens ? 16 : 0;
            const ulong from=valid ? (ulong)t*(width/2)+block*64+col : 0;
            bonsai_copy16(&x[packed/64][col],input+from,valid);
        }
        for (uint plane=0;plane<2;++plane) {
            const __global uchar *matrix=plane ? weight_up : weight_gate;
        for (uint i=thread;i<64*32;i+=256) {
            const uint r=row_base+i/32,quad=i%32;
            const ulong from=((ulong)r*blocks+block)*34;
            const uint2 g=r<rows ? bonsai_ffn_unpack(matrix+from,quad) : (uint2)(0);
            w[i/32][quad*2]=g.x; w[i/32][quad*2+1]=g.y;

        }
        if (plane==0) bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<64;k+=8) {
            const uint4 a=bonsai_load_a(&w[r0][0],68,k,lane);
            #pragma unroll
            for (uint f=0;f<4;++f) {
                const uint2 b=bonsai_load_b(&x[t0+8*f][0],68,k,lane);
                if (plane==0) acc_gate[f]=bonsai_mma16x8(acc_gate[f],a.x,a.y,a.z,a.w,b.x,b.y);
                else acc_up[f]=bonsai_mma16x8(acc_up[f],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
        }
    }
    const uint row=row_base+r0+lane/4,token=token_base+t0+2*(lane%4);
    #pragma unroll
    for (uint f=0;f<4;++f) {
        bonsai_store_mma(output,bonsai_ffn_result(acc_gate[f],acc_up[f]),rows,tokens,row,token+8*f);
    }
}
// Exact SwiGLU, signed FWHT and FP16 pack. No global intermediate
// between these three operations. The following MMQ reads the packed output.
__kernel void bonsai_ffn_tail(__global const float *gate,__global const float *up,
                              __global const int *signs,__global uint *packed,uint width) {
    __local float values[1024];
    const ulong token=(ulong)flash_grid_y()*width;
    const uint base=work_group()*1024,lane=local_id();
    for (uint i=lane;i<1024;i+=256) {
        const float result=bonsai_ffn_swiglu(gate[token+base+i],up[token+base+i]);
        values[i]=result*(float)signs[base+i];
    }
    sync_threads();
    for (uint span=1;span<1024;span*=2) {
        for (uint i=lane;i<512;i+=256) {
            const uint a=(i/span)*(2*span)+i%span;
            const float x=values[a],y=values[a+span];
            values[a]=x+y;
            values[a+span]=x-y;
        }
        sync_threads();
    }
    for (uint i=lane;i<512;i+=256) {
        packed[(token+base)/2+i]=bonsai_half_pair(values[2*i]*0.03125f,values[2*i+1]*0.03125f);
    }
}

__kernel void bonsai_pq2_mmq_64x64_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[64][36];
    const uint row_base=64*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint blocks=width/128;
    float4 acc[4];
    #pragma unroll
    for (uint i=0;i<4;++i) acc[i]=(float4)(0.0f);
    for (uint step=0;step<2*blocks;++step) {
        for (uint packed=thread*4;packed<64*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        for (uint index=thread;index<64*16;index+=256) {
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<4;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<4;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}


__kernel void bonsai_pq2_mmq_64x128_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    for (uint step=0;step<2*blocks;++step) {
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        for (uint index=thread;index<64*16;index+=256) {
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<8;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}


__kernel void bonsai_pq2_mmq_128x64_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[128][36];
    __local __attribute__((aligned(16))) uint x[64][36];
    const uint row_base=128*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/1),warp_token=64*(warp%1);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    for (uint step=0;step<2*blocks;++step) {
        for (uint packed=thread*4;packed<64*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        for (uint index=thread;index<128*16;index+=256) {
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<8;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}


__kernel void bonsai_pq2_mmq_128x128_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[128][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=128*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/1),warp_token=128*(warp%1);
    const uint blocks=width/128;
    float4 acc[16];
    #pragma unroll
    for (uint i=0;i<16;++i) acc[i]=(float4)(0.0f);
    for (uint step=0;step<2*blocks;++step) {
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        for (uint index=thread;index<128*16;index+=256) {
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<16;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<16;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

static inline float q4_value(__global const uchar *block, uint lane) {
    float scale;
    const ushort bits = *(__global const ushort *)block;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(bits));
    const uint pair = block[2 + (lane & 15)];
    const uint nibble = lane < 16 ? pair & 15 : pair >> 4;
    return ((float)nibble - 8.0f) * scale;
}

static inline float bonsai_decode_warp_sum(float value) {
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
            const float score = bonsai_decode_warp_sum(dot[h]) * scale + 0.0f;
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
static inline __attribute__((always_inline)) void bonsai_pq2_rows(
    __global const uchar *restrict matrix, __global const float *restrict input,
    __global float *restrict output, uint width, uint rows, uint row_group) {
    const uint thread = local_id();
    const uint lane = thread & 31;
    const uint first = (work_group() * 4 + thread / 32) * row_group;
    if (first >= rows) return;
    const uint blocks = width / 128;
    const size_t stride = (size_t)blocks * 34;
    const uint shift = (lane & 3u) * 2u;
    float sums[4] = {0};
    __global const uchar *row[4];
    #pragma unroll
    for (uint j = 0; j < row_group; ++j) {
        const uint index = first + j < rows ? first + j : rows - 1;
        row[j] = matrix + index * stride;
    }
    const __global float *x = input + lane;
    for (uint block = 0; block < blocks; ++block, x += 128) {
        const float x0 = x[0], x1 = x[32], x2 = x[64], x3 = x[96];
        uint codes[4][4];
        float scales[4];
        #pragma unroll
        for (uint j = 0; j < row_group; ++j) {
            const __global uchar *q = row[j] + block * 34;
            const ushort bits = *(__global const ushort *)q;
            __asm__("cvt.f32.f16 %0, %1;" : "=f"(scales[j]) : "h"(bits));
            const __global uchar *packed = q + 2 + lane / 4;
            codes[j][0] = packed[0];
            codes[j][1] = packed[8];
            codes[j][2] = packed[16];
            codes[j][3] = packed[24];
        }
        #pragma unroll
        for (uint j = 0; j < row_group; ++j) {
            float sum = sums[j];
            const float scale = scales[j];
            sum = bonsai_ordered_fma(codes[j][0], shift, scale, x0, sum);
            sum = bonsai_ordered_fma(codes[j][1], shift, scale, x1, sum);
            sum = bonsai_ordered_fma(codes[j][2], shift, scale, x2, sum);
            sums[j] = bonsai_ordered_fma(codes[j][3], shift, scale, x3, sum);
        }
    }
    #pragma unroll
    for (uint j = 0; j < row_group; ++j) {
        float sum = sums[j];
        for (int bit = 16; bit > 0; bit /= 2) {
            float peer;
            __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;"
                : "=f"(peer) : "f"(sum), "r"(bit));
            sum += peer;
        }
        if (lane == 0 && first + j < rows) output[first + j] = sum;
    }
}

#define ROWS(N) \
__kernel void bonsai_pq2_matvec_rows##N(__global const uchar *matrix, \
    __global const float *input, __global float *output, uint width, uint rows) { \
    bonsai_pq2_rows(matrix, input, output, width, rows, N); \
}
ROWS(2)
ROWS(4)
static float2 local_flash_q4_pair(__local const uchar *row, uint pair) {
    const uint block = pair / 16, index = pair % 16;
    const __local uchar *x = row + 18 * block;
    const ushort bits = *(__local const ushort *)x;
    float scale;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(bits));
    const uint lo = x[2 + (2*index)%16], hi = x[3 + (2*index)%16];
    const uint shift = index >= 8 ? 4 : 0;
    return (float2)(scale * (float)((int)((lo >> shift) & 15) - 8),
                    scale * (float)((int)((hi >> shift) & 15) - 8));
}

static uint local_flash_pack_q4_pair(__local const uchar *row, uint pair) {
    const float2 x = local_flash_q4_pair(row, pair);
    return bonsai_half_pair(x.x, x.y);
}


static inline void flash_copy_q4_tile(__local uint *packed, __global const uchar *kv,
    uint head, uint first, uint rows, uint thread) {
    for (uint task = thread; task < 32 * 9; task += 128) {
        const uint row = first + task / 9;
        const uint offset = task % 9;
        const uint bytes = row < rows ? 16 : 0;
        const size_t src = bytes ? (size_t)row * 576 + head * 144 + offset * 16 : 0;
        bonsai_copy16(packed + task * 4, (__global const uint *)(kv + src), bytes);
    }
    bonsai_copy_complete();
    sync_threads();
}

__kernel void bonsai_flash_direct_k(__global const float *query, __global const uchar *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local __attribute__((aligned(16))) uint raw[32 * 36];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        flash_copy_q4_tile(raw, keys, kv_head, key_start+kb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            staging[key*132+pair] = key_start+kb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
        }
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}

__kernel void bonsai_flash_direct_v(__global const float *query, __global const uint *keys,
                                   __global const uchar *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local __attribute__((aligned(16))) uint raw[32 * 36];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
        const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
        for (uint index = thread*4; index < 32*128; index += 512) {
            bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
        }
        bonsai_copy_complete();
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        flash_copy_q4_tile(raw, values, kv_head, key_start+vb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            const uint packed = key_start+vb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
            __local ushort *values_t = (__local ushort *)staging;
            values_t[pair*80+key] = (ushort)packed;
            values_t[pair*80+40+key] = (ushort)(packed>>16);
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}

__kernel void bonsai_flash_direct_kv(__global const float *query, __global const uchar *keys,
                                   __global const uchar *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local __attribute__((aligned(16))) uint raw[32 * 36];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile = flash_grid_y(), key_start = tile * FLASH_KEYS;
    if (key_start >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);
    __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;

    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
        flash_copy_q4_tile(raw, keys, kv_head, key_start+kb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            staging[key*132+pair] = key_start+kb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
        }
        sync_threads();
        float4 acc = (float4)(0.0f);
        #pragma unroll 8
        for (uint k = 0; k < 128; k += 8) {
            const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
            const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
            acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
        }
        const uint key_col = kb + warp*8 + 2*lane_pair;
        score[lane_row][key_col] = acc.x * 0.0625f;
        score[lane_row][key_col+1] = acc.y * 0.0625f;
        score[lane_row+8][key_col] = acc.z * 0.0625f;
        score[lane_row+8][key_col+1] = acc.w * 0.0625f;
        sync_threads();
    }

    for (uint r = warp; r < FLASH_ROWS; r += 4) {
        const uint token = (first_row+r)/6;
        const uint visible = flash_min(key_count, first_visible + token);
        const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
        float mx = neg_inf;
        for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
        for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
        float sum = 0.0f;
        for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
            const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
            const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
            sum += lo + hi;
            probability[r][z/2] = bonsai_half_pair(lo,hi);
        }
        for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
        if (lane == 0) {
            out[r*FLASH_PARTIAL] = mx;
            out[r*FLASH_PARTIAL+1] = sum;
        }
    }
    sync_threads();

    float4 acc[8];
    #pragma unroll
    for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
    for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
        flash_copy_q4_tile(raw, values, kv_head, key_start+vb, key_count, thread);
        for (uint index = thread; index < 4096; index += 128) {
            const uint key = index/128, pair = index%128;
            const uint packed = key_start+vb+key < key_count
                ? local_flash_pack_q4_pair((__local const uchar *)(raw + key*36), pair) : 0;
            __local ushort *values_t = (__local ushort *)staging;
            values_t[pair*80+key] = (ushort)packed;
            values_t[pair*80+40+key] = (ushort)(packed>>16);
        }
        sync_threads();
        #pragma unroll
        for (uint ksub = 0; ksub < 16; ksub += 8) {
            const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
            #pragma unroll
            for (uint result = 0; result < 8; ++result) {
                const uint feature_block = (32*result + 8*warp)*20;
                const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint column = 32*result + 8*warp + 2*lane_pair + 2;
        out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
        out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
        out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
        out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
    }
}

__kernel void bonsai_flash_tile_group2(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y() * 2;
    if (tile_base * FLASH_KEYS >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint j = 0; j < 2; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + last_row/6) break;
        __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = thread*4; index < 32*128; index += 512) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
            score[lane_row+8][key_col] = acc.z * 0.0625f;
            score[lane_row+8][key_col+1] = acc.w * 0.0625f;
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
                sum += lo + hi;
                probability[r][z/2] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            if (lane == 0) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = thread*4; index < 32*128; index += 512) {
                bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*20;
                    const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        #pragma unroll
        for (uint result = 0; result < 8; ++result) {
            const uint column = 32*result + 8*warp + 2*lane_pair + 2;
            out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
            out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
            out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
            out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
        }
    }
}
__kernel void bonsai_flash_tile_group4(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint query_half[FLASH_ROWS][132];
    __local __attribute__((aligned(16))) uint staging[256 * 20];
    __local float score[FLASH_ROWS][FLASH_KEYS];
    __local __attribute__((aligned(16))) uint probability[FLASH_ROWS][FLASH_KEYS/2];

    const uint work = work_group() + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y() * 4;
    if (tile_base * FLASH_KEYS >= first_visible + last_row/6) return;
    const uint thread = local_id(), warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint j = 0; j < 4; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + last_row/6) break;
        __global float *out = partial + ((ulong)work_group()*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = thread*4; index < 32*128; index += 512) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
            score[lane_row+8][key_col] = acc.z * 0.0625f;
            score[lane_row+8][key_col+1] = acc.w * 0.0625f;
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
                sum += lo + hi;
                probability[r][z/2] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            if (lane == 0) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = thread*4; index < 32*128; index += 512) {
                bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*20;
                    const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        #pragma unroll
        for (uint result = 0; result < 8; ++result) {
            const uint column = 32*result + 8*warp + 2*lane_pair + 2;
            out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
            out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
            out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
            out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
        }
    }
}
float __nv_roundf(float);

static inline float mvq_half(ushort bits) {
    float result;
    __asm__("cvt.f32.f16 %0, %1;" : "=f"(result) : "h"(bits));
    return result;
}

__kernel void bonsai_mv_pack_q8(__global const float *x, __global uchar *out, uint groups) {
    const uint thread = local_id(), lane = thread & 31;
    const uint group = work_group()*4 + thread/32;
    if (group >= groups) return;
    const float value = x[group*32 + lane];
    float peak = as_float(as_uint(value) & 0x7fffffff);
    for (uint bit = 16; bit; bit >>= 1) peak = flash_max(peak,flash_shuffle(peak,bit));
    float scale, normalized = 0.0f;
    __asm__("div.rn.f32 %0, %1, %2;" : "=f"(scale) : "f"(peak), "f"(127.0f));
    if (peak != 0.0f) {
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(normalized) : "f"(value), "f"(scale));
    }
    ushort encoded;
    __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(encoded) : "f"(scale));
    __global uchar *row = out + group*36;
    if (lane == 0) *(__global uint *)row = encoded;
    row[4+lane] = (uchar)(int)__nv_roundf(normalized);
}

static inline uint mvq_permute(uint a, uint b, uint order) {
    uint out;
    __asm__("prmt.b32 %0, %1, %2, %3;" : "=r"(out) : "r"(a), "r"(b), "r"(order));
    return out;
}

static inline int mvq_dot(__global const ushort *pair, __global const uint *a) {
    const uint map = 0x020100ff;
    int sum = 0;
    #pragma unroll
    for (uint i = 0; i < 4; ++i) {
        const uint code = pair[i];
        const uint even = mvq_permute(map,map,code & 0x3333);
        const uint odd = mvq_permute(map,map,(code>>2) & 0x3333);
        const uint w0 = mvq_permute(even,odd,0x5140);
        const uint w1 = mvq_permute(even,odd,0x7362);
        __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(sum) : "r"(w0), "r"(a[2*i]));
        __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(sum) : "r"(w1), "r"(a[2*i+1]));
    }
    return sum;
}

static inline __attribute__((always_inline)) void mvq_rows(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread=local_id(), lane=thread&31;
    const uint start=(work_group()*4+thread/32)*grouping;
    if (start>=rows) return;
    const uint blocks=width/128;
    const uint groups=width/32;
    float value[2] = {0.0f,0.0f};
    for (uint i=lane; i<groups; i+=32) {
        const __global uchar *a=input+i*36;
        const float dx=mvq_half(*(__global const ushort *)a);
        #pragma unroll
        for (uint r=0; r<grouping; ++r) {
            const uint row=start+r < rows ? start+r : rows-1;
            const __global uchar *w=weights+((size_t)row*blocks+i/4)*34;
            const int dot=mvq_dot((__global const ushort *)(w+2+(i%4)*8),(__global const uint *)(a+4));
            const float dw=mvq_half(*(__global const ushort *)w);
            value[r]+=(dw*dx)*(float)dot;
        }
    }
    #pragma unroll
    for (uint r=0; r<grouping; ++r) {
        for (uint bit=16; bit; bit>>=1) value[r]+=flash_shuffle(value[r],bit);
        if (lane==0 && start+r<rows) output[start+r]=value[r];
    }
}
#define MVQ(N) \
__kernel void bonsai_pq2_mvq_q8_##N(__global const uchar *weights, __global const uchar *input, \
    __global float *output, uint width, uint rows) { \
    mvq_rows(weights,input,output,width,rows,N); \
}
MVQ(1)
MVQ(2)
static inline __attribute__((always_inline)) void wide_norm(
    __global const float *x, __global const float *weight, __global float *out,
    float eps, __local float *cache, __local float *divisor, uint threads, uint stage) {
    const uint t = local_id();
    const size_t base = (size_t)work_group() * 5120;
    if (stage) {
        for (uint i=t; i<5120; i+=threads) cache[i]=x[base+i];
        sync_threads();
    }
    if (t<32) {
        float sum=0.0f;
        #pragma unroll 4
        for (uint i=t; i<5120; i+=32) {
            const float value = stage ? cache[i] : x[base+i];
            sum += value * value;
        }
        sum=warp_sum(sum);
        if (t==0) *divisor=__nv_sqrtf(sum / 5120.0f + eps);
    }
    sync_threads();
    const float d=*divisor;
    for (uint i=t; i<5120; i+=threads) {
        const float value=stage ? cache[i] : x[base+i];
        out[base+i]=(value / d) * weight[i];
    }
}

#define WIDE(T,S,C) \
__kernel void bonsai_norm_wide_##T##_##S(__global const float *x, __global const float *weight, \
    __global float *out, float eps) { \
    __local float cache[C], divisor; \
    wide_norm(x,weight,out,eps,cache,&divisor,T,S); \
}
WIDE(128,0,1)
WIDE(256,0,1)
WIDE(128,1,5120)
WIDE(256,1,5120)
WIDE(512,1,5120)
static uint bonsai_quantize_q8_value(float value, float scale) {
    int result = 0;
    if (scale > 0.0f) {
        const float normalized = value/scale;
        __asm__("cvt.rni.s32.f32 %0, %1;" : "=r"(result) : "f"(normalized));
        result = result < -127 ? -127 : (result > 127 ? 127 : result);
    }
    return (uint)result&255u;
}

// Launch 128 threads: each of the four warps owns one token/PQ2 block.
__kernel void bonsai_pack_q8_128(__global const float *input, __global uint *packed,
                                 __global float *scales, uint groups) {
    const uint thread=local_id(), lane=thread%32;
    const uint row=4*work_group()+thread/32;
    if (row>=groups) return;
    const __global float *x=input+(ulong)row*128+4*lane;
    const float4 v=(float4)(x[0],x[1],x[2],x[3]);
    float largest=as_float(as_uint(v.x)&0x7fffffffu);
    largest=flash_max(largest,as_float(as_uint(v.y)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.z)&0x7fffffffu));
    largest=flash_max(largest,as_float(as_uint(v.w)&0x7fffffffu));
    for (uint bit=16;bit;bit/=2) largest=flash_max(largest,flash_shuffle(largest,bit));
    const float scale=largest/127.0f;
    packed[(ulong)row*32+lane]=bonsai_quantize_q8_value(v.x,scale)
        | (bonsai_quantize_q8_value(v.y,scale)<<8)
        | (bonsai_quantize_q8_value(v.z,scale)<<16)
        | (bonsai_quantize_q8_value(v.w,scale)<<24);
    if (lane==0) scales[row]=scale;
}

static uint bonsai_expand_pq2_byte(uint code) {
    const uint selector=(code&3u) | ((code&12u)<<2) | ((code&48u)<<4) | ((code&192u)<<6);
    uint result;
    __asm__("prmt.b32 %0, %1, %1, %2;" : "=r"(result) : "r"(0x020100ffu), "r"(selector));
    return result;
}

static int4 bonsai_mma_i8(int4 sum, uint4 a, uint2 b) {
    int s0=sum.x,s1=sum.y,s2=sum.z,s3=sum.w;
    __asm__("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
            "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(s0),"+r"(s1),"+r"(s2),"+r"(s3)
        : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (int4)(s0,s1,s2,s3);
}

__kernel void bonsai_pq2_mmq_int8(__global const uchar *weights, __global const uint *input,
                                 __global const float *scales, __global float *output,
                                 uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36],x[64][36];
    __local float weight_scale[64],token_scale[64];
    const uint blocks=width/128, row_base=64*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint row=warp_row+lane/4, token=warp_token+2*(lane%4);
    float4 result[4];
    #pragma unroll
    for (uint i=0;i<4;++i) result[i]=(float4)(0.0f);

    for (uint block=0;block<blocks;++block) {
        for (uint packed=4*thread;packed<64*32;packed+=1024) {
            const uint t=token_base+packed/32;
            const uint size=t<tokens ? 16 : 0;
            const ulong from=size ? ((ulong)t*blocks+block)*32+packed%32 : 0;
            bonsai_copy16(&x[packed/32][packed%32],input+from,size);
        }
        if (thread<64) {
            const uint t=token_base+thread, r=row_base+thread;
            token_scale[thread]=t<tokens ? scales[(ulong)t*blocks+block] : 0.0f;
            float scale=0.0f;
            if (r<rows) {
                const __global ushort *encoded=(__global const ushort *)(weights+((ulong)r*blocks+block)*34);
                __asm__("cvt.f32.f16 %0, %1;" : "=f"(scale) : "h"(*encoded));
            }
            weight_scale[thread]=scale;
        }
        for (uint i=thread;i<64*32;i+=256) {
            const uint r=row_base+i/32;
            const uint code=r<rows ? weights[((ulong)r*blocks+block)*34+2+i%32] : 0x55u;
            w[i/32][i%32]=bonsai_expand_pq2_byte(code);
        }
        bonsai_copy_complete();
        sync_threads();

        int4 dot[4];
        #pragma unroll
        for (uint f=0;f<4;++f) dot[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<4;++f) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                dot[f]=bonsai_mma_i8(dot[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint f=0;f<4;++f) {
            const float xa=token_scale[token+8*f],xb=token_scale[token+8*f+1];
            const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
            const int4 d=dot[f];
            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
        }
        sync_threads();
    }
    #pragma unroll
    for (uint f=0;f<4;++f) bonsai_store_mma(output,result[f],rows,tokens,row_base+row,token_base+token+8*f);
}
static inline void decay_result(uint token, uint head, int gate_row, float dot,
    __global const float *dt, __global const float *a, __global float *gate, __global float *mix) {
    if (gate_row) {
        const float p = dot + dt[head];
        gate[token*48+head] = a[head] * (p > 0.0f ?
            p + __nv_log1pf(__nv_expf(-p)) : __nv_log1pf(__nv_expf(p)));
    } else {
        mix[token*48+head] = 1.0f / (1.0f + __nv_expf(-dot));
    }
}

static inline __attribute__((always_inline)) void staged_decay(
    __global const float *x, __global const float *alpha, __global const float *beta,
    __global const float *dt, __global const float *a, __global float *gate, __global float *mix,
    __local float *products, uint threads, int split) {
    const uint t = local_id();
    const uint head = work_group() % 48;
    const uint token = work_group() / 48;
    const uint channel = split ? flash_grid_y() : 0;
    x += token * 5120;
    alpha += head * 5120;
    beta += head * 5120;
    for (uint i = t; i < 5120; i += threads) {
        const float value = x[i];
        if (split) {
            products[i] = value * (channel == 0 ? alpha[i] : beta[i]);
        } else {
            products[i] = value * alpha[i];
            products[5120+i] = value * beta[i];
        }
    }
    sync_threads();
    if (t >= (split ? 32 : 64)) return;
    const uint lane = t & 31;
    const uint offset = split ? 0 : (t/32) * 5120;
    float sum = 0.0f;
    #pragma unroll 8
    for (uint i = lane; i < 5120; i += 32) sum += products[offset+i];
    sum = warp_sum(sum);
    if (lane == 0) decay_result(token,head,(split ? channel : t/32)==0,sum,dt,a,gate,mix);
}

#define STAGED(T,S,N) \
__kernel void bonsai_decay_stage_##T##_##S(__global const float *x, __global const float *alpha, \
    __global const float *beta, __global const float *dt, __global const float *a, \
    __global float *gate, __global float *mix) { \
    __local float products[N]; \
    staged_decay(x,alpha,beta,dt,a,gate,mix,products,T,S); \
}
STAGED(128,0,10240)
STAGED(256,0,10240)
STAGED(128,1,5120)
STAGED(256,1,5120)
__kernel void bonsai_flash_kv_panel2(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[15488];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112);
    __local float (*score)[FLASH_KEYS] = (__local float (*)[FLASH_KEYS])(panel_scratch + 4224 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS/2] = (__local uint (*)[FLASH_KEYS/2])(panel_scratch + 8320 + panel * 1024);
    __local uint *staging = panel_scratch + 10368;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y() * 2;
    if (tile_base * FLASH_KEYS >= first_visible + pair_last/6) return;
    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint j = 0; j < 2; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + pair_last/6) break;
        const int live = key_start < first_visible + last_row/6;
        __global float *out = partial + ((ulong)group*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
            score[lane_row+8][key_col] = acc.z * 0.0625f;
            score[lane_row+8][key_col+1] = acc.w * 0.0625f;
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
                sum += lo + hi;
                probability[r][z/2] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            if (lane == 0 && live) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*20;
                    const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        #pragma unroll
        for (uint result = 0; result < (live ? 8u : 0u); ++result) {
            const uint column = 32*result + 8*warp + 2*lane_pair + 2;
            out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
            out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
            out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
            out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
        }
    }
}

__kernel void bonsai_flash_kv_lean2(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[12544];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112);
    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])(panel_scratch + 4224 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS] = score;
    __local uint *staging = panel_scratch + 8320;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y() * 2;
    if (tile_base * FLASH_KEYS >= first_visible + pair_last/6) return;
    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint j = 0; j < 2; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + pair_last/6) break;
        const int live = key_start < first_visible + last_row/6;
        __global float *out = partial + ((ulong)group*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = as_uint(acc.x * 0.0625f);
            score[lane_row][key_col+1] = as_uint(acc.y * 0.0625f);
            score[lane_row+8][key_col] = as_uint(acc.z * 0.0625f);
            score[lane_row+8][key_col+1] = as_uint(acc.w * 0.0625f);
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,as_float(score[r][z]));
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            uint probabilities[2];
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(as_float(score[r][z])-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(as_float(score[r][z+1])-mx) : 0.0f;
                sum += lo + hi;
                probabilities[z/64] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            probability[r][lane] = probabilities[0];
            probability[r][lane+32] = probabilities[1];
            if (lane == 0 && live) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/16)*16+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*16;
                    const uint2 b = bonsai_load_b(staging+feature_block,16,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        #pragma unroll
        for (uint result = 0; result < (live ? 8u : 0u); ++result) {
            const uint column = 32*result + 8*warp + 2*lane_pair + 2;
            out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
            out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
            out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
            out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
        }
    }
}

__kernel void bonsai_flash_kv_panel4(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[15488];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112);
    __local float (*score)[FLASH_KEYS] = (__local float (*)[FLASH_KEYS])(panel_scratch + 4224 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS/2] = (__local uint (*)[FLASH_KEYS/2])(panel_scratch + 8320 + panel * 1024);
    __local uint *staging = panel_scratch + 10368;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y() * 4;
    if (tile_base * FLASH_KEYS >= first_visible + pair_last/6) return;
    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint j = 0; j < 4; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + pair_last/6) break;
        const int live = key_start < first_visible + last_row/6;
        __global float *out = partial + ((ulong)group*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = acc.x * 0.0625f;
            score[lane_row][key_col+1] = acc.y * 0.0625f;
            score[lane_row+8][key_col] = acc.z * 0.0625f;
            score[lane_row+8][key_col+1] = acc.w * 0.0625f;
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,score[r][z]);
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(score[r][z]-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(score[r][z+1]-mx) : 0.0f;
                sum += lo + hi;
                probability[r][z/2] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            if (lane == 0 && live) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/16)*20+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS/2,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*20;
                    const uint2 b = bonsai_load_b(staging+feature_block,20,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        #pragma unroll
        for (uint result = 0; result < (live ? 8u : 0u); ++result) {
            const uint column = 32*result + 8*warp + 2*lane_pair + 2;
            out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
            out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
            out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
            out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
        }
    }
}

__kernel void bonsai_flash_kv_lean4(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[12544];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112);
    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])(panel_scratch + 4224 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS] = score;
    __local uint *staging = panel_scratch + 8320;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint last_row = flash_min(first_row+FLASH_ROWS, queries*6)-1;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;
    const uint tile_base = flash_grid_y() * 4;
    if (tile_base * FLASH_KEYS >= first_visible + pair_last/6) return;
    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    sync_threads();

    for (uint j = 0; j < 4; ++j) {
        const uint tile = tile_base + j, key_start = tile * FLASH_KEYS;
        if (tile >= tile_count || key_start >= first_visible + pair_last/6) break;
        const int live = key_start < first_visible + last_row/6;
        __global float *out = partial + ((ulong)group*tile_count + tile) * FLASH_ROWS * FLASH_PARTIAL;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = as_uint(acc.x * 0.0625f);
            score[lane_row][key_col+1] = as_uint(acc.y * 0.0625f);
            score[lane_row+8][key_col] = as_uint(acc.z * 0.0625f);
            score[lane_row+8][key_col+1] = as_uint(acc.w * 0.0625f);
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,as_float(score[r][z]));
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            uint probabilities[2];
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(as_float(score[r][z])-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(as_float(score[r][z+1])-mx) : 0.0f;
                sum += lo + hi;
                probabilities[z/64] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            probability[r][lane] = probabilities[0];
            probability[r][lane+32] = probabilities[1];
            if (lane == 0 && live) {
                out[r*FLASH_PARTIAL] = mx;
                out[r*FLASH_PARTIAL+1] = sum;
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/16)*16+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*16;
                    const uint2 b = bonsai_load_b(staging+feature_block,16,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        #pragma unroll
        for (uint result = 0; result < (live ? 8u : 0u); ++result) {
            const uint column = 32*result + 8*warp + 2*lane_pair + 2;
            out[lane_row*FLASH_PARTIAL+column] = acc[result].x;
            out[lane_row*FLASH_PARTIAL+column+1] = acc[result].y;
            out[(lane_row+8)*FLASH_PARTIAL+column] = acc[result].z;
            out[(lane_row+8)*FLASH_PARTIAL+column+1] = acc[result].w;
        }
    }
}

__kernel void bonsai_pq2_pipe_64x64_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[2][64][36];
    const uint row_base=64*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint blocks=width/128;
    float4 acc[4];
    #pragma unroll
    for (uint i=0;i<4;++i) acc[i]=(float4)(0.0f);
        for (uint packed=thread*4;packed<64*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+0*32+col : 0;
            bonsai_copy16(&x[0][packed/32][col],input+source,valid);
        }
        __asm__ volatile("cp.async.commit_group;" ::: "memory");
    for (uint step=0; step<2*blocks; ++step) {
        const int next = step + 1 < 2*blocks;
        if (next) {
            for (uint packed=thread*4;packed<64*32;packed+=1024) {
                const uint token=token_base+packed/32,col=packed%32;
                const uint valid=token<tokens ? 16 : 0;
                const ulong source=valid ? (ulong)token*(width/2)+(step+1)*32+col : 0;
                bonsai_copy16(&x[((step+1)&1)][packed/32][col],input+source,valid);
            }
            __asm__ volatile("cp.async.commit_group;" ::: "memory");
        }
        for (uint index=thread;index<64*16;index+=256) {
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        if (next) {
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        } else {
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<4;++i) {
                const uint2 b=bonsai_load_b(&x[step&1][warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<4;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

__kernel void bonsai_pq2_pipe_64x128_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[2][128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+0*32+col : 0;
            bonsai_copy16(&x[0][packed/32][col],input+source,valid);
        }
        __asm__ volatile("cp.async.commit_group;" ::: "memory");
    for (uint step=0; step<2*blocks; ++step) {
        const int next = step + 1 < 2*blocks;
        if (next) {
            for (uint packed=thread*4;packed<128*32;packed+=1024) {
                const uint token=token_base+packed/32,col=packed%32;
                const uint valid=token<tokens ? 16 : 0;
                const ulong source=valid ? (ulong)token*(width/2)+(step+1)*32+col : 0;
                bonsai_copy16(&x[((step+1)&1)][packed/32][col],input+source,valid);
            }
            __asm__ volatile("cp.async.commit_group;" ::: "memory");
        }
        for (uint index=thread;index<64*16;index+=256) {
            const uint row=row_base+index/16, quad=index%16;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                pair=bonsai_ffn_unpack(q,quad+16*(step&1));
            }
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        if (next) {
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        } else {
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<8;++i) {
                const uint2 b=bonsai_load_b(&x[step&1][warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

__kernel void bonsai_pq2_pipe_64x128_k32(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][20];
    __local __attribute__((aligned(16))) uint x[2][128][20];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
        for (uint packed=thread*4;packed<128*16;packed+=1024) {
            const uint token=token_base+packed/16,col=packed%16;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+0*16+col : 0;
            bonsai_copy16(&x[0][packed/16][col],input+source,valid);
        }
        __asm__ volatile("cp.async.commit_group;" ::: "memory");
    for (uint step=0; step<4*blocks; ++step) {
        const int next = step + 1 < 4*blocks;
        if (next) {
            for (uint packed=thread*4;packed<128*16;packed+=1024) {
                const uint token=token_base+packed/16,col=packed%16;
                const uint valid=token<tokens ? 16 : 0;
                const ulong source=valid ? (ulong)token*(width/2)+(step+1)*16+col : 0;
                bonsai_copy16(&x[((step+1)&1)][packed/16][col],input+source,valid);
            }
            __asm__ volatile("cp.async.commit_group;" ::: "memory");
        }
        for (uint index=thread;index<64*8;index+=256) {
            const uint row=row_base+index/8, quad=index%8;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/4)*34;
                pair=bonsai_ffn_unpack(q,quad+8*(step&3));
            }
            w[index/8][quad*2]=pair.x;
            w[index/8][quad*2+1]=pair.y;
        }
        if (next) {
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        } else {
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }
        sync_threads();
        #pragma unroll
        for (uint k=0;k<16;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],20,k,lane);
            #pragma unroll
            for (uint i=0;i<8;++i) {
                const uint2 b=bonsai_load_b(&x[step&1][warp_token+8*i][0],20,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

__kernel void bonsai_pq2_pipe_64x64_k32(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][20];
    __local __attribute__((aligned(16))) uint x[2][64][20];
    const uint row_base=64*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint blocks=width/128;
    float4 acc[4];
    #pragma unroll
    for (uint i=0;i<4;++i) acc[i]=(float4)(0.0f);
        for (uint packed=thread*4;packed<64*16;packed+=1024) {
            const uint token=token_base+packed/16,col=packed%16;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+0*16+col : 0;
            bonsai_copy16(&x[0][packed/16][col],input+source,valid);
        }
        __asm__ volatile("cp.async.commit_group;" ::: "memory");
    for (uint step=0; step<4*blocks; ++step) {
        const int next = step + 1 < 4*blocks;
        if (next) {
            for (uint packed=thread*4;packed<64*16;packed+=1024) {
                const uint token=token_base+packed/16,col=packed%16;
                const uint valid=token<tokens ? 16 : 0;
                const ulong source=valid ? (ulong)token*(width/2)+(step+1)*16+col : 0;
                bonsai_copy16(&x[((step+1)&1)][packed/16][col],input+source,valid);
            }
            __asm__ volatile("cp.async.commit_group;" ::: "memory");
        }
        for (uint index=thread;index<64*8;index+=256) {
            const uint row=row_base+index/8, quad=index%8;
            uint2 pair=(uint2)(0);
            if (row<rows) {
                const __global uchar *q=weights+((ulong)row*blocks+step/4)*34;
                pair=bonsai_ffn_unpack(q,quad+8*(step&3));
            }
            w[index/8][quad*2]=pair.x;
            w[index/8][quad*2+1]=pair.y;
        }
        if (next) {
            __asm__ volatile("cp.async.wait_group 1;" ::: "memory");
        } else {
            __asm__ volatile("cp.async.wait_group 0;" ::: "memory");
        }
        sync_threads();
        #pragma unroll
        for (uint k=0;k<16;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],20,k,lane);
            #pragma unroll
            for (uint i=0;i<4;++i) {
                const uint2 b=bonsai_load_b(&x[step&1][warp_token+8*i][0],20,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<4;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

static uint2 bonsai_ffn_unpack_cached(ushort bits,uint codes) {
    float scale;
    __asm__("cvt.f32.f16 %0,%1;" : "=f"(scale) : "h"(bits));
    return (uint2)(bonsai_half_pair(scale*(float)((int)(codes&3)-1),
                                   scale*(float)((int)((codes>>2)&3)-1)),
                  bonsai_half_pair(scale*(float)((int)((codes>>4)&3)-1),
                                   scale*(float)((int)(codes>>6)-1)));
}

__kernel void bonsai_pq2_cached_64x64_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[64][36];
    const uint row_base=64*work_group(), token_base=64*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=32*(warp%2);
    const uint blocks=width/128;
    float4 acc[4];
    #pragma unroll
    for (uint i=0;i<4;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<64*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<4;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<4;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

__kernel void bonsai_pq2_cached_64x128_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint k=0;k<32;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<8;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}


__kernel void bonsai_flash_kv_fused_ordered(__global const float *query, __global const uint *keys,
                                   __global const uint *values, __global float *output,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[12544];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*query_half)[132] = (__local uint (*)[132])(panel_scratch + panel * 2112);
    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])(panel_scratch + 4224 + panel * 2048);
    __local uint (*probability)[FLASH_KEYS] = score;
    __local uint *staging = panel_scratch + 8320;

    const uint group = work_group() * 2 + panel;
    const uint work = group + group_start;
    const uint groups_per_head = (queries*6 + FLASH_ROWS-1)/FLASH_ROWS;
    const uint kv_head = work / groups_per_head;
    const uint first_row = (work % groups_per_head)*FLASH_ROWS;
    const uint pair_last = flash_min(first_row+(2-panel)*FLASH_ROWS, queries*6)-1;

    const uint thread = physical_thread % 128, warp = thread / 32, lane = thread % 32;
    const uint lane_row = lane / 4, lane_pair = lane % 4;
    const float neg_inf = as_float(0xff800000u);


    for (uint index = thread; index < FLASH_ROWS * 128; index += 128) {
        const uint r = index / 128, col = (index % 128)*2;
        const uint original = first_row+r, token = original/6, h = original%6;
        uint packed = 0;
        if (token < queries) {
            const __global float *q = query + ((ulong)token * 24 + kv_head*6 + h)*256 + col;
            packed = bonsai_half_pair(q[0], q[1]);
        }
        query_half[r][index%128] = packed;
    }
    if (thread < FLASH_ROWS) {
        query_half[thread][128] = as_uint(neg_inf);
        query_half[thread][129] = 0;
    }
    sync_threads();

    float4 values_acc[8];
    #pragma unroll
    for (uint r = 0; r < 8; ++r) values_acc[r] = (float4)(0.0f);
    const uint visible_end = flash_min(key_count, first_visible + pair_last/6);
    for (uint tile = 0; tile < tile_count; ++tile) {
        const uint key_start = tile * FLASH_KEYS;
        if (key_start >= visible_end) break;
        for (uint kb = 0; kb < FLASH_KEYS; kb += 32) {
            const uint chunk = (key_start+kb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/128)*132+index%128,keys+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            float4 acc = (float4)(0.0f);
            #pragma unroll 8
            for (uint k = 0; k < 128; k += 8) {
                const uint4 a = bonsai_load_a(&query_half[0][0],132,k,lane);
                const uint2 b = bonsai_load_b(staging+warp*8*132,132,k,lane);
                acc = bonsai_mma16x8(acc,a.x,a.y,a.z,a.w,b.x,b.y);
            }
            const uint key_col = kb + warp*8 + 2*lane_pair;
            score[lane_row][key_col] = as_uint(acc.x * 0.0625f);
            score[lane_row][key_col+1] = as_uint(acc.y * 0.0625f);
            score[lane_row+8][key_col] = as_uint(acc.z * 0.0625f);
            score[lane_row+8][key_col+1] = as_uint(acc.w * 0.0625f);
            sync_threads();
        }

        for (uint r = warp; r < FLASH_ROWS; r += 4) {
            const uint token = (first_row+r)/6;
            const uint visible = flash_min(key_count, first_visible + token);
            const uint count = token < queries && visible > key_start ? flash_min(FLASH_KEYS, visible-key_start) : 0;
            float mx = neg_inf;
            for (uint z = lane; z < count; z += 32) mx = flash_max(mx,as_float(score[r][z]));
            for (uint bit = 16; bit; bit /= 2) mx = flash_max(mx,flash_shuffle(mx,bit));
            float sum = 0.0f;
            uint probabilities[2];
            for (uint z = lane*2; z < FLASH_KEYS; z += 64) {
                const float lo = z < count ? flash_exp(as_float(score[r][z])-mx) : 0.0f;
                const float hi = z+1 < count ? flash_exp(as_float(score[r][z+1])-mx) : 0.0f;
                sum += lo + hi;
                probabilities[z/64] = bonsai_half_pair(lo,hi);
            }
            for (uint bit = 16; bit; bit /= 2) sum += flash_shuffle(sum,bit);
            __asm__ volatile("bar.warp.sync 0xffffffff;" ::: "memory");
            probability[r][lane] = probabilities[0];
            probability[r][lane+32] = probabilities[1];
            if (lane == 0) {
                if (count != 0) {
                    const float prior = as_float(query_half[r][128]), seen = flash_max(prior,mx);
                    const float keep = flash_exp(prior-seen);
                    const float add = flash_exp(mx-seen);
                    query_half[r][128] = as_uint(seen);
                    query_half[r][129] = as_uint(as_float(query_half[r][129])*keep + sum*add);
                    query_half[r][130] = as_uint(keep);
                    query_half[r][131] = as_uint(add);
                }
            }
        }
        sync_threads();

        float4 acc[8];
        #pragma unroll
        for (uint result = 0; result < 8; ++result) acc[result] = (float4)(0.0f);
        for (uint vb = 0; vb < FLASH_KEYS; vb += 32) {
            const uint chunk = (key_start+vb)/32, available = chunk < chunks ? 16 : 0;
            const ulong base = available ? ((ulong)kv_head*chunks+chunk)*4096 : 0;
            for (uint index = physical_thread*4; index < 32*128; index += 1024) {
                bonsai_copy16(staging+(index/16)*16+index%16,values+base+index,available);
            }
            bonsai_copy_complete();
            sync_threads();
            #pragma unroll
            for (uint ksub = 0; ksub < 16; ksub += 8) {
                const uint4 a = bonsai_load_a(&probability[0][0],FLASH_KEYS,vb/2+ksub,lane);
                #pragma unroll
                for (uint result = 0; result < 8; ++result) {
                    const uint feature_block = (32*result + 8*warp)*16;
                    const uint2 b = bonsai_load_b(staging+feature_block,16,ksub,lane);
                    acc[result] = bonsai_mma16x8(acc[result],a.x,a.y,a.z,a.w,b.x,b.y);
                }
            }
            sync_threads();
        }
        const uint token0 = (first_row+lane_row)/6, token1 = (first_row+lane_row+8)/6;
        const int valid0 = token0 < queries && first_visible+token0 > key_start;
        const int valid1 = token1 < queries && first_visible+token1 > key_start;
        #pragma unroll
        for (uint result = 0; result < 8; ++result) {
            if (valid0) {
                values_acc[result].xy = values_acc[result].xy*as_float(query_half[lane_row][130]) + acc[result].xy*as_float(query_half[lane_row][131]);
            }
            if (valid1) {
                values_acc[result].zw = values_acc[result].zw*as_float(query_half[lane_row+8][130]) + acc[result].zw*as_float(query_half[lane_row+8][131]);
            }
        }
        sync_threads();
    }

    if (thread < FLASH_ROWS) {
        float inv;
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inv) : "f"(as_float(query_half[thread][129])));
        query_half[thread][129] = as_uint(inv);
    }
    sync_threads();
    const uint row0 = first_row+lane_row, row1 = row0+8;
    const ulong offset0 = ((ulong)(row0/6)*24 + kv_head*6 + row0%6)*256;
    const ulong offset1 = ((ulong)(row1/6)*24 + kv_head*6 + row1%6)*256;
    #pragma unroll
    for (uint result = 0; result < 8; ++result) {
        const uint col = 32*result + 8*warp + 2*lane_pair;
        if (row0/6 < queries)
            *(__global float2 *)(output+offset0+col) = values_acc[result].xy*as_float(query_half[lane_row][129]);
        if (row1/6 < queries)
            *(__global float2 *)(output+offset1+col) = values_acc[result].zw*as_float(query_half[lane_row+8][129]);
    }
}


static uint2 bonsai_mma_half_acc(uint2 acc, uint4 a, uint2 b) {
    uint lo=acc.x,hi=acc.y;
    __asm__("mma.sync.aligned.m16n8k16.row.col.f16.f16.f16.f16 "
            "{%0,%1}, {%2,%3,%4,%5}, {%6,%7}, {%0,%1};"
            : "+r"(lo),"+r"(hi)
            : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (uint2)(lo,hi);
}

static float bonsai_unpack_half(ushort x) {
    float out;
    __asm__("cvt.f32.f16 %0,%1;" : "=f"(out) : "h"(x));
    return out;
}

static float4 bonsai_widen_partial(uint2 acc) {
    return (float4)(bonsai_unpack_half((ushort)acc.x),bonsai_unpack_half((ushort)(acc.x>>16)),
                    bonsai_unpack_half((ushort)acc.y),bonsai_unpack_half((ushort)(acc.y>>16)));
}
__kernel void bonsai_pq2_cached_fp16_acc_k32(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}

__kernel void bonsai_pq2_cached_fp16_acc_k64(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=width/128;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                uint bits=0,low=0,high=0;
                if (row<rows) {
                    const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                    bits=*(__global const ushort *)q;
                    low=q[2+quad];
                    high=q[18+quad];
                }
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*32+col : 0;
            bonsai_copy16(&x[packed/32][col],input+source,valid);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=32) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<4;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
}
__kernel void bonsai_pq2_half_full_5120(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=40;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*2560+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_full_17408(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*8704+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_full_6144(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=48;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*3072+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_ffn_unpack_cached((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

static inline uint bonsai_pq2_permute(uint a, uint b, uint selector) {
    uint result;
    __asm__("prmt.b32 %0, %1, %2, %3;" : "=r"(result) : "r"(a), "r"(b), "r"(selector));
    return result;
}

static inline uint2 bonsai_pq2_half2(ushort bits, uint code) {
    const uint selector = (code & 3u) | ((code & 12u) << 2) |
        ((code & 48u) << 4) | ((code & 192u) << 6);
    const uint factors = bonsai_pq2_permute(0x403c00bcu, 0, selector);
    const uint a = bonsai_pq2_permute(factors, 0, 0x1404);
    const uint b = bonsai_pq2_permute(factors, 0, 0x3424);
    const uint scale = (uint)bits | ((uint)bits << 16);
    uint first, second;
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(first) : "r"(a), "r"(scale));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(second) : "r"(b), "r"(scale));
    return (uint2)(first, second);
}

__kernel void bonsai_pq2_half_unpack_lut_5120(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=40;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*2560+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_quad((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_half2_5120(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=40;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*2560+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_half2((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_lut_6144(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=48;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*3072+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_quad((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_half2_6144(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=48;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*3072+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_half2((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_lut_17408(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*8704+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_quad((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}

__kernel void bonsai_pq2_half_unpack_half2_17408(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    uint raw[4];
    for (uint step=0;step<2*blocks;++step) {
        if ((step&1)==0) {
            #pragma unroll
            for (uint lane_row=0;lane_row<4;++lane_row) {
                const uint index=thread+256*lane_row;
                const uint row=row_base+index/16, quad=index%16;
                const __global uchar *q=weights+((ulong)row*blocks+step/2)*34;
                const uint bits=*(__global const ushort *)q;
                const uint low=q[2+quad],high=q[18+quad];
                raw[lane_row]=bits|(low<<16)|(high<<24);
            }
        }
        for (uint packed=thread*4;packed<128*32;packed+=1024) {
            const uint token=token_base+packed/32,col=packed%32;
            const ulong source=(ulong)token*8704+step*32+col;
            bonsai_copy16(&x[packed/32][col],input+source,16);
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;
            const uint entry=raw[lane_row];
            const uint codes=(entry >> (16+8*(step&1))) & 255;
            const uint2 pair=bonsai_pq2_half2((ushort)entry,codes);
            w[index/16][quad*2]=pair.x;
            w[index/16][quad*2+1]=pair.y;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint segment=0;segment<32;segment+=16) {
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<2;++sub) {
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }
            }
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}
__kernel void bonsai_pq2_half_unpack_half2_17408_k128(__global const uchar *weights,
    __global const uint *input,__global float *output,uint width,uint rows,uint tokens) {
    __local __attribute__((aligned(16))) uint paired[2][192][36];
    const uint row_base=64*work_group(), token_base=128*flash_grid_y();
    const uint thread=local_id(),warp=thread/32,lane=thread%32;
    const uint warp_row=16*(warp/2),warp_token=64*(warp%2);
    const uint blocks=136;
    float4 acc[8];
    #pragma unroll
    for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);
    for (uint block=0;block<blocks;++block) {
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {
            for (uint packed=thread*4;packed<128*32;packed+=1024) {
                const uint token=token_base+packed/32,col=packed%32;
                const ulong source=(ulong)token*8704+(2*block+half_index)*32+col;
                bonsai_copy16(&paired[half_index][64+packed/32][col],input+source,16);
            }
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row;
            const uint row=row_base+index/16, quad=index%16;
            const __global uchar *q=weights+((ulong)row*blocks+block)*34;
            const ushort bits=*(__global const ushort *)q;
            const uint2 first=bonsai_pq2_half2(bits,q[2+quad]);
            const uint2 second=bonsai_pq2_half2(bits,q[18+quad]);
            *(__local uint2 *)&paired[0][index/16][quad*2]=first;
            *(__local uint2 *)&paired[1][index/16][quad*2]=second;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {
            #pragma unroll
            for (uint segment=0;segment<32;segment+=16) {
                uint2 partial[8];
                #pragma unroll
                for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
                #pragma unroll
                for (uint sub=0;sub<2;++sub) {
                    const uint k=segment+8*sub;
                    const uint4 a=bonsai_load_a(&paired[half_index][warp_row][0],36,k,lane);
                    #pragma unroll
                    for (uint i=0;i<8;++i) {
                        const uint2 b=bonsai_load_b(&paired[half_index][64+warp_token+8*i][0],36,k,lane);
                        partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                    }
                }
                #pragma unroll
                for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
            }
        }
        sync_threads();
    }
    const uint row=row_base+warp_row+lane/4,token=token_base+warp_token+2*(lane%4);
    #pragma unroll
    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }
}
