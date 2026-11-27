#pragma OPENCL EXTENSION cl_khr_fp64 : enable
extern float __nv_sqrtf(float);
extern float __nv_logf(float);

static float read_history(__global const float *input, __global const float *past, int token, int channel) {
    return token >= 0 ? input[1024 * token + channel] : past[3 * channel + 3 + token];
}

static float convolve(__global const float *input, __global const float *past,
                      __global const float *filter, __global const float *taps,
                      int token, int channel, int sliding) {
    float h0 = read_history(input, past, token - 3, channel);
    float h1 = read_history(input, past, token - 2, channel);
    float h2 = read_history(input, past, token - 1, channel);
    float current = input[1024 * token + channel];
    int at = 4 * channel;
    float mixed = 0.0f;
    mixed += h0 * filter[at];
    mixed += h1 * filter[at + 1];
    mixed += h2 * filter[at + 2];
    mixed += current * filter[at + 3];
    if (!sliding) {
        int tap = 32 * token + 4 * (channel / 128);
        mixed += current * taps[tap];
        mixed += h2 * taps[tap + 1];
        mixed += h1 * taps[tap + 2];
        mixed += h0 * taps[tap + 3];
    }
    const float scale = sliding ? 1.0f / __nv_sqrtf(5.0f) : 1.0f / __nv_sqrtf(9.0f);
    return (current + mixed) * scale;
}

__kernel void goose_i2_prepare_attention(__global const float *raw, __global const float *coeff,
    __global const float *history, __global ushort *q, __global float *r, __global float *k,
    __global float *v, int count, int first, int sliding) {
    uint head, row, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(head));
    __asm__("mov.u32 %0, %%ctaid.y;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const int relative = head >= 24 && head < 48;
    const uint width = relative ? 16 : 128;
    const uint offset = head < 24 ? head * 128 : relative ? (head - 24) * 16 : ((head - 48) & 7) * 128;
    const uint ch = offset + lane;
    float value = 0.0f;
    if (head < 24) {
        value = raw[3072 * row + ch] * coeff[ch];
    } else if (relative && lane < width) {
        value = raw[5120 * count + 384 * row + ch] * coeff[3072 + ch];
    } else if (!relative) {
        const int is_value = head >= 56;
        value = convolve(raw + (is_value ? 4096 : 3072) * count,
                         history + (is_value ? 3072 : 0),
                         coeff + 3456 + (is_value ? 4096 : 0),
                         raw + (is_value ? 8632 : 8600) * count, row, ch, sliding);
    }

    __local double sum[128];
    const float square = value * value;
    sum[lane] = (double)square;
    __asm__ volatile("bar.sync 0;" ::: "memory");
    for (uint stride = 64; stride; stride >>= 1) {
        if (lane < stride) sum[lane] += sum[lane + stride];
        __asm__ volatile("bar.sync 0;" ::: "memory");
    }
    if (lane >= width) return;
    float scale;
    if (head >= 56) {
        scale = 1.0f / __nv_sqrtf((float)sum[0] / 128.0f + 1e-6f);
    } else {
        const float clip = (relative ? 8.0f : 22.627416610717773f) / (__nv_sqrtf((float)sum[0]) + 1e-6f);
        scale = clip < 1.0f ? clip : 1.0f;
    }
    value *= scale;
    if (head < 48 && !sliding && first + (int)row + 1 > 8192) {
        const float tau = 1.0f + 0.1f * __nv_logf((float)(first + row + 1) / 8192.0f);
        value *= tau;
    }
    if (head < 24) {
        ushort half_word;
        __asm__("cvt.rn.f16.f32 %0, %1;" : "=h"(half_word) : "f"(value));
        q[3072 * row + ch] = half_word;
    } else if (relative) r[384 * row + ch] = value;
    else if (head < 56) k[1024 * row + ch] = value;
    else v[1024 * row + ch] = value;
}

__kernel void goose_i2_update_history(__global const float *raw, __global float *history, int count) {
    uint block, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint channel = (block * 128 + lane) % 1024;
    const int is_value = block >= 8;
    const __global float *input = raw + (is_value ? 4096 : 3072) * count;
    __global float *past = history + (is_value ? 3072 : 0);
    float h0 = read_history(input, past, count - 3, channel);
    float h1 = read_history(input, past, count - 2, channel);
    float h2 = read_history(input, past, count - 1, channel);
    past[3 * channel] = h0;
    past[3 * channel + 1] = h1;
    past[3 * channel + 2] = h2;
}
