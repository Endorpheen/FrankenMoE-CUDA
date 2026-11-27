extern float __nv_tanhf(float);
__kernel void goose_tanh_f32(__global const float *src, __global float *dst, uint count) {
    uint group, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(group));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint i = group * 256 + lane;
    if (i < count) dst[i] = __nv_tanhf(src[i]);
}
