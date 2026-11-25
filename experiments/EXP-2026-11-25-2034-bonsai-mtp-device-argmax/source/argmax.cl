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
