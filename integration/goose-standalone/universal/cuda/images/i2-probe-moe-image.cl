__kernel void goose_i2_moe_epilogue(__global const float *input, __global const float *residual,
    __global const float *weights, __global float *history, __global float *output, int rows) {
    uint group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint channel = group * 128 + lane;
    const uint width = 2304;
    const uint h = 3 * channel;
    const uint w = 4 * channel;
    float h0 = history[h], h1 = history[h + 1], h2 = history[h + 2];
    for (int row = 0; row < rows; ++row) {
        const uint i = row * width + channel;
        const float current = input[i];
        float sum = 0.0f;
        sum += h0 * weights[w];
        sum += h1 * weights[w + 1];
        sum += h2 * weights[w + 2];
        sum += current * weights[w + 3];
        output[i] = (current + sum) * 0x1.c9f25cp-2f + residual[i];
        h0 = h1;
        h1 = h2;
        h2 = current;
    }
    history[h] = h0;
    history[h + 1] = h1;
    history[h + 2] = h2;
}
