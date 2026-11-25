static uint bonsai_grid_y(void) {
    uint n;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(n));
    return n;
}

__kernel void bonsai_embed_batch(__global const uchar *weights, __global float *output,
                                __global const uint *token_ids) {
    const uint token = bonsai_grid_y();
    const uint col = work_group() * 256 + local_id();
    const __global uchar *row = weights + (ulong)token_ids[token] * 1360;
    output[(ulong)token * 5120 + col] = pq2_element(row, col);
}

__kernel void bonsai_conv_batch(__global const float *input, __global const float *weight,
                               __global float *history, __global float *out, uint count) {
    const uint col = work_group() * 256 + local_id();
    const uint width = 10240;
    float a = history[col], b = history[width + col], c = history[2 * width + col];
    const __global float *w = weight + 4 * col;
    for (uint token = 0; token < count; ++token) {
        const uint at = token * width + col;
        const float d = input[at];
        out[at] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * d);
        a = b;
        b = c;
        c = d;
    }
    history[col] = a;
    history[width + col] = b;
    history[2 * width + col] = c;
}
