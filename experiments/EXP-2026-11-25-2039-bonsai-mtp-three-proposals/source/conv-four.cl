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
