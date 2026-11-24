#include "rms-reference-types.h"

static float fg168_load(uint64_t p, uint64_t i) { return fg_device_global_load(p + 4 * i); }
static void fg168_store(uint64_t p, uint64_t i, float v) { fg_device_global_store(p + 4 * i, v); }
static RmsVec_g fg168_load4(uint64_t p, int32_t i) {
    const float4 value = ((__global const float4 *) p)[i];
    return (RmsVec_g) {value.x, value.y, value.z, value.w};
}
static void fg168_store4(uint64_t p, int32_t i, RmsVec_g v) {
    ((__global float4 *) p)[i] = (float4) (v.x_g, v.y_g, v.z_g, v.w_g);
}
static float fg168_shuffle(float v, int32_t delta) { return fg_device_shfl(v, delta); }
static void fg168_barrier(void) { fg_device_barrier(); }
static float fg168_fma(float a, float b, float c) { return fg_device_fma_ftz(a, b, c); }
static float fg168_rsqrt(float v) {
    float value;
    __asm__("rsqrt.approx.f32 %0, %1;" : "=f"(value) : "f"(v));
    return value;
}
static float fg_rms_shared_load(uint64_t p, uint64_t i) { return ((__local float *) p)[i]; }
static void fg_rms_shared_store(uint64_t p, uint64_t i, float v) { ((__local float *) p)[i] = v; }

#include "rms-reference-device.h"

__kernel void goose_rms_reference(__global uint8_t * x, __global uint8_t * weights,
    __global uint8_t * out, vector_launch_packet cfg, float eps, int32_t vectors, float postscale) {
    uint32_t row, thread, lanes;
    __local float shared[32];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("mov.u32 %0, %%ntid.x;" : "=r"(lanes));
    fg_reference_rms_g((uint64_t) x, (uint64_t) weights, (uint64_t) out, (uint64_t) cfg.words, eps,
        (uint64_t) shared, row, thread, lanes, vectors, postscale);
}

// One-vector variant: same row arithmetic as goose_rms_reference with the vector
// count fixed at one, so the staging array of the general entry is not needed and the
// kernel carries no local frame.
__kernel void goose_rms_reference_one(__global uint8_t * x, __global uint8_t * weights,
    __global uint8_t * out, vector_launch_packet cfg, float eps, float postscale) {
    uint32_t row, thread, lanes;
    __local float shared[32];
    __asm__("mov.u32 %0, %%ctaid.x;" : "=r"(row));
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    __asm__("mov.u32 %0, %%ntid.x;" : "=r"(lanes));
    fg_reference_rms_one_g((uint64_t) x, (uint64_t) weights, (uint64_t) out, (uint64_t) cfg.words, eps,
        (uint64_t) shared, row, thread, lanes, postscale);
}

__kernel void goose_rms_decode_2048(__global const uint8_t * x,
    __global const uint8_t * weights, __global uint8_t * out, float eps) {
    uint32_t thread;
    __local float shared[32];
    __asm__("mov.u32 %0, %%tid.x;" : "=r"(thread));
    fg_rms_decode_2048_g((uint64_t) x, (uint64_t) weights, (uint64_t) out,
                        (uint64_t) shared, thread, eps);
}
