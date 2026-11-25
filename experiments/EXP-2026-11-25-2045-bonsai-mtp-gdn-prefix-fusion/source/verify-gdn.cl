static inline float bonsai_gdn_fma(float a, float b, float c) {
    float result;
    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(result) : "f"(a), "f"(b), "f"(c));
    return result;
}

static inline float bonsai_gdn_sum8(float value) {
    for (uint distance = 4; distance; distance >>= 1) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 6175, -1;"
            : "=f"(peer) : "f"(value), "r"(distance));
        value += peer;
    }
    return value;
}

__kernel void bonsai_verify_gdn_prefix(__global const float *q, __global const float *k,
    __global const float *v, __global const float *decay, __global const float *beta,
    __global const float *initial, __global float *output, __global float *final,
    __global float *first, __global float *second, __global float *third, uint tokens) {
    uint tile;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(tile));
    const uint head = work_group(), thread = local_id(), lane = thread % 8;
    const uint column = tile * 16 + thread / 8;
    const size_t at = ((size_t)head * 128 + column) * 128 + lane;
    const uint qk_head = (head % 16) * 128 + lane;
    float state[16];
    #pragma unroll
    for (uint r = 0; r < 16; ++r) state[r] = initial[at + r * 8];

    for (uint t = 0; t < tokens; ++t) {
        __global const float *key = k + (size_t)t * 4096 + qk_head;
        __global const float *query = q + (size_t)t * 4096 + qk_head;
        const float g = attention_exp(decay[t * 48 + head]);
        const float b = beta[t * 48 + head];
        float product = 0.0f;
        #pragma unroll
        for (uint r = 0; r < 16; ++r) product = bonsai_gdn_fma(state[r], key[r * 8], product);
        const float reduction = bonsai_gdn_sum8(product);
        const float delta = b * (v[t * 10240 + head * 128 + column] - g * reduction);
        __global float *destination = t + 1 == tokens ? final : t == 0 ? first : t == 1 ? second : third;
        float result = 0.0f;
        #pragma unroll
        for (uint r = 0; r < 16; ++r) {
            state[r] = bonsai_gdn_fma(g, state[r], key[r * 8] * delta);
            result = bonsai_gdn_fma(state[r], query[r * 8], result);
            destination[at + r * 8] = state[r];
        }
        result = bonsai_gdn_sum8(result);
        if (lane == 0) output[t * 6144 + head * 128 + column] = result * 0x1.6a09e6p-4f;
    }
}
