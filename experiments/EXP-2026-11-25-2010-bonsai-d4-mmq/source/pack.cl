float __nv_roundf(float);

static uint d4_code(float x, float inv) {
    const float rounded = __nv_roundf(x * inv);
    return (uint)(int)rounded & 255u;
}

__kernel void bonsai_pack_d4(__global const float *input, __global uint *packed,
    __global float *scale, uint groups) {
    const uint lane = local_id() & 7;
    const uint group = work_group() * 16 + local_id() / 8;
    const __global float *p = input + (size_t)(group < groups ? group : 0) * 32 + 4 * lane;
    const float4 x = (float4)(p[0], p[1], p[2], p[3]);
    float peak = flash_max(flash_max(as_float(as_uint(x.x) & 0x7fffffffu),
                                    as_float(as_uint(x.y) & 0x7fffffffu)),
                           flash_max(as_float(as_uint(x.z) & 0x7fffffffu),
                                    as_float(as_uint(x.w) & 0x7fffffffu)));
    for (uint offset = 4; offset; offset >>= 1) {
        uint other;
        __asm__("shfl.sync.bfly.b32 %0, %1, %2, 31, -1;"
            : "=r"(other) : "r"(as_uint(peak)), "r"(offset));
        peak = flash_max(peak, as_float(other));
    }
    float inverse = 0.0f, step = 0.0f;
    if (peak != 0.0f) {
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(inverse) : "f"(127.0f), "f"(peak));
        __asm__("div.rn.f32 %0, %1, %2;" : "=f"(step) : "f"(1.0f), "f"(inverse));
    }
    if (group >= groups) return;
    packed[(size_t)group * 8 + lane] = d4_code(x.x, inverse) | (d4_code(x.y, inverse) << 8)
        | (d4_code(x.z, inverse) << 16) | (d4_code(x.w, inverse) << 24);
    if (lane == 0) scale[group] = step;
}
