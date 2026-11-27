#pragma OPENCL FP_CONTRACT OFF
__kernel void goose_moe_shared_residual_prefill(__global const float4 * routed,
    __global const float4 * shared, __global const float * gate,
    __global const float4 * residual, __global float4 * out, uint32_t vectors) {
    uint32_t group, thread;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    const uint32_t index = group * 128 + thread;
    if (index < vectors) {
        const float4 product = shared[index] * gate[index / 512];
        const float4 mixture = routed[index] + product;
        out[index] = mixture + residual[index];
    }
}
