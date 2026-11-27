extern float __nv_expf(float);

__kernel void goose_i2_attention_gate(__global float *output, __global const float *gate, uint count) {
    uint group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint i = group * 256 + lane;
    if (i < count) output[i] *= 1.0f / (1.0f + __nv_expf(-gate[i]));
}
