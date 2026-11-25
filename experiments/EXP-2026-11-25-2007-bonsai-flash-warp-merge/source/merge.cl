static inline float merge_from_lane(float value, uint source) {
    uint other;
    __asm__("shfl.sync.idx.b32 %0, %1, %2, 31, -1;"
        : "=r"(other) : "r"(as_uint(value)), "r"(source));
    return as_float(other);
}

static inline __attribute__((always_inline)) void merge_warp(
    __global const float *partial, __global float *output, uint queries,
    uint first_visible, uint tile_count, uint group_start, uint warps) {
    const uint group = work_group() / FLASH_ROWS;
    const uint work = group + group_start;
    const uint slot = work_group() % FLASH_ROWS;
    const uint groups_per_head = (queries * 6 + FLASH_ROWS - 1) / FLASH_ROWS;
    const uint original = (work % groups_per_head) * FLASH_ROWS + slot;
    const uint token = original / 6;
    const uint head = (work / groups_per_head) * 6 + original % 6;
    if (token >= queries) return;

    const uint thread = local_id();
    const uint lane = thread & 31;
    const uint rows = (first_visible + token + FLASH_KEYS - 1) / FLASH_KEYS;
    const size_t stride = FLASH_ROWS * FLASH_PARTIAL;
    const __global float *part = partial + ((size_t)group * tile_count * FLASH_ROWS + slot) * FLASH_PARTIAL;
    float2 values[4];
    #pragma unroll
    for (uint pair = 0; pair < 4 / warps; ++pair) values[pair] = (float2)(0.0f);
    float maximum = as_float(0xff800000u);
    float sum = 0.0f;

    for (uint base = 0; base < rows; base += 32, part += 32 * stride) {
        const uint active = flash_min(rows - base, 32);
        const __global float *tile = part + (size_t)lane * stride;
        const float mx = lane < active ? tile[0] : as_float(0xff800000u);
        float seen = flash_max(maximum, mx);
        #pragma unroll
        for (uint bit = 1; bit < 32; bit *= 2) {
            const float prior = bonsai_shuffle_up(seen, bit);
            if (lane >= bit) seen = flash_max(prior, seen);
        }
        const float prior = bonsai_shuffle_up(seen, 1);
        const float keep = flash_exp((lane == 0 ? maximum : prior) - seen);
        const float add = flash_exp(mx - seen);
        float weighted_mass = 0.0f;
        if (lane < active) weighted_mass = tile[1] * add;
        maximum = bonsai_last_lane(seen);
        for (uint i = 0; i < active; ++i) {
            const float old = merge_from_lane(keep, i);
            const float weight = merge_from_lane(add, i);
            const float mass = merge_from_lane(weighted_mass, i);
            if (lane == 0) sum = sum * old + mass;
            const __global float2 *src = (__global const float2 *)(part + (size_t)i * stride + 2);
            #pragma unroll
            for (uint pair = 0; pair < 4 / warps; ++pair) {
                const float2 input = src[thread + pair * warps * 32];
                values[pair] = values[pair] * old + input * weight;
            }
        }
    }
    sum = merge_from_lane(sum, 0);
    float inverse;
    __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(inverse) : "f"(sum));
    __global float2 *dst = (__global float2 *)(output + ((size_t)token * 24 + head) * 256);
    #pragma unroll
    for (uint pair = 0; pair < 4 / warps; ++pair) dst[thread + pair * warps * 32] = values[pair] * inverse;
}

#define MERGE(W) \
__kernel void bonsai_flash_warp_merge##W(__global const float *partial, __global float *output, \
    uint queries, uint first_visible, uint tile_count, uint group_start) { \
    merge_warp(partial, output, queries, first_visible, tile_count, group_start, W); \
}
MERGE(1)
MERGE(2)
MERGE(4)
