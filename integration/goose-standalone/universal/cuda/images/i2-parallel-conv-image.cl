__kernel void i2_postconv_parallel(__global const float *input,
    __global const float *weights, __global const float *history,
    __global float *output, const int rows) {
    uint group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const int channel = group * 128 + lane;
    int row;
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(row));
    const int width = 2304;
    if (row >= rows) return;
    const int offset = row * width + channel;
    const int weight = 4 * channel;
    float sum = 0.0f;
    for (int tap = 0; tap < 4; ++tap) {
        const int source = row + tap - 3;
        const float value = source < 0 ?
            history[3 * channel + source + 3] : input[source * width + channel];
        sum += value * weights[weight + tap];
    }
    output[offset] = (input[offset] + sum) * 0x1.c9f25cp-2f;
}

__kernel void i2_postconv_commit(__global const float *input,
    __global float *history, const int rows) {
    uint group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const int channel = group * 128 + lane;
    const int width = 2304;
    const int offset = 3 * channel;
    for (int tap = 0; tap < 3; ++tap) {
        const int source = rows + tap - 3;
        history[offset + tap] = source < 0 ?
            history[offset + source + 3] : input[source * width + channel];
    }
}
