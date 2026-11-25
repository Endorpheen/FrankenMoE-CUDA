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
