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
