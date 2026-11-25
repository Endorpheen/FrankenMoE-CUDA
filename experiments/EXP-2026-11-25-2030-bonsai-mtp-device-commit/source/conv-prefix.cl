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
