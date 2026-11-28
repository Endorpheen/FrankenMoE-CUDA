#include "bonsai-common.clh"

__kernel void bonsai_mtp_eh_q4(__global const uchar *weights,
    __global const float *input, __global float *output) {
    const uint thread = local_id(), lane = thread & 31;
    const uint row = work_group() * 4 + thread / 32;
    __global const uchar *w = weights + (size_t)row * 320 * 18;
    float sum = 0.0f;
    for (uint block = 0; block < 320; ++block) {
        const float weight = q4_value(w + block * 18, lane);
        const float value = input[block * 32 + lane];
        __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum) : "f"(weight), "f"(value));
    }
    sum = warp_sum(sum);
    if (lane == 0) output[row] = sum;
}
__kernel void bonsai_mtp_mat_q4(__global const uchar *weights,
    __global const float *input, __global float *output, uint blocks) {
    const uint thread = local_id(), lane = thread & 31;
    const uint row = work_group() * 4 + thread / 32;
    __global const uchar *w = weights + (size_t)row * blocks * 18;
    float sum = 0.0f;
    for (uint block = 0; block < blocks; ++block) {
        const float weight = q4_value(w + block * 18, lane);
        const float value = input[block * 32 + lane];
        __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum) : "f"(weight), "f"(value));
    }
    sum = warp_sum(sum);
    if (lane == 0) output[row] = sum;
}
static inline __attribute__((always_inline)) void mvq_three(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[2][3] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[3];
        float dx[3];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[3] = {0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 3; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 3; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}

#define MVQ_VERIFY(N) \
__kernel void bonsai_pq2_mvq_q8_three_##N(__global const uchar *weights, __global const uchar *input, \
    __global float *output, uint width, uint rows) { \
    mvq_three(weights, input, output, width, rows, N); \
}
MVQ_VERIFY(1)
MVQ_VERIFY(2)
__kernel void bonsai_conv_verify_prefix(__global const float *input, __global const float *weight,
    __global float *final, __global float *out, __global float *first, __global float *second) {
    const uint col = work_group() * 256 + local_id();
    const uint width = 10240;
    float a = final[col], b = final[width + col], c = final[2 * width + col];
    const __global float *w = weight + 4 * col;
    out[col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * input[col]);
    first[col] = b;
    first[width + col] = c;
    first[2 * width + col] = input[col];

    a = b; b = c; c = input[col];
    out[width + col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * input[width + col]);
    second[col] = b;
    second[width + col] = c;
    second[2 * width + col] = input[width + col];

    a = b; b = c; c = input[width + col];
    out[2 * width + col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * input[2 * width + col]);
    final[col] = b;
    final[width + col] = c;
    final[2 * width + col] = input[2 * width + col];
}
__kernel void bonsai_mtp_concat(__global const float *embeddings,
    __global const float *hidden, __global float *joined) {
    const uint flat = work_group() * 256 + local_id();
    const uint row = flat / 5120, column = flat % 5120;
    joined[(size_t)row * 10240 + column] = embeddings[flat];
    joined[(size_t)row * 10240 + 5120 + column] = hidden[flat];
}

__kernel void bonsai_mtp_q4_four(__global const uchar *weights,
    __global const float *input, __global float *output, uint blocks,
    uint rows, uint tokens) {
    const uint thread = local_id(), lane = thread & 31;
    const uint row = work_group() * 4 + thread / 32;
    const uint first = bonsai_grid_y() * 4;
    const size_t width = (size_t)blocks * 32;
    const __global uchar *w = weights + (size_t)row * blocks * 18;
    float sum[4] = {0, 0, 0, 0};
    for (uint block = 0; block < blocks; ++block) {
        const float weight = q4_value(w + block * 18, lane);
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            if (first + t < tokens) {
                const float value = input[(first + t) * width + block * 32 + lane];
                __asm__("fma.rn.f32 %0, %1, %2, %0;" : "+f"(sum[t]) : "f"(weight), "f"(value));
            }
        }
    }
    #pragma unroll
    for (uint t = 0; t < 4; ++t) {
        const float value = warp_sum(sum[t]);
        if (lane == 0 && first + t < tokens) output[(size_t)(first + t) * rows + row] = value;
    }
}
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
typedef struct {
    uint token;
    float logit;
    uint invalid;
} bonsai_choice;

static inline bonsai_choice choice_empty(void) {
    return (bonsai_choice){0xffffffffu, as_float(0xff800000u), 0};
}

static inline bonsai_choice choice_best(bonsai_choice a, bonsai_choice b) {
    const uint invalid = a.invalid | b.invalid;
    if (b.logit > a.logit || (b.logit == a.logit && b.token < a.token)) a = b;
    a.invalid = invalid;
    return a;
}

static inline uint choice_peer(uint value, uint distance) {
    uint peer;
    __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=r"(peer) : "r"(value), "r"(distance));
    return peer;
}

static inline bonsai_choice choice_warp(bonsai_choice value) {
    for (uint distance = 16; distance; distance >>= 1) {
        const bonsai_choice peer = {
            choice_peer(value.token, distance),
            as_float(choice_peer(as_uint(value.logit), distance)),
            choice_peer(value.invalid, distance),
        };
        value = choice_best(value, peer);
    }
    return value;
}

static inline bonsai_choice choice_block(bonsai_choice value, __local bonsai_choice *scratch) {
    const uint thread = local_id();
    value = choice_warp(value);
    if ((thread & 31) == 0) scratch[thread / 32] = value;
    sync_threads();
    if (thread < 32) value = choice_warp(thread < 8 ? scratch[thread] : choice_empty());
    return value;
}

__kernel void bonsai_choice_tiles(__global const float *logits,
    __global const uint *mask, __global bonsai_choice *tiles, uint width, uint jobs) {
    const uint block = work_group(), row = bonsai_grid_y();
    __local bonsai_choice scratch[8];
    bonsai_choice maximum = choice_empty();
    for (uint i = 0; i < 4; ++i) {
        const uint token = block * 1024 + i * 256 + local_id();
        if (token >= width) continue;
        const float x = logits[(size_t)row * width + token];
        if ((as_uint(x) & 0x7f800000u) == 0x7f800000u) {
            maximum.invalid = 1;
        } else if (!mask || ((mask[token / 32] >> (token % 32)) & 1u) == 0) {
            maximum = choice_best(maximum, (bonsai_choice){token, x, 0});
        }
    }
    maximum = choice_block(maximum, scratch);
    if (local_id() == 0) tiles[(size_t)row * jobs + block] = maximum;
}

__kernel void bonsai_choice_final(__global const bonsai_choice *tiles,
    __global bonsai_choice *result, uint jobs) {
    const uint row = work_group(), thread = local_id();
    __local bonsai_choice scratch[8];
    const bonsai_choice value = thread < jobs ? tiles[(size_t)row * jobs + thread] : choice_empty();
    const bonsai_choice best = choice_block(value, scratch);
    if (thread == 0) result[row] = best;
}
static inline void verify_merge_store(__global float *dest, float2 value, float mass) {
    if (mass > 0) {
        float a, b;
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(a) : "f"(value.x), "f"(mass));
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(b) : "f"(value.y), "f"(mass));
        value = (float2)(a, b);
    } else {
        value = (float2)(0.0f);
    }
    *(__global float2 *)dest = value;
}

static inline __attribute__((always_inline)) void verify_merge_impl(
    __global const float *scratch, __global float *out, uint first_keys, uint max_tiles,
    uint parallel, __local float *work) {
    const uint job = work_group(), thread = local_id(), offset = thread * 2;
    const uint token = job / 24, head = job % 24;
    const uint tiles = (first_keys + token + 255) / 256;
    const __global float *part = scratch + ((size_t)token * 24 * max_tiles + head * tiles) * 258;
    __global float *dest = out + (size_t)job * 256 + offset;
    float2 values = (float2)(0.0f);
    float maximum = as_float(0xff800000u), mass = 0.0f;

    if (!parallel) {
        for (uint t = 0; t < tiles; ++t) {
            const __global float *tile = part + t * 258;
            const float m = tile[1];
            if (m == 0.0f) continue;
            const float mx = tile[0];
            const float next = mx > maximum ? mx : maximum;
            const float keep = attention_exp(maximum - next), add = attention_exp(mx - next);
            maximum = next;
            mass = mass * keep + m * add;
            values = values * keep + *(__global const float2 *)(tile + 2 + offset) * add;
        }
        verify_merge_store(dest, values, mass);
        return;
    }

    __local float *prev = work, *next_weight = work + 32, *tile_mass = work + 64;
    for (uint base = 0; base < tiles; base += 32) {
        const uint active = tiles - base < 32 ? tiles - base : 32;
        if (thread < 32) {
            const __global float *tile = part + (base + thread) * 258;
            const float m = thread < active ? tile[1] : 0.0f;
            const float mx = m == 0.0f ? as_float(0xff800000u) : tile[0];
            float seen = mx > maximum ? mx : maximum;
            #pragma unroll
            for (uint bit = 1; bit < 32; bit *= 2) {
                const float peer = bonsai_shuffle_up(seen, bit);
                if (thread >= bit) seen = seen > peer ? seen : peer;
            }
            const float before = bonsai_shuffle_up(seen, 1);
            tile_mass[thread] = m;
            if (m != 0.0f) {
                prev[thread] = attention_exp((thread == 0 ? maximum : before) - seen);
                next_weight[thread] = attention_exp(mx - seen);
            }
            maximum = bonsai_last_lane(seen);
        }
        sync_threads();
        for (uint i = 0; i < active; ++i) {
            const float m = tile_mass[i];
            if (m == 0.0f) continue;
            const float keep = prev[i], add = next_weight[i];
            const __global float *tile = part + (base + i) * 258;
            if (thread == 0) mass = mass * keep + m * add;
            values = values * keep + *(__global const float2 *)(tile + 2 + offset) * add;
        }
        sync_threads();
    }
    if (thread == 0) work[96] = mass;
    sync_threads();
    verify_merge_store(dest, values, work[96]);
}

__kernel void bonsai_verify_merge_direct(__global const float *scratch, __global float *out,
    uint first_keys, uint max_tiles) {
    verify_merge_impl(scratch, out, first_keys, max_tiles, 0, (__local float *)0);
}

__kernel void bonsai_verify_merge_parallel(__global const float *scratch, __global float *out,
    uint first_keys, uint max_tiles) {
    __local float work[97];
    verify_merge_impl(scratch, out, first_keys, max_tiles, 1, work);
}
__kernel void bonsai_flash_q_packed_panel(__global const uint *query, __global const uint *keys,
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


    for (uint index = thread * 4; index < FLASH_ROWS * 128; index += 512) {
        const uint r = index / 128, col = index % 128;
        const uint original = first_row + r, token = original / 6, h = original % 6;
        const uint valid = token < queries ? 16 : 0;
        const size_t source = valid ? ((size_t)token * 24 + kv_head * 6 + h) * 128 + col : 0;
        bonsai_copy16(&query_half[r][col], query + source, valid);
    }
    bonsai_copy_complete();
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

static inline uint bonsai_packed_query(__global const uint *query, uint row,
                                       uint queries, uint pair) {
    const uint token = row / 6, head = row % 6;
    return token < queries ? query[((size_t)token * 24 + head) * 128 + pair] : 0;
}
__kernel void bonsai_flash_q_direct_panel(__global const uint *query, __global const uint *keys,
                                   __global const uint *values, __global float *partial,
                                   uint queries, uint key_count, uint first_visible,
                                   uint tile_count, uint group_start, uint chunks) {
    __local __attribute__((aligned(16))) uint panel_scratch[8320];
    const uint physical_thread = local_id();
    const uint panel = physical_thread / 128;
    __local uint (*score)[FLASH_KEYS] = (__local uint (*)[FLASH_KEYS])(panel_scratch + panel * 2048);
    __local uint (*probability)[FLASH_KEYS] = score;
    __local uint *staging = panel_scratch + 4096;

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
    const __global uint *qbase = query + kv_head * 6 * 128;
    const float neg_inf = as_float(0xff800000u);



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
                const uint r0 = first_row + lane_row, r1 = r0 + 8;
                const uint p = k + lane_pair;
                const uint4 a = (uint4)(
                    bonsai_packed_query(qbase, r0, queries, p),
                    bonsai_packed_query(qbase, r1, queries, p),
                    bonsai_packed_query(qbase, r0, queries, p + 4),
                    bonsai_packed_query(qbase, r1, queries, p + 4));
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

__kernel void bonsai_flash_q_linear_panel(__global const uint *query, __global const uint *keys,
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


    const __global uint *query_panel = query + (size_t)work * FLASH_ROWS * 128;
    for (uint index = thread * 4; index < FLASH_ROWS * 128; index += 512) {
        bonsai_copy16(&query_half[index / 128][index % 128], query_panel + index, 16);
    }
    bonsai_copy_complete();
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
__kernel void bonsai_pack_query_panels(__global const float *input, __global uint *output,
    uint queries, uint groups_per_head) {
    const uint group = work_group(), kv_head = group / groups_per_head;
    const uint first_row = (group % groups_per_head) * 16;
    for (uint index = local_id(); index < 2048; index += 256) {
        const uint row = first_row + index / 128, token = row / 6;
        uint packed = 0;
        if (token < queries) {
            const size_t offset = ((size_t)token * 24 + kv_head * 6 + row % 6) * 256 + 2 * (index % 128);
            packed = bonsai_half_pair(input[offset], input[offset + 1]);
        }
        output[(size_t)group * 2048 + index] = packed;
    }
}
static inline float bonsai_softmax_broadcast(float value, uint lane) {
    float result;
    __asm__("shfl.sync.idx.b32 %0, %1, %2, 31, -1;" : "=f"(result) : "f"(value), "r"(lane));
    return result;
}

static inline __attribute__((always_inline)) void verify_grouped_lane(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 2 ? first + 256 : first_keys + 2;
    float query[3][2][8], sum[3][2][8];
    float maximum = as_float(0xff800000u), mass = 0.0f;
    #pragma unroll
    for (uint t = 0; t < 3; ++t) {
        #pragma unroll
        for (uint h = 0; h < heads; ++h) {
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
        float assigned = 0.0f;
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            if (row >= first_keys + t) continue;
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const float score = bonsai_decode_warp_sum(dot[t][h]) * scale + 0.0f;
                if (lane == t * heads + h) assigned = score;
            }
        }
        float keep = 0.0f, add = 0.0f;
        if (lane < 3 * heads && row < first_keys + lane / heads) {
            const float next = assigned > maximum ? assigned : maximum;
            keep = attention_exp(maximum - next);
            add = attention_exp(assigned - next);
            mass = mass * keep + add;
            maximum = next;
        }
        float previous[3][2], weight[3][2];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const uint owner = t * heads + h;
                previous[t][h] = bonsai_softmax_broadcast(keep, owner);
                weight[t][h] = bonsai_softmax_broadcast(add, owner);
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
            if (lane == t * heads + h) {
                out[0] = maximum;
                out[1] = mass;
            }
            #pragma unroll
            for (uint b = 0; b < 8; ++b) out[2 + b * 32 + lane] = sum[t][h][b];
        }
    }
}


__kernel void bonsai_verify_q4_lane(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    verify_grouped_lane(q, k, v, scratch, keys, max_tiles, scale, 2);
}

static inline __attribute__((always_inline)) void verify_grouped_tail(
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
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
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
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


__kernel void bonsai_verify_q4_tail(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    verify_grouped_tail(q, k, v, scratch, keys, max_tiles, scale, 2);
}

static inline __attribute__((always_inline)) void verify_grouped_lane_tail(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 2 ? first + 256 : first_keys + 2;
    float query[3][2][8], sum[3][2][8];
    float maximum = as_float(0xff800000u), mass = 0.0f;
    #pragma unroll
    for (uint t = 0; t < 3; ++t) {
        #pragma unroll
        for (uint h = 0; h < heads; ++h) {
            #pragma unroll
            for (uint b = 0; b < 8; ++b) {
                query[t][h][b] = q[t * 6144 + (first_head + h) * 256 + b * 32 + lane];
                sum[t][h][b] = 0.0f;
            }
        }
    }

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
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
        float assigned = 0.0f;
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const float score = bonsai_decode_warp_sum(dot[t][h]) * scale + 0.0f;
                if (lane == t * heads + h) assigned = score;
            }
        }
        float keep = 0.0f, add = 0.0f;
        if (lane < 3 * heads) {
            const float next = assigned > maximum ? assigned : maximum;
            keep = attention_exp(maximum - next);
            add = attention_exp(assigned - next);
            mass = mass * keep + add;
            maximum = next;
        }
        float previous[3][2], weight[3][2];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const uint owner = t * heads + h;
                previous[t][h] = bonsai_softmax_broadcast(keep, owner);
                weight[t][h] = bonsai_softmax_broadcast(add, owner);
            }
        }
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float value = q4_value(vr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 3; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
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
        float assigned = 0.0f;
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            if (row >= first_keys + t) continue;
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const float score = bonsai_decode_warp_sum(dot[t][h]) * scale + 0.0f;
                if (lane == t * heads + h) assigned = score;
            }
        }
        float keep = 0.0f, add = 0.0f;
        if (lane < 3 * heads && row < first_keys + lane / heads) {
            const float next = assigned > maximum ? assigned : maximum;
            keep = attention_exp(maximum - next);
            add = attention_exp(assigned - next);
            mass = mass * keep + add;
            maximum = next;
        }
        float previous[3][2], weight[3][2];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const uint owner = t * heads + h;
                previous[t][h] = bonsai_softmax_broadcast(keep, owner);
                weight[t][h] = bonsai_softmax_broadcast(add, owner);
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
            if (lane == t * heads + h) {
                out[0] = maximum;
                out[1] = mass;
            }
            #pragma unroll
            for (uint b = 0; b < 8; ++b) out[2 + b * 32 + lane] = sum[t][h][b];
        }
    }
}


__kernel void bonsai_verify_q4_lane_tail(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    verify_grouped_lane_tail(q, k, v, scratch, keys, max_tiles, scale, 2);
}
static inline __attribute__((always_inline)) void mvq_four(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[2][4] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[4];
        float dx[4];
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[4] = {0, 0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 4; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 4; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}

#define MVQ_VERIFY_FOUR(N) \
__kernel void bonsai_pq2_mvq_q8_four_##N(__global const uchar *weights, __global const uchar *input, \
    __global float *output, uint width, uint rows) { \
    mvq_four(weights, input, output, width, rows, N); \
}
MVQ_VERIFY_FOUR(1)
MVQ_VERIFY_FOUR(2)

static inline __attribute__((always_inline)) void verify_grouped_tiles_four(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 3 ? first + 256 : first_keys + 3;
    float query[4][2][8], sum[4][2][8], maximum[4][2], mass[4][2];
    #pragma unroll
    for (uint t = 0; t < 4; ++t) {
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
        float dot[4][2] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 4; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[4][2], weight[4][2];
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
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
            for (uint t = 0; t < 4; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 4; ++t) {
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

#define VERIFY_ATTN_GEN_FOUR(H) \
__kernel void bonsai_verify_q4_four_gqa##H(__global const float *q, __global const uchar *k, \
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) { \
    verify_grouped_tiles_four(q, k, v, scratch, keys, max_tiles, scale, H); \
}
VERIFY_ATTN_GEN_FOUR(1)
VERIFY_ATTN_GEN_FOUR(2)

__kernel void bonsai_conv_verify_prefix_four(__global const float *input, __global const float *weight,
    __global float *final, __global float *out, __global float *first,
    __global float *second, __global float *third) {
    const uint col = work_group() * 256 + local_id(), width = 10240;
    float a = final[col], b = final[width + col], c = final[2 * width + col];
    const __global float *w = weight + 4 * col;
    __global float *history[] = {first, second, third, final};
    #pragma unroll
    for (uint t = 0; t < 4; ++t) {
        const float d = input[t * width + col];
        out[t * width + col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * d);
        history[t][col] = b;
        history[t][width + col] = c;
        history[t][2 * width + col] = d;
        a = b;
        b = c;
        c = d;
    }
}
static inline __attribute__((always_inline)) void bonsai_verify_four_tail2_impl(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 3 ? first + 256 : first_keys + 3;
    float query[4][2][8], sum[4][2][8], maximum[4][2], mass[4][2];
    #pragma unroll
    for (uint t = 0; t < 4; ++t) {
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[4][2] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 4; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[4][2], weight[4][2];
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
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
            for (uint t = 0; t < 4; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[4][2] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 4; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[4][2], weight[4][2];
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
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
            for (uint t = 0; t < 4; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 4; ++t) {
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


__kernel void bonsai_verify_four_tail2(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    bonsai_verify_four_tail2_impl(q, k, v, scratch, keys, max_tiles, scale, 2);
}

static inline __attribute__((always_inline)) void bonsai_verify_pair_tail2_impl(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    uint pair;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(pair));
    const uint first_query = pair * 2;
    q += first_query * 6144;
    scratch += (size_t)first_query * 24 * max_tiles * 258;
    first_keys += first_query;
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 1 ? first + 256 : first_keys + 1;
    float query[2][2][8], sum[2][2][8], maximum[2][2], mass[2][2];
    #pragma unroll
    for (uint t = 0; t < 2; ++t) {
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[2][2] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[2][2], weight[2][2];
        #pragma unroll
        for (uint t = 0; t < 2; ++t) {
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
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[2][2] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[2][2], weight[2][2];
        #pragma unroll
        for (uint t = 0; t < 2; ++t) {
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
            for (uint t = 0; t < 2; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 2; ++t) {
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


__kernel void bonsai_verify_pair_tail2(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    bonsai_verify_pair_tail2_impl(q, k, v, scratch, keys, max_tiles, scale, 2);
}

static inline __attribute__((always_inline)) void bonsai_verify_pair_tail3_impl(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    uint pair;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(pair));
    const uint first_query = pair * 2;
    q += first_query * 6144;
    scratch += (size_t)first_query * 24 * max_tiles * 258;
    first_keys += first_query;
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 1 ? first + 256 : first_keys + 1;
    float query[2][3][8], sum[2][3][8], maximum[2][3], mass[2][3];
    #pragma unroll
    for (uint t = 0; t < 2; ++t) {
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[2][3] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[2][3], weight[2][3];
        #pragma unroll
        for (uint t = 0; t < 2; ++t) {
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
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[2][3] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[2][3], weight[2][3];
        #pragma unroll
        for (uint t = 0; t < 2; ++t) {
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
            for (uint t = 0; t < 2; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 2; ++t) {
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


__kernel void bonsai_verify_pair_tail3(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    bonsai_verify_pair_tail3_impl(q, k, v, scratch, keys, max_tiles, scale, 3);
}
__kernel void bonsai_ffn_prefix_fp16(__global const float *hidden, __global const float *branch,
    __global const float *weight, __global const int *signs, __global float *residual,
    __global uint *packed, float epsilon) {
    const uint thread = local_id();
    const size_t row = (size_t)work_group() * 5120;
    __local float cache[5120], divisor;

    for (uint col = thread; col < 5120; col += 256) {
        const float value = hidden[row + col] + branch[row + col];
        cache[col] = value;
        residual[row + col] = value;
    }
    sync_threads();

    if (thread < 32) {
        float sum = 0.0f;
        #pragma unroll 4
        for (uint col = thread; col < 5120; col += 32) {
            const float value = cache[col];
            sum += value * value;
        }
        sum = warp_sum(sum);
        if (thread == 0) divisor = __nv_sqrtf(sum / 5120.0f + epsilon);
    }
    sync_threads();

    const float d = divisor;
    for (uint col = thread; col < 5120; col += 256) {
        cache[col] = ((cache[col] / d) * weight[col]) * (float)signs[col];
    }
    sync_threads();

    for (uint span = 1; span < 1024; span *= 2) {
        for (uint i = thread; i < 2560; i += 256) {
            const uint lo = (i / span) * (2 * span) + i % span;
            const float a = cache[lo], b = cache[lo + span];
            cache[lo] = a + b;
            cache[lo + span] = a - b;
        }
        sync_threads();
    }

    for (uint i = thread; i < 2560; i += 256) {
        packed[row / 2 + i] = bonsai_half_pair(cache[2*i] * 0.03125f, cache[2*i + 1] * 0.03125f);
    }
}

__kernel void bonsai_ffn_prefix_q8(__global const float *hidden, __global const float *branch,
    __global const float *weight, __global const int *signs, __global float *residual,
    __global uint *packed, __global float *scales, float epsilon) {
    const uint thread = local_id();
    const size_t row = (size_t)work_group() * 5120;
    __local float cache[5120], divisor;

    for (uint col = thread; col < 5120; col += 256) {
        const float value = hidden[row + col] + branch[row + col];
        cache[col] = value;
        residual[row + col] = value;
    }
    sync_threads();

    if (thread < 32) {
        float sum = 0.0f;
        #pragma unroll 4
        for (uint col = thread; col < 5120; col += 32) {
            const float value = cache[col];
            sum += value * value;
        }
        sum = warp_sum(sum);
        if (thread == 0) divisor = __nv_sqrtf(sum / 5120.0f + epsilon);
    }
    sync_threads();

    const float d = divisor;
    for (uint col = thread; col < 5120; col += 256) {
        cache[col] = ((cache[col] / d) * weight[col]) * (float)signs[col];
    }
    sync_threads();

    for (uint span = 1; span < 1024; span *= 2) {
        for (uint i = thread; i < 2560; i += 256) {
            const uint lo = (i / span) * (2 * span) + i % span;
            const float a = cache[lo], b = cache[lo + span];
            cache[lo] = a + b;
            cache[lo + span] = a - b;
        }
        sync_threads();
    }

    const uint lane = thread % 32, warp = thread / 32;
    for (uint group = warp; group < 40; group += 8) {
        const __local float *x = cache + group * 128 + 4 * lane;
        const float4 v = (float4)(x[0],x[1],x[2],x[3]) * 0.03125f;
        float largest = as_float(as_uint(v.x) & 0x7fffffffu);
        largest = flash_max(largest, as_float(as_uint(v.y) & 0x7fffffffu));
        largest = flash_max(largest, as_float(as_uint(v.z) & 0x7fffffffu));
        largest = flash_max(largest, as_float(as_uint(v.w) & 0x7fffffffu));
        for (uint bit = 16; bit; bit /= 2) largest = flash_max(largest, flash_shuffle(largest, bit));
        const float scale = largest / 127.0f;
        const size_t packet = row / 128 + group;
        packed[packet * 32 + lane] = bonsai_quantize_q8_value(v.x,scale)
            | (bonsai_quantize_q8_value(v.y,scale) << 8)
            | (bonsai_quantize_q8_value(v.z,scale) << 16)
            | (bonsai_quantize_q8_value(v.w,scale) << 24);
        if (lane == 0) scales[packet] = scale;
    }
}
static inline __attribute__((always_inline)) void mvq_five(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[2][5] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[5];
        float dx[5];
        #pragma unroll
        for (uint t = 0; t < 5; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[5] = {0, 0, 0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 5; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 5; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 5; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}

#define MVQ_VERIFY_FIVE(N) \
__kernel void bonsai_pq2_mvq_q8_five_##N(__global const uchar *weights, __global const uchar *input, \
    __global float *output, uint width, uint rows) { \
    mvq_five(weights, input, output, width, rows, N); \
}
MVQ_VERIFY_FIVE(1)
MVQ_VERIFY_FIVE(2)

__kernel void bonsai_conv_verify_prefix_five(__global const float *input, __global const float *weight,
    __global float *final, __global float *out, __global float *first,
    __global float *second, __global float *third, __global float *fourth) {
    const uint col = work_group() * 256 + local_id(), width = 10240;
    float a = final[col], b = final[width + col], c = final[2 * width + col];
    const __global float *w = weight + 4 * col;
    __global float *history[] = {first, second, third, fourth, final};
    #pragma unroll
    for (uint t = 0; t < 5; ++t) {
        const float d = input[t * width + col];
        out[t * width + col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * d);
        history[t][col] = b;
        history[t][width + col] = c;
        history[t][2 * width + col] = d;
        a = b;
        b = c;
        c = d;
    }
}

static inline __attribute__((always_inline)) void five_tile_one(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 0 ? first + 256 : first_keys + 0;
    float query[1][3][8], sum[1][3][8], maximum[1][3], mass[1][3];
    #pragma unroll
    for (uint t = 0; t < 1; ++t) {
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[1][3] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 1; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[1][3], weight[1][3];
        #pragma unroll
        for (uint t = 0; t < 1; ++t) {
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
            for (uint t = 0; t < 1; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[1][3] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 1; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[1][3], weight[1][3];
        #pragma unroll
        for (uint t = 0; t < 1; ++t) {
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
            for (uint t = 0; t < 1; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 1; ++t) {
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



static inline __attribute__((always_inline)) void five_tile_pair(
    __global const float *q, __global const uchar *k, __global const uchar *v,
    __global float *scratch, uint first_keys, uint max_tiles, float scale, uint heads) {
    const uint lane = local_id(), job = work_group(), tile = bonsai_grid_y();
    const uint kv = job / (6 / heads), first_head = job * heads;
    const uint first = tile * 256, last = first + 256 < first_keys + 1 ? first + 256 : first_keys + 1;
    float query[2][3][8], sum[2][3][8], maximum[2][3], mass[2][3];
    #pragma unroll
    for (uint t = 0; t < 2; ++t) {
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[2][3] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[2][3], weight[2][3];
        #pragma unroll
        for (uint t = 0; t < 2; ++t) {
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
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
        const __global uchar *kr = k + (size_t)row * 576 + kv * 144;
        const __global uchar *vr = v + (size_t)row * 576 + kv * 144;
        float dot[2][3] = {{0}};
        #pragma unroll
        for (uint b = 0; b < 8; ++b) {
            const float key = q4_value(kr + 18 * b, lane);
            #pragma unroll
            for (uint t = 0; t < 2; ++t) {
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[t][h][b]), "f"(key), "f"(dot[t][h]));
                    dot[t][h] = next;
                }
            }
        }
        float previous[2][3], weight[2][3];
        #pragma unroll
        for (uint t = 0; t < 2; ++t) {
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
            for (uint t = 0; t < 2; ++t) {
                if (row >= first_keys + t) continue;
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }

    #pragma unroll
    for (uint t = 0; t < 2; ++t) {
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



static inline __attribute__((always_inline)) void five_tile_triple(
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

    const uint common_end = last < first_keys ? last : first_keys;
    for (uint row = first; row < common_end; ++row) {
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
                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    sum[t][h][b] = sum[t][h][b] * previous[t][h] + weight[t][h] * value;
                }
            }
        }
    }
    for (uint row = common_end > first ? common_end : first; row < last; ++row) {
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




static inline uint bonsai_verify_five_group(void) {
    uint group;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(group));
    return group;
}

__kernel void bonsai_verify_five_221(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    const uint group = bonsai_verify_five_group(), row = group * 2;
    q += row * 6144;
    scratch += (size_t)row * 24 * max_tiles * 258;
    if (group < 2) {
        five_tile_pair(q, k, v, scratch, keys + row, max_tiles, scale, 3);
    } else {
        five_tile_one(q, k, v, scratch, keys + row, max_tiles, scale, 3);
    }
}

__kernel void bonsai_verify_five_32(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    if (bonsai_verify_five_group() == 0) {
        five_tile_triple(q, k, v, scratch, keys, max_tiles, scale, 2);
    } else if (work_group() < 8) {
        five_tile_pair(q + 3 * 6144, k, v, scratch + (size_t)3 * 24 * max_tiles * 258,
                       keys + 3, max_tiles, scale, 3);
    }
}
static inline uint bonsai_pq2_spread_byte(uint x) {
    x = (x | (x << 12)) & 0x000f000fu;
    x = (x | (x << 6)) & 0x03030303u;
    return ((x | 0x80808080u) - 0x01010101u) ^ 0x80808080u;
}

__kernel void bonsai_pq2_spread_check(__global uint *result) {
    const uint code = work_group() * 256 + local_id();
    result[code] = bonsai_pq2_spread_byte(code);
}

static inline __attribute__((always_inline)) void mvq_tile_3_permutation(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[4][3] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[3];
        float dx[3];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[3] = {0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 3; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 3; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}



__kernel void bonsai_mvq_3_permutation_1(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_3_permutation(weights, input, output, width, rows, 1);
}


__kernel void bonsai_mvq_3_permutation_2(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_3_permutation(weights, input, output, width, rows, 2);
}


__kernel void bonsai_mvq_3_permutation_4(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_3_permutation(weights, input, output, width, rows, 4);
}

static inline __attribute__((always_inline)) void mvq_tile_3_spread(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[4][3] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[3];
        float dx[3];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[3] = {0, 0, 0};
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint w0 = bonsai_pq2_spread_byte(code & 255u);
                const uint w1 = bonsai_pq2_spread_byte(code >> 8);
                #pragma unroll
                for (uint t = 0; t < 3; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 3; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}



__kernel void bonsai_mvq_3_spread_1(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_3_spread(weights, input, output, width, rows, 1);
}


__kernel void bonsai_mvq_3_spread_2(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_3_spread(weights, input, output, width, rows, 2);
}


__kernel void bonsai_mvq_3_spread_4(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_3_spread(weights, input, output, width, rows, 4);
}

static inline __attribute__((always_inline)) void mvq_tile_4_permutation(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[4][4] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[4];
        float dx[4];
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[4] = {0, 0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 4; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 4; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}



__kernel void bonsai_mvq_4_permutation_1(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_4_permutation(weights, input, output, width, rows, 1);
}


__kernel void bonsai_mvq_4_permutation_2(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_4_permutation(weights, input, output, width, rows, 2);
}


__kernel void bonsai_mvq_4_permutation_4(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_4_permutation(weights, input, output, width, rows, 4);
}

static inline __attribute__((always_inline)) void mvq_tile_4_spread(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[4][4] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[4];
        float dx[4];
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[4] = {0, 0, 0, 0};
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint w0 = bonsai_pq2_spread_byte(code & 255u);
                const uint w1 = bonsai_pq2_spread_byte(code >> 8);
                #pragma unroll
                for (uint t = 0; t < 4; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 4; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 4; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}



__kernel void bonsai_mvq_4_spread_1(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_4_spread(weights, input, output, width, rows, 1);
}


__kernel void bonsai_mvq_4_spread_2(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_4_spread(weights, input, output, width, rows, 2);
}


__kernel void bonsai_mvq_4_spread_4(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_4_spread(weights, input, output, width, rows, 4);
}

static inline __attribute__((always_inline)) void mvq_tile_5_permutation(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[4][5] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[5];
        float dx[5];
        #pragma unroll
        for (uint t = 0; t < 5; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[5] = {0, 0, 0, 0, 0};
            const uint map = 0x020100ff;
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);
                #pragma unroll
                for (uint t = 0; t < 5; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 5; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 5; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}



__kernel void bonsai_mvq_5_permutation_1(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_5_permutation(weights, input, output, width, rows, 1);
}


__kernel void bonsai_mvq_5_permutation_2(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_5_permutation(weights, input, output, width, rows, 2);
}


__kernel void bonsai_mvq_5_permutation_4(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_5_permutation(weights, input, output, width, rows, 4);
}

static inline __attribute__((always_inline)) void mvq_tile_5_spread(
    __global const uchar *weights, __global const uchar *input,
    __global float *output, uint width, uint rows, uint grouping) {
    const uint thread = local_id(), lane = thread & 31;
    const uint start = (work_group() * 4 + thread / 32) * grouping;
    if (start >= rows) return;
    const uint groups = width / 32, blocks = width / 128;
    float values[4][5] = {{0}};
    for (uint i = lane; i < groups; i += 32) {
        const __global uchar *a[5];
        float dx[5];
        #pragma unroll
        for (uint t = 0; t < 5; ++t) {
            a[t] = input + ((size_t)t * groups + i) * 36;
            dx[t] = mvq_half(*(__global const ushort *)a[t]);
        }
        #pragma unroll
        for (uint r = 0; r < grouping; ++r) {
            const uint row = start + r < rows ? start + r : rows - 1;
            const __global uchar *w = weights + ((size_t)row * blocks + i/4) * 34;
            const __global ushort *pairs = (__global const ushort *)(w + 2 + (i % 4) * 8);
            const float dw = mvq_half(*(__global const ushort *)w);
            int dot[5] = {0, 0, 0, 0, 0};
            #pragma unroll
            for (uint j = 0; j < 4; ++j) {
                const uint code = pairs[j];
                const uint w0 = bonsai_pq2_spread_byte(code & 255u);
                const uint w1 = bonsai_pq2_spread_byte(code >> 8);
                #pragma unroll
                for (uint t = 0; t < 5; ++t) {
                    const __global uint *q = (__global const uint *)(a[t] + 4);
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w0), "r"(q[2*j]));
                    __asm__("dp4a.s32.s32 %0, %1, %2, %0;" : "+r"(dot[t]) : "r"(w1), "r"(q[2*j+1]));
                }
            }
            #pragma unroll
            for (uint t = 0; t < 5; ++t) values[r][t] += (dw * dx[t]) * (float)dot[t];
        }
    }
    #pragma unroll
    for (uint r = 0; r < grouping; ++r) {
        #pragma unroll
        for (uint t = 0; t < 5; ++t) {
            for (uint bit = 16; bit; bit >>= 1) values[r][t] += flash_shuffle(values[r][t], bit);
            if (lane == 0 && start + r < rows) output[(size_t)t * rows + start + r] = values[r][t];
        }
    }
}



__kernel void bonsai_mvq_5_spread_1(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_5_spread(weights, input, output, width, rows, 1);
}


__kernel void bonsai_mvq_5_spread_2(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_5_spread(weights, input, output, width, rows, 2);
}


__kernel void bonsai_mvq_5_spread_4(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {
    mvq_tile_5_spread(weights, input, output, width, rows, 4);
}
