extern float __nv_expf(float);
extern float __nv_log1pf(float);
extern float __nv_sqrtf(float);

static float warp_sum(float sum) {
    for (int bit = 16; bit > 0; bit /= 2) {
        float peer;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;" : "=f"(peer) : "f"(sum), "r"(bit));
        sum += peer;
    }
    return sum;
}

static float silu(float x) {
    return x / (1.0f + __nv_expf(-x));
}

__kernel void bonsai_norm(__global const float *x, __global const float *weight,
                          __global const float *gate, __global float *out,
                          uint width, float eps, uint mode) {
    const uint lane = local_id(), row = work_group();
    float sum = 0.0f;
    for (uint i = lane; i < width; i += 32) {
        const float value = x[row * width + i];
        sum += value * value;
    }
    sum = warp_sum(sum);
    const float l2 = __nv_sqrtf(sum);
    const float d = mode == 1 ? (l2 > eps ? l2 : eps)
                              : __nv_sqrtf(sum / (float)width + eps);
    for (uint i = lane; i < width; i += 32) {
        const uint index = row * width + i;
        float value = x[index] / d;
        if (mode != 1) value *= weight[i];
        if (mode == 2) value *= silu(gate[index]);
        out[index] = value;
    }
}

__kernel void bonsai_decay(__global const float *x, __global const float *alpha,
                           __global const float *beta, __global const float *dt,
                           __global const float *a, __global float *gate,
                           __global float *mix) {
    const uint lane = local_id(), head = work_group();
    float al = 0.0f, be = 0.0f;
    for (uint i = lane; i < 5120; i += 32) {
        al += x[i] * alpha[head * 5120 + i];
        be += x[i] * beta[head * 5120 + i];
    }
    al = warp_sum(al);
    be = warp_sum(be);
    if (lane == 0) {
        const float p = al + dt[head];
        gate[head] = a[head] * (p > 0.0f ? p + __nv_log1pf(__nv_expf(-p)) : __nv_log1pf(__nv_expf(p)));
        mix[head] = 1.0f / (1.0f + __nv_expf(-be));
    }
}

__kernel void bonsai_conv_step(__global const float *input, __global const float *weight,
                               __global float *history, __global float *out) {
    const uint col = work_group() * 256 + local_id();
    const uint width = 10240;
    const float a = history[col], b = history[width + col], c = history[2 * width + col];
    const float d = input[col];
    const __global float *w = weight + 4 * col;
    out[col] = silu(((w[0] * a + w[1] * b) + w[2] * c) + w[3] * d);
    history[col] = b;
    history[width + col] = c;
    history[2 * width + col] = d;
}

__kernel void bonsai_residual(__global const float *a, __global const float *b,
                              __global float *out) {
    const uint col = work_group() * 256 + local_id();
    out[col] = a[col] + b[col];
}
