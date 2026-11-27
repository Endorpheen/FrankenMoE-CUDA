extern float __nv_tanhf(float);

static float route_exp(float value) {
    float result;
    __asm__("ex2.approx.ftz.f32 %0, %1;" : "=f"(result) : "f"(value * 1.4426950408889634f));
    return result;
}

__kernel void goose_i2_moe_situ(__global const float *gate_up, __global float *output,
                              uint elements, uint activation_reference) {
    uint block, lane;
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(block));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(lane));
    const uint i = 256 * block + lane;
    if (i >= elements) return;
    const uint source = i + 1152 * (i / 1152);
    const float gate = gate_up[source], up = gate_up[source + 1152];
    const float gate_clip = __nv_tanhf(gate * 0.25f + 0.0f) * 4.0f + 0.0f;
    const float up_clip = __nv_tanhf(up * as_float(1025758986u) + 0.0f) * 25.0f + 0.0f;
    const float denominator = 1.0f + route_exp(-gate);
    float sigmoid;
    if (activation_reference) {
        __asm__("rcp.approx.ftz.f32 %0, %1;" : "=f"(sigmoid) : "f"(denominator));
    } else {
        sigmoid = 1.0f / denominator;
    }
    output[i] = (gate_clip * sigmoid) * up_clip;
}
