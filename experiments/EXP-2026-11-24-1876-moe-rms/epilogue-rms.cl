#pragma OPENCL FP_CONTRACT OFF
static float fg_moe_rms_warp(float value) {
    for (int delta = 16; delta > 0; delta >>= 1)
        value += fg_device_shfl(value, delta);
    return value;
}

__kernel void goose_moe_epilogue_rms(__global const float4 * routed,
    __global const float4 * shared, __global const float * gate, __global const float4 * residual,
    __global const float4 * weight, __global float4 * layer_out, __global float4 * norm_out, float epsilon) {
    uint32_t row, thread;
    __local float warp_sums[16];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const unsigned lane = thread % 32;
    const uint64_t index = (uint64_t) row * 512 + thread;
    const float4 scaled = shared[index] * gate[row];
    const float4 value = (routed[index] + scaled) + residual[index];
    layer_out[index] = value;
    const float xy = fg_device_fma_ftz(value.x, value.x, value.y * value.y);
    const float xyz = fg_device_fma_ftz(value.z, value.z, xy);
    const float sum = fg_moe_rms_warp(0.0f + fg_device_fma_ftz(value.w, value.w, xyz));
    if (lane == 0) warp_sums[thread / 32] = sum;
    fg_device_barrier();
    const float squares = fg_moe_rms_warp(lane < 16 ? warp_sums[lane] : 0.0f);
    const float scale = fg168_rsqrt(squares / 2048.0f + epsilon);
    float4 norm = scale * value;
    if (weight) norm *= weight[thread];
    norm_out[index] = norm;
}
#pragma OPENCL FP_CONTRACT ON
