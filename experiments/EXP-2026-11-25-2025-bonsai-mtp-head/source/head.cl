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
