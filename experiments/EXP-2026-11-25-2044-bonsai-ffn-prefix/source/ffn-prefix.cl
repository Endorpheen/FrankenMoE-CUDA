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
